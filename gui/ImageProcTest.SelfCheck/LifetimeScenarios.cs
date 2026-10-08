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
internal class ScenarioBackend : IXpeBackend
{
    private readonly IXpeBackend _inner = new MockXpeBackend(new RawImageLoader(), "xpe_common.dll", false, "xpe_display.dll", false);

    public int LoadCalls;
    public int PresetCalls;
    public int ChainCalls;
    public int ApplyCalls;

    /// <summary>How many chain calls requested the AI stage, and for each call, in order, whether it did (GUI-C-191).</summary>
    public int AiCalls;

    public readonly System.Collections.Concurrent.ConcurrentQueue<bool> AiStageRequestedPerCall = new();

    /// <summary>The 1-based RunChain call that waits for <see cref="ChainRelease"/> (0: none).</summary>
    public int BlockChainCall;

    /// <summary>The held call throws when released (else it carries on and succeeds).</summary>
    public bool ThrowAfterRelease;

    public readonly ManualResetEventSlim ChainBlocked = new();
    public readonly ManualResetEventSlim ChainRelease = new();

    /// <summary>When set, Shutdown waits for it: the transition stays running until the scenario lets it end.</summary>
    public ManualResetEventSlim? ShutdownMayEnd;

    public virtual ChainResult RunChain(LoadedImageFrame rawFrame, IReadOnlyList<StageRequest> stages, AppSettings settings)
    {
        var call = Interlocked.Increment(ref ChainCalls);
        var requestsAi = stages.Any(stage => stage.StageId == StageIds.AiBoneSuppression && stage.Enabled);
        AiStageRequestedPerCall.Enqueue(requestsAi);
        if (requestsAi)
        {
            Interlocked.Increment(ref AiCalls);
        }

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

    public virtual LoadedImageFrame LoadRawImage(string path, AppSettings settings)
    {
        Interlocked.Increment(ref LoadCalls);
        return _inner.LoadRawImage(path, settings);
    }

    public VoiPreset CreateVoiPreset(XpeBodyPartEnum bodyPart)
    {
        Interlocked.Increment(ref PresetCalls);
        return _inner.CreateVoiPreset(bodyPart);
    }

    public virtual LoadedImageFrame ApplyDisplayPipeline(LoadedImageFrame rawFrame, ushort[] displayInput, AppSettings settings)
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

/// <summary>
/// GUI-C-233b: a scripted display stage that REPORTS the window it applied (as RealXpeBackend does): an automatic window answers fixed numbers that differ from the settings' center/width, a hand-chosen one
/// answers the settings', and <see cref="FailAutomaticWindow"/> makes the automatic window fail the way a refused xpe_voi_auto_window call does (the render throws).
/// </summary>
internal sealed class VoiReportingBackend : ScenarioBackend
{
    public const float AutoCenter = 1234.5f;
    public const float AutoWidth = 987.0f;

    public bool FailAutomaticWindow;

    public override LoadedImageFrame ApplyDisplayPipeline(LoadedImageFrame rawFrame, ushort[] displayInput, AppSettings settings)
    {
        if (settings.VoiWindowAuto && FailAutomaticWindow)
        {
            throw new InvalidOperationException("xpe_voi_auto_window failed (-1)");
        }

        var inner = base.ApplyDisplayPipeline(rawFrame, displayInput, settings);
        return new LoadedImageFrame
        {
            Preview = inner.Preview,
            ProcessedPreview = inner.ProcessedPreview,
            Summary = inner.Summary,
            MetadataText = inner.MetadataText,
            RawPixels = inner.RawPixels,
            Width = inner.Width,
            Height = inner.Height,
            BitsStored = inner.BitsStored,
            DisplayPipelineApplied = inner.DisplayPipelineApplied,
            DisplayPipelineSummary = inner.DisplayPipelineSummary,
            AppliedVoi = settings.VoiWindowAuto
                ? new AppliedVoiWindow("LinearExact", AutoCenter, AutoWidth, true)
                : new AppliedVoiWindow(settings.VoiLutMode, settings.VoiWindowCenter, settings.VoiWindowWidth, false),
        };
    }
}

/// <summary>
/// GUI-C-232c: a scripted backend that also offers a Save Corrected Image candidate for every chain run (its Floats carry the run's number, taken when the run STARTS), and refuses to load a path
/// ending in "bad.raw". The candidate hangs on the run's own <see cref="ChainResult"/>, as in <see cref="RealXpeBackend"/>.
/// </summary>
internal sealed class CandidateBackend : ScenarioBackend, ICorrectedImageSource
{
    private readonly System.Runtime.CompilerServices.ConditionalWeakTable<ChainResult, CorrectedImage> _candidates = new();
    private int _runs;

    public CorrectedImage? CorrectedFor(ChainResult chain) => _candidates.TryGetValue(chain, out var corrected) ? corrected : null;

    public override ChainResult RunChain(LoadedImageFrame rawFrame, IReadOnlyList<StageRequest> stages, AppSettings settings)
    {
        var id = Interlocked.Increment(ref _runs);
        var result = base.RunChain(rawFrame, stages, settings);
        _candidates.Add(result, new CorrectedImage([id], 1, 1, rawFrame.RawPixels));
        return result;
    }

    public override LoadedImageFrame LoadRawImage(string path, AppSettings settings) =>
        path.EndsWith("bad.raw", StringComparison.Ordinal) ? throw new InvalidDataException("scripted: this file cannot be opened") : base.LoadRawImage(path, settings);
}

/// <summary>
/// A scenario backend with a scripted AI session (GUI-C-191): it is an <see cref="IAiSessionBackend"/>, which the plain one must not be
/// (the view model asks, and Restart AI on a backend without a session answers differently). Every chain call that requests the AI
/// stage counts as one failure of the scripted worker; <see cref="AiCeiling"/> in a row switch it off.
/// </summary>
internal sealed class AiScenarioBackend : ScenarioBackend, IAiSessionBackend
{
    public bool HasAiSession => true;

    /// <summary>The scripted worker ceiling.</summary>
    public int AiCeiling = 3;

    public AiWorkerStatus GetAiWorkerStatus()
    {
        var failures = Math.Min(Volatile.Read(ref AiCalls), AiCeiling);
        return new AiWorkerStatus(failures >= AiCeiling ? AiWorkerState.Disabled : AiWorkerState.Active, (uint)failures, (uint)AiCeiling);
    }

    public AiRestartResult RestartAiSession(string modelDirectory) => new(false, "scripted backend: no session to restart");
}

/// <summary>
/// An AI session whose FIRST status read is held inside the module and answers from what it saw when it STARTED (GUI-C-192): the read of
/// an Apply that is slow, behind the AI session gate, and whose answer is already old when it comes back.
/// </summary>
internal sealed class HeldReadBackend : ScenarioBackend, IAiSessionBackend
{
    public bool HasAiSession => true;

    public int AiCeiling = 3;
    public readonly ManualResetEventSlim FirstReadEntered = new();
    public readonly ManualResetEventSlim FirstReadRelease = new();
    private int _reads;

    public AiWorkerStatus GetAiWorkerStatus()
    {
        var seen = Volatile.Read(ref AiCalls);
        if (Interlocked.Increment(ref _reads) == 1)
        {
            FirstReadEntered.Set();
            FirstReadRelease.Wait(TimeSpan.FromSeconds(30));
        }

        return new AiWorkerStatus(AiWorkerState.Active, (uint)Math.Min(seen, AiCeiling), (uint)AiCeiling);
    }

    public AiRestartResult RestartAiSession(string modelDirectory) => new(false, "scripted backend: no session to restart");
}

/// <summary>
/// GUI-C-192e: an AI session whose FIRST status read is held and answers "switched off" (the old session's answer), while every later read
/// answers from the new session ("active, 0 of 3"). The restart does not touch the epoch unless asked to, so the view model's own Restart AI wiring
/// is what is observed; <see cref="ReplaceSessionWithoutTellingAnyone"/> moves the epoch the way a frame's re-init would.
/// </summary>
internal sealed class ReplacedSessionBackend : ScenarioBackend, IAiSessionBackend
{
    public bool HasAiSession => true;

    public readonly ManualResetEventSlim OldReadEntered = new();
    public readonly ManualResetEventSlim OldReadRelease = new();
    private int _reads;
    private int _epoch;

    public AiWorkerStatus GetAiWorkerStatus()
    {
        if (Interlocked.Increment(ref _reads) == 1)
        {
            OldReadEntered.Set();
            OldReadRelease.Wait(TimeSpan.FromSeconds(30));
            return new AiWorkerStatus(AiWorkerState.Disabled, 3, 3);
        }

        return new AiWorkerStatus(AiWorkerState.Active, 0, 3);
    }

    public AiRestartResult RestartAiSession(string modelDirectory) => new(true, "AI session restarted: scripted");

    public void ReplaceSessionWithoutTellingAnyone() => Interlocked.Increment(ref _epoch);

    int IAiSessionBackend.AiSessionEpoch => Volatile.Read(ref _epoch);
}

/// <summary>
/// GUI-C-192f: an AI session whose status read answers from a script the scenario changes (Disabled first, then a failure), and whose epoch the
/// scenario moves the way a frame's re-init does: with nobody calling Reset.
/// </summary>
internal sealed class ScriptedSessionBackend : ScenarioBackend, IAiSessionBackend
{
    public bool HasAiSession => true;

    public volatile bool ReadsFail;
    private int _epoch;

    public AiWorkerStatus GetAiWorkerStatus() =>
        ReadsFail ? throw new InvalidOperationException("scripted: the new session's read fails") : new AiWorkerStatus(AiWorkerState.Disabled, 3, 3);

    public AiRestartResult RestartAiSession(string modelDirectory) => new(true, "AI session restarted: scripted");

    public void ReplaceSessionWithoutTellingAnyone() => Interlocked.Increment(ref _epoch);

    int IAiSessionBackend.AiSessionEpoch => Volatile.Read(ref _epoch);
}

internal static class LifetimeScenarios
{
    private static readonly List<string> Failures = [];
    private static string _scenario = string.Empty;

    /// <summary>
    /// The scenarios are about ORDER (what a late result may change), not about pixels, so they run on a small generated image: the
    /// 3072x3072 fixture cost about a second per scenario in load and render alone, and the whole runner has to finish inside the
    /// 15 s the app waits for it (MainWindow, row 15). A04 skipped when it did not (GUI-C-190b).
    /// </summary>
    private const int SmallSide = 256;

    private static string WriteSmallRaw(string directory)
    {
        var path = Path.Combine(directory, "lifetime-256.raw");
        var bytes = new byte[SmallSide * SmallSide * 2];
        for (var i = 0; i < SmallSide * SmallSide; i++)
        {
            var value = (ushort)(((i % SmallSide) * 211 + (i / SmallSide) * 97) & 0xFFFF);   // a ramp with structure, not a constant
            bytes[2 * i] = (byte)(value & 0xFF);
            bytes[2 * i + 1] = (byte)(value >> 8);
        }

        File.WriteAllBytes(path, bytes);
        return path;
    }

    /// <summary>Lets everything already queued on the UI thread run (a continuation posted by a finished await included), then returns.</summary>
    private static async Task FlushUi() => await Dispatcher.Yield(DispatcherPriority.ApplicationIdle);

    public static void Run()
    {
        var scratch = TempDirectory();
        var rawPath = WriteSmallRaw(scratch);
        var width = SmallSide;
        var height = SmallSide;
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
                        await Timed(() => StartedApplyIsDroppedByAShutdown(rawPath, width, height));
                        await Timed(() => StartedApplyThatFailsAfterAShutdownRaisesNothing(rawPath, width, height));
                        await Timed(() => EveryEntryPointRefusesDuringTheTransition(rawPath, width, height));
                        await Timed(() => AnOldLaneThatFailsAfterAReplacementKeepsTheNewLanes(rawPath, width, height));
                        await Timed(() => AnOldLaneThatSucceedsAfterAShutdownChangesNothing(rawPath, width, height));
                        await Timed(() => AnOlderApplyFinishingLateChangesNothing(rawPath, width, height, holdLane: false, fail: false));
                        await Timed(() => AnOlderApplyFinishingLateChangesNothing(rawPath, width, height, holdLane: false, fail: true));
                        await Timed(() => AnOlderApplyFinishingLateChangesNothing(rawPath, width, height, holdLane: true, fail: false));
                        await Timed(() => AnOlderApplyFinishingLateChangesNothing(rawPath, width, height, holdLane: true, fail: true));
                        await Timed(() => TheCandidateLaneIsASecondAiCall_AndTheStatusIsReadAfterIt(rawPath, width, height));
                        await Timed(() => EqualLanesMakeOneAiCallPerApply(rawPath, width, height));
                        await Timed(() => TheHudShowsTheWindowThatWasApplied_NotTheSettings(rawPath, width, height));
                        await Timed(() => TheAutomationReportDescribesTheCurrentFrame_NotThePreviousOne(rawPath, width, height));
                        await Timed(() => TheSaveCandidateOfAnOlderApplyFinishingLateIsNeverCommitted(rawPath, width, height));
                        await Timed(() => AFailedOpenKeepsTheSaveNameAndTheCandidateOfTheImageThatIsStillOpen(rawPath, width, height));
#if XPE_TEST_FAULTS
                        await Timed(() => TheAiWorkerDisabledFault_IsInertWithoutTheArgument_AndAnswersOffWithIt(rawPath, width, height));
#endif
                        await Timed(() => AnOlderStatusReadIsNeverShownAfterANewerRequest(rawPath, width, height));
                        await Timed(() => AnOldDisabledAnswerIsNeverShownForANewSession(rawPath, width, height, viaRestartCommand: true));
                        await Timed(() => AnOldDisabledAnswerIsNeverShownForANewSession(rawPath, width, height, viaRestartCommand: false));
                        await Timed(() => ADisabledAlreadyOnScreenIsTakenBackWhenTheSessionIsReplaced(rawPath, width, height));
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

        Directory.Delete(scratch, recursive: true);
        Console.WriteLine($"Lifetime scenarios passed ({_scenariosRun} scenarios).");
    }

    /// <summary>Runs one scenario and prints how long it took: the whole runner has to finish inside the app's wait for it (15 s, MainWindow), and this says where the time goes.</summary>
    private static int _scenariosRun;

    private static async Task Timed(Func<Task> scenario)
    {
        var watch = System.Diagnostics.Stopwatch.StartNew();
        await scenario();
        _scenariosRun++;
        Console.WriteLine($"  scenario '{_scenario}': {watch.ElapsedMilliseconds} ms");
    }

    private static string Id(object? image) => image is null ? "null" : System.Runtime.CompilerServices.RuntimeHelpers.GetHashCode(image).ToString("x", System.Globalization.CultureInfo.InvariantCulture);

    /// <summary>What a red run needs to be read without a rerun: which image objects are on screen, and the last log lines (they carry the request numbers of a dropped result).</summary>
    private static string State(MainWindowViewModel vm) =>
        $"[screen: processed={Id(vm.ProcessedImage)} laneA={Id(vm.LaneAImage)} laneB={Id(vm.LaneBImage)} laneBStale={vm.LaneBIsStale}; last logs: {string.Join(" | ", vm.Logs.TakeLast(6))}]";

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

    private static MainWindowViewModel NewViewModel(int width, int height, Func<int, ScenarioBackend> backendFor, string directory, out Func<int> builds, float laneBVoiWindowWidth = 500f, bool aiInChain = false)
    {
        var count = 0;
        builds = () => count;
        var settings = new AppSettings { RawWidth = width, RawHeight = height, LaneBVoiWindowWidth = laneBVoiWindowWidth, AiBoneSuppressionInChain = aiInChain };
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
            await FlushUi();                                    // the rest of the first render's continuation (timing line, flags)
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
            await Until(() => vm.Logs.Any(line => line.Contains("result dropped", StringComparison.Ordinal)), "the held Apply's result to be dropped");
            await FlushUi();                                  // anything its continuation would still do after the log line

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
            await FlushUi();

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
            vm.RunDeterministicBaselineCommand.Execute(null);      // Deterministic Baseline
            vm.InitializeBackendCommand.Execute(null);             // Initialize backend
            vm.SetBackendModeCommand.Execute("Mock");              // (goes through Initialize backend)
            await Until(() => new[] { "Load image", "VOI preset", "Display pipeline", "Restart AI", "Deterministic Baseline", "Initialize backend" }.All(entry =>
                vm.Logs.Any(line => line.Contains(entry + ": The backend is shutting down", StringComparison.Ordinal))), "every entry point to be refused");
            await FlushUi();                                       // a refusal that was NOT one would have started work by now

            foreach (var entry in new[] { "Load image", "VOI preset", "Display pipeline", "Restart AI", "Deterministic Baseline", "Initialize backend" })
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

            var laneBOfTheFirstRender = vm.LaneBImage;
            old.BlockChainCall = old.ChainCalls + 2;          // the next Apply's Lane B call (main is +1) waits, then fails
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => old.ChainBlocked.IsSet, "the old Lane B call to be held");

            vm.InitializeBackendCommand.Execute(null);        // the backend is replaced while that lane task waits
            vm.ApplyDisplayPipelineCommand.Execute(null);     // and the new backend draws new lanes
            await Until(() => fresh.ChainCalls >= 2 && fresh.ApplyCalls >= 2, "the new backend's lanes");
            // The new backend's own end, awaited as an event (this used to be a 300 ms sleep that a slow runner outran: GUI-C-190b): its
            // Lane B is drawn, which is an image object other than the one the first render left on screen.
            await Until(() => !ReferenceEquals(vm.LaneBImage, laneBOfTheFirstRender), "the new backend's Lane B to be drawn");
            await FlushUi();
            var laneA = vm.LaneAImage;
            var laneB = vm.LaneBImage;
            Check(laneA is not null && laneB is not null, $"the new backend did not draw both lanes {State(vm)}");

            old.ChainRelease.Set();                           // the OLD task now fails
            await Until(() => vm.Logs.Any(line => line.Contains("Lane rendering of a backend that is no longer current failed", StringComparison.Ordinal)), "the old lane task's outcome to be logged");
            await FlushUi();

            Check(ReferenceEquals(vm.LaneAImage, laneA) && ReferenceEquals(vm.LaneBImage, laneB),
                $"a stale lane failure erased or replaced the new lanes (before: laneA={Id(laneA)} laneB={Id(laneB)}) {State(vm)}");
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
            await FlushUi();

            Check(ReferenceEquals(vm.LaneBImage, laneB), $"a stale lane success replaced Lane B after the shutdown {State(vm)}");
            // The Apply's own tail (the timing line) belongs to the render that was dropped: it must not run after the lanes' wait.
            Check(vm.PipelineTimings == timingsBefore, "the dropped render's timing line was written after the lanes' wait");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    // ---- 4: two Applies on the SAME backend; the older one finishes last (Codex #42, GUI-C-190) ----------------------------------------
    //
    // The lifetime generation only tells "this backend" from "a shutdown or a replacement": both Applies hold the same one, so each
    // looks current. The request number is what makes the older one stale. A native call cannot be cut short, so it is not stopped:
    // it runs to its end and its result is dropped. Four variants: the held call is the main chain or Lane B's chain, and it
    // succeeds or fails once released.

    private static async Task AnOlderApplyFinishingLateChangesNothing(string rawPath, int width, int height, bool holdLane, bool fail)
    {
        _scenario = $"4 older Apply finishes last ({(holdLane ? "Lane B" : "main chain")}, {(fail ? "fails" : "succeeds")})";
        var directory = TempDirectory();
        try
        {
            var backend = new ScenarioBackend { ThrowAfterRelease = fail };
            var vm = NewViewModel(width, height, _ => backend, directory, out _);
            await LoadAndDrawLanes(vm, rawPath);                 // chain calls 1 (main) and 2 (Lane B)

            // Apply A: its main chain is call 3, its Lane B chain is call 4. One of them is held.
            backend.BlockChainCall = holdLane ? 4 : 3;
            var settingsOfA = vm.Settings.LaneBVoiWindowWidth;
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => backend.ChainBlocked.IsSet, "Apply A's held chain call");

            // Apply B: different settings, on the same backend, started and finished while A still waits (its main and its Lane B chain).
            var laneBBeforeB = vm.LaneBImage;
            vm.Settings.LaneBVoiWindowWidth = settingsOfA + 300f;
            vm.ApplyDisplayPipelineCommand.Execute(null);
            // A's Lane B chain is call 4 only when A got that far (the held lane variants); B then makes the next two calls.
            var chainCallsAfterB = holdLane ? 6 : 5;
            await Until(() => backend.ChainCalls >= chainCallsAfterB, "Apply B's two chain calls");
            // B's own end, awaited as an event: its Lane B drawn with the new settings. Its tail (the timing line) runs in the same
            // continuation chain, which the flush lets finish.
            await Until(() => !ReferenceEquals(vm.LaneBImage, laneBBeforeB) && !vm.LaneBIsStale, "Apply B's Lane B");
            await FlushUi();

            var processed = vm.ProcessedImage;
            var laneA = vm.LaneAImage;
            var laneB = vm.LaneBImage;
            var status = vm.StatusText;
            var timings = vm.PipelineTimings;
            var alerts = vm.Alerts.Count;
            var stale = vm.PreviewStaleReason;
            var chainCalls = backend.ChainCalls;
            Check(laneB is not null && !vm.LaneBIsStale, "Apply B did not draw its Lane B with its own settings");

            backend.ChainRelease.Set();                          // A's held call ends, long after B finished
            await Until(() => vm.Logs.Any(line => line.Contains("a newer Apply", StringComparison.Ordinal)
                || line.Contains("no longer current failed", StringComparison.Ordinal)), "A's late outcome to be recognised as stale");
            await FlushUi();                                     // whatever A's continuation would still do after the log line
            await FlushUi();

            Check(ReferenceEquals(vm.ProcessedImage, processed), $"the older Apply replaced the main image {State(vm)}");
            Check(ReferenceEquals(vm.LaneAImage, laneA) && ReferenceEquals(vm.LaneBImage, laneB), $"the older Apply replaced or erased the lanes {State(vm)}");
            Check(!vm.LaneBIsStale, $"the older Apply put its own Lane B settings back (the lanes no longer match the settings on screen) {State(vm)}");
            Check(vm.StatusText == status, $"the status line changed ('{status}' -> '{vm.StatusText}')");
            Check(vm.PipelineTimings == timings, "the older Apply wrote its timing line");
            Check(vm.Alerts.Count == alerts, "the older Apply's failure raised an alert");
            Check(vm.PreviewStaleReason == stale, "the older Apply's failure marked the preview stale");
            Check(backend.ChainCalls == chainCalls, $"the older Apply started more work after B (chain calls {chainCalls} -> {backend.ChainCalls})");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    // ---- 5: the Candidate lane is a SECOND AI call, and the worker status is read after it (GUI-C-191) --------------------------------
    //
    // The first native C-09 run (main CI): the module said "off" on the second attempt, and every attempt made one AI call more than the
    // chain text showed. This is where it comes from: the Candidate lane's chain is the main chain with the lane's overrides, so it
    // requests the AI stage too whenever the lanes differ. The status read that ReportChain asks for comes BEFORE the lanes, so a
    // worker switched off by the lane's call was not shown until the next Apply.

    private static async Task TheCandidateLaneIsASecondAiCall_AndTheStatusIsReadAfterIt(string rawPath, int width, int height)
    {
        _scenario = "5 Candidate lane = second AI call, status read after it";
        var directory = TempDirectory();
        try
        {
            var backend = new AiScenarioBackend { AiCeiling = 2 };   // two AI calls in a row switch the scripted worker off
            var vm = NewViewModel(width, height, _ => backend, directory, out _, laneBVoiWindowWidth: 500f, aiInChain: true);
            await LoadAndDrawLanes(vm, rawPath);                   // ONE Apply: main chain + Candidate chain, both with the AI stage

            Check(backend.AiCalls == 2, $"one Apply with differing lanes should make 2 AI calls (main + Candidate), made {backend.AiCalls}");
            Check(backend.AiStageRequestedPerCall.ToArray().All(requested => requested),
                $"both chains of the Apply should request the AI stage: {string.Join(",", backend.AiStageRequestedPerCall)}");

            // The status is the module's: switched off by the SECOND call. It has to reach the screen without another Apply.
            await Until(() => vm.AiWorkerMarkVisible, "the worker switched off by the Candidate lane's call to be shown", 5000);
            Check(vm.AiWorkerStatusSummary == "worker=Disabled; failures=2; ceiling=2",
                $"the status after the Candidate lane's call was '{vm.AiWorkerStatusSummary}' {State(vm)}");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static async Task EqualLanesMakeOneAiCallPerApply(string rawPath, int width, int height)
    {
        _scenario = "5b equal lanes = one AI call per Apply";
        var directory = TempDirectory();
        try
        {
            var backend = new AiScenarioBackend { AiCeiling = 2 };
            var vm = NewViewModel(width, height, _ => backend, directory, out _, laneBVoiWindowWidth: 0f, aiInChain: true);
            await LoadAndDrawLanes(vm, rawPath);

            Check(backend.AiCalls == 1, $"one Apply with equal lanes should make 1 AI call, made {backend.AiCalls}");
            Check(backend.ChainCalls == 1, $"one Apply with equal lanes should make 1 chain call (the Candidate is the Reference), made {backend.ChainCalls}");
            await Until(() => vm.AiWorkerStatusSummary == "worker=Active; failures=1; ceiling=2", "the status of the one AI call", 5000);
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    // ---- 6: the ai-worker-disabled automation fault (GUI-C-191b) -------------------------------------------------------------------------
    //
    // The seam that lets a UI test put the "switched off" mark on screen without a native module. It has to be inert unless asked for
    // and, when asked, answer exactly "switched off, 3 of 3" while everything else passes through to the real backend.

#if XPE_TEST_FAULTS
    private static Task TheAiWorkerDisabledFault_IsInertWithoutTheArgument_AndAnswersOffWithIt(string rawPath, int width, int height)
    {
        _scenario = "6 ai-worker-disabled fault";
        _ = rawPath;
        _ = width;
        _ = height;
        IXpeBackend Mock() => new MockXpeBackend(new RawImageLoader(), "xpe_common.dll", false, "xpe_display.dll", false);

        // Off: the backend itself comes back, and it is not an AI session (the Mock has none).
        var plain = Mock();
        var unwrapped = FaultInjectingBackend.Wrap(plain, failAfter: null, aiWorkerDisabled: false);
        Check(ReferenceEquals(unwrapped, plain), "Wrap changed the backend although no fault was requested");
        Check(unwrapped is not IAiSessionBackend, "the plain Mock backend became an AI session backend");

        // On: the wrapper answers the injected status and nothing else about the AI session.
        var armed = FaultInjectingBackend.Wrap(Mock(), failAfter: null, aiWorkerDisabled: true);
        Check(armed is FaultInjectingBackend, "the fault did not wrap the backend");
        var status = (armed as IAiSessionBackend)?.GetAiWorkerStatus();
        Check(status == new AiWorkerStatus(AiWorkerState.Disabled, 3, 3), $"the injected status was {status}");
        Check(AiBoneSuppressionStage.DescribeStatus(status!) == "worker=Disabled; failures=3; ceiling=3", $"the summary of the injected status was '{AiBoneSuppressionStage.DescribeStatus(status!)}'");
        Check(AiBoneSuppressionStage.ShowsMark(status!), "the injected status does not show the mark");
        Check(FaultInjectingBackend.Describe() == "faultInjection=ai-worker-disabled calls=0", $"the fault status line was '{FaultInjectingBackend.Describe()}'");

        // The display fault, when also given, keeps its own wording and behaviour.
        var both = FaultInjectingBackend.Wrap(Mock(), failAfter: 2, aiWorkerDisabled: true);
        Check(FaultInjectingBackend.Describe() == "faultInjection=display-pipeline-after:2 calls=0 ai-worker-disabled", $"the combined fault status line was '{FaultInjectingBackend.Describe()}'");
        _ = both;
        return Task.CompletedTask;
    }
#endif

    // ---- 7: an older status read is never shown after a newer request (GUI-C-192, the GUI-C-191b reproduction) ---------------------------
    //
    // Recorded where the screen's property changes (PropertyChanged), not sampled: a stale value shown for a millisecond is exactly what
    // a poll misses (the first version of this experiment did miss it).

    private static async Task AnOlderStatusReadIsNeverShownAfterANewerRequest(string rawPath, int width, int height)
    {
        _scenario = "7 older status read never shown";
        var directory = TempDirectory();
        try
        {
            var backend = new HeldReadBackend();
            var vm = NewViewModel(width, height, _ => backend, directory, out _, laneBVoiWindowWidth: 0f, aiInChain: true);
            var timeline = new List<string>();
            vm.PropertyChanged += (_, e) =>
            {
                if (e.PropertyName == nameof(MainWindowViewModel.AiWorkerStatusSummary))
                {
                    timeline.Add(vm.AiWorkerStatusSummary);
                }
            };

            // ReportChain logs one line per stage and asks for the status read right after publishing the chain. (LastChain is no use as the
            // event: a chain equal to the last one is not a change, and Apply B's chain equals Apply A's.)
            int ChainsReported() => vm.Logs.Count(line => line.Contains("Chain ai_bone_suppress:", StringComparison.Ordinal));

            Environment.SetEnvironmentVariable("XPE_GUI_AUTOMATION_RAW_PATH", rawPath);
            try
            {
                vm.LoadImageCommand.Execute(null);                 // Apply A: one AI call; its status read starts and is held
            }
            finally
            {
                Environment.SetEnvironmentVariable("XPE_GUI_AUTOMATION_RAW_PATH", null);
            }

            await Until(() => backend.FirstReadEntered.IsSet, "the first status read to be inside the module");
            await Until(() => vm.ProcessedImage is not null, "Apply A's image");
            await FlushUi();

            var chainsBeforeB = ChainsReported();
            vm.ApplyDisplayPipelineCommand.Execute(null);          // Apply B: a second AI call, its read is requested while the first is held
            // Apply B's own request for a status read comes at the END of its chain (ReportChain), not when its AI call starts: the held
            // read can only be "older than a newer request" once that request has been made, so B is waited for as an event.
            await Until(() => ChainsReported() > chainsBeforeB, "Apply B's chain to be reported");
            await FlushUi();
            Check(timeline.Count == 0, $"a status reached the screen while the only read was held: {string.Join(" | ", timeline)}");

            backend.FirstReadRelease.Set();                        // the old read answers (it saw fewer AI calls than there are now)
            await Until(() => timeline.Any(entry => entry.Contains("failures=2", StringComparison.Ordinal)), "the newest status to be shown", 5000);
            await FlushUi();

            Check(timeline.Count == 1 && timeline[0] == "worker=Active; failures=2; ceiling=3",
                $"the screen showed {timeline.Count} change(s) [{string.Join(" | ", timeline)}]; expected exactly the newest answer, never the older one");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    // ---- 8: an older "switched off" answer is never shown for a NEW session (GUI-C-192e, Codex #62 finding 1) ----------------------------
    //
    // The first read is held inside the module and will answer "switched off" (the old session). Then the session is replaced, through the real
    // Restart AI command (viaRestartCommand) or the way a frame's re-init would, with nobody calling Reset (the epoch). The old answer is released
    // AFTER; it must never reach the screen. Recorded at PropertyChanged, not sampled.

    private static async Task AnOldDisabledAnswerIsNeverShownForANewSession(string rawPath, int width, int height, bool viaRestartCommand)
    {
        _scenario = viaRestartCommand ? "8 Restart AI vs an older Disabled answer" : "8b session replaced without Reset (epoch)";
        var directory = TempDirectory();
        try
        {
            var backend = new ReplacedSessionBackend();
            var vm = NewViewModel(width, height, _ => backend, directory, out _, laneBVoiWindowWidth: 0f, aiInChain: true);
            var timeline = new List<string>();
            vm.PropertyChanged += (_, e) =>
            {
                if (e.PropertyName == nameof(MainWindowViewModel.AiWorkerStatusSummary))
                {
                    timeline.Add(vm.AiWorkerStatusSummary);
                }
            };

            int ChainsReported() => vm.Logs.Count(line => line.Contains("Chain ai_bone_suppress:", StringComparison.Ordinal));

            Environment.SetEnvironmentVariable("XPE_GUI_AUTOMATION_RAW_PATH", rawPath);
            try
            {
                vm.LoadImageCommand.Execute(null);                 // Apply A: its status read starts and is held (the old session's answer)
            }
            finally
            {
                Environment.SetEnvironmentVariable("XPE_GUI_AUTOMATION_RAW_PATH", null);
            }

            await Until(() => backend.OldReadEntered.IsSet, "the old status read to be inside the module");
            await Until(() => vm.ProcessedImage is not null, "Apply A's image");
            await FlushUi();

            if (viaRestartCommand)
            {
                vm.RestartAiSessionCommand.Execute(null);          // the real command: it must raise the generation itself
                await Until(() => vm.Logs.Any(line => line.Contains("AI session restarted", StringComparison.Ordinal)), "the restart to finish");
            }
            else
            {
                var chainsBefore = ChainsReported();
                backend.ReplaceSessionWithoutTellingAnyone();      // no Reset: only the epoch moves
                vm.ApplyDisplayPipelineCommand.Execute(null);      // a newer request, as in the real wiring
                await Until(() => ChainsReported() > chainsBefore, "Apply B's chain to be reported");
            }

            await FlushUi();
            Check(timeline.Count == 0 || timeline.All(entry => !entry.Contains("Disabled", StringComparison.Ordinal)),
                $"a 'switched off' status reached the screen before the old read was released: {string.Join(" | ", timeline)}");

            backend.OldReadRelease.Set();                          // the old session's answer comes back, late
            await Until(() => timeline.Any(entry => entry.Contains("worker=Active; failures=0", StringComparison.Ordinal)), "the new session's status to be shown", 5000);
            await FlushUi();

            Check(timeline.All(entry => !entry.Contains("Disabled", StringComparison.Ordinal)),
                $"the old session's 'switched off' answer was shown for the new session: [{string.Join(" | ", timeline)}]");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    // ---- 9: a "switched off" ALREADY on screen is taken back when the session is replaced with no Reset (GUI-C-192f, Codex #66 finding 1) -----
    //
    // The mark is on screen. A frame then replaces the session (the epoch moves; Reset is NOT called) and every later read fails. The mark must go
    // at the next timer tick (one second) and stay gone: a failing read is exactly what used to leave the old banner up for good.

    private static async Task ADisabledAlreadyOnScreenIsTakenBackWhenTheSessionIsReplaced(string rawPath, int width, int height)
    {
        _scenario = "9 Disabled on screen vs a replaced session";
        var directory = TempDirectory();
        try
        {
            var backend = new ScriptedSessionBackend();
            var vm = NewViewModel(width, height, _ => backend, directory, out _, laneBVoiWindowWidth: 0f, aiInChain: true);

            Environment.SetEnvironmentVariable("XPE_GUI_AUTOMATION_RAW_PATH", rawPath);
            try
            {
                vm.LoadImageCommand.Execute(null);
            }
            finally
            {
                Environment.SetEnvironmentVariable("XPE_GUI_AUTOMATION_RAW_PATH", null);
            }

            await Until(() => vm.AiWorkerMarkVisible, "the switched-off mark to be on screen", 5000);

            backend.ReadsFail = true;                              // the new session's reads fail
            backend.ReplaceSessionWithoutTellingAnyone();          // no Reset: only the epoch moves
            await Until(() => !vm.AiWorkerMarkVisible, "the old switched-off mark to be taken back (the timer ticks once a second)", 5000);
            Check(vm.AiWorkerStatusSummary == "worker=Unknown", $"after the withdrawal the summary was '{vm.AiWorkerStatusSummary}', expected worker=Unknown");

            await Task.Delay(1500);                                // the failing reads go on; the old mark must not come back
            await FlushUi();
            Check(!vm.AiWorkerMarkVisible && !vm.AiWorkerDisabled, "the old switched-off state came back while the new session's reads were failing");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    // ---- GUI-C-232c (Codex #166 / #165 items 2 and 3): the Save Corrected Image candidate and the save name -----------------------------------

    private static CorrectedImage? Committed(MainWindowViewModel vm) =>
        (CorrectedImage?)typeof(MainWindowViewModel).GetField("_committedCorrected", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance)!.GetValue(vm);

    private static string? SaveSourcePath(MainWindowViewModel vm) =>
        (string?)typeof(MainWindowViewModel).GetProperty("ActiveImageSourcePath", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance)!.GetValue(vm);

    /// <summary>Apply A starts first and finishes LAST; Apply B starts later and finishes first. The save candidate stays B's, byte for byte (its Floats carry the run number).</summary>
    private static async Task TheSaveCandidateOfAnOlderApplyFinishingLateIsNeverCommitted(string rawPath, int width, int height)
    {
        _scenario = "8 save candidate: an older Apply finishing late";
        var directory = TempDirectory();
        try
        {
            var backend = new CandidateBackend();
            var vm = NewViewModel(width, height, _ => backend, directory, out _);
            await LoadAndDrawLanes(vm, rawPath);                 // chain runs 1 and 2
            Check(Committed(vm) is not null, "no save candidate after the first render");

            backend.BlockChainCall = 3;                          // Apply A's main chain
            var settingsOfA = vm.Settings.LaneBVoiWindowWidth;
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => backend.ChainBlocked.IsSet, "Apply A's held chain run");

            var laneBBeforeB = vm.LaneBImage;
            vm.Settings.LaneBVoiWindowWidth = settingsOfA + 300f;
            vm.ApplyDisplayPipelineCommand.Execute(null);        // Apply B: runs 4 and 5
            await Until(() => backend.ChainCalls >= 5, "Apply B's two chain runs");
            await Until(() => !ReferenceEquals(vm.LaneBImage, laneBBeforeB) && !vm.LaneBIsStale, "Apply B's Lane B");
            await FlushUi();

            var committedByB = Committed(vm);
            Check(committedByB is not null && committedByB.Floats[0] == 4f, $"after B the candidate is not B's run 4 (got {committedByB?.Floats[0]})");

            backend.ChainRelease.Set();                          // A (run 3) ends long after B
            await Until(() => vm.Logs.Any(line => line.Contains("a newer Apply", StringComparison.Ordinal)
                || line.Contains("no longer current failed", StringComparison.Ordinal)), "A's late outcome to be recognised as stale");
            await FlushUi();
            await FlushUi();

            var after = Committed(vm);
            Check(ReferenceEquals(after, committedByB), $"the older Apply replaced the save candidate (run {after?.Floats[0]} instead of 4)");
            Check(after!.Floats.SequenceEqual(new[] { 4f }), "the candidate bytes changed");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    /// <summary>Open A (and run it), then fail to open B: the save name still comes from A and the candidate is still A's.</summary>
    private static async Task AFailedOpenKeepsTheSaveNameAndTheCandidateOfTheImageThatIsStillOpen(string rawPath, int width, int height)
    {
        _scenario = "9 failed open keeps the save name and candidate";
        var directory = TempDirectory();
        try
        {
            var backend = new CandidateBackend();
            var vm = NewViewModel(width, height, _ => backend, directory, out _);
            await LoadAndDrawLanes(vm, rawPath);
            var candidate = Committed(vm);
            Check(candidate is not null, "no save candidate after opening A");
            Check(SaveSourcePath(vm) == rawPath, $"the save name does not come from A ({SaveSourcePath(vm)})");

            var badPath = Path.Combine(directory, "bad.raw");
            File.WriteAllBytes(badPath, new byte[2]);               // the automation path is only used when the file exists; the scripted backend refuses it
            Environment.SetEnvironmentVariable("XPE_GUI_AUTOMATION_RAW_PATH", badPath);
            try
            {
                vm.LoadImageCommand.Execute(null);
                await Until(() => vm.StatusText.Contains("fail", StringComparison.OrdinalIgnoreCase) || vm.Logs.Any(l => l.Contains("cannot be opened", StringComparison.Ordinal)), "the failed open to be reported");
                await FlushUi();
            }
            finally
            {
                Environment.SetEnvironmentVariable("XPE_GUI_AUTOMATION_RAW_PATH", null);
            }

            Check(SaveSourcePath(vm) == rawPath, $"the failed open of B renamed the save name ({SaveSourcePath(vm)})");
            Check(ReferenceEquals(Committed(vm), candidate), "the failed open changed the save candidate");
            Check(!SaveSourcePath(vm)!.EndsWith("bad.raw", StringComparison.Ordinal), "the save name is B's");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    // ---- GUI-C-233b (Codex #171 finding 1): the HUD and the settings panel show the window that was APPLIED --------------------------------

    private static async Task TheHudShowsTheWindowThatWasApplied_NotTheSettings(string rawPath, int width, int height)
    {
        _scenario = "10 HUD shows the applied window";
        var directory = TempDirectory();
        try
        {
            var backend = new VoiReportingBackend();
            var vm = NewViewModel(width, height, _ => backend, directory, out _);
            Check(vm.Settings.VoiWindowAuto, "the automatic window is not the default");
            await LoadAndDrawLanes(vm, rawPath);

            // 1. automatic: the HUD carries the numbers the display stage chose, not the settings' fixed center/width/mode
            Check(vm.RenderedVoiCenter == VoiReportingBackend.AutoCenter && vm.RenderedVoiWidth == VoiReportingBackend.AutoWidth && vm.RenderedVoiMode == "LinearExact",
                $"the HUD does not show the applied automatic window (C={vm.RenderedVoiCenter} W={vm.RenderedVoiWidth} mode={vm.RenderedVoiMode}; settings say C={vm.Settings.VoiWindowCenter} W={vm.Settings.VoiWindowWidth} mode={vm.Settings.VoiLutMode})");
            Check(vm.PreviewStaleReason is null, $"a fresh automatic render is already stale: {vm.PreviewStaleReason}");

            // 2. automatic -> chosen by hand: the picture on screen no longer matches what is asked for (stale), then Apply makes the HUD show the chosen numbers
            vm.Settings.VoiWindowAuto = false;
            Check(vm.PreviewStaleReason is not null, "switching the automatic window off did not mark the picture stale (VoiWindowAuto is not in the comparison)");
            var applies = backend.ApplyCalls;
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => backend.ApplyCalls > applies, "the manual-window render");
            await Until(() => vm.PreviewStaleReason is null && vm.RenderedVoiMode == vm.Settings.VoiLutMode, "the manual-window render to be shown");
            Check(vm.RenderedVoiCenter == vm.Settings.VoiWindowCenter && vm.RenderedVoiWidth == vm.Settings.VoiWindowWidth, "after a manual render the HUD does not show the chosen window");

            // 3. chosen by hand -> automatic, and the automatic window FAILS: nothing new is drawn, the HUD keeps describing the picture that is still on screen, and the failure is shown
            backend.FailAutomaticWindow = true;
            var shown = vm.ProcessedImage;
            var (centerBefore, widthBefore, modeBefore) = (vm.RenderedVoiCenter, vm.RenderedVoiWidth, vm.RenderedVoiMode);
            vm.UseAutomaticWindowCommand.Execute(null);
            await Until(() => vm.StatusText.StartsWith("Display pipeline failed", StringComparison.Ordinal), "the failed automatic render to be reported");   // (the stale reason is already set by the settings change itself)
            await FlushUi();
            Check(ReferenceEquals(vm.ProcessedImage, shown), "a failed automatic render replaced the picture on screen");
            Check(vm.RenderedVoiCenter == centerBefore && vm.RenderedVoiWidth == widthBefore && vm.RenderedVoiMode == modeBefore, "a failed automatic render changed what the HUD says was rendered");
            Check(vm.StatusText.Contains("xpe_voi_auto_window failed (-1)", StringComparison.Ordinal), "the status line does not name the failed automatic window and its code");
            Check(vm.Alerts.Any(a => a.Code == "DISPLAY_PIPELINE_FAILED" && a.Message.Contains("xpe_voi_auto_window", StringComparison.Ordinal)), "no DISPLAY_PIPELINE_FAILED alert names the automatic window");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    // ---- GUI-C-233c (Codex #173 finding 1): the automation report describes THIS frame's render ------------------------------------------

    private static System.Text.Json.JsonElement ReportDisplayPipeline(MainWindowViewModel vm)
    {
        typeof(MainWindowViewModel).GetMethod("ExportAutomationReport", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance)!.Invoke(vm, null);
        var path = Path.Combine(AppContext.BaseDirectory, "menu-command-report.json");
        return System.Text.Json.JsonDocument.Parse(File.ReadAllText(path)).RootElement.GetProperty("displayPipeline");
    }

    private static async Task TheAutomationReportDescribesTheCurrentFrame_NotThePreviousOne(string rawPath, int width, int height)
    {
        _scenario = "11 report describes the current frame";
        var directory = TempDirectory();
        try
        {
            var backend = new VoiReportingBackend();
            var vm = NewViewModel(width, height, _ => backend, directory, out _);
            await LoadAndDrawLanes(vm, rawPath);

            // (b) a successful automatic render: the report carries the window that was APPLIED (not the settings' numbers) and says it was automatic
            var ok = ReportDisplayPipeline(vm);
            Check(ok.GetProperty("windowSource").GetString() == "automatic", $"report windowSource for a successful automatic render is '{ok.GetProperty("windowSource").GetString()}'");
            Check(Math.Abs(ok.GetProperty("center").GetSingle() - VoiReportingBackend.AutoCenter) < 0.01f && Math.Abs(ok.GetProperty("width").GetSingle() - VoiReportingBackend.AutoWidth) < 0.01f && ok.GetProperty("mode").GetString() == "LinearExact",
                $"report window is not the applied one: {ok}");
            var summaryA = vm.DisplayPipelineSummary;
            Check(!string.IsNullOrEmpty(summaryA) && ok.GetProperty("summary").GetString() == summaryA, "report summary is not the render's summary");

            // (a) frame B arrives and ITS automatic window fails: the report is B's, with none of A's values
            var secondFrame = Path.Combine(directory, "second_frame.raw");
            File.Copy(rawPath, secondFrame);
            backend.FailAutomaticWindow = true;
            Environment.SetEnvironmentVariable("XPE_GUI_AUTOMATION_RAW_PATH", secondFrame);
            try
            {
                vm.LoadImageCommand.Execute(null);
                await Until(() => vm.StatusText.StartsWith("Display pipeline failed", StringComparison.Ordinal), "frame B's failed automatic render to be reported");
                await FlushUi();
            }
            finally
            {
                Environment.SetEnvironmentVariable("XPE_GUI_AUTOMATION_RAW_PATH", null);
            }

            var failed = ReportDisplayPipeline(vm);
            Check(failed.GetProperty("windowSource").GetString() == "failed", $"report windowSource after B's failure is '{failed.GetProperty("windowSource").GetString()}'");
            Check(failed.GetProperty("center").ValueKind == System.Text.Json.JsonValueKind.Null && failed.GetProperty("width").ValueKind == System.Text.Json.JsonValueKind.Null && failed.GetProperty("mode").ValueKind == System.Text.Json.JsonValueKind.Null,
                $"report still carries a window after B's failure: {failed}");
            var summaryB = failed.GetProperty("summary").GetString() ?? string.Empty;
            Check(summaryB != summaryA && !summaryB.Contains("Display: Modality", StringComparison.Ordinal), $"report summary after B's failure is still A's success: '{summaryB}'");
            Check(string.IsNullOrEmpty(vm.PipelineTimings), $"the previous frame's timings survived into frame B: '{vm.PipelineTimings}'");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }
}
