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

    /// <summary>GUI-C-234c: the display call over a chain run's result (the one the view model makes); the default is the 16-bit call.</summary>
    public virtual LoadedImageFrame ApplyDisplayPipeline(LoadedImageFrame rawFrame, ChainResult chain, AppSettings settings) =>
        ApplyDisplayPipeline(rawFrame, chain.DisplayInput, settings);

    public void Shutdown()
    {
        ShutdownMayEnd?.Wait(TimeSpan.FromSeconds(30));
        _inner.Shutdown();
    }

    public virtual BackendRuntimeInfo Initialize(AppSettings settings) => _inner.Initialize(settings);

    public string GetVersion() => _inner.GetVersion();

    public bool SupportsPreprocessing => _inner.SupportsPreprocessing;

    public string GetDisplayVersion() => _inner.GetDisplayVersion();

    public TelemetrySnapshot GetTelemetrySince(int logsSeen, int alertsSeen) => _inner.GetTelemetrySince(logsSeen, alertsSeen);

    public virtual BackendRuntimeInfo GetRuntimeInfo() => _inner.GetRuntimeInfo();
}

/// <summary>GUI-C-233k: a scripted backend whose preprocess stage reports the status the scenario chooses (the Mock never runs one); null leaves the Mock's own answer.</summary>
internal sealed class PreprocessOutcomeBackend : ScenarioBackend
{
    public StageStatus? Force;

    /// <summary>GUI-C-234c: identify as the native backend (RealXpeBackend) so the Native-only skip warning can be driven without the DLLs.</summary>
    public bool AsNative;

    public override BackendRuntimeInfo Initialize(AppSettings settings) => AsNative ? NativeNamedBackend.Info() : base.Initialize(settings);

    public override BackendRuntimeInfo GetRuntimeInfo() => AsNative ? NativeNamedBackend.Info() : base.GetRuntimeInfo();

    public override ChainResult RunChain(LoadedImageFrame rawFrame, IReadOnlyList<StageRequest> stages, AppSettings settings)
    {
        var chain = base.RunChain(rawFrame, stages, settings);
        if (Force is not { } status)
        {
            return chain;
        }

        var replaced = chain.Stages.Select(st => st.StageId == StageIds.Preprocess ? st with { Status = status, Reason = status == StageStatus.Skipped ? "no calibration data: scripted" : "scripted: " + status } : st).ToList();
        return chain with { Stages = replaced };
    }
}

/// <summary>
/// GUI-C-234c: a scripted backend whose chain ends in a stage that carries a float image (as the enhancement does), the SAME rounded pixels for every run but a float array tagged with the run's number, and whose
/// display call records the tag of the chain it was called with and the tag of the floats it was given.
/// </summary>
internal sealed class FloatTaggingBackend : ScenarioBackend
{
    private int _runs;

    public readonly System.Collections.Concurrent.ConcurrentQueue<(int ChainTag, int FloatsTag)> DisplayCallQueue = new();

    public IReadOnlyCollection<(int ChainTag, int FloatsTag)> DisplayCalls => DisplayCallQueue;

    /// <summary>True while every display call came through the chain overload (none through the 16-bit one).</summary>
    public bool ChainOverloadOnly => Volatile.Read(ref _ushortCalls) == 0;

    private int _ushortCalls;

    [ThreadStatic]
    private static bool _inChainCall;

    public override ChainResult RunChain(LoadedImageFrame rawFrame, IReadOnlyList<StageRequest> stages, AppSettings settings)
    {
        var tag = Interlocked.Increment(ref _runs);
        var chain = base.RunChain(rawFrame, stages, settings);
        var raw = rawFrame.RawPixels ?? chain.Raw;
        var floats = new float[raw.Length];
        Array.Fill(floats, tag);   // tag in every element; the pixels below are identical for all runs
        var tagged = new StageOutcome(StageIds.EnhanceBasic, StageStatus.Applied, (ushort[])raw.Clone(), "run " + tag, 0, 0, floats);
        return chain with { Stages = new List<StageOutcome>(chain.Stages) { tagged } };
    }

    public override LoadedImageFrame ApplyDisplayPipeline(LoadedImageFrame rawFrame, ChainResult chain, AppSettings settings)
    {
        var last = chain.Stages.LastOrDefault(s => s.Pixels is not null);
        var chainTag = last is null ? -1 : int.Parse(last.Reason.AsSpan(4), System.Globalization.CultureInfo.InvariantCulture);
        var floatsTag = chain.DisplayFloats is { Length: > 0 } f ? (int)f[0] : -2;
        DisplayCallQueue.Enqueue((chainTag, floatsTag));
        _inChainCall = true;   // the scenario backend's chain overload delegates to the 16-bit one on this thread; that delegation is not a 16-bit call by the view model
        try
        {
            return base.ApplyDisplayPipeline(rawFrame, chain, settings);
        }
        finally
        {
            _inChainCall = false;
        }
    }

    public override LoadedImageFrame ApplyDisplayPipeline(LoadedImageFrame rawFrame, ushort[] displayInput, AppSettings settings)
    {
        if (!_inChainCall)
        {
            Interlocked.Increment(ref _ushortCalls);
        }

        return base.ApplyDisplayPipeline(rawFrame, displayInput, settings);
    }
}

/// <summary>GUI-C-233g: a scripted backend that identifies itself as the native one (RealXpeBackend, its own display version), so a Mock to Native switch can be driven without the DLLs.</summary>
internal sealed class NativeNamedBackend : ScenarioBackend
{
    internal static BackendRuntimeInfo Info() => new() { BackendName = "RealXpeBackend", Version = "native-9.9", DisplayVersion = "native-display-9.9", NativeSource = "scripted" };

    public override BackendRuntimeInfo Initialize(AppSettings settings) => Info();

    public override BackendRuntimeInfo GetRuntimeInfo() => Info();
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

    /// <summary>GUI-C-233j: a window chosen by hand fails (the Candidate lane's settings carry one; the main render, on the automatic window, is untouched).</summary>
    public bool FailManualWindow;

    public override LoadedImageFrame ApplyDisplayPipeline(LoadedImageFrame rawFrame, ushort[] displayInput, AppSettings settings)
    {
        if (settings.VoiWindowAuto && FailAutomaticWindow)
        {
            throw new InvalidOperationException("xpe_voi_auto_window failed (-1)");
        }

        if (!settings.VoiWindowAuto && FailManualWindow)
        {
            throw new InvalidOperationException("scripted failure of a manual window (the Candidate lane)");
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
                        await Timed(() => TheOrdinaryChainListsTheEnhanceStage_AndTheExposureIndexSaysWhyItIsMissing(rawPath, width, height));
                        await Timed(() => TheAutomationReportDescribesTheCurrentFrame_NotThePreviousOne(rawPath, width, height));
                        await Timed(() => EveryEventLeavesTheHudAndTheReportEqualToTheRenderRecord(rawPath, width, height));
                        await Timed(() => TheScreensLaneTagsAndStageCountDescribeTheRender_AndAFailedCandidateLeavesNoPicture(rawPath, width, height));
                        await Timed(() => TheHudCalibrationLineSaysWhatTheChainRan_NotHowManyModesAreConfigured(rawPath, width, height));
                        await Timed(() => TheSaveCandidateOfAnOlderApplyFinishingLateIsNeverCommitted(rawPath, width, height));
                        await Timed(() => ASkippedPreprocessWarnsOnNative_AndTheWarningStaysWithTheImage(rawPath, width, height));
                        await Timed(() => ChoosingTheReferenceAlgorithmNeverSwitchesTheMainPreprocessingOff(rawPath, width, height));
                        await Timed(() => AStoredOffIsKeptAndSaidUntilAcknowledged(rawPath, width, height));
                        await Timed(() => EachDisplayCallGetsTheFloatsOfItsOwnRun_EvenWhenTheRoundedPixelsAreTheSame(rawPath, width, height));
                        await Timed(() => AFailedOpenKeepsTheSaveNameAndTheCandidateOfTheImageThatIsStillOpen(rawPath, width, height));
#if XPE_TEST_FAULTS
                        await Timed(() => TheAiWorkerDisabledFault_IsInertWithoutTheArgument_AndAnswersOffWithIt(rawPath, width, height));
#endif
                        await Timed(() => TheFrameTimingOfAnOpen_IsNeverWrittenLate_AndSaysWhenItTimedOut_AndLeavesNoSubscription(rawPath, width, height));
                        await Timed(() => ThePinnedNativeSearch_HasNoWayBackToTheApplicationFolder(rawPath, width, height));
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

    // ---- 10: the frame-timing line of an open (GUI-C-236c, Codex #191 finding 3) --------------------------------------------------------
    //
    // The line is written when the second composition tick after an open's processed picture has passed. Three ways it must NOT say something false: a newer open began while the tick was awaited
    // (the late result of the older open is dropped), the tick never came (the line is written and says TIMED OUT), the window is closing (nothing is written). And the subscription to
    // CompositionTarget.Rendering is released on every way out. The runner has no window, so the wait for ticks is scripted through CompositionTicksOverride; the real wait is exercised directly.

    private const string TimingPrefix = "open-to-2nd-tick (proxy) #";

    private static async Task TheFrameTimingOfAnOpen_IsNeverWrittenLate_AndSaysWhenItTimedOut_AndLeavesNoSubscription(string rawPath, int width, int height)
    {
        _scenario = "10 frame timing of an open";

        // control: an undisturbed open writes its line (so the absence checks below are not an artifact of a wait that never publishes)
        var directory = TempDirectory();
        try
        {
            var control = NewViewModel(width, height, _ => new ScenarioBackend(), directory, out _);
            control.CompositionTicksOverride = (_, _, _) => Task.FromResult(MainWindowViewModel.CompositionWait.Ticked);
            await LoadAndDrawLanes(control, rawPath);
            await Until(() => control.FrameTimingText.StartsWith(TimingPrefix + "1:", StringComparison.Ordinal), "the control open's timing line");
            Check(!control.FrameTimingText.Contains("TIMED OUT", StringComparison.Ordinal), $"an undisturbed open was marked timed out: {control.FrameTimingText}");

            // a newer open begins while the older one still waits for its tick: the older line must never appear, even when its wait ends later
            var calls = 0;
            var held = new TaskCompletionSource<MainWindowViewModel.CompositionWait>(TaskCreationOptions.RunContinuationsAsynchronously);
            var vm = NewViewModel(width, height, _ => new ScenarioBackend(), directory, out _);
            vm.CompositionTicksOverride = (_, _, _) => ++calls == 1 ? held.Task : Task.FromResult(MainWindowViewModel.CompositionWait.Ticked);
            await LoadAndDrawLanes(vm, rawPath);                       // open 1: its wait is held
            await FlushUi();
            Check(vm.FrameTimingText.Length == 0, $"the held wait of open 1 already wrote a line: {vm.FrameTimingText}");
            await LoadAndDrawLanes(vm, rawPath);                       // open 2: its wait ends at once
            await Until(() => vm.FrameTimingText.StartsWith(TimingPrefix, StringComparison.Ordinal), "open 2's timing line");
            var lineOfOpen2 = vm.FrameTimingText;
            held.SetResult(MainWindowViewModel.CompositionWait.Ticked);   // open 1's tick comes late
            await Until(() => vm.Logs.Any(l => l.Contains("Frame timing of open #1 dropped", StringComparison.Ordinal)), "open 1's late result to be dropped and logged");
            await FlushUi();
            Check(vm.FrameTimingText == lineOfOpen2, $"the older open's late result replaced the line ('{lineOfOpen2}' -> '{vm.FrameTimingText}')");
            var lineCount = vm.Logs.Count(l => l.Contains(TimingPrefix, StringComparison.Ordinal));
            Check(lineCount == 1, $"expected exactly one timing line in the log (open 2's), found {lineCount}");

            // the tick never comes: the line is written, marked, and the log says it too
            var timedOut = NewViewModel(width, height, _ => new ScenarioBackend(), directory, out _);
            timedOut.CompositionTicksOverride = (_, _, _) => Task.FromResult(MainWindowViewModel.CompositionWait.TimedOut);
            await LoadAndDrawLanes(timedOut, rawPath);
            await Until(() => timedOut.FrameTimingText.Length > 0, "the timed-out line");
            Check(timedOut.FrameTimingText.Contains("TIMED OUT", StringComparison.Ordinal), $"a wait that timed out was not marked in the status line: {timedOut.FrameTimingText}");
            Check(timedOut.Logs.Any(l => l.Contains("TIMED OUT", StringComparison.Ordinal)), "a wait that timed out was not marked in the log");

            // the window closes while the wait runs (the scripted wait honours the token as the real one does): nothing is written
            var closing = NewViewModel(width, height, _ => new ScenarioBackend(), directory, out _);
            closing.CompositionTicksOverride = (_, _, token) =>
            {
                var cancelled = new TaskCompletionSource<MainWindowViewModel.CompositionWait>(TaskCreationOptions.RunContinuationsAsynchronously);
                token.Register(() => cancelled.TrySetResult(MainWindowViewModel.CompositionWait.Cancelled));
                return cancelled.Task;
            };
            await LoadAndDrawLanes(closing, rawPath);
            await FlushUi();
            closing.BeginShutdown();
            await Until(() => closing.Logs.Any(l => l.Contains("not written: the window is closing", StringComparison.Ordinal)), "the cancelled wait to be logged");
            Check(closing.FrameTimingText.Length == 0, $"a line was written while the window was closing: {closing.FrameTimingText}");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }

        // the REAL wait: whichever way it ends - ticks, timeout, cancellation - the subscription is gone. (No window here, so a tick may never come; the outcome is printed, the count is what is checked.)
        Check(MainWindowViewModel.ActiveCompositionSubscriptions == 0, $"a subscription was already held before the check ({MainWindowViewModel.ActiveCompositionSubscriptions})");
        // GUI-C-241: the time-out path is made deterministic by asking for ticks that cannot come (int.MaxValue of them), so the limit is the only way out; and no count is asserted WHILE this wait runs:
        // its limit is short, and a loaded runner can stall this thread for longer than the limit between the call and the check (measured: 2 of 12 instances run in parallel failed with
        // "a running wait holds 0 subscriptions"). The running state is asserted below on a wait that only the scenario can end (a 30 s limit, ended by cancellation).
        var endedBy = await MainWindowViewModel.WaitForCompositionTicksAsync(int.MaxValue, TimeSpan.FromMilliseconds(150), CancellationToken.None);
        Check(endedBy == MainWindowViewModel.CompositionWait.TimedOut, $"a wait for ticks that cannot come ended as {endedBy}, expected TimedOut");
        Check(MainWindowViewModel.ActiveCompositionSubscriptions == 0, $"after the wait ended ({endedBy}) {MainWindowViewModel.ActiveCompositionSubscriptions} subscriptions were left");
        using var cancel = new CancellationTokenSource();
        var byCancel = MainWindowViewModel.WaitForCompositionTicksAsync(2, TimeSpan.FromSeconds(30), cancel.Token);
        Check(MainWindowViewModel.ActiveCompositionSubscriptions == 1, $"a second running wait holds {MainWindowViewModel.ActiveCompositionSubscriptions} subscriptions, expected 1");
        cancel.Cancel();
        var cancelledBy = await byCancel;
        Check(cancelledBy == MainWindowViewModel.CompositionWait.Cancelled, $"a cancelled wait ended as {cancelledBy}");
        Check(MainWindowViewModel.ActiveCompositionSubscriptions == 0, $"after the cancellation {MainWindowViewModel.ActiveCompositionSubscriptions} subscriptions were left");
        var alreadyCancelled = await MainWindowViewModel.WaitForCompositionTicksAsync(2, TimeSpan.FromSeconds(30), cancel.Token);
        Check(alreadyCancelled == MainWindowViewModel.CompositionWait.Cancelled && MainWindowViewModel.ActiveCompositionSubscriptions == 0, $"a wait started with a token already cancelled ended as {alreadyCancelled} with {MainWindowViewModel.ActiveCompositionSubscriptions} subscriptions left");
        Console.WriteLine($"    real wait without a window ended by: {endedBy}");
    }

    // ---- 11: the pinned native search has no way back to the default search (GUI-C-238b, Codex #193) ------------------------------------------------
    //
    // XPE_NATIVE_DIR_EXCLUSIVE=1 means: the app's native DLLs come from XPE_NATIVE_DIR and from nowhere else. The decoy is a real, loadable DLL (a copy of version.dll) with a module's name, put in the
    // APPLICATION folder (the runner's own folder). The control shows that the decoy IS found and loaded when nothing is pinned; with the search pinned the same name must NOT be loaded from it - the resolver
    // throws instead of handing the name to the default loader, which would search the application folder.

    private static Task ThePinnedNativeSearch_HasNoWayBackToTheApplicationFolder(string rawPath, int width, int height)
    {
        _scenario = "11 pinned native search";
        _ = (rawPath, width, height);
        var assembly = typeof(ImageProcTest.Services.Native.GuiNativeLibraryResolver).Assembly;
        var decoy = Path.Combine(AppContext.BaseDirectory, "xpe_ai.dll");
        var pinnedFolder = Path.Combine(TempDirectory(), "pinned");
        Directory.CreateDirectory(pinnedFolder);
        var systemDll = Path.Combine(Environment.SystemDirectory, "version.dll");
        var previousDir = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
        var previousExclusive = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR_EXCLUSIVE");
        File.Copy(systemDll, decoy, overwrite: true);
        try
        {

            // control: nothing pinned - the decoy beside the runner is found and loads
            Environment.SetEnvironmentVariable("XPE_NATIVE_DIR", null);
            Environment.SetEnvironmentVariable("XPE_NATIVE_DIR_EXCLUSIVE", null);
            var control = ImageProcTest.Services.Native.GuiNativeLibraryResolver.Resolve("xpe_ai.dll", assembly, null);
            Check(control != IntPtr.Zero, "the control failed: the decoy beside the runner was not loaded although nothing was pinned");
            System.Runtime.InteropServices.NativeLibrary.Free(control);

            // pinned to an EMPTY folder: the decoy beside the runner must not be used; the failure names the pinned folder
            Environment.SetEnvironmentVariable("XPE_NATIVE_DIR", pinnedFolder);
            Environment.SetEnvironmentVariable("XPE_NATIVE_DIR_EXCLUSIVE", "1");
            try
            {
                var leaked = ImageProcTest.Services.Native.GuiNativeLibraryResolver.Resolve("xpe_ai.dll", assembly, null);
                System.Runtime.InteropServices.NativeLibrary.Free(leaked);
                Fail("xpe_ai.dll was loaded although the search was pinned to an empty folder: the default search (application folder) was used");
            }
            catch (DllNotFoundException ex)
            {
                Check(ex.Message.Contains(pinnedFolder, StringComparison.Ordinal) && ex.Message.Contains("default search", StringComparison.Ordinal), $"the failure does not name the pinned folder and the refused default search: {ex.Message}");
            }

            // pinned, with a DLL of that name in the pinned folder: it is loaded, from there
            File.Copy(systemDll, Path.Combine(pinnedFolder, "xpe_ai.dll"));
            var inside = ImageProcTest.Services.Native.GuiNativeLibraryResolver.Resolve("xpe_ai.dll", assembly, null);
            Check(inside != IntPtr.Zero, "the DLL in the pinned folder was not loaded");
            System.Runtime.InteropServices.NativeLibrary.Free(inside);
            Check(string.Equals(ImageProcTest.Services.Native.GuiNativeLibraryResolver.ResolvedPath("xpe_ai.dll"), Path.Combine(pinnedFolder, "xpe_ai.dll"), StringComparison.OrdinalIgnoreCase),
                $"xpe_ai.dll resolved to '{ImageProcTest.Services.Native.GuiNativeLibraryResolver.ResolvedPath("xpe_ai.dll")}', not the pinned folder's file");

            // pinned without a folder: no quiet fall back to the default search
            Environment.SetEnvironmentVariable("XPE_NATIVE_DIR", null);
            try
            {
                var leaked = ImageProcTest.Services.Native.GuiNativeLibraryResolver.Resolve("xpe_ai.dll", assembly, null);
                System.Runtime.InteropServices.NativeLibrary.Free(leaked);
                Fail("xpe_ai.dll was loaded although EXCLUSIVE=1 names no folder");
            }
            catch (DllNotFoundException ex)
            {
                Check(ex.Message.Contains("does not name it", StringComparison.Ordinal), $"the failure does not say that XPE_NATIVE_DIR names nothing: {ex.Message}");
            }
        }
        finally
        {
            Environment.SetEnvironmentVariable("XPE_NATIVE_DIR", previousDir);
            Environment.SetEnvironmentVariable("XPE_NATIVE_DIR_EXCLUSIVE", previousExclusive);
            try { File.Delete(decoy); } catch (IOException) { /* loaded copy: the process end removes it */ }
        }

        // the audit: which native modules are NOT from the pinned folder (pure on its inputs)
        var modules = ImageProcTest.Services.Native.NativeModuleAudit.Classify(
            [@"C:\pinned\xpe_common.dll", @"C:\pinned\dcmdata.dll", @"C:\app\fmt.dll", @"C:\Windows\System32\kernel32.dll", @"c:\PINNED\xpe_display.dll"], @"C:\pinned");
        Check(modules.Count == 4, $"the audit should list the 4 native modules and skip kernel32, listed {modules.Count}");
        Check(modules.Count(m => !m.InsidePinnedFolder) == 1 && modules.Single(m => !m.InsidePinnedFolder).Name == "fmt.dll", "the audit should name fmt.dll (C:\\app) as the one module from outside the pinned folder");
        Check(modules.Single(m => m.Name == "xpe_display.dll").InsidePinnedFolder, "the folder comparison must ignore the case of the path");
        var report = ImageProcTest.Services.Native.NativeModuleAudit.Report("test", @"C:\pinned", [@"C:\pinned\xpe_common.dll", @"C:\app\fmt.dll"]);
        Check(report.Count == 2 && report[1].Contains("fmt.dll", StringComparison.Ordinal) && report[1].Contains("is NOT the pinned folder", StringComparison.Ordinal), "the report should carry one line for the outsider");
        return Task.CompletedTask;
    }

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

    private static System.Text.Json.JsonElement ReportRoot(MainWindowViewModel vm)
    {
        typeof(MainWindowViewModel).GetMethod("ExportAutomationReport", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance)!.Invoke(vm, null);
        var path = Path.Combine(AppContext.BaseDirectory, "menu-command-report.json");
        return System.Text.Json.JsonDocument.Parse(File.ReadAllText(path)).RootElement.Clone();
    }

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

            // GUI-C-233d (Codex #174): after frame A the report's processingChain is A's; remember it so the next check can prove it is gone
            var chainA = ReportRoot(vm).GetProperty("processingChain");
            Check(chainA.GetProperty("stages").GetArrayLength() > 0 && chainA.GetProperty("displayInput").GetString() != "not run", $"frame A's report has no chain to compare with: {chainA}");
            var statusA = chainA.GetProperty("status").GetString();

            // GUI-C-233e (a): A is applied; ONE calibration mode is changed and the frame is NOT re-applied (the report is read in the same synchronous step, before any re-render can start).
            // The summary and the seven modes must still be A's applied values; only `requested` carries the new one.
            var calibA = ReportRoot(vm).GetProperty("calibrationEvaluation");
            var summaryAppliedA = calibA.GetProperty("summary").GetString();
            var offsetAppliedA = calibA.GetProperty("offset").GetString();
            var newOffset = string.Equals(vm.Settings.OffsetCorrectionMode, "Off", StringComparison.OrdinalIgnoreCase) ? "On" : "Off";
            vm.Settings.OffsetCorrectionMode = newOffset;
            var calibChanged = ReportRoot(vm).GetProperty("calibrationEvaluation");
            Check(calibChanged.GetProperty("summary").GetString() == summaryAppliedA && calibChanged.GetProperty("offset").GetString() == offsetAppliedA,
                $"the report's calibration summary/offset followed the settings instead of the applied render: {calibChanged}");
            Check(calibChanged.GetProperty("requested").GetProperty("offset").GetString() == newOffset && calibChanged.GetProperty("requested").GetProperty("summary").GetString()!.Contains($"Offset={newOffset}", StringComparison.Ordinal),
                $"requested does not carry the new offset '{newOffset}': {calibChanged.GetProperty("requested")}");
            Check(summaryAppliedA!.Contains($"Offset={offsetAppliedA}", StringComparison.Ordinal), $"the applied summary does not agree with the applied offset: '{summaryAppliedA}' / '{offsetAppliedA}'");
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

            // GUI-C-233d: the WHOLE report is B's. B's display threw before its chain was reported, so the chain says "not run" - nothing of A's stages, status or display input, and no leftover preprocess/AI diagnostics
            var rootB = ReportRoot(vm);
            var chainB = rootB.GetProperty("processingChain");
            Check(chainB.GetProperty("stages").GetArrayLength() == 0, $"report stages after B's failure are still A's: {chainB.GetProperty("stages")}");
            Check(chainB.GetProperty("displayInput").GetString() == "not run", $"report displayInput after B's failure is '{chainB.GetProperty("displayInput").GetString()}'");
            Check(chainB.GetProperty("status").GetString() == "chain: not run" && chainB.GetProperty("status").GetString() != statusA, $"report chain status after B's failure is '{chainB.GetProperty("status").GetString()}'");
            Check(!vm.PreprocessRan && string.IsNullOrEmpty(vm.PreprocessStages) && vm.LastChain is null, "B kept A's preprocess/chain diagnostics");
            // the calibration and display fields describe what a render USED: none for B; what is asked for now is reported apart, as `requested`
            Check(failed.GetProperty("requested").GetProperty("automatic").GetBoolean(), $"the report does not say what is asked for now: {failed}");
            Check(rootB.GetProperty("calibrationEvaluation").GetProperty("summary").ValueKind == System.Text.Json.JsonValueKind.Null, $"B's report carries a calibration summary no render of B produced: {rootB.GetProperty("calibrationEvaluation").GetProperty("summary")}");
            Check(rootB.GetProperty("calibrationEvaluation").GetProperty("offset").ValueKind == System.Text.Json.JsonValueKind.Null && failed.GetProperty("bodyPart").ValueKind == System.Text.Json.JsonValueKind.Null, $"B's report carries calibration/display values no render of B used: {rootB.GetProperty("calibrationEvaluation")}");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    // ---- GUI-C-233g: every event x every field -------------------------------------------------------------------------------------------------

    /// <summary>One row of the event x field table: what the record, the HUD properties and the whole report say right after an event.</summary>
    private static string TableRow(string eventName, MainWindowViewModel vm)
    {
        var root = ReportRoot(vm);
        var display = root.GetProperty("displayPipeline");
        var calibration = root.GetProperty("calibrationEvaluation");
        var chain = root.GetProperty("processingChain");
        string Text(System.Text.Json.JsonElement e) => e.ValueKind == System.Text.Json.JsonValueKind.Null ? "null" : e.ToString();
        var by = display.GetProperty("renderedBy");
        return $"| {eventName,-44} | {(vm.CurrentRender is null ? "none" : vm.CurrentRender.Backend.Mode),-6} | {Text(display.GetProperty("applied")),-5} | {(by.ValueKind == System.Text.Json.JsonValueKind.Null ? "null" : by.GetProperty("backendMode").GetString()),-6} | "
            + $"{Text(display.GetProperty("version")),-18} | {Text(display.GetProperty("windowSource")),-13} | {(Text(calibration.GetProperty("summary")).Length > 0 && calibration.GetProperty("summary").ValueKind != System.Text.Json.JsonValueKind.Null ? "set" : "null"),-4} | "
            + $"{(chain.GetProperty("stages").GetArrayLength() == 0 ? "empty" : "stages"),-6} | {chain.GetProperty("displayInput").GetString(),-7} | {(vm.PreviewStaleReason is null ? "-" : vm.PreviewStaleReason == MainWindowViewModel.StaleBackendReplaced ? "backend" : vm.PreviewStaleReason == MainWindowViewModel.StalePipelineFailed ? "failed" : "settings"),-9} | "
            + $"{(vm.LaneAImage is null ? "-" : "A")}/{(vm.LaneBImage is null ? "-" : "B")}{(vm.CurrentRender is { LaneA: not null } r && r.Backend.Mode is { } m ? " by " + m : string.Empty),-9} | {root.GetProperty("actualBackendMode").GetString()} |";
    }

    /// <summary>With no render standing, EVERY field the record feeds says so: the properties, the HUD values and the whole report agree on "none".</summary>
    private static void CheckNoRender(MainWindowViewModel vm, string after)
    {
        var root = ReportRoot(vm);
        var display = root.GetProperty("displayPipeline");
        var calibration = root.GetProperty("calibrationEvaluation");
        var chain = root.GetProperty("processingChain");
        Check(vm.CurrentRender is null && vm.LastChain is null && !vm.HasPipelineDiagnostics, $"after {after}: a render record still stands");
        Check(vm.CalibStageCountDisplay == "—", $"after {after}: the HUD calibration line still describes a render: '{vm.CalibStageCountDisplay}'");
        Check(vm.LaneAImage is null && vm.LaneBImage is null && !vm.LaneBIsStale, $"after {after}: the comparison lanes still show a render's pictures (A={(vm.LaneAImage is null ? "none" : "set")}, B={(vm.LaneBImage is null ? "none" : "set")})");
        Check(vm.ChainStatus == "chain: not run" && string.IsNullOrEmpty(vm.PipelineTimings) && vm.DisplayPipelineSummary == "Display pipeline has not run.", $"after {after}: status/timings/summary are still a render's: '{vm.ChainStatus}' / '{vm.PipelineTimings}' / '{vm.DisplayPipelineSummary}'");
        Check(vm.RenderedVoiCenter is null && vm.RenderedVoiWidth is null && vm.RenderedVoiMode is null && vm.RenderedVoiAutomatic is null, $"after {after}: the HUD window is still a render's");
        Check(!vm.PreprocessRan && string.IsNullOrEmpty(vm.PreprocessStages) && string.IsNullOrEmpty(vm.AiProcessedLabel), $"after {after}: preprocess/AI diagnostics are still a render's");
        Check(!display.GetProperty("applied").GetBoolean() && display.GetProperty("version").ValueKind == System.Text.Json.JsonValueKind.Null && display.GetProperty("renderedBy").ValueKind == System.Text.Json.JsonValueKind.Null, $"after {after}: the report names a producer or says applied: {display}");
        Check(display.GetProperty("windowSource").GetString() is "not applied" or "failed" && display.GetProperty("mode").ValueKind == System.Text.Json.JsonValueKind.Null && display.GetProperty("center").ValueKind == System.Text.Json.JsonValueKind.Null && display.GetProperty("bodyPart").ValueKind == System.Text.Json.JsonValueKind.Null, $"after {after}: the report's window/bodyPart are still a render's: {display}");
        Check(calibration.GetProperty("summary").ValueKind == System.Text.Json.JsonValueKind.Null && calibration.GetProperty("offset").ValueKind == System.Text.Json.JsonValueKind.Null, $"after {after}: the report's calibration is still a render's: {calibration}");
        Check(chain.GetProperty("stages").GetArrayLength() == 0 && chain.GetProperty("displayInput").GetString() == "not run" && chain.GetProperty("status").GetString() == "chain: not run" && chain.GetProperty("exposureKvp").ValueKind == System.Text.Json.JsonValueKind.Null, $"after {after}: the report's chain is still a render's: {chain}");
    }

    private static async Task EveryEventLeavesTheHudAndTheReportEqualToTheRenderRecord(string rawPath, int width, int height)
    {
        _scenario = "13 event x field table";
        var directory = TempDirectory();
        try
        {
            // builds 1 and 2 are the constructor's; 3 is the switch to Native, 4 the switch back to Mock, 5 a re-initialisation, 6 the second Native
            var built = new List<ScenarioBackend>();
            ScenarioBackend For(int build)
            {
                ScenarioBackend backend = build is 3 or 6 ? new NativeNamedBackend() : new VoiReportingBackend();
                built.Add(backend);
                return backend;
            }

            var vm = NewViewModel(width, height, For, directory, out _);
            var rows = new List<string>();
            Console.WriteLine("| event                                        | record | appl. | by     | display version    | windowSource  | cal. | chain  | input   | stale     | lanes A/B     | actual |");

            // 0. before any load
            CheckNoRender(vm, "start-up");
            rows.Add(TableRow("start-up (nothing loaded)", vm));

            // 1. frame A rendered by the first (Mock-named) backend
            await LoadAndDrawLanes(vm, rawPath);
            var recordA = vm.CurrentRender;
            Check(recordA is not null && recordA.Backend.Mode == "Mock" && recordA.Chain.Stages.Count > 0, "frame A has no Mock record");
            var rootA = ReportRoot(vm).GetProperty("displayPipeline");
            Check(rootA.GetProperty("applied").GetBoolean() && rootA.GetProperty("renderedBy").GetProperty("backendMode").GetString() == "Mock", $"A's report does not name the Mock backend: {rootA}");
            Check(vm.RenderedVoiMode == recordA!.Voi.Mode && vm.ChainStatus == recordA.ChainStatus && vm.DisplayPipelineSummary == recordA.DisplaySummary, "A's HUD/status properties are not the record's");
            Check(vm.LaneAImage is not null && vm.LaneBImage is not null && ReferenceEquals(recordA.LaneA, vm.LaneAImage) && ReferenceEquals(vm.LaneAImage, vm.ProcessedImage), "A's lanes are not the record's / the Reference is not the main picture");
            rows.Add(TableRow("A rendered (Mock)", vm));

            // 2. a setting changes and nothing is re-applied: the SAME record stands, only `requested` moves
            vm.Settings.OffsetCorrectionMode = string.Equals(vm.Settings.OffsetCorrectionMode, "Off", StringComparison.OrdinalIgnoreCase) ? "On" : "Off";
            Check(ReferenceEquals(vm.CurrentRender, recordA), "a settings change replaced or dropped the record");
            rows.Add(TableRow("setting changed, not re-applied", vm));

            // 2b. the SAME frame is rendered again and the display fails: the picture on screen is unchanged, so its record stands (same object), marked failed - the report says "failed", not "applied just now"
            var current = (VoiReportingBackend)built[^1];
            current.FailAutomaticWindow = true;
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => vm.PreviewStaleReason == MainWindowViewModel.StalePipelineFailed, "the same-frame re-render's failure to be reported");
            Check(ReferenceEquals(vm.CurrentRender, recordA), "a failed re-render of the same frame replaced or dropped the record of the picture that is still shown");
            Check(vm.LaneAImage is not null && vm.LaneBImage is not null && ReferenceEquals(vm.LaneAImage, recordA.LaneA), "a failed same-frame re-render took the lanes away although the picture and its record stand");
            Check(ReportRoot(vm).GetProperty("displayPipeline").GetProperty("windowSource").GetString() == "failed", "the report does not say the last attempt failed");
            rows.Add(TableRow("same frame re-render, display fails", vm));

            // 2c. a NEW frame whose display fails: nothing of A survives
            var secondFrame = Path.Combine(directory, "second_frame.raw");
            File.Copy(rawPath, secondFrame);
            Environment.SetEnvironmentVariable("XPE_GUI_AUTOMATION_RAW_PATH", secondFrame);
            try
            {
                vm.LoadImageCommand.Execute(null);
                await Until(() => vm.StatusText.StartsWith("Display pipeline failed", StringComparison.Ordinal) && vm.CurrentRender is null, "frame B's failed render to be reported");
                await FlushUi();
            }
            finally
            {
                Environment.SetEnvironmentVariable("XPE_GUI_AUTOMATION_RAW_PATH", null);
            }

            CheckNoRender(vm, "new frame whose display fails");
            rows.Add(TableRow("new frame, display fails", vm));

            // back to a standing render for the backend switches
            current.FailAutomaticWindow = false;
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => vm.CurrentRender is not null && vm.PreviewStaleReason is null, "frame B to render once the display works");
            await FlushUi();
            rows.Add(TableRow("B rendered (Mock)", vm));

            // 3. Mock -> Native: the Mock-made picture is not described as the Native backend's
            vm.SetBackendModeCommand.Execute("Native");
            await FlushUi();
            CheckNoRender(vm, "Mock -> Native");
            Check(vm.ActualBackendMode == "Native" && ReportRoot(vm).GetProperty("actualBackendMode").GetString() == "Native", "the current runtime is not Native after the switch");
            Check(vm.PreviewStaleReason == MainWindowViewModel.StaleBackendReplaced, $"the stale reason after the switch is '{vm.PreviewStaleReason}'");
            Check(ReferenceEquals(vm.ProcessedImage, vm.SourceImage), "the viewport still shows the old backend's processed picture after the switch");
            rows.Add(TableRow("Mock -> Native", vm));

            // 4. a successful re-render under the new backend: the record names it
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => vm.CurrentRender is not null && vm.LaneBImage is not null, "the re-render under the Native backend to commit with its lanes");
            await FlushUi();
            Check(vm.CurrentRender!.Backend.Mode == "Native" && vm.LaneAImage is not null, "the lanes after the Native re-render are not drawn by the Native backend's render");
            var recordN = vm.CurrentRender!;
            Check(recordN.Backend.Mode == "Native" && recordN.Backend.DisplayVersion == "native-display-9.9" && vm.PreviewStaleReason is null, $"the re-render's record is not the Native backend's: {recordN.Backend}");
            Check(ReportRoot(vm).GetProperty("displayPipeline").GetProperty("version").GetString() == "native-display-9.9", "the report's display version is not the producing backend's");
            rows.Add(TableRow("re-render succeeded (Native)", vm));

            // 5. Native -> Mock
            vm.SetBackendModeCommand.Execute("Mock");
            await FlushUi();
            CheckNoRender(vm, "Native -> Mock");
            Check(vm.ActualBackendMode == "Mock", "the current runtime is not Mock after the switch back");
            rows.Add(TableRow("Native -> Mock", vm));

            // 6. a render, then a re-initialisation of the same mode
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => vm.CurrentRender is not null, "the re-render under the Mock backend to commit");
            await FlushUi();
            vm.InitializeBackendCommand.Execute(null);
            await FlushUi();
            CheckNoRender(vm, "re-initialisation");
            rows.Add(TableRow("re-initialise (same mode)", vm));

            foreach (var row in rows)
            {
                Console.WriteLine(row);
            }
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    // ---- GUI-C-233j: what the screen says about the drawn image -------------------------------------------------------------------------------

    private static async Task TheScreensLaneTagsAndStageCountDescribeTheRender_AndAFailedCandidateLeavesNoPicture(string rawPath, int width, int height)
    {
        _scenario = "14 lane tags, stage count, failed candidate";
        var directory = TempDirectory();
        try
        {
            var built = new List<ScenarioBackend>();
            var vm = NewViewModel(width, height, build => { var b = new VoiReportingBackend(); built.Add(b); return b; }, directory, out _);

            // before anything is rendered nothing is claimed
            Check(vm.RenderedLaneAAlgorithm is null && vm.RenderedLaneBAlgorithm is null && vm.CalibStageCountDisplay == "—" && !vm.LaneBIsStale, "start-up: the lane tags / stage count claim a render");

            await LoadAndDrawLanes(vm, rawPath);
            var tagA = vm.RenderedLaneAAlgorithm;
            var tagB = vm.RenderedLaneBAlgorithm;
            var stages = vm.CalibStageCountDisplay;
            Check(tagA == vm.CurrentRender!.Inputs.LaneAAlgorithm && tagB == vm.CurrentRender.Inputs.LaneBAlgorithm && stages == "not run (native backend required)", $"after the render the tags/count are not the record's: '{tagA}' '{tagB}' '{stages}'");

            // settings change, nothing re-applied: the tags and the count still describe what was DRAWN; the Candidate is stale
            var otherA = MainWindowViewModel.AlgorithmOptions.First(o => o != vm.Settings.LaneAAlgorithm);
            var otherB = MainWindowViewModel.AlgorithmOptions.First(o => o != vm.Settings.LaneBAlgorithm);
            vm.Settings.LaneAAlgorithm = otherA;
            vm.Settings.LaneBAlgorithm = otherB;
            vm.Settings.OffsetCorrectionMode = string.Equals(vm.Settings.OffsetCorrectionMode, "Off", StringComparison.OrdinalIgnoreCase) ? "On" : "Off";
            vm.Settings.DefectCorrectionMode = string.Equals(vm.Settings.DefectCorrectionMode, "Off", StringComparison.OrdinalIgnoreCase) ? "On" : "Off";
            Check(vm.RenderedLaneAAlgorithm == tagA && vm.RenderedLaneBAlgorithm == tagB, $"the lane tags followed the settings instead of the render: '{vm.RenderedLaneAAlgorithm}' '{vm.RenderedLaneBAlgorithm}' (settings now {otherA} / {otherB})");
            Check(vm.CalibStageCountDisplay == stages, $"the HUD stage count followed the settings: was '{stages}', now '{vm.CalibStageCountDisplay}'");
            Check(vm.LaneBIsStale, "the Candidate is not marked stale after its algorithm setting changed");

            // a render that fails on the Candidate lane: the main render commits, the lanes are NOT left holding anything
            var current = (VoiReportingBackend)built[^1];
            current.FailManualWindow = true;
            vm.Settings.LaneBVoiWindowWidth = 900f;   // a Candidate override is a window chosen by hand
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => vm.CurrentRender is not null && !ReferenceEquals(vm.CurrentRender.Inputs, null) && vm.CurrentRender.Inputs.LaneBVoiWindowWidth > 0f, "the render with a failing Candidate to commit");
            await Task.Delay(300);
            await FlushUi();
            Check(vm.CurrentRender is not null && vm.LaneAImage is null && vm.LaneBImage is null, $"a failed Candidate left lane pictures behind (A={(vm.LaneAImage is null ? "none" : "set")}, B={(vm.LaneBImage is null ? "none" : "set")})");
            // GUI-C-233k: the lanes hold no picture (the Candidate failed), so neither tag names an algorithm - the screen shows "—" rather than the name of something that was not drawn
            Check(vm.RenderedLaneAAlgorithm is null && vm.RenderedLaneBAlgorithm is null, $"a lane without a picture still names an algorithm: A='{vm.RenderedLaneAAlgorithm}' B='{vm.RenderedLaneBAlgorithm}'");
            Check(vm.CurrentRender!.Inputs.LaneAAlgorithm == otherA && vm.CurrentRender.Inputs.LaneBAlgorithm == otherB, "the second render's record does not carry the algorithms it was rendered with");
            Console.WriteLine($"| lane tags after render {tagA}/{tagB} -> after unapplied change {vm.RenderedLaneAAlgorithm}/{vm.RenderedLaneBAlgorithm} (requested {otherA}/{otherB}, record {vm.CurrentRender!.Inputs.LaneAAlgorithm}/{vm.CurrentRender.Inputs.LaneBAlgorithm}); stages {stages}; Candidate failed -> lanes {(vm.LaneAImage is null ? "none" : "set")}/{(vm.LaneBImage is null ? "none" : "set")} |");

            // the new frame / backend replacement cases are rows of scenario 13; here the tags go with them
            vm.InitializeBackendCommand.Execute(null);
            await FlushUi();
            Check(vm.RenderedLaneAAlgorithm is null && vm.RenderedLaneBAlgorithm is null && vm.CalibStageCountDisplay == "—" && string.IsNullOrEmpty(vm.MetadataText) == false, "after the backend was replaced the tags / stage count still describe the old render");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    // ---- GUI-C-233k: the HUD's calibration line comes from what the chain ran --------------------------------------------------------------------

    private static async Task TheHudCalibrationLineSaysWhatTheChainRan_NotHowManyModesAreConfigured(string rawPath, int width, int height)
    {
        _scenario = "15 HUD calibration line vs chain outcome";
        var directory = TempDirectory();
        try
        {
            var built = new List<ScenarioBackend>();
            var vm = NewViewModel(width, height, build => { var b = new PreprocessOutcomeBackend(); built.Add(b); return b; }, directory, out _);
            var backend = (PreprocessOutcomeBackend)built[^1];
            var configured = vm.CalibrationEvaluationSummary;   // the seven configured modes, whatever they are

            string Cross(string label, string expectedHud, string expectedStatus, bool expectedRan, string expectedEiPart)
            {
                var executed = ReportRoot(vm).GetProperty("calibrationEvaluation").GetProperty("executed");
                var hud = executed.GetProperty("hud").GetString();
                var preprocess = executed.GetProperty("preprocess").GetString();
                Check(vm.CalibStageCountDisplay == expectedHud && hud == expectedHud, $"{label}: HUD says '{vm.CalibStageCountDisplay}' / report '{hud}', expected '{expectedHud}'");
                Check(preprocess == expectedStatus, $"{label}: the report's preprocess status is '{preprocess}', expected '{expectedStatus}'");
                Check(vm.PreprocessRan == expectedRan, $"{label}: PreprocessRan is {vm.PreprocessRan}, expected {expectedRan}");
                Check(vm.CurrentRender!.Chain.Stages.Single(st => st.StageId == StageIds.Preprocess).Status.ToString() == expectedStatus, $"{label}: the record's chain disagrees with the report");
                Check(vm.CalibrationEvaluationSummary == configured, $"{label}: the configured modes changed during the scenario");
                // GUI-C-234b: the exposure-index text gives the same reason as the HUD ("no value" and "not run" cannot disagree about why).
                Check(vm.ExposureIndexText.Contains(expectedEiPart, StringComparison.Ordinal), $"{label}: the exposure index text '{vm.ExposureIndexText}' does not carry '{expectedEiPart}'");
                Check(executed.GetProperty("reason").ValueKind == System.Text.Json.JsonValueKind.String, $"{label}: the report's preprocess `reason` is missing");   // the HUD is NOT a function of them
                return $"| {label,-34} | {vm.CalibStageCountDisplay,-32} | {preprocess,-20} | {vm.PreprocessRan,-5} | {vm.ChainStatus.Split(';')[0]} |";
            }

            var rows = new List<string>();

            // (1) default settings (GUI-C-234b, U3): preprocessing IS requested; this backend (the Mock) has no module, so it is skipped WITH its reason - not left out silently, and not "failed"
            Check(vm.Settings.PreprocessInChain, "the default PreprocessInChain is expected to be on (U3)");
            await LoadAndDrawLanes(vm, rawPath);
            rows.Add(Cross("default (requested; backend has no module)", "not run (native backend required)", "Skipped", false, "preprocessing did not run: native backend required"));

            // (1b) requested and skipped for want of calibration data
            backend.Force = StageStatus.Skipped;
            var before0 = vm.CurrentRender;
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => vm.CurrentRender is not null && !ReferenceEquals(vm.CurrentRender, before0), "the render with a skipped preprocess to commit");
            await FlushUi();
            rows.Add(Cross("preprocess skipped, no calibration", "not run (no calibration data)", "Skipped", false, "preprocessing did not run: no calibration data"));

            // (2) requested and failed
            backend.Force = StageStatus.RequestedNotApplied;
            var before = vm.CurrentRender;
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => vm.CurrentRender is not null && !ReferenceEquals(vm.CurrentRender, before), "the render with a failing preprocess to commit");
            await FlushUi();
            rows.Add(Cross("preprocess requested, failed", "failed", "RequestedNotApplied", false, "preprocessing did not run: it failed"));

            // (3) requested and ran
            backend.Force = StageStatus.Applied;
            before = vm.CurrentRender;
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => vm.CurrentRender is not null && !ReferenceEquals(vm.CurrentRender, before), "the render with a successful preprocess to commit");
            await FlushUi();
            rows.Add(Cross("preprocess requested, ran", "3 stages (offset · gain · defect)", "Applied", true, "Exposure index:"));

            // (3b) switched off by the user: the only way the stage is NotRequested now
            vm.Settings.PreprocessInChain = false;
            backend.Force = null;
            before = vm.CurrentRender;
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => vm.CurrentRender is not null && !ReferenceEquals(vm.CurrentRender, before), "the render with preprocessing switched off to commit");
            await FlushUi();
            rows.Add(Cross("preprocess switched off", "not run", "NotRequested", false, "preprocessing did not run: it is switched off"));

            // (4) a new frame: nothing is claimed until it is rendered
            Console.WriteLine("| case                               | HUD \"Calib:\" line               | report preprocess    | ran   | chain |");
            foreach (var row in rows)
            {
                Console.WriteLine(row);
            }
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    // ---- GUI-C-234: the ordinary chain carries (5)-(8) and the exposure index says why it is missing -------------------------------------

    private static async Task TheOrdinaryChainListsTheEnhanceStage_AndTheExposureIndexSaysWhyItIsMissing(string rawPath, int width, int height)
    {
        _scenario = "16 enhance stage in the ordinary chain; EI text";
        var directory = TempDirectory();
        try
        {
            var backend = new ScenarioBackend();   // the Mock behind it: it can run no native stage, but it must LIST the same stages as the Native backend does
            var vm = NewViewModel(width, height, _ => backend, directory, out _);
            await LoadAndDrawLanes(vm, rawPath);

            var chain = vm.ChainStatus;
            var order = new[] { "preprocess=", "enhance_basic=", "gsvg=", "ai_bone_suppress=" }.Select(token => chain.IndexOf(token, StringComparison.Ordinal)).ToArray();
            Check(order.All(i => i >= 0) && order.SequenceEqual(order.OrderBy(i => i)), $"the chain line does not list preprocess, enhance_basic, gsvg, ai_bone_suppress in that order: '{chain}'");
            Check(vm.ExposureIndexText.StartsWith("Exposure index: not measured (preprocessing did not run", StringComparison.Ordinal), $"the exposure index text does not say why it is missing: '{vm.ExposureIndexText}'");
            Check(vm.StatusText.Contains(vm.ExposureIndexText, StringComparison.Ordinal), "the status line does not carry the exposure index text");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    // ---- GUI-C-234c (Codex #181): the four findings ---------------------------------------------------------------------------------------

    private static bool HasAlert(MainWindowViewModel vm, string severity, string code) =>
        vm.Alerts.Any(a => a.Severity == severity && a.Code == code);

    // finding 3: preprocessing is on by default, so a skip leaves the image uncorrected - Native warns and the warning stays with the image; Mock only informs.
    private static async Task ASkippedPreprocessWarnsOnNative_AndTheWarningStaysWithTheImage(string rawPath, int width, int height)
    {
        _scenario = "17 skipped preprocess: Native warns, Mock informs";
        var directory = TempDirectory();
        try
        {
            var built = new List<ScenarioBackend>();
            var vm = NewViewModel(width, height, build => { var b = new PreprocessOutcomeBackend { AsNative = true, Force = StageStatus.Skipped }; built.Add(b); return b; }, directory, out _);
            var backend = (PreprocessOutcomeBackend)built[^1];
            await LoadAndDrawLanes(vm, rawPath);

            Check(HasAlert(vm, "WARN", "PREPROCESS_SKIPPED_NO_CALIBRATION"), "Native: a skipped preprocess raised no WARN alert");
            Check(!HasAlert(vm, "INFO", "PREPROCESS_UNAVAILABLE_IN_MOCK"), "Native: the Mock's INFO alert was raised");
            Check(vm.HasRenderedPreprocessWarning && vm.RenderedPreprocessWarning!.Contains("NOT CORRECTED", StringComparison.Ordinal) && vm.RenderedPreprocessWarning.Contains("no calibration data", StringComparison.Ordinal),
                $"Native: the render record carries no warning ('{vm.RenderedPreprocessWarning}')");

            // The warning is the record's, not an alert's or the status line's: clearing the alerts, writing another status and changing a setting leave it.
            vm.ClearAlertsCommand.Execute(null);
            vm.Settings.VoiWindowCenter += 1f;
            await FlushUi();
            Check(vm.HasRenderedPreprocessWarning, "Native: the warning went away with the alert list / a settings change while the same image is on screen");

            // A render that applied preprocessing takes it back; the next skipped one brings it again.
            backend.Force = StageStatus.Applied;
            var before = vm.CurrentRender;
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => vm.CurrentRender is not null && !ReferenceEquals(vm.CurrentRender, before), "the render with an applied preprocess to commit");
            await FlushUi();
            Check(!vm.HasRenderedPreprocessWarning, "Native: the warning stayed after a render that ran preprocessing");
            backend.Force = StageStatus.Skipped;
            before = vm.CurrentRender;
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => vm.CurrentRender is not null && !ReferenceEquals(vm.CurrentRender, before), "the render with a skipped preprocess to commit");
            await FlushUi();
            Check(vm.HasRenderedPreprocessWarning, "Native: the warning did not come back with the next skipped render");

            // Mock: the module is absent altogether - an INFO of its own, no warning on the picture.
            var mockDirectory = TempDirectory();
            try
            {
                var mockVm = NewViewModel(width, height, _ => new PreprocessOutcomeBackend { Force = StageStatus.Skipped }, mockDirectory, out _);
                await LoadAndDrawLanes(mockVm, rawPath);
                Check(HasAlert(mockVm, "INFO", "PREPROCESS_UNAVAILABLE_IN_MOCK"), "Mock: no INFO alert for the skipped preprocess");
                Check(!HasAlert(mockVm, "WARN", "PREPROCESS_SKIPPED_NO_CALIBRATION"), "Mock: the Native WARN was raised");
                Check(!mockVm.HasRenderedPreprocessWarning, "Mock: a warning is on the picture");
            }
            finally
            {
                Directory.Delete(mockDirectory, recursive: true);
            }
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    // finding 4: the Reference lane's algorithm is the grid correction; the preprocessing switch is not the preset's to turn off.
    private static async Task ChoosingTheReferenceAlgorithmNeverSwitchesTheMainPreprocessingOff(string rawPath, int width, int height)
    {
        _scenario = "18 Lane A preset keeps main preprocessing";
        var directory = TempDirectory();
        try
        {
            var vm = NewViewModel(width, height, _ => new ScenarioBackend(), directory, out _);
            await LoadAndDrawLanes(vm, rawPath);
            Check(vm.Settings.PreprocessInChain, "preprocessing is not on by default");
            foreach (var preset in AlgorithmPreset.All)
            {
                vm.LaneAAlgorithm = preset.Name == vm.LaneAAlgorithm ? "No grid correction" : preset.Name;
                vm.LaneAAlgorithm = preset.Name;
                Check(vm.Settings.PreprocessInChain, $"choosing '{preset.Name}' switched the main preprocessing off");
                Check(vm.Settings.GsvgMode == preset.GsvgMode, $"choosing '{preset.Name}' did not set the grid correction ({vm.Settings.GsvgMode})");
            }

            // The render after the choice still asks for the preprocessing.
            vm.LaneAAlgorithm = "Grid suppression";
            var before = vm.CurrentRender;
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => vm.CurrentRender is not null && !ReferenceEquals(vm.CurrentRender, before), "the render after the choice to commit");
            await FlushUi();
            var preprocess = vm.CurrentRender!.Chain.Stages.Single(st => st.StageId == StageIds.Preprocess);
            Check(preprocess.Status != StageStatus.NotRequested, $"the render after choosing Grid suppression did not request preprocessing ({preprocess.Status})");

            // A preset that includes preprocessing may switch it ON; none switches it off.
            vm.Settings.PreprocessInChain = false;
            vm.LaneAAlgorithm = "No grid correction";
            Check(!vm.Settings.PreprocessInChain, "choosing a preset switched an operator's OFF back on");
            vm.LaneAAlgorithm = "Preprocess + virtual grid";
            Check(vm.Settings.PreprocessInChain, "'Preprocess + virtual grid' did not switch preprocessing on");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    // finding 1 (234c) / findings 1-2 (234d, Codex #182): a stored "preprocessInChain": false is kept; an OFF written by an EARLIER build is announced until the operator acknowledges it; an OFF chosen with this version never is.
    private static Task AStoredOffIsKeptAndSaidUntilAcknowledged(string rawPath, int width, int height)
    {
        _scenario = "19 stored preprocessing-off: announced until acknowledged, only when it came from an earlier build";
        var directory = TempDirectory();
        try
        {
            var path = Path.Combine(directory, "appsettings.json");
            var service = new AppSettingsService(path);

            // A "restart": load the file again and build a view model from it, as the app does.
            MainWindowViewModel Start() => new(service.Load().Settings, service, _ => new ScenarioBackend());
            bool Told(MainWindowViewModel vm) => HasAlert(vm, "WARN", "PREPROCESS_OFF_FROM_EARLIER_SETTINGS");

            // (1) a new install: the operator switches preprocessing off, saves, restarts - no notice (it is their choice, saved by this version).
            var fresh = Start();
            Check(fresh.Settings.PreprocessInChain && !Told(fresh) && !fresh.HasPreprocessOffNotice, "(1) a new install started with a notice or with preprocessing off");
            fresh.Settings.PreprocessInChain = false;
            fresh.SaveSettingsCommand.Execute(null);
            Check(File.ReadAllText(path).Contains("\"settingsSchemaVersion\": " + AppSettings.CurrentSchemaVersion, StringComparison.Ordinal), "(1) the saved file carries no schema version");
            var again = Start();
            Check(!again.Settings.PreprocessInChain && !Told(again) && !again.HasPreprocessOffNotice, "(1) an OFF chosen with this version was announced as an earlier version's");

            // (2) a file written by an earlier build (no schema version) with the OFF: kept, and announced - alert, notice and status line.
            File.WriteAllText(path, "{ \"rawWidth\": " + width + ", \"rawHeight\": " + height + ", \"preprocessInChain\": false }");
            var before = File.ReadAllText(path);
            var old = Start();
            Check(!old.Settings.PreprocessInChain, "(2) the stored off was not kept");
            Check(Told(old) && old.HasPreprocessOffNotice && old.PreprocessOffNotice!.Contains("earlier version", StringComparison.Ordinal), "(2) no alert and notice for an OFF from an earlier build");
            Check(old.Alerts.Count(a => a.Code == "PREPROCESS_OFF_FROM_EARLIER_SETTINGS") == 1, "(2) the alert was raised more than once");
            Check(File.ReadAllText(path) == before, "(2) starting the view model wrote the settings file (the notice must not be consumed by being shown)");

            // (3) it was not acknowledged and the app was closed (a close saves the settings): the next start tells again.
            old.SaveSettingsCommand.Execute(null);
            var restarted = Start();
            Check(Told(restarted) && restarted.HasPreprocessOffNotice && !restarted.Settings.PreprocessInChain, "(3) the notice did not come back after a restart without an acknowledgement");

            // (4) clearing the alert list is not an acknowledgement: the notice stands, and so does it after a restart.
            restarted.ClearAlertsCommand.Execute(null);
            Check(restarted.HasPreprocessOffNotice, "(4) clearing the alerts took the notice away");
            restarted.SaveSettingsCommand.Execute(null);
            var afterClear = Start();
            Check(Told(afterClear) && afterClear.HasPreprocessOffNotice, "(4) the notice did not come back after the alerts were cleared and the app restarted");

            // (5) acknowledged: no notice at the next start, and preprocessing is still off.
            afterClear.AcknowledgePreprocessOffNoticeCommand.Execute(null);
            Check(!afterClear.HasPreprocessOffNotice, "(5) the notice stayed after the acknowledgement");
            var acknowledged = Start();
            Check(!Told(acknowledged) && !acknowledged.HasPreprocessOffNotice, "(5) the notice came back after it was acknowledged");
            Check(!acknowledged.Settings.PreprocessInChain, "(5) preprocessing did not stay off after the acknowledgement");

            // switching preprocessing on is an answer too; an old file without the OFF, or with an ON, gets nothing.
            File.WriteAllText(path, "{ \"preprocessInChain\": false }");
            var turnedOn = Start();
            Check(turnedOn.HasPreprocessOffNotice, "(6) precondition: the old OFF is announced");
            turnedOn.Settings.PreprocessInChain = true;
            Check(!turnedOn.HasPreprocessOffNotice, "(6) switching preprocessing on left the notice up");
            File.WriteAllText(path, "{ \"rawWidth\": " + width + " }");
            Check(!Start().HasPreprocessOffNotice, "(6) a file without the key was announced");
            File.WriteAllText(path, "{ \"preprocessInChain\": true }");
            Check(!Start().HasPreprocessOffNotice, "(6) a stored ON was announced");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }

        return Task.CompletedTask;
    }

    // finding 2: the float image behind a display call is the one of the run that made the pixels - several runs at once, the rounded pixels identical.
    private static async Task EachDisplayCallGetsTheFloatsOfItsOwnRun_EvenWhenTheRoundedPixelsAreTheSame(string rawPath, int width, int height)
    {
        _scenario = "20 display floats belong to their own run";
        var directory = TempDirectory();
        try
        {
            var backend = new FloatTaggingBackend();
            var vm = NewViewModel(width, height, _ => backend, directory, out _);
            await LoadAndDrawLanes(vm, rawPath);

            // Apply A's main chain is held; Apply B runs to its end meanwhile; A's chain then ends last - its display call must still get A's floats.
            backend.BlockChainCall = backend.ChainCalls + 1;
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => backend.ChainBlocked.IsSet, "Apply A's held chain call");
            var settled = backend.DisplayCalls.Count;
            vm.Settings.LaneBVoiWindowWidth += 300f;
            vm.ApplyDisplayPipelineCommand.Execute(null);
            await Until(() => backend.DisplayCalls.Count >= settled + 2, "Apply B's display calls");
            backend.ChainRelease.Set();
            await Until(() => backend.DisplayCalls.Count >= settled + 3, "Apply A's display call");
            await FlushUi();

            Check(backend.ChainOverloadOnly, "the view model made a display call without the chain (the floats could not travel with it)");
            Check(backend.DisplayCalls.Count >= 5, $"too few display calls to say anything ({backend.DisplayCalls.Count})");
            foreach (var (chainTag, floatsTag) in backend.DisplayCalls)
            {
                Check(chainTag == floatsTag, $"a display call for the chain of run {chainTag} was given the floats of run {floatsTag}");
            }

            Check(backend.DisplayCalls.Select(c => c.ChainTag).Distinct().Count() >= 3, "the runs were not told apart (the scenario did not overlap anything)");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }
}
