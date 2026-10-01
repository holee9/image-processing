// #225 row 10 (GUI-C-186c A1 + A3, GUI-C-186d): the status is read in the background with no limit and applied only while current;
// an init that threw has ONE reason string; no control byte stands in for a regex backreference.
using System.Collections.Concurrent;
using ImageProcTest.Models;
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// The real <see cref="AiStatusRefresher"/>, <see cref="AiSessionGate"/>, <see cref="AiSessionTracker"/> and <see cref="ProcessingChainRunner"/>
/// with a fake UI thread (a queue the test pumps) and fake reads: no native code runs.
/// </summary>
[Trait("Category", "Functional")]
public sealed class AiStatusRefresherTests
{
    private static readonly AiWorkerStatus Active = new(AiWorkerState.Active, 0, 3);
    private static readonly AiWorkerStatus Disabled = new(AiWorkerState.Disabled, 3, 3);

    /// <summary>
    /// The refresher with a fake UI thread (a queue the test pumps) and a real background thread for the read. What the "UI thread"
    /// does is only what the test pumps, so a read that blocked it would show as a test that cannot proceed.
    /// </summary>
    private sealed class Rig
    {
        public object Current = new();
        public readonly ConcurrentQueue<Action> Ui = new();
        public readonly List<AiWorkerStatus> Applied = [];
        public int Reads;
        public Func<object?, AiWorkerStatus?> Read = _ => Active;
        public readonly AiStatusRefresher Refresher;

        public Rig(bool backgroundReads = true)
        {
            Refresher = new AiStatusRefresher(
                () => Current,
                backend =>
                {
                    Interlocked.Increment(ref Reads);
                    return Read(backend);
                },
                Applied.Add,
                backgroundReads ? work => Task.Run(work) : work => work(),
                Ui.Enqueue);
        }

        /// <summary>Runs what is waiting for the UI thread; how many actions ran.</summary>
        public int Pump()
        {
            var ran = 0;
            while (Ui.TryDequeue(out var action))
            {
                action();
                ran++;
            }

            return ran;
        }

        public void PumpUntil(Func<bool> done, string what)
        {
            var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(10);
            while (!done() && DateTime.UtcNow < deadline)
            {
                Pump();
                if (!done())
                {
                    Thread.Sleep(5);
                }
            }

            Assert.True(done(), what);
        }
    }

    private static readonly TimeSpan Long = TimeSpan.FromSeconds(10);

    // ---- A2 (structure, GUI-C-186d): the read is in the background, with no limit and no retry count -----------------------------

    /// <summary>
    /// Codex #31 finding 1: a frame that holds the gate longer than any retry window must not leave the screen stale. The frame's
    /// duration is an event here (the property is that nothing depends on it), and the proof is that after it ends NO further event
    /// is needed: the one read that was waiting completes and is shown.
    /// </summary>
    [Fact]
    public void AFrameThatHoldsTheGateForAsLongAsItLikes_ThenEnds_LeavesTheScreenCurrent_WithNoFurtherEvent()
    {
        var gate = new AiSessionGate();
        var frameIn = new ManualResetEventSlim();
        var frameMayEnd = new ManualResetEventSlim();
        var frame = Task.Run(() => gate.WithLock(() =>
        {
            frameIn.Set();
            Assert.True(frameMayEnd.Wait(Long));
            return 0;
        }));
        Assert.True(frameIn.Wait(Long));

        var rig = new Rig { Read = _ => gate.WithLock(() => Disabled) };
        rig.Refresher.Request();

        Assert.Empty(rig.Applied);                 // the frame still has the gate: nothing to show yet
        frameMayEnd.Set();                         // the frame ends; nobody asks again
        #pragma warning disable xUnit1031 // a bounded wait on a real thread: what is measured here
        Assert.True(frame.Wait(Long));
        #pragma warning restore xUnit1031
        rig.PumpUntil(() => rig.Applied.Count == 1, "The read that was waiting for the gate never reached the screen.");

        Assert.Equal(Disabled, rig.Applied[0]);
        Assert.Equal(1, rig.Reads);                // one read, no retries
    }

    /// <summary>Codex #31 finding 3: while the gate is held, the caller of <c>Request</c> and the UI thread both go on.</summary>
    [Fact]
    public void WhileTheGateIsHeld_RequestReturnsAtOnce_AndTheUiThreadKeepsWorking()
    {
        var gate = new AiSessionGate();
        var frameIn = new ManualResetEventSlim();
        var frameMayEnd = new ManualResetEventSlim();
        var frame = Task.Run(() => gate.WithLock(() =>
        {
            frameIn.Set();
            Assert.True(frameMayEnd.Wait(Long));
            return 0;
        }));
        Assert.True(frameIn.Wait(Long));

        var rig = new Rig { Read = _ => gate.WithLock(() => Active) };
        var watch = System.Diagnostics.Stopwatch.StartNew();
        rig.Refresher.Request();
        rig.Refresher.Request();
        var requested = watch.Elapsed;

        var uiWorkRan = false;
        rig.Ui.Enqueue(() => uiWorkRan = true);    // other UI work that arrives while the read waits
        rig.Pump();

        frameMayEnd.Set();
        #pragma warning disable xUnit1031 // a bounded wait on a real thread: what is measured here
        Assert.True(frame.Wait(Long));
        #pragma warning restore xUnit1031
        rig.PumpUntil(() => rig.Applied.Count == 1, "The read never completed after the frame ended.");

        Assert.True(requested < TimeSpan.FromSeconds(1), $"Request waited {requested.TotalMilliseconds:0} ms for a gate that was held: the UI thread would have waited the same.");
        Assert.True(uiWorkRan, "UI work queued while the read waited was not processed.");
    }

    /// <summary>Codex #31 finding 2: a status read for a backend that was replaced is dropped, and the screen shows what the new one says.</summary>
    [Fact]
    public void AfterTheBackendIsReplaced_AStaleReadIsDropped_AndTheScreenShowsTheNewBackend()
    {
        var oldBackend = new object();
        var newBackend = new object();
        var releaseOld = new ManualResetEventSlim();
        var oldReading = new ManualResetEventSlim();
        var rig = new Rig { Current = oldBackend };
        rig.Read = backend =>
        {
            if (ReferenceEquals(backend, oldBackend))
            {
                oldReading.Set();
                Assert.True(releaseOld.Wait(Long));
                return Disabled;                    // what the old (native) backend would still say
            }

            return AiWorkerStatus.Unknown;          // the new (mock) backend has no AI session
        };

        rig.Refresher.Request();
        Assert.True(oldReading.Wait(Long));        // the read for the old backend is running and blocked

        rig.Current = newBackend;                   // the view model replaces the backend ...
        rig.Refresher.Reset();                      // ... raising the generation and showing Unknown at once
        rig.Refresher.Request();                    // ... and asks again after the initialisation (one more, after the running one)
        Assert.Equal([AiWorkerStatus.Unknown], rig.Applied);

        releaseOld.Set();
        rig.PumpUntil(() => rig.Reads == 2 && rig.Ui.IsEmpty && rig.Applied.Count >= 2, "The read for the new backend never reached the screen.");
        rig.Pump();

        Assert.DoesNotContain(Disabled, rig.Applied);   // the old backend's answer was never shown
        Assert.Equal(AiWorkerStatus.Unknown, rig.Applied[^1]);
    }

    /// <summary>
    /// The backend shutdown keeps the same backend object: only the generation tells a read started before it from a read after it.
    /// </summary>
    [Fact]
    public void ARead_StartedBeforeAReset_OfTheSameBackend_IsDropped_AndTheOneAfterItIsShown()
    {
        var release = new ManualResetEventSlim();
        var reading = new ManualResetEventSlim();
        var rig = new Rig();
        var calls = 0;
        rig.Read = _ =>
        {
            if (Interlocked.Increment(ref calls) == 1)
            {
                reading.Set();
                Assert.True(release.Wait(Long));
                return Disabled;                    // the session that was ending when the read began
            }

            return Active;                          // what the module says once the shutdown and restart are over
        };

        rig.Refresher.Request();
        Assert.True(reading.Wait(Long));
        rig.Refresher.Reset();                      // the shutdown begins: same backend object, new generation
        rig.Refresher.Request();                    // and a read is wanted after it
        release.Set();
        rig.PumpUntil(() => rig.Applied.Count == 2, "The read after the reset never reached the screen.");

        Assert.Equal([AiWorkerStatus.Unknown, Active], rig.Applied);   // the old session's Disabled never was shown
    }

    /// <summary>The generation alone is not the guard: a backend swapped without a <c>Reset</c> is caught by identity.</summary>
    [Fact]
    public void ARead_ForABackendThatIsNoLongerTheCurrentOne_IsDropped_EvenWithTheSameGeneration()
    {
        var first = new object();
        var second = new object();
        var release = new ManualResetEventSlim();
        var reading = new ManualResetEventSlim();
        var rig = new Rig { Current = first };
        rig.Read = backend =>
        {
            if (ReferenceEquals(backend, first))
            {
                reading.Set();
                Assert.True(release.Wait(Long));
                return Disabled;
            }

            return Active;
        };

        rig.Refresher.Request();
        Assert.True(reading.Wait(Long));
        rig.Current = second;                       // no Reset: only the identity differs
        release.Set();
        rig.PumpUntil(() => rig.Applied.Count == 1, "The read for the current backend never reached the screen.");

        Assert.Equal(Active, rig.Applied[0]);       // the stale Disabled was dropped, the current backend was read instead
        Assert.Equal(2, rig.Reads);
    }

    /// <summary>The application is closing: a read that was running, and one requested afterwards, change nothing.</summary>
    [Fact]
    public void AfterStop_ARunningReadIsNotApplied_AndANewRequestStartsNothing()
    {
        var release = new ManualResetEventSlim();
        var reading = new ManualResetEventSlim();
        var rig = new Rig();
        rig.Read = _ =>
        {
            reading.Set();
            Assert.True(release.Wait(Long));
            return Disabled;
        };

        rig.Refresher.Request();
        Assert.True(reading.Wait(Long));
        rig.Refresher.Stop();
        release.Set();
        Thread.Sleep(200);                          // let the finished read's completion reach the queue, if it is going to
        rig.Pump();

        rig.Refresher.Request();                    // after the stop
        rig.Refresher.Reset();
        rig.Pump();
        Thread.Sleep(100);
        rig.Pump();

        Assert.Empty(rig.Applied);                  // not the running read, not Reset's Unknown
        Assert.Equal(1, rig.Reads);                 // and no read was started after the stop
    }

    [Fact]
    public void RequestsThatPileUpWhileAReadRuns_BecomeExactlyOneMoreRead()
    {
        var release = new ManualResetEventSlim();
        var reading = new ManualResetEventSlim();
        var rig = new Rig();
        rig.Read = _ =>
        {
            reading.Set();
            Assert.True(release.Wait(Long));
            return Active;
        };

        rig.Refresher.Request();
        Assert.True(reading.Wait(Long));
        for (var i = 0; i < 6; i++)
        {
            rig.Refresher.Request();                // six more while one runs
        }

        release.Set();
        rig.PumpUntil(() => rig.Applied.Count == 2, "The one extra read never completed.");
        Thread.Sleep(100);
        rig.Pump();

        Assert.Equal(2, rig.Reads);                 // the running one, and ONE more for all six requests
    }

    [Fact]
    public void AReadThatThrows_ShowsNothing_AndTheNextRequestStillWorks()
    {
        var rig = new Rig();
        var fail = true;
        rig.Read = _ => fail ? throw new InvalidOperationException("the read failed") : Active;

        rig.Refresher.Request();
        Thread.Sleep(200);                          // the failed read has finished by now; its completion is queued for the UI
        rig.Pump();
        Assert.Equal(1, rig.Reads);
        Assert.Empty(rig.Applied);

        fail = false;
        rig.Refresher.Request();                    // the failed read must not leave "a read is running" set
        rig.PumpUntil(() => rig.Applied.Count == 1, "A request after a failed read never completed.");
        Assert.Equal(Active, rig.Applied[0]);
    }

    [Fact]
    public void TheGateRead_WaitsForAFrame_WithNoLimit_AndReturnsWhenItEnds()
    {
        var gate = new AiSessionGate();
        var frameIn = new ManualResetEventSlim();
        var frameMayEnd = new ManualResetEventSlim();
        var frame = Task.Run(() => gate.WithLock(() =>
        {
            frameIn.Set();
            Assert.True(frameMayEnd.Wait(Long));
            return 0;
        }));
        Assert.True(frameIn.Wait(Long));

        var read = Task.Run(() => gate.WithLock(() => 7));
        #pragma warning disable xUnit1031 // a bounded wait on a real thread: what is measured here
        Assert.False(read.Wait(TimeSpan.FromMilliseconds(400)), "The read got through a gate that a frame was holding.");
        #pragma warning restore xUnit1031

        frameMayEnd.Set();
        #pragma warning disable xUnit1031 // a bounded wait on a real thread: what is measured here
        Assert.True(frame.Wait(Long));
        Assert.True(read.Wait(Long));
        Assert.Equal(7, read.Result);
        #pragma warning restore xUnit1031
    }

    [Fact]
    public void TheViewModel_RefreshesInTheBackground_ResetsAtEverySwap_AndStopsAtClose()
    {
        var source = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/ViewModels/MainWindowViewModel.cs"));

        // The refresher is built with a real background runner and the UI dispatcher, and the refresh path waits for nothing.
        Assert.Contains("new AiStatusRefresher(() => _backend, ReadAiWorkerStatus, ApplyAiWorkerStatus, work => Task.Run(work), PostToUi)", source, StringComparison.Ordinal);
        Assert.Contains("private void RefreshAiWorkerStatus() => AiStatus.Request();", source, StringComparison.Ordinal);
        Assert.Contains("_uiDispatcher.BeginInvoke(", source, StringComparison.Ordinal);

        // The swap: Reset BEFORE the new backend is built, a read requested AFTER the initialisation, in the path that is taken
        // whether the initialisation worked or not.
        var init = source.IndexOf("private void InitializeBackend()", StringComparison.Ordinal);
        var initEnd = source.IndexOf("private void RememberRecentRawFile", init, StringComparison.Ordinal);
        Assert.True(init >= 0 && initEnd > init, "InitializeBackend was not found.");
        var initBody = source[init..initEnd];
        var reset = initBody.IndexOf("AiStatus.Reset();", StringComparison.Ordinal);
        var build = initBody.IndexOf("_backend = _backendFactory(Settings);", StringComparison.Ordinal);
        var catchAt = initBody.IndexOf("catch (Exception ex)", StringComparison.Ordinal);
        var request = initBody.LastIndexOf("RefreshAiWorkerStatus();", StringComparison.Ordinal);
        Assert.True(reset >= 0 && build > reset && catchAt > build && request > catchAt,
            "InitializeBackend must reset before it builds the backend and request a read after the try/catch.");

        // The shutdown (GUI-C-186e moved it to the background): the status is reset when it starts, and read again when it is done.
        var begin = source.IndexOf("public void BeginShutdown(", StringComparison.Ordinal);
        var finish = source.IndexOf("private void FinishShutdown(", begin, StringComparison.Ordinal);
        var finishEnd = source.IndexOf("public void ShutdownBackendBlocking()", finish, StringComparison.Ordinal);
        Assert.True(begin >= 0 && finish > begin && finishEnd > finish, "BeginShutdown / FinishShutdown were not found.");
        Assert.Contains("AiStatus.Reset();", source[begin..finish], StringComparison.Ordinal);
        Assert.Contains("RefreshAiWorkerStatus();", source[finish..finishEnd], StringComparison.Ordinal);

        // The close: updates stop before the shutdown that precedes the close is started.
        var window = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/MainWindow.xaml.cs"));
        var stop = window.IndexOf("viewModel.StopAiStatusUpdates();", StringComparison.Ordinal);
        Assert.True(stop >= 0 && window.IndexOf("viewModel.BeginShutdown(", stop, StringComparison.Ordinal) > stop,
            "OnClosing must stop the status updates before it starts the shutdown.");

        // The old structure is gone: no bounded read, no timer, no retry numbers, anywhere in the app's sources.
        var banned = new[] { "TryWithLock", "StateReadWait", "ScheduleOnUiThread", "MaxRetries", "RetryDelay" };
        var root = Path.GetDirectoryName(BenchmarkRunnerServiceTests.ResolveRepositoryFile("CMakeLists.txt"))!;
        var appFiles = Directory.EnumerateFiles(Path.Combine(root, "gui", "ImageProcTest"), "*.cs", SearchOption.AllDirectories)
            .Where(path => !path.Contains($"{Path.DirectorySeparatorChar}obj{Path.DirectorySeparatorChar}") && !path.Contains($"{Path.DirectorySeparatorChar}bin{Path.DirectorySeparatorChar}"))
            .ToList();
        Assert.True(appFiles.Count > 50, "The scan found too few app sources to mean anything.");
        foreach (var path in appFiles)
        {
            var text = File.ReadAllText(path);
            foreach (var word in banned)
            {
                Assert.False(text.Contains(word, StringComparison.Ordinal), $"{Path.GetRelativePath(root, path)} still mentions '{word}'.");
            }
        }
    }

    // ---- A3: one reason string for an init that threw ----------------------------------------------------------------------------

    [Fact]
    public void TheInitFailureReason_NamesTheExceptionTypeAndItsMessage_EndingWithAFullStop()
    {
        Assert.Equal("xpe_ai_init threw InvalidOperationException: boom.",
            AiBoneSuppressionStage.InitFailureReason(new InvalidOperationException("boom")));
        Assert.Equal("xpe_ai_init threw InvalidOperationException: boom.",
            AiBoneSuppressionStage.InitFailureReason(new InvalidOperationException("boom.")));
    }

    [Fact]
    public void TheTrackersDetail_AndTheChainsStageReason_ForTheSameThrownInit_AreTheSameString()
    {
        var thrown = new InvalidOperationException("the runtime refused");
        var reason = AiBoneSuppressionStage.InitFailureReason(thrown);

        // The tracker side: what the status line shows (AiSessionTracker.OwnStatus carries the recorded detail).
        var tracker = new AiSessionTracker();
        tracker.InitFailed(reason);
        var trackerDetail = tracker.OwnStatus()!.Detail;

        // The chain side: the exception that goes up from the init is an AiInitException with that same reason as its message.
        var chain = ProcessingChainRunner.Run([1, 2, 3], [new(StageIds.AiBoneSuppression, true)],
            (_, _) => throw new AiInitException(reason, thrown));
        var chainReason = chain.Stages[0].Reason;

        Assert.Equal(reason, trackerDetail);
        Assert.Equal($"{StageIds.AiBoneSuppression} threw: {reason}", chainReason);
        Assert.Contains(trackerDetail!, chainReason, StringComparison.Ordinal);
    }

    // ---- A1: the C-09 assertion's regex, and the control byte that once stood in for its backreference ------------------------------

    private const string C09StatePattern = @"worker=Disabled; failures=(\d+); ceiling=\1$";

    [Fact]
    public void TheC09Pattern_MatchesOnlyAStateWhoseFailuresEqualItsCeiling()
    {
        Assert.Matches(C09StatePattern, "worker=Disabled; failures=3; ceiling=3");
        Assert.DoesNotMatch(C09StatePattern, "worker=Disabled; failures=2; ceiling=3");
        Assert.DoesNotMatch(C09StatePattern, "worker=Active; failures=3; ceiling=3");
    }

    [Fact]
    public void TheE2EAssertion_CarriesThatPattern_AsTheTwoCharactersBackslashOne()
    {
        var source = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("clients/ImageProcTest.E2ETests/Scenarios/Workflows/ProcessingChainScenarios.cs"));

        Assert.Contains("Assert.Matches(@\"" + C09StatePattern + "\", before);", source, StringComparison.Ordinal);
    }

    [Fact]
    public void NoSourceFileOfTheTwoClients_CarriesAControlByte()
    {
        var root = Path.GetDirectoryName(BenchmarkRunnerServiceTests.ResolveRepositoryFile("CMakeLists.txt"))!;
        var files = new[] { "clients", "gui" }
            .SelectMany(top => Directory.EnumerateFiles(Path.Combine(root, top), "*.*", SearchOption.AllDirectories))
            .Where(path => path.EndsWith(".cs", StringComparison.OrdinalIgnoreCase) || path.EndsWith(".xaml", StringComparison.OrdinalIgnoreCase))
            .Where(path => !path.Contains($"{Path.DirectorySeparatorChar}obj{Path.DirectorySeparatorChar}") && !path.Contains($"{Path.DirectorySeparatorChar}bin{Path.DirectorySeparatorChar}"))
            .ToList();
        Assert.True(files.Count > 100, $"Only {files.Count} source files were found: the scan is not looking at the sources."); // the control: it reads files

        var offenders = new List<string>();
        foreach (var path in files)
        {
            var bytes = File.ReadAllBytes(path);
            var at = Array.FindIndex(bytes, b => b < 0x20 && b != (byte)'\t' && b != (byte)'\n' && b != (byte)'\r');
            if (at >= 0)
            {
                offenders.Add($"{Path.GetRelativePath(root, path)} (byte 0x{bytes[at]:x2}, line {bytes.Take(at).Count(b => b == (byte)'\n') + 1})");
            }
        }

        Assert.True(offenders.Count == 0, "A control byte in a source file: " + string.Join("; ", offenders));
    }

    [Fact]
    public void TheNativeInit_RecordsTheReasonItThrows_FromOneVariable()
    {
        var source = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/Services/Native/GuiAiRunner.cs"));
        var start = source.IndexOf("catch (Exception ex)", StringComparison.Ordinal);
        Assert.True(start >= 0, "The generic catch of the init was not found.");
        var block = source[start..Math.Min(source.Length, start + 700)];

        var make = block.IndexOf("var reason = AiBoneSuppressionStage.InitFailureReason(ex);", StringComparison.Ordinal);
        var record = block.IndexOf("Tracker.InitFailed(reason);", StringComparison.Ordinal);
        var rethrow = block.IndexOf("throw new AiInitException(reason, ex);", StringComparison.Ordinal);

        Assert.True(make >= 0 && record > make && rethrow > record,
            "The init must build the reason once, record it, and throw an exception carrying that same string.");
    }
}
