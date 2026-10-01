// #225 row 10 (GUI-C-186f, Codex #36): the real MainWindowViewModel, with a scripted backend, through a shutdown or a replacement.
//
// WHY HERE. The rules these scenarios pin (work that outlives a backend changes nothing on screen and starts nothing; every
// entry point refuses during a transition) live in the view model, which is WPF and so cannot be linked into the net8.0 test
// project. This runner references the app, and the `gui-shell-runners` CI job runs it (no native module, no input device).
//
// A failure is reported with its scenario and step, and the runner exits non-zero.
using System.IO;
using System.Windows.Threading;
using ImageProcTest.Models;
using ImageProcTest.Services;
using ImageProcTest.ViewModels;

namespace ImageProcTest.SelfCheck;

/// <summary>
/// A Mock backend with counters and two scripts: one RunChain call can be held (and then fail or succeed on release), and the
/// shutdown can be held. Nothing else is changed: every answer is the Mock's.
/// </summary>
internal sealed class ScenarioBackend : IXpeBackend
{
    private readonly IXpeBackend _inner = new MockXpeBackend(new RawImageLoader(), "xpe_common.dll", false, "xpe_display.dll", false);

    public int LoadCalls;
    public int PresetCalls;
    public int ChainCalls;
    public int ApplyCalls;

    /// <summary>The 1-based RunChain call that waits for <see cref="ChainRelease"/> (0: none).</summary>
    public int BlockChainCall;

    /// <summary>The held call throws when released (else it carries on and succeeds).</summary>
    public bool ThrowAfterRelease;

    public readonly ManualResetEventSlim ChainBlocked = new();
    public readonly ManualResetEventSlim ChainRelease = new();

    /// <summary>When set, Shutdown waits for it: the transition stays running until the scenario lets it end.</summary>
    public ManualResetEventSlim? ShutdownMayEnd;

    public ChainResult RunChain(LoadedImageFrame rawFrame, IReadOnlyList<StageRequest> stages, AppSettings settings)
    {
        var call = Interlocked.Increment(ref ChainCalls);
        if (call == BlockChainCall)
        {
            ChainBlocked.Set();
            if (!ChainRelease.Wait(TimeSpan.FromSeconds(30)))
            {
                throw new TimeoutException("the scenario never released the held chain call");
            }

            if (ThrowAfterRelease)
            {
                throw new InvalidOperationException("scripted failure of the held chain call");
            }
        }

        return _inner.RunChain(rawFrame, stages, settings);
    }

    public LoadedImageFrame LoadRawImage(string path, AppSettings settings)
    {
        Interlocked.Increment(ref LoadCalls);
        return _inner.LoadRawImage(path, settings);
    }

    public VoiPreset CreateVoiPreset(XpeBodyPartEnum bodyPart)
    {
        Interlocked.Increment(ref PresetCalls);
        return _inner.CreateVoiPreset(bodyPart);
    }

    public LoadedImageFrame ApplyDisplayPipeline(LoadedImageFrame rawFrame, ushort[] displayInput, AppSettings settings)
    {
        Interlocked.Increment(ref ApplyCalls);
        return _inner.ApplyDisplayPipeline(rawFrame, displayInput, settings);
    }

    public void Shutdown()
    {
        ShutdownMayEnd?.Wait(TimeSpan.FromSeconds(30));
        _inner.Shutdown();
    }

    public BackendRuntimeInfo Initialize(AppSettings settings) => _inner.Initialize(settings);

    public string GetVersion() => _inner.GetVersion();

    public bool SupportsPreprocessing => _inner.SupportsPreprocessing;

    public string GetDisplayVersion() => _inner.GetDisplayVersion();

    public TelemetrySnapshot GetTelemetrySince(int logsSeen, int alertsSeen) => _inner.GetTelemetrySince(logsSeen, alertsSeen);

    public BackendRuntimeInfo GetRuntimeInfo() => _inner.GetRuntimeInfo();
}

internal static class LifetimeScenarios
{
    private static readonly List<string> Failures = [];
    private static string _scenario = string.Empty;

    public static void Run(string rawPath, int width, int height)
    {
        Exception? error = null;
        var thread = new Thread(() =>
        {
            try
            {
                var dispatcher = Dispatcher.CurrentDispatcher;
                dispatcher.InvokeAsync(async () =>
                {
                    try
                    {
                        await StartedApplyIsDroppedByAShutdown(rawPath, width, height);
                        await StartedApplyThatFailsAfterAShutdownRaisesNothing(rawPath, width, height);
                        await EveryEntryPointRefusesDuringTheTransition(rawPath, width, height);
                        await AnOldLaneThatFailsAfterAReplacementKeepsTheNewLanes(rawPath, width, height);
                        await AnOldLaneThatSucceedsAfterAShutdownChangesNothing(rawPath, width, height);
                    }
                    catch (Exception ex)
                    {
                        Fail($"EXCEPTION {ex}");
                    }
                    finally
                    {
                        dispatcher.BeginInvokeShutdown(DispatcherPriority.Background);
                    }
                });
                Dispatcher.Run();
            }
            catch (Exception ex)
            {
                error = ex;
            }
        });
        thread.SetApartmentState(ApartmentState.STA);
        thread.Start();
        thread.Join();

        if (error is not null)
        {
            throw new InvalidOperationException($"Lifetime scenarios could not run: {error.Message}", error);
        }

        if (Failures.Count > 0)
        {
            throw new InvalidOperationException("Lifetime scenarios failed:" + Environment.NewLine + string.Join(Environment.NewLine, Failures));
        }

        Console.WriteLine("Lifetime scenarios passed (5 scenarios).");
    }

    private static void Fail(string message) => Failures.Add($"[{_scenario}] {message}");

    private static void Check(bool condition, string message)
    {
        if (!condition)
        {
            Fail(message);
        }
    }

    private static async Task Until(Func<bool> condition, string what, int milliseconds = 15000)
    {
        var deadline = DateTime.UtcNow.AddMilliseconds(milliseconds);
        while (!condition() && DateTime.UtcNow < deadline)
        {
            await Task.Delay(20);
        }

        Check(condition(), $"timed out waiting for: {what}");
    }

    private static MainWindowViewModel NewViewModel(int width, int height, Func<int, ScenarioBackend> backendFor, string directory, out Func<int> builds)
    {
        var count = 0;
        builds = () => count;
        var settings = new AppSettings { RawWidth = width, RawHeight = height, LaneBVoiWindowWidth = 500f };
        // The constructor builds one backend and InitializeBackend replaces it (builds 1 and 2), so the scripted one answers both.
        return new MainWindowViewModel(
            settings,
            new AppSettingsService(Path.Combine(directory, "appsettings.json")),
            _ => backendFor(++count));
    }

    /// <summary>Loads the fixture image through the public command (the automation path variable skips the file dialog) and waits for both lanes.</summary>
    private static async Task LoadAndDrawLanes(MainWindowViewModel vm, string rawPath)
    {
        Environment.SetEnvironmentVariable("XPE_GUI_AUTOMATION_RAW_PATH", rawPath);
        try
        {
            vm.LoadImageCommand.Execute(null);
            await Until(() => vm.LaneBImage is not null, "the first render's Lane B");
            await Task.Delay(100);
        }
        finally
        {
            Environment.SetEnvironmentVariable("XPE_GUI_AUTOMATION_RAW_PATH", null);
        }
    }

    private static string TempDirectory()
    {
        var directory = Path.Combine(Path.GetTempPath(), "xpe-lifetime-" + Guid.NewGuid().ToString("N")[..8]);
        Directory.CreateDirectory(directory);
        return directory;
    }

    // ---- 1: an Apply that was already running when the shutdown began (Codex #36 finding 1) -----------------------------------------

    private static async Task StartedApplyIsDroppedByAShutdown(string rawPath, int width, int height)
    {
        _scenario = "1 started Apply vs shutdown";
        var directory = TempDirectory();
        try
        {
            var backend = new ScenarioBackend();
            var vm = NewViewModel(width, height, _ => backend, directory, out _);
            await LoadAndDrawLanes(vm, rawPath);

            var drawn = vm.ProcessedImage;
            var chainsBefore = backend.ChainCalls;
            var appliesBefore = backend.ApplyCalls;
            Check(chainsBefore == 2, $"the first render should have made 2 chain calls (main + Lane B), made {chainsBefore}");

            backend.BlockChainCall = chainsBefore + 1;        // the NEXT Apply's main chain waits (as it would for the AI gate)
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => backend.ChainBlocked.IsSet, "the held main chain");

            vm.BeginShutdown();                               // the shutdown begins while that Apply is running ...
            await Until(() => !vm.IsBackendTransitioning, "the shutdown to finish");
            backend.ChainRelease.Set();                       // ... and the held chain is released only afterwards
            await Until(() => backend.ApplyCalls > appliesBefore, "the held Apply's display call");
            await Task.Delay(400);                            // the UI-side continuation

            Check(ReferenceEquals(vm.ProcessedImage, drawn), "the held Apply's result was applied after the shutdown began");
            Check(backend.ChainCalls == chainsBefore + 1, $"a Lane B chain was started after the shutdown began (chain calls {backend.ChainCalls}, expected {chainsBefore + 1})");
            Check(vm.Logs.Any(line => line.Contains("result dropped", StringComparison.Ordinal)), "the drop was not logged");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    // ---- 1b: the same Apply, but its chain FAILS once the shutdown has begun ---------------------------------------------------------------

    private static async Task StartedApplyThatFailsAfterAShutdownRaisesNothing(string rawPath, int width, int height)
    {
        _scenario = "1b started Apply fails after shutdown";
        var directory = TempDirectory();
        try
        {
            var backend = new ScenarioBackend { ThrowAfterRelease = true };
            var vm = NewViewModel(width, height, _ => backend, directory, out _);
            await LoadAndDrawLanes(vm, rawPath);

            var alertsBefore = vm.Alerts.Count;
            var staleBefore = vm.PreviewStaleReason;
            var statusBefore = vm.StatusText;
            backend.BlockChainCall = backend.ChainCalls + 1;
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => backend.ChainBlocked.IsSet, "the held main chain");

            vm.BeginShutdown();
            await Until(() => !vm.IsBackendTransitioning, "the shutdown to finish");
            var statusAfterShutdown = vm.StatusText;
            backend.ChainRelease.Set();                       // the held chain now FAILS, for a backend that has been shut down
            await Until(() => vm.Logs.Any(line => line.Contains("no longer current failed", StringComparison.Ordinal)), "the stale failure to be recognised");
            await Task.Delay(300);

            Check(vm.Alerts.Count == alertsBefore, "a stale Apply failure raised an alert");
            Check(vm.PreviewStaleReason == staleBefore, "a stale Apply failure marked the preview stale");
            Check(vm.StatusText == statusAfterShutdown, $"a stale Apply failure changed the status line ('{statusAfterShutdown}' -> '{vm.StatusText}')");
            _ = statusBefore;
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    // ---- 2: every entry point, during a transition and after it (Codex #36 finding 2) --------------------------------------------------

    private static async Task EveryEntryPointRefusesDuringTheTransition(string rawPath, int width, int height)
    {
        _scenario = "2 entry points vs transition";
        var directory = TempDirectory();
        try
        {
            var backend = new ScenarioBackend();
            var replacement = new ScenarioBackend();
            var vm = NewViewModel(width, height, n => n <= 2 ? backend : replacement, directory, out var builds);
            await LoadAndDrawLanes(vm, rawPath);

            backend.ShutdownMayEnd = new ManualResetEventSlim();
            vm.BeginShutdown();
            Check(vm.IsBackendTransitioning, "the shutdown should be running (held)");

            int Calls() => backend.LoadCalls + backend.PresetCalls + backend.ChainCalls + backend.ApplyCalls;
            var callsBefore = Calls();
            var buildsBefore = builds();
            var source = vm.SourceImage;
            var processed = vm.ProcessedImage;
            var laneB = vm.LaneBImage;

            Environment.SetEnvironmentVariable("XPE_GUI_AUTOMATION_RAW_PATH", rawPath);
            try
            {
                vm.LoadImageCommand.Execute(null);                  // Load image
            }
            finally
            {
                Environment.SetEnvironmentVariable("XPE_GUI_AUTOMATION_RAW_PATH", null);
            }

            vm.ApplyBodyPartPresetCommand.Execute(null);           // VOI preset
            vm.ApplyDisplayPipelineCommand.Execute(null);          // Display pipeline
            vm.RestartAiSessionCommand.Execute(null);              // Restart AI
            vm.InitializeBackendCommand.Execute(null);             // Initialize backend
            vm.SetBackendModeCommand.Execute("Mock");              // (goes through Initialize backend)
            await Task.Delay(500);

            foreach (var entry in new[] { "Load image", "VOI preset", "Display pipeline", "Restart AI", "Initialize backend" })
            {
                Check(vm.Logs.Any(line => line.Contains(entry + ": The backend is shutting down", StringComparison.Ordinal) && line.Contains("not run", StringComparison.Ordinal)),
                    $"'{entry}' was not refused during the transition");
            }

            Check(Calls() == callsBefore, $"a backend call was made during the transition (calls {callsBefore} -> {Calls()})");
            Check(builds() == buildsBefore, $"a backend was built during the transition (builds {buildsBefore} -> {builds()})");
            Check(ReferenceEquals(vm.SourceImage, source) && ReferenceEquals(vm.ProcessedImage, processed) && ReferenceEquals(vm.LaneBImage, laneB),
                "the screen changed during the transition");

            backend.ShutdownMayEnd!.Set();
            await Until(() => !vm.IsBackendTransitioning, "the shutdown to finish");

            // After it: every entry point is admitted again, in the order that keeps the same backend until the replacement.
            var loadsBefore = backend.LoadCalls;
            Environment.SetEnvironmentVariable("XPE_GUI_AUTOMATION_RAW_PATH", rawPath);
            try
            {
                vm.LoadImageCommand.Execute(null);
            }
            finally
            {
                Environment.SetEnvironmentVariable("XPE_GUI_AUTOMATION_RAW_PATH", null);
            }

            await Until(() => backend.LoadCalls == loadsBefore + 1, "Load image to be admitted after the transition");

            var presetsBefore = backend.PresetCalls;
            vm.ApplyBodyPartPresetCommand.Execute(null);
            await Until(() => backend.PresetCalls == presetsBefore + 1, "the VOI preset to be admitted after the transition");

            var chainsBefore = backend.ChainCalls;
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => backend.ChainCalls > chainsBefore, "Display pipeline to be admitted after the transition");

            vm.RestartAiSessionCommand.Execute(null);
            await Until(() => vm.StatusText.Contains("needs the native backend", StringComparison.Ordinal), "Restart AI to be admitted after the transition");

            vm.InitializeBackendCommand.Execute(null);
            await Until(() => builds() == buildsBefore + 1, "Initialize backend to be admitted after the transition");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    // ---- 3: an old Lane B task that FAILS after the backend was replaced (Codex #36 finding 3) -------------------------------------------

    private static async Task AnOldLaneThatFailsAfterAReplacementKeepsTheNewLanes(string rawPath, int width, int height)
    {
        _scenario = "3 stale failing Lane B";
        var directory = TempDirectory();
        try
        {
            var old = new ScenarioBackend { ThrowAfterRelease = true };
            var fresh = new ScenarioBackend();
            var vm = NewViewModel(width, height, n => n <= 2 ? old : fresh, directory, out _);
            await LoadAndDrawLanes(vm, rawPath);

            old.BlockChainCall = old.ChainCalls + 2;          // the next Apply's Lane B call (main is +1) waits, then fails
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => old.ChainBlocked.IsSet, "the old Lane B call to be held");

            vm.InitializeBackendCommand.Execute(null);        // the backend is replaced while that lane task waits
            vm.ApplyDisplayPipelineCommand.Execute(null);     // and the new backend draws new lanes
            await Until(() => fresh.ChainCalls >= 2 && fresh.ApplyCalls >= 2, "the new backend's lanes");
            await Task.Delay(300);
            var laneA = vm.LaneAImage;
            var laneB = vm.LaneBImage;
            Check(laneA is not null && laneB is not null, "the new backend did not draw both lanes");

            old.ChainRelease.Set();                           // the OLD task now fails
            await Until(() => vm.Logs.Any(line => line.Contains("Lane rendering", StringComparison.Ordinal)), "the old lane task's outcome to be logged");
            await Task.Delay(300);

            Check(ReferenceEquals(vm.LaneAImage, laneA) && ReferenceEquals(vm.LaneBImage, laneB), "a stale lane failure erased or replaced the new lanes");
            Check(vm.Logs.Any(line => line.Contains("no longer current failed", StringComparison.Ordinal)), "the stale failure was not recognised as stale");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    // ---- 3b: an old Lane B task that SUCCEEDS after a shutdown started and finished ------------------------------------------------------

    private static async Task AnOldLaneThatSucceedsAfterAShutdownChangesNothing(string rawPath, int width, int height)
    {
        _scenario = "3b stale succeeding Lane B";
        var directory = TempDirectory();
        try
        {
            var backend = new ScenarioBackend();
            var vm = NewViewModel(width, height, _ => backend, directory, out _);
            await LoadAndDrawLanes(vm, rawPath);

            backend.BlockChainCall = backend.ChainCalls + 2;
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => backend.ChainBlocked.IsSet, "the Lane B call to be held");

            var laneB = vm.LaneBImage;
            var timingsBefore = vm.PipelineTimings;
            vm.BeginShutdown();                               // the shutdown begins AND ends while the lane task waits: the same backend
            await Until(() => !vm.IsBackendTransitioning, "the shutdown to finish");   // object is back in place, no transition running

            backend.ChainRelease.Set();                       // the old task now succeeds
            await Until(() => vm.Logs.Any(line => line.Contains("Lane B result dropped", StringComparison.Ordinal)), "the stale success to be dropped");
            await Task.Delay(200);

            Check(ReferenceEquals(vm.LaneBImage, laneB), "a stale lane success replaced Lane B after the shutdown");
            // The Apply's own tail (the timing line) belongs to the render that was dropped: it must not run after the lanes' wait.
            Check(vm.PipelineTimings == timingsBefore, "the dropped render's timing line was written after the lanes' wait");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }
}
