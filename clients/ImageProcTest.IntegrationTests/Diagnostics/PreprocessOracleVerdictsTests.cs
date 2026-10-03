// GUI-C-219 / 219b / 219c (#249): the oracle's verdict is produced off the asking thread, once, from a private snapshot of the DLLs, and shared.
using System.Collections.Concurrent;
using System.Diagnostics;
using System.Security.Cryptography;

namespace ImageProcTest.IntegrationTests.Diagnostics;

/// <summary>
/// 212b moved the preprocess synthetic oracle into a child process and left the legacy app asking for it three times, synchronously, on the UI thread at startup: the window stopped answering for
/// a median of 963 ms and up to 4.4 s (GUI-C-218). The holder under test produces the verdict on a thread-pool thread from a PRIVATE SNAPSHOT of the DLLs (GUI-C-219c: what is hashed is what the
/// worker loads), keeps it per DLL, and answers "not yet" without waiting or reading the file. Its <c>Runner</c> is replaced here by a controllable stand-in, so the threading, the sharing and
/// the bytes the worker is given are observed rather than inferred.
/// </summary>
[Collection("PreprocessOracleVerdicts")]   // the holder is process-wide state: these tests take turns
[Trait("Category", "P1AReady")]
public sealed class PreprocessOracleVerdictsTests : IDisposable
{
    private readonly string _directory;
    private readonly string _dll;

    public PreprocessOracleVerdictsTests()
    {
        PreprocessOracleVerdicts.ResetForTests();
        _directory = Path.Combine(Path.GetTempPath(), $"xpe_verdict_test_{Guid.NewGuid():N}");
        Directory.CreateDirectory(_directory);
        _dll = Path.Combine(_directory, "xpe_preprocess.dll");
        File.WriteAllText(_dll, "x");
    }

    public void Dispose()
    {
        OracleThreadGuard.OnUiThread = null;
        PreprocessOracleVerdicts.ResetForTests();
        try { Directory.Delete(_directory, recursive: true); } catch (IOException) { /* temp folder */ }
    }

    private static PreprocessSyntheticOracleResult Verdict(string status) =>
        PreprocessSyntheticOracleResult.Failed(status, "stand-in");

    private static string Sha(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes));

    private void Replace(string content, DateTime? stamp = null)
    {
        File.WriteAllText(_dll, content);
        if (stamp is not null) File.SetLastWriteTimeUtc(_dll, stamp.Value);
    }

    [Fact]
    public void TryGet_NeverWaits_AnswersNullWhileTheRunIsInProgress_ThenTheVerdict()
    {
        using var release = new ManualResetEventSlim();
        using var started = new ManualResetEventSlim();
        PreprocessOracleVerdicts.Runner = _ => { started.Set(); release.Wait(); return Verdict("done"); };
        using var completed = new ManualResetEventSlim();
        PreprocessOracleVerdicts.Completed += _ => completed.Set();

        var watch = Stopwatch.StartNew();
        var first = PreprocessOracleVerdicts.TryGet(_dll);
        watch.Stop();

        Assert.Null(first);
        Assert.True(watch.ElapsedMilliseconds < 1000, $"TryGet waited {watch.ElapsedMilliseconds} ms for a run that was blocked");
        Assert.True(started.Wait(5000), "the run was never started");
        Assert.Null(PreprocessOracleVerdicts.TryGet(_dll));   // still in progress, still not waiting

        release.Set();
        Assert.True(completed.Wait(5000), "the completion was never announced");
        Assert.Equal("done", PreprocessOracleVerdicts.TryGet(_dll)!.Status);
    }

    /// <summary>The point of the card: the oracle must not run on the thread that asked (the UI thread, in the app).</summary>
    [Fact]
    public void TheRun_HappensOnAnotherThread_ANonBlockingPoolThread()
    {
        var runnerThread = -1;
        var runnerIsPool = false;
        using var done = new ManualResetEventSlim();
        PreprocessOracleVerdicts.Runner = _ =>
        {
            runnerThread = Environment.CurrentManagedThreadId;
            runnerIsPool = Thread.CurrentThread.IsThreadPoolThread;
            done.Set();
            return Verdict("ok");
        };

        PreprocessOracleVerdicts.TryGet(_dll);

        Assert.True(done.Wait(5000));
        Assert.NotEqual(Environment.CurrentManagedThreadId, runnerThread);
        Assert.True(runnerIsPool);
    }

    /// <summary>
    /// GUI-C-219c (Codex #112, medium): the asking thread does not read the DLL. The copy and the hash are on the worker; here the worker is held right after the copy (a slow disk) and the
    /// asker, which stands for the UI thread, still gets its answer at once, every time it asks.
    /// </summary>
    [Fact]
    public void TryGet_DoesNotReadTheFileOnTheCallersThread_EvenWhenTheReadIsSlow()
    {
        using var hold = new ManualResetEventSlim();
        using var inCopy = new ManualResetEventSlim();
        PreprocessOracleVerdicts.AfterSnapshotCopy = _ => { inCopy.Set(); hold.Wait(15000); };   // bounded: a regression that reads on the caller's thread fails the timing check instead of hanging
        PreprocessOracleVerdicts.Runner = _ => Verdict("done");

        var watch = Stopwatch.StartNew();
        var first = PreprocessOracleVerdicts.TryGet(_dll);
        var second = PreprocessOracleVerdicts.TryGet(_dll);
        watch.Stop();

        Assert.Null(first);
        Assert.Null(second);
        Assert.True(inCopy.Wait(5000), "the snapshot was never started");
        Assert.True(watch.ElapsedMilliseconds < 300, $"two asks took {watch.ElapsedMilliseconds} ms while the worker's file read was held");
        hold.Set();
        Assert.Equal("done", PreprocessOracleVerdicts.Wait(_dll).Status);
    }

    /// <summary>Three asks (the diagnostics report, the module readiness, a second refresh) are one run.</summary>
    [Fact]
    public void ManyAsks_AreOneRun_AndEveryAskerGetsTheSameVerdict()
    {
        var runs = 0;
        using var release = new ManualResetEventSlim();
        PreprocessOracleVerdicts.Runner = _ => { Interlocked.Increment(ref runs); release.Wait(); return Verdict("shared"); };

        PreprocessOracleVerdicts.TryGet(_dll);
        PreprocessOracleVerdicts.TryGet(_dll);
        var waiter = Task.Run(() => PreprocessOracleVerdicts.Wait(_dll));
        PreprocessOracleVerdicts.TryGet(_dll);
        release.Set();
        var viaWait = waiter.GetAwaiter().GetResult();
        var viaTry = PreprocessOracleVerdicts.TryGet(_dll);

        Assert.Equal(1, runs);
        Assert.Same(viaWait, viaTry);
        Assert.Equal("shared", viaWait.Status);
    }

    [Fact]
    public void Wait_ReturnsTheVerdict_ForCallersWithNoWindowToKeepResponsive()
    {
        PreprocessOracleVerdicts.Runner = _ => { Thread.Sleep(150); return Verdict("waited"); };

        var verdict = PreprocessOracleVerdicts.Wait(_dll);

        Assert.Equal("waited", verdict.Status);
    }

    [Fact]
    public void Invalidate_DiscardsAFinishedVerdict_SoTheNextAskRunsAgain_AndOnlyThen()
    {
        var runs = 0;
        PreprocessOracleVerdicts.Runner = _ => { Interlocked.Increment(ref runs); return Verdict($"run{runs}"); };

        Assert.Equal("run1", PreprocessOracleVerdicts.Wait(_dll).Status);
        Assert.Equal("run1", PreprocessOracleVerdicts.TryGet(_dll)!.Status);   // asking again does not run it again
        Assert.Equal(1, runs);

        PreprocessOracleVerdicts.Invalidate();
        Assert.Equal("run2", PreprocessOracleVerdicts.Wait(_dll).Status);
        Assert.Equal(2, runs);
    }

    [Fact]
    public void Invalidate_LeavesARunInProgressAlone_NoSecondConcurrentRun()
    {
        var runs = 0;
        using var release = new ManualResetEventSlim();
        PreprocessOracleVerdicts.Runner = _ => { Interlocked.Increment(ref runs); release.Wait(); return Verdict("only"); };

        PreprocessOracleVerdicts.TryGet(_dll);
        PreprocessOracleVerdicts.Invalidate();
        PreprocessOracleVerdicts.TryGet(_dll);
        release.Set();
        PreprocessOracleVerdicts.Wait(_dll);

        Assert.Equal(1, runs);
    }

    /// <summary>GUI-C-219b: a verdict belongs to a file's CONTENT. Touching the file (a new write time, the same bytes) is not a new subject.</summary>
    [Fact]
    public void ATimestampChangeAlone_IsNotANewSubject()
    {
        var runs = 0;
        PreprocessOracleVerdicts.Runner = _ => { Interlocked.Increment(ref runs); return Verdict("v"); };
        PreprocessOracleVerdicts.Wait(_dll);

        File.SetLastWriteTimeUtc(_dll, DateTime.UtcNow.AddMinutes(5));
        PreprocessOracleVerdicts.Wait(_dll);

        Assert.Equal(1, runs);
    }

    /// <summary>
    /// GUI-C-219b (Codex #109, high): a DIFFERENT file of the same size and the same write time is another subject. The key used to be the path and the write time, so this was handed the old
    /// verdict, and a binary that had never been checked became "ready".
    /// </summary>
    [Fact]
    public void ADifferentFileWithTheSameSizeAndTimestamp_IsANewSubject()
    {
        var runs = 0;
        PreprocessOracleVerdicts.Runner = _ => { Interlocked.Increment(ref runs); return Verdict($"run{runs}"); };
        Assert.Equal("run1", PreprocessOracleVerdicts.Wait(_dll).Status);
        var stamp = File.GetLastWriteTimeUtc(_dll);
        var size = new FileInfo(_dll).Length;

        Replace("y", stamp);                       // the same length (1 byte), other bytes, the time put back
        Assert.Equal(size, new FileInfo(_dll).Length);
        Assert.Equal(stamp, File.GetLastWriteTimeUtc(_dll));

        Assert.Equal("run2", PreprocessOracleVerdicts.Wait(_dll).Status);
        Assert.Equal(2, runs);
    }

    /// <summary>
    /// GUI-C-219c (Codex #112, high): the worker is handed a PRIVATE COPY, never the original. The copy has the same name and the same bytes, lives in its own folder, and is removed once the
    /// verdict is stored.
    /// </summary>
    [Fact]
    public void TheWorkerIsHandedASnapshot_NotTheOriginal_AndTheSnapshotIsRemovedAfterwards()
    {
        string? handed = null;
        byte[]? bytesSeen = null;
        PreprocessOracleVerdicts.Runner = path => { handed = path; bytesSeen = File.ReadAllBytes(path); return Verdict("seen"); };

        PreprocessOracleVerdicts.Wait(_dll);

        Assert.NotNull(handed);
        Assert.NotEqual(_dll, handed);
        Assert.Equal("xpe_preprocess.dll", Path.GetFileName(handed));
        Assert.StartsWith("xpe-oracle-snap-", Path.GetFileName(Path.GetDirectoryName(handed)!), StringComparison.Ordinal);
        Assert.Equal(File.ReadAllBytes(_dll), bytesSeen);
        Assert.False(Directory.Exists(Path.GetDirectoryName(handed)!), "the snapshot folder is still there after the verdict was stored");
    }

    /// <summary>
    /// GUI-C-219c, deterministic A→B→A: the original is changed to B while the worker is running and put back to A before the run ends. The worker reads the copy, so it judges A, and the
    /// stored identity is the hash of the bytes it read. With the original handed to the worker (219b) it read B, the before/after hashes both said A, and the verdict was stored under A.
    /// </summary>
    [Fact]
    public void AFileChangedToBAndBackToADuringTheRun_IsJudgedByTheBytesTheWorkerLoaded_WhichAreA()
    {
        var original = File.ReadAllBytes(_dll);
        var stamp = File.GetLastWriteTimeUtc(_dll);
        using var started = new ManualResetEventSlim();
        using var changedToB = new ManualResetEventSlim();
        using var read = new ManualResetEventSlim();
        using var restored = new ManualResetEventSlim();
        byte[]? seen = null;
        var runs = 0;
        PreprocessOracleVerdicts.Runner = path =>
        {
            Interlocked.Increment(ref runs);
            started.Set();
            changedToB.Wait();
            seen = File.ReadAllBytes(path);   // what the worker loads, at the moment the original is B
            read.Set();
            restored.Wait();
            return Verdict("judged");
        };
        using var completed = new ManualResetEventSlim();
        PreprocessOracleVerdicts.Completed += _ => completed.Set();

        Assert.Null(PreprocessOracleVerdicts.TryGet(_dll));
        Assert.True(started.Wait(5000));
        Replace("B", stamp);                                  // A -> B (same length, time kept)
        changedToB.Set();
        Assert.True(read.Wait(5000));
        File.WriteAllBytes(_dll, original);                   // B -> A
        File.SetLastWriteTimeUtc(_dll, stamp);
        restored.Set();
        Assert.True(completed.Wait(5000));

        Assert.Equal(original, seen);                         // the worker saw A, not B
        Assert.Contains("xpe_preprocess.dll=" + Sha(original), PreprocessOracleVerdicts.StoredIdentityOf(_dll));
        Thread.Sleep(300);                                    // the background verification of the stored verdict finds the same bytes: nothing more to do
        Assert.Equal(1, runs);
    }

    /// <summary>
    /// GUI-C-219c: the original is replaced (for good) while the oracle runs. The run judges the bytes it was given and stores them under their own identity; the next verification, which is
    /// what every ask starts, finds the DLL is something else, drops that verdict (Changed), and judges the new content. No retry counter is involved: nothing ever changes under a run.
    /// </summary>
    [Fact]
    public void AFileReplacedDuringTheRun_IsJudgedAsWhatItWas_ThenTheNewContentIsChecked()
    {
        var original = File.ReadAllBytes(_dll);
        var stamp = File.GetLastWriteTimeUtc(_dll);
        var runs = 0;
        using var firstStarted = new ManualResetEventSlim();
        using var release = new ManualResetEventSlim();
        using var firstDone = new ManualResetEventSlim();
        using var secondDone = new ManualResetEventSlim();
        var changed = 0;
        var announced = 0;
        PreprocessOracleVerdicts.Changed += _ => Interlocked.Increment(ref changed);
        PreprocessOracleVerdicts.Completed += _ =>
        {
            if (Interlocked.Increment(ref announced) == 1) firstDone.Set(); else secondDone.Set();
        };
        PreprocessOracleVerdicts.Runner = _ =>
        {
            var number = Interlocked.Increment(ref runs);
            if (number == 1) { firstStarted.Set(); release.Wait(); }
            return Verdict($"run{number}");
        };

        Assert.Null(PreprocessOracleVerdicts.TryGet(_dll));
        Assert.True(firstStarted.Wait(5000));
        Replace("z", stamp);                                  // the file is replaced under the run (same length, time kept)
        release.Set();
        Assert.True(firstDone.Wait(5000));
        Assert.Contains("xpe_preprocess.dll=" + Sha(original), PreprocessOracleVerdicts.StoredIdentityOf(_dll));   // run 1 is stored under the bytes it was given

        Assert.Equal("run1", PreprocessOracleVerdicts.TryGet(_dll)!.Status);   // the stored verdict, at once; its verification starts in the background
        Assert.True(secondDone.Wait(10000), "the new content was never checked");

        Assert.Equal("run2", PreprocessOracleVerdicts.TryGet(_dll)!.Status);
        Assert.Contains("xpe_preprocess.dll=" + Sha(File.ReadAllBytes(_dll)), PreprocessOracleVerdicts.StoredIdentityOf(_dll));
        Assert.Equal(2, runs);
        Assert.Equal(1, changed);
    }

    /// <summary>
    /// GUI-C-219c: the ask that arrives while a job is running, AFTER that job copied the files, is not covered by it (the file may have changed since the copy). It gets a pass of its own, which
    /// finds the new bytes. Without it the ask is swallowed by the running job and the change is not noticed until some later ask. (Found by a flaky run of the test above.)
    /// </summary>
    [Fact]
    public void AnAskThatComesAfterTheRunningJobsSnapshot_GetsAPassOfItsOwn()
    {
        var stamp = File.GetLastWriteTimeUtc(_dll);
        var runs = 0;
        using var firstStarted = new ManualResetEventSlim();
        using var release = new ManualResetEventSlim();
        using var secondDone = new ManualResetEventSlim();
        var announced = 0;
        PreprocessOracleVerdicts.Completed += _ => { if (Interlocked.Increment(ref announced) == 2) secondDone.Set(); };
        PreprocessOracleVerdicts.Runner = _ =>
        {
            var number = Interlocked.Increment(ref runs);
            if (number == 1) { firstStarted.Set(); release.Wait(); }
            return Verdict($"run{number}");
        };

        Assert.Null(PreprocessOracleVerdicts.TryGet(_dll));
        Assert.True(firstStarted.Wait(5000));              // the job is running and its snapshot is taken
        Replace("q", stamp);                               // the file changes after the copy
        Assert.Null(PreprocessOracleVerdicts.TryGet(_dll));   // an ask now: the running job does not cover it
        release.Set();

        Assert.True(secondDone.Wait(10000), "the ask that came after the snapshot was never verified");
        Assert.Equal("run2", PreprocessOracleVerdicts.TryGet(_dll)!.Status);
    }

    /// <summary>
    /// GUI-C-219c: a private copy that cannot be made (the temp folder is a file, the disk is full) is a verdict, not a crash and not a storm: the DLL was not judged, so it is not ready; the answer
    /// is announced once, and the asks the window makes in response (each one verifies again, and fails the same way) announce nothing. The legacy E2E R02 found this: every refresh restarted a pass
    /// that failed and announced, and the window never settled.
    /// </summary>
    [Fact]
    public void ASnapshotThatCannotBeMade_IsAFailedVerdict_AnnouncedOnce_NotARefreshStorm()
    {
        var announced = 0;
        PreprocessOracleVerdicts.Completed += _ => Interlocked.Increment(ref announced);
        PreprocessOracleVerdicts.AfterSnapshotCopy = _ => throw new IOException("the temp folder is full");
        PreprocessOracleVerdicts.Runner = _ => throw new InvalidOperationException("the oracle must not run without a snapshot");

        var first = PreprocessOracleVerdicts.Wait(_dll);
        for (var i = 0; i < 5; i++)
        {
            PreprocessOracleVerdicts.TryGet(_dll);   // what the window does after every announcement
            Thread.Sleep(50);
        }

        Thread.Sleep(300);
        Assert.False(first.Passed);
        Assert.Equal("Synthetic oracle setup failed", first.Status);
        Assert.Contains("temp folder is full", first.Details, StringComparison.Ordinal);
        Assert.Equal(1, announced);
    }

    /// <summary>GUI-C-219c (Codex #112): the identity covers the DLLs the worker loads with the preprocess DLL, not the preprocess DLL alone.</summary>
    [Fact]
    public void ADependencyDll_IsPartOfTheIdentity_AndIsInTheSnapshot()
    {
        var dependency = Path.Combine(_directory, "fmt.dll");   // named for xpe_preprocess.dll by the application's own loader
        File.WriteAllText(dependency, "d1");
        var runs = 0;
        var sawNextToTheDll = new List<string>();
        PreprocessOracleVerdicts.Runner = path =>
        {
            Interlocked.Increment(ref runs);
            sawNextToTheDll.AddRange(Directory.GetFiles(Path.GetDirectoryName(path)!).Select(Path.GetFileName)!);
            return Verdict($"run{runs}");
        };
        Assert.Equal("run1", PreprocessOracleVerdicts.Wait(_dll).Status);
        Assert.Contains("fmt.dll", sawNextToTheDll);
        Assert.Contains("fmt.dll=" + Sha(File.ReadAllBytes(dependency)), PreprocessOracleVerdicts.StoredIdentityOf(_dll));
        var stamp = File.GetLastWriteTimeUtc(dependency);

        File.WriteAllText(dependency, "d2");                    // the dependency changes (same size, same time); the preprocess DLL does not
        File.SetLastWriteTimeUtc(dependency, stamp);

        Assert.Equal("run2", PreprocessOracleVerdicts.Wait(_dll).Status);
        Assert.Equal(2, runs);
    }

    /// <summary>The import tables are what say which of our DLLs a module loads: read from a real module when this run has one staged.</summary>
    [SkippableFact]
    public void TheImportTable_NamesTheModulesDependencies_AndTheSnapshotFollowsThem()
    {
        var real = Path.Combine(AppContext.BaseDirectory, "xpe_preprocess.dll");
        Skip.IfNot(File.Exists(real) && File.Exists(Path.Combine(AppContext.BaseDirectory, "xpe_common.dll")), "xpe_preprocess.dll and xpe_common.dll are not staged in the test output.");

        Assert.Contains("xpe_common.dll", PeImports.Of(real), StringComparer.OrdinalIgnoreCase);
        Assert.Empty(PeImports.Of(_dll));   // control: a file that is not a PE names nothing, and nothing throws

        var names = PreprocessOracleSnapshot.Closure(real).Select(c => c.Name).ToList();
        Assert.Contains("xpe_preprocess.dll", names, StringComparer.OrdinalIgnoreCase);
        Assert.Contains("xpe_common.dll", names, StringComparer.OrdinalIgnoreCase);   // an import, not named by the loader's own list for this module
    }

    /// <summary>
    /// GUI-C-219c (Codex #112, low, leader decision): the rule is enforced where the blocking calls are. With the guard saying "this thread is the UI thread", <c>Wait</c> and the host's
    /// <c>Run</c> throw; <c>TryGet</c>, which is what the UI uses, does not.
    /// </summary>
    [Fact]
    public void TheBlockingEntryPoints_ThrowWhenCalledOnTheUiThread_AndTryGetDoesNot()
    {
        PreprocessOracleVerdicts.Runner = _ => Verdict("ok");
        var uiThread = Environment.CurrentManagedThreadId;
        OracleThreadGuard.OnUiThread = () => Environment.CurrentManagedThreadId == uiThread;

        var wait = Assert.Throws<InvalidOperationException>(() => PreprocessOracleVerdicts.Wait(_dll));
        Assert.Contains("UI thread", wait.Message, StringComparison.Ordinal);
        Assert.Throws<InvalidOperationException>(() => XpePreprocessOracleProcess.Run(_dll));
        Assert.Null(PreprocessOracleVerdicts.TryGet(_dll));   // does not throw

        // from any other thread the same calls are allowed (the oracle's own worker thread, a headless caller)
        PreprocessSyntheticOracleResult? other = null;
        var thread = new Thread(() => other = PreprocessOracleVerdicts.Wait(_dll));   // a dedicated thread: a pool task could be run inline by the thread that waits for it
        thread.Start();
        thread.Join();
        Assert.Equal("ok", other!.Status);
    }

    [Fact]
    public void ARunnerThatThrows_BecomesAFailedVerdict_NotAnException()
    {
        PreprocessOracleVerdicts.Runner = _ => throw new InvalidOperationException("boom");

        var verdict = PreprocessOracleVerdicts.Wait(_dll);

        Assert.False(verdict.Passed);
        Assert.Equal("Synthetic oracle exception", verdict.Status);
        Assert.Contains("boom", verdict.Details);
    }

    /// <summary>The window is told when the answer is in, and the answer is already readable at that moment.</summary>
    [Fact]
    public void Completed_IsRaisedOncePerRun_AfterTheVerdictIsStored()
    {
        var seen = new ConcurrentQueue<(string Path, string? Status)>();
        using var completed = new ManualResetEventSlim();
        PreprocessOracleVerdicts.Runner = _ => Verdict("told");
        PreprocessOracleVerdicts.Completed += path =>
        {
            seen.Enqueue((path, PreprocessOracleVerdicts.TryGet(path)?.Status));
            completed.Set();
        };

        PreprocessOracleVerdicts.TryGet(_dll);
        Assert.True(completed.Wait(5000));
        PreprocessOracleVerdicts.TryGet(_dll);   // asking again: its verification finds the same bytes and announces nothing
        Thread.Sleep(300);

        var only = Assert.Single(seen);
        Assert.Equal(_dll, only.Path);
        Assert.Equal("told", only.Status);
    }

    /// <summary>
    /// GUI-C-219b (Codex #109, low): the guard against bringing the synchronous oracle back is a CALL-BOUNDARY check on the UI entry points (the window and its view models), not the presence of a
    /// word. GUI-C-219c: the ground of the rule is the thread assertion above; this scan stays as a cheap second line that names the file. Source text, named as such.
    /// </summary>
    [Fact]
    public void TheWindowAndItsViewModels_NeverCallTheOracleOrWaitForIt()
    {
        var appDir = FindAppDir();
        var uiFiles = new[] { Path.Combine(appDir, "MainWindow.xaml.cs") }
            .Concat(Directory.EnumerateFiles(Path.Combine(appDir, "ViewModels"), "*.cs", SearchOption.AllDirectories))
            .ToList();
        var forbidden = new[] { "XpePreprocessOracleProcess.Run(", "XpePreprocessSyntheticOracle.Run(", "PreprocessOracleVerdicts.Wait(" };
        var mustSayNo = new[] { "NativeReadinessProbe.WriteReport(", "XpePreprocessReadinessProbe.Check(", "ModuleReadinessService.Evaluate(", "moduleReadinessViewModel.Refresh(" };
        var seen = 0;
        foreach (var file in uiFiles)
        {
            var text = File.ReadAllText(file);
            foreach (var call in forbidden)
            {
                Assert.True(!text.Contains(call, StringComparison.Ordinal), $"{Path.GetFileName(file)} calls {call}: the oracle would run on, or be waited for by, the UI thread.");
            }

            foreach (var call in mustSayNo)
            {
                foreach (var args in ArgumentsOfEveryCall(text, call))
                {
                    seen++;
                    var isViewModelForwarding = Path.GetFileName(file) == "ModuleReadinessViewModel.cs";
                    var ok = isViewModelForwarding
                        ? args.Contains("waitForOracle", StringComparison.Ordinal)
                        : System.Text.RegularExpressions.Regex.IsMatch(args, @"waitForOracle\s*:\s*false");
                    Assert.True(ok, $"{Path.GetFileName(file)}: {call}{args}) does not say waitForOracle: false, so the default (wait) applies.");
                }
            }
        }

        Assert.True(seen >= 3, $"the scan found only {seen} calls into the readiness chain in the window and its view models.");
    }

    private static IEnumerable<string> ArgumentsOfEveryCall(string text, string call)
    {
        var from = 0;
        while ((from = text.IndexOf(call, from, StringComparison.Ordinal)) >= 0)
        {
            var open = from + call.Length;
            var depth = 1;
            var i = open;
            for (; i < text.Length && depth > 0; i++)
            {
                if (text[i] == '(') depth++;
                else if (text[i] == ')') depth--;
            }

            yield return text[open..Math.Max(open, i - 1)];
            from = i;
        }
    }

    private static string FindAppDir()
    {
        for (var dir = new DirectoryInfo(AppContext.BaseDirectory); dir is not null; dir = dir.Parent)
        {
            var candidate = Path.Combine(dir.FullName, "clients", "ImageProcTest");
            if (File.Exists(Path.Combine(candidate, "App.xaml.cs"))) return candidate;
        }

        throw new DirectoryNotFoundException("clients/ImageProcTest was not found above the test output.");
    }

    [Fact]
    public void TheCheckingResult_IsNeitherReadyNorAFailure_AndSaysItIsChecking()
    {
        var checking = PreprocessSyntheticOracleResult.Checking();

        Assert.False(checking.Passed);      // every gate that asks "did it pass" stays closed, as for an unconfirmed module
        Assert.False(checking.Executed);    // and it is not a run that failed
        Assert.Equal("Checking", checking.Status);
    }
}
