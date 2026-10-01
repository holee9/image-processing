// #225 row 10 (GUI-C-186e, Codex #33): ending a backend waits for the AI session gate, so it is done off the UI thread.
using System.Collections.Concurrent;
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// The real <see cref="BackendLifecycle"/> and <see cref="AiSessionGate"/> with a fake UI thread (a queue the test pumps). What the
/// "UI thread" does is only what the test pumps, so a shutdown that waited on it would show as a test that cannot proceed.
/// </summary>
[Trait("Category", "Functional")]
public sealed class BackendLifecycleTests
{
    private static readonly TimeSpan Long = TimeSpan.FromSeconds(10);

    private sealed class Rig
    {
        public readonly ConcurrentQueue<Action> Ui = new();
        public readonly BackendLifecycle Lifecycle;
        public readonly List<Exception?> Completed = [];

        public Rig(bool background = true)
        {
            Lifecycle = new BackendLifecycle(background ? work => Task.Run(work) : work => work(), Ui.Enqueue);
        }

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
            var deadline = DateTime.UtcNow + Long;
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

    /// <summary>A frame that holds the session gate until the test lets it go (the stand-in for an AI frame waiting on a silent worker).</summary>
    private static (AiSessionGate Gate, ManualResetEventSlim MayEnd, Task Frame) HoldTheGate()
    {
        var gate = new AiSessionGate();
        var inFrame = new ManualResetEventSlim();
        var mayEnd = new ManualResetEventSlim();
        var frame = Task.Run(() => gate.WithLock(() =>
        {
            inFrame.Set();
            Assert.True(mayEnd.Wait(Long));
            return 0;
        }));
        Assert.True(inFrame.Wait(Long));
        return (gate, mayEnd, frame);
    }

    // ---- The UI thread stays responsive ---------------------------------------------------------------------------------------------

    [Fact]
    public void ShuttingDown_WhileAFrameHoldsTheGate_ReturnsAtOnce_AndTheUiThreadKeepsWorking_UntilItIsDone()
    {
        var (gate, mayEnd, frame) = HoldTheGate();
        var rig = new Rig();

        var watch = System.Diagnostics.Stopwatch.StartNew();
        var started = rig.Lifecycle.Begin(() => gate.WithLock(() => 0), rig.Completed.Add); // waits for the gate, like GuiAiSession.Shutdown
        var returned = watch.Elapsed;

        var markerPostedAt = watch.Elapsed;
        var markerRanAt = TimeSpan.Zero;
        rig.Ui.Enqueue(() => markerRanAt = watch.Elapsed); // other UI work that arrives while the shutdown waits
        rig.Pump();
        var uiDelay = markerRanAt - markerPostedAt;

        Assert.True(started);
        Assert.True(rig.Lifecycle.IsTransitioning);
        Assert.True(returned < TimeSpan.FromSeconds(1), $"Begin waited {returned.TotalMilliseconds:0} ms for a gate that was held: the UI thread would have waited the same.");
        Assert.True(uiDelay < TimeSpan.FromSeconds(1), $"UI work waited {uiDelay.TotalMilliseconds:0} ms behind the shutdown.");
        Assert.Empty(rig.Completed);                       // not done: the frame still has the gate

        mayEnd.Set();
        Assert.True(frame.Wait(Long));
        rig.PumpUntil(() => rig.Completed.Count == 1, "The shutdown never completed after the frame ended.");

        Assert.Null(rig.Completed[0]);
        Assert.False(rig.Lifecycle.IsTransitioning);
    }

    // ---- Requests during the transition, and after it -------------------------------------------------------------------------------

    [Fact]
    public void ProcessingRequests_AreRefusedDuringTheTransition_AndAdmittedAfterIt()
    {
        var (gate, mayEnd, frame) = HoldTheGate();
        var rig = new Rig();
        Assert.True(rig.Lifecycle.TryAdmit(out var before));
        Assert.Null(before);

        rig.Lifecycle.Begin(() => gate.WithLock(() => 0), rig.Completed.Add);

        Assert.False(rig.Lifecycle.TryAdmit(out var during));
        Assert.Contains("shutting down", during, StringComparison.Ordinal);

        mayEnd.Set();
        Assert.True(frame.Wait(Long));
        rig.PumpUntil(() => rig.Completed.Count == 1, "The shutdown never completed.");

        Assert.True(rig.Lifecycle.TryAdmit(out var after));   // the new backend is attached next, and requests go to it
        Assert.Null(after);
    }

    [Fact]
    public void ASecondTransition_WhileOneRuns_StartsNothing()
    {
        var (gate, mayEnd, frame) = HoldTheGate();
        var rig = new Rig();
        var runs = 0;

        Assert.True(rig.Lifecycle.Begin(() => { Interlocked.Increment(ref runs); gate.WithLock(() => 0); }, rig.Completed.Add));
        Assert.False(rig.Lifecycle.Begin(() => Interlocked.Increment(ref runs), rig.Completed.Add));

        mayEnd.Set();
        Assert.True(frame.Wait(Long));
        rig.PumpUntil(() => rig.Completed.Count == 1, "The shutdown never completed.");

        Assert.Equal(1, runs);
        Assert.Single(rig.Completed);
    }

    [Fact]
    public void WhatFollows_RunsOnceTheTransitionIsDone_AndNotBefore()
    {
        var (gate, mayEnd, frame) = HoldTheGate();
        var rig = new Rig();
        var closed = 0;

        rig.Lifecycle.Begin(() => gate.WithLock(() => 0), rig.Completed.Add);
        rig.Lifecycle.WhenIdle(() => closed++);
        rig.Pump();
        Assert.Equal(0, closed);                              // the shutdown is still waiting: the window stays open

        mayEnd.Set();
        Assert.True(frame.Wait(Long));
        rig.PumpUntil(() => closed == 1, "What follows the shutdown never ran.");
        Assert.Single(rig.Completed);                         // and it ran after the completion handler, not instead of it

        rig.Lifecycle.WhenIdle(() => closed++);               // idle now: runs at once
        Assert.Equal(2, closed);
    }

    [Fact]
    public void AShutdownThatThrows_ReportsTheError_AndLeavesTheTransition()
    {
        var rig = new Rig();

        rig.Lifecycle.Begin(() => throw new InvalidOperationException("the module refused"), rig.Completed.Add);
        rig.PumpUntil(() => rig.Completed.Count == 1, "A throwing shutdown never completed.");

        Assert.IsType<InvalidOperationException>(rig.Completed[0]);
        Assert.False(rig.Lifecycle.IsTransitioning);          // a failed shutdown must not leave requests refused for ever
        Assert.True(rig.Lifecycle.TryAdmit(out _));
    }

    // ---- The wiring (source readings: they see this tree's text only) ------------------------------------------------------------------

    private static string Source(string relative) =>
        File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile(relative));

    [Fact]
    public void TheViewModel_ShutsDownInTheBackground_AndRefusesRequestsWhileItDoes()
    {
        var source = Source("gui/ImageProcTest/ViewModels/MainWindowViewModel.cs");

        Assert.Contains("new BackendLifecycle(work => Task.Run(work), PostToUi)", source, StringComparison.Ordinal);
        // The only synchronous shutdown call is the blocking fallback; the real one is inside the background work.
        Assert.Contains("Lifecycle.Begin(() => backend.Shutdown(), error => FinishShutdown(backend, error))", source, StringComparison.Ordinal);
        Assert.Equal(1, CountOf(source, "_backend.Shutdown();"));
        var blocking = source.IndexOf("public void ShutdownBackendBlocking()", StringComparison.Ordinal);
        Assert.True(blocking >= 0 && source.IndexOf("_backend.Shutdown();", StringComparison.Ordinal) > blocking,
            "The one synchronous Shutdown call must be the blocking fallback.");
        Assert.Contains("ShutdownBackendCommand = new RelayCommand(() => BeginShutdown());", source, StringComparison.Ordinal);

        // Every path that would start work on the backend asks first.
        foreach (var (method, what) in new[]
        {
            ("private async Task ApplyDisplayPipelineAsync()", "Display pipeline"),
            ("private async void RestartAiSession()", "Restart AI"),
            ("private void InitializeBackend()", "Initialize backend"),
        })
        {
            var at = source.IndexOf(method, StringComparison.Ordinal);
            Assert.True(at >= 0, $"{method} was not found.");
            var head = source[at..Math.Min(source.Length, at + 700)];
            // The exact guard: a refusal that returns. Not merely a mention of the helper (a condition that can never hold keeps the text).
            Assert.Matches("if \\(RefusedWhileTransitioning\\(\"" + what + "\"\\)\\)\\s*\\{\\s*return;\\s*\\}", head);
        }
    }

    [Fact]
    public void TheCandidateLane_RunsItsChainInTheBackground_ForTheBackendThatWasCurrent()
    {
        var source = Source("gui/ImageProcTest/ViewModels/MainWindowViewModel.cs");

        Assert.Contains("await RenderLanesAsync(sourceFrame, inputs, ProcessedImage);", source, StringComparison.Ordinal);
        Assert.Contains("await Task.Run(() => RenderLane(backend, sourceFrame, candidate))", source, StringComparison.Ordinal);
        Assert.Equal(1, CountOf(source, "RenderLane(backend, sourceFrame, candidate)"));          // no second, synchronous call
        Assert.DoesNotContain("RenderLane(sourceFrame, candidate)", source, StringComparison.Ordinal);
        Assert.Contains("private static System.Windows.Media.ImageSource? RenderLane(IXpeBackend backend,", source, StringComparison.Ordinal);
        // The result is applied only if the backend is still the one it was made for and nothing is shutting down.
        var drop = source.IndexOf("if (!ReferenceEquals(backend, _backend) || Lifecycle.IsTransitioning)", StringComparison.Ordinal);
        var apply = source.IndexOf("LaneBImage = candidateImage;", StringComparison.Ordinal);
        Assert.True(drop >= 0 && apply > drop, "The lane result must be checked against the current backend before it is applied.");
    }

    [Fact]
    public void TheWindow_StartsTheShutdownInTheBackgroundAndClosesItselfWhenDone_WithNoCloseAnyway()
    {
        var source = Source("gui/ImageProcTest/MainWindow.xaml.cs");
        var closing = source.IndexOf("protected override void OnClosing(", StringComparison.Ordinal);
        var closed = source.IndexOf("protected override void OnClosed(", StringComparison.Ordinal);
        Assert.True(closing >= 0 && closed > closing, "OnClosing / OnClosed were not found.");
        var onClosing = source[closing..closed];

        Assert.Contains("e.Cancel = true;", onClosing, StringComparison.Ordinal);
        Assert.Contains("viewModel.BeginShutdown(() =>", onClosing, StringComparison.Ordinal);
        Assert.Equal(1, CountOf(onClosing, "Close();"));                    // the one close is the callback of the shutdown
        // A second close request while the first waits schedules nothing more (a second Close() on a closing window would throw).
        Assert.Matches(@"if \(!_closeScheduled\)\s*\{\s*_closeScheduled = true;", onClosing);

        // The blocking fallback is only for a close that could not wait.
        var onClosed = source[closed..];
        Assert.Contains("if (!_closeAfterShutdown)", onClosed, StringComparison.Ordinal);
        Assert.Contains("viewModel.ShutdownBackendBlocking();", onClosed, StringComparison.Ordinal);
        Assert.Contains("_forceClose = true; // GUI-C-186e", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Leader decision on GUI-C-186e (premise correction): replacing the backend never waited on the AI gate, so it stays synchronous.
    /// This pins WHY: no backend is <c>IDisposable</c> (the <c>(_backend as IDisposable)?.Dispose()</c> in InitializeBackend does nothing),
    /// <c>Initialize</c> does not touch the AI session, and the one place the app ends the session is <c>RealXpeBackend.Shutdown</c>.
    /// If one of these changes, replacement needs the same background treatment.
    /// </summary>
    [Fact]
    public void ReplacingTheBackend_TakesNoAiGate_BecauseNoBackendIsDisposable_AndInitializeDoesNotTouchTheSession()
    {
        foreach (var file in new[] { "RealXpeBackend.cs", "MockXpeBackend.cs", "FaultInjectingBackend.cs" })
        {
            var text = Source($"gui/ImageProcTest/Services/{file}");
            Assert.DoesNotContain("IDisposable", text, StringComparison.Ordinal);
            Assert.DoesNotContain("void Dispose(", text, StringComparison.Ordinal);
        }

        var real = Source("gui/ImageProcTest/Services/RealXpeBackend.cs");
        var init = real.IndexOf("public BackendRuntimeInfo Initialize(", StringComparison.Ordinal);
        var initEnd = real.IndexOf("public string GetVersion()", init, StringComparison.Ordinal);
        Assert.True(init >= 0 && initEnd > init, "RealXpeBackend.Initialize was not found.");
        Assert.DoesNotContain("GuiAiSession", real[init..initEnd], StringComparison.Ordinal);

        var root = Path.GetDirectoryName(BenchmarkRunnerServiceTests.ResolveRepositoryFile("CMakeLists.txt"))!;
        var users = Directory.EnumerateFiles(Path.Combine(root, "gui", "ImageProcTest"), "*.cs", SearchOption.AllDirectories)
            .Where(path => !path.Contains($"{Path.DirectorySeparatorChar}obj{Path.DirectorySeparatorChar}") && !path.Contains($"{Path.DirectorySeparatorChar}bin{Path.DirectorySeparatorChar}"))
            .Where(path => File.ReadAllText(path).Contains("GuiAiSession.", StringComparison.Ordinal))
            .Select(path => Path.GetFileName(path))
            .OrderBy(name => name, StringComparer.Ordinal)
            .ToList();
        Assert.Equal(["GuiAiRunner.cs", "RealXpeBackend.cs"], users);   // the control: the scan finds the two files that do use it
    }

    private static int CountOf(string text, string needle)
    {
        var count = 0;
        for (var at = text.IndexOf(needle, StringComparison.Ordinal); at >= 0; at = text.IndexOf(needle, at + needle.Length, StringComparison.Ordinal))
        {
            count++;
        }

        return count;
    }
}
