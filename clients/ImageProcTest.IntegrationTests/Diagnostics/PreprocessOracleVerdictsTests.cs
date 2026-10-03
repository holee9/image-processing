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

    /// <summary>A DLL that changed on disk is another subject: its verdict is asked for again.</summary>
    [Fact]
    public void ADllRewritten_IsANewSubject()
    {
        var runs = 0;
        PreprocessOracleVerdicts.Runner = _ => { Interlocked.Increment(ref runs); return Verdict("v"); };
        PreprocessOracleVerdicts.Wait(_dll);

        File.SetLastWriteTimeUtc(_dll, DateTime.UtcNow.AddMinutes(5));
        PreprocessOracleVerdicts.Wait(_dll);

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

    [Fact]
    public void TheCheckingResult_IsNeitherReadyNorAFailure_AndSaysItIsChecking()
    {
        var checking = PreprocessSyntheticOracleResult.Checking();

        Assert.False(checking.Passed);      // every gate that asks "did it pass" stays closed, as for an unconfirmed module
        Assert.False(checking.Executed);    // and it is not a run that failed
        Assert.Equal("Checking", checking.Status);
    }
}
