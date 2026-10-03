// GUI-C-219 (#249): the oracle's verdict is produced off the asking thread, once, and shared.
using System.Collections.Concurrent;
using System.Diagnostics;

namespace ImageProcTest.IntegrationTests.Diagnostics;

/// <summary>
/// 212b moved the preprocess synthetic oracle into a child process and left the legacy app asking for it three times, synchronously, on the UI thread at startup: the window stopped answering for
/// a median of 963 ms and up to 4.4 s (GUI-C-218). The holder under test produces the verdict on a thread-pool thread, keeps it per DLL file, and answers "not yet" without waiting. Its
/// <c>Runner</c> is replaced here by a controllable stand-in, so the threading and the sharing are observed rather than inferred.
/// </summary>
[Collection("PreprocessOracleVerdicts")]   // the holder is process-wide state: these tests take turns
[Trait("Category", "P1AReady")]
public sealed class PreprocessOracleVerdictsTests : IDisposable
{
    private readonly string _dll;

    public PreprocessOracleVerdictsTests()
    {
        PreprocessOracleVerdicts.ResetForTests();
        _dll = Path.Combine(Path.GetTempPath(), $"xpe_fake_{Guid.NewGuid():N}.dll");
        File.WriteAllText(_dll, "x");
    }

    public void Dispose()
    {
        PreprocessOracleVerdicts.ResetForTests();
        try { File.Delete(_dll); } catch (IOException) { /* temp file */ }
    }

    private static PreprocessSyntheticOracleResult Verdict(string status) =>
        PreprocessSyntheticOracleResult.Failed(status, "stand-in");

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

        File.WriteAllText(_dll, "y");              // the same length (1 byte), other bytes
        File.SetLastWriteTimeUtc(_dll, stamp);     // the time put back
        Assert.Equal(size, new FileInfo(_dll).Length);
        Assert.Equal(stamp, File.GetLastWriteTimeUtc(_dll));

        Assert.Equal("run2", PreprocessOracleVerdicts.Wait(_dll).Status);
        Assert.Equal(2, runs);
    }

    /// <summary>GUI-C-219b: a file that changes while the oracle runs on it. The result of that run belongs to neither content: it is not stored under the old one, and the new one is checked.</summary>
    [Fact]
    public void AFileThatChangesWhileTheOracleRuns_IsNotJudgedByTheRunThatStartedBeforeTheChange()
    {
        var runs = 0;
        using var firstStarted = new ManualResetEventSlim();
        using var release = new ManualResetEventSlim();
        using var completed = new ManualResetEventSlim();
        var announced = 0;
        PreprocessOracleVerdicts.Completed += _ => { Interlocked.Increment(ref announced); completed.Set(); };
        PreprocessOracleVerdicts.Runner = _ =>
        {
            var number = Interlocked.Increment(ref runs);
            if (number == 1) { firstStarted.Set(); release.Wait(); }
            return Verdict($"run{number}");
        };

        Assert.Null(PreprocessOracleVerdicts.TryGet(_dll));
        Assert.True(firstStarted.Wait(5000));
        var stamp = File.GetLastWriteTimeUtc(_dll);
        File.WriteAllText(_dll, "z");              // the file changes under the run (same length, time put back: only the content tells)
        File.SetLastWriteTimeUtc(_dll, stamp);
        release.Set();

        Assert.True(completed.Wait(10000), "the new content was never checked");
        Assert.Equal("run2", PreprocessOracleVerdicts.TryGet(_dll)!.Status);   // the answer for what is on disk now, not run 1's
        Assert.Equal(2, runs);
        Thread.Sleep(200);
        Assert.Equal(1, announced);                                           // the stale run announced nothing
    }

    /// <summary>GUI-C-219b: the file is put back to content that already has a verdict while a run for other content is in progress: the window still hears the answer.</summary>
    [Fact]
    public void AFilePutBackDuringARun_StillAnnouncesTheKnownAnswer()
    {
        var runs = 0;
        using var secondStarted = new ManualResetEventSlim();
        using var release = new ManualResetEventSlim();
        using var completed = new ManualResetEventSlim();
        PreprocessOracleVerdicts.Runner = _ =>
        {
            var number = Interlocked.Increment(ref runs);
            if (number == 2) { secondStarted.Set(); release.Wait(); }
            return Verdict($"run{number}");
        };
        Assert.Equal("run1", PreprocessOracleVerdicts.Wait(_dll).Status);       // content "x"
        var original = File.ReadAllBytes(_dll);
        var stamp = File.GetLastWriteTimeUtc(_dll);
        File.WriteAllText(_dll, "w");
        PreprocessOracleVerdicts.Completed += _ => completed.Set();
        Assert.Null(PreprocessOracleVerdicts.TryGet(_dll));                      // run 2 starts, for content "w"
        Assert.True(secondStarted.Wait(5000));
        File.WriteAllBytes(_dll, original);                                      // back to "x" while run 2 is in progress
        File.SetLastWriteTimeUtc(_dll, stamp);
        release.Set();

        Assert.True(completed.Wait(10000), "the window was never told");
        Assert.Equal("run1", PreprocessOracleVerdicts.TryGet(_dll)!.Status);     // the earlier answer for the content that is back
        Assert.Equal(2, runs);
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
        PreprocessOracleVerdicts.TryGet(_dll);   // asking again: no second announcement
        Thread.Sleep(200);

        var only = Assert.Single(seen);
        Assert.Equal(_dll, only.Path);
        Assert.Equal("told", only.Status);
    }

    /// <summary>
    /// GUI-C-219b (Codex #109, low): the guard against bringing the synchronous oracle back is a CALL-BOUNDARY check on the UI entry points (the window and its view models), not the presence of a
    /// word. A source scan for "PreprocessOracleVerdicts." passes with a direct <c>XpePreprocessOracleProcess.Run(</c> added next to it. Here: no UI entry point calls the oracle or the blocking
    /// <c>Wait</c>, and every call into the readiness chain says whether it may wait, with the answer "no" (the default is "yes", which is for the headless callers). Source text, named as such.
    /// </summary>
    [Fact]
    public void TheWindowAndItsViewModels_NeverCallTheOracleOrWaitForIt()
    {
        var appDir = FindAppDir();
        var uiFiles = new[] { Path.Combine(appDir, "MainWindow.xaml.cs") }
            .Concat(Directory.EnumerateFiles(Path.Combine(appDir, "ViewModels"), "*.cs", SearchOption.AllDirectories))
            .ToList();
        var forbidden = new[] { "XpePreprocessOracleProcess.Run(", "XpePreprocessSyntheticOracle.Run(", "PreprocessOracleVerdicts.Wait(" };
        // each entry point into the readiness chain, and what its call must carry
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
                        ? args.Contains("waitForOracle", StringComparison.Ordinal)                       // the view model passes its caller's answer on; its callers are checked here too
                        : System.Text.RegularExpressions.Regex.IsMatch(args, @"waitForOracle\s*:\s*false");
                    Assert.True(ok, $"{Path.GetFileName(file)}: {call}{args}) does not say waitForOracle: false, so the default (wait) applies.");
                }
            }
        }

        // control: the scan saw the calls that must exist, or "found none" would be a pass
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
