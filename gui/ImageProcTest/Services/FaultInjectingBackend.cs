// #171 (GUI-C-79): a test fault, armed only from the command line.
#if XPE_TEST_FAULTS   // GUI-C-193: this type exists only in a test build; a shipped build does not contain it.
using ImageProcTest.Models;

namespace ImageProcTest.Services;

/// <summary>
/// Lets the first <c>failAfter</c> display pipeline calls through and makes every later one throw.
///
/// <para><b>Why it exists.</b> Neither backend can be made to fail its display pipeline from the UI —
/// both clamp their inputs (GUI-C-78) — so the path where a render fails and the old image stays up had
/// no way to be exercised end to end. The user allowed a fault seam on three conditions (#171): command
/// line only, completely inert without the argument, and loud when armed.</para>
///
/// <para><b>Inert without the argument</b> is structural: <see cref="Wrap"/> returns the backend it was
/// given when no fault was requested, so this type is never constructed. <see cref="Describe"/> is what
/// the automation tree reports in both cases, and the E2E suite checks it on an unarmed app as well as
/// on an armed one.</para>
/// </summary>
public sealed class FaultInjectingBackend : IXpeBackend, IAiSessionBackend
{
    private readonly IXpeBackend _inner;
    private readonly int _failAfter;
    private readonly bool _displayFault;
    private readonly bool _aiWorkerDisabled;
    private readonly int? _aiWorkerSilentAfter;
    private int _calls;
    private int _statusReads;

    private FaultInjectingBackend(IXpeBackend inner, int? failAfter, bool aiWorkerDisabled, int? aiWorkerSilentAfter)
    {
        _inner = inner;
        _displayFault = failAfter is not null;
        _failAfter = failAfter ?? int.MaxValue;
        _aiWorkerDisabled = aiWorkerDisabled;
        _aiWorkerSilentAfter = aiWorkerSilentAfter;
    }

    /// <summary>The last wrapper created in this process, or null when fault injection is off.</summary>
    public static FaultInjectingBackend? Armed { get; private set; }

    /// <summary>Returns <paramref name="backend"/> itself unless a fault was requested.</summary>
    public static IXpeBackend Wrap(IXpeBackend backend, int? failAfter, bool aiWorkerDisabled = false, int? aiWorkerSilentAfter = null)
    {
        if (failAfter is null && !aiWorkerDisabled && aiWorkerSilentAfter is null)
        {
            return backend;
        }

        Armed = new FaultInjectingBackend(backend, failAfter, aiWorkerDisabled, aiWorkerSilentAfter);
        return Armed;
    }

    /// <summary>What the automation tree reports: <c>off</c>, or the armed fault and its call count.</summary>
    public static string Describe() =>
        Armed is null
            ? "faultInjection=off"
            : Armed._displayFault
                ? $"faultInjection={AutomationArgs.DisplayPipelineFaultPrefix}{Armed._failAfter} calls={Armed._calls}"
                    + (Armed._aiWorkerDisabled ? $" {AutomationArgs.AiWorkerDisabledFault}" : string.Empty)
                : $"faultInjection={(Armed._aiWorkerSilentAfter is 0 ? AutomationArgs.AiWorkerSilentFromStartFault : Armed._aiWorkerSilentAfter is not null ? AutomationArgs.AiWorkerSilentFault : AutomationArgs.AiWorkerDisabledFault)} calls={Armed._calls}";

    public LoadedImageFrame ApplyDisplayPipeline(LoadedImageFrame rawFrame, ushort[] displayInput, AppSettings settings)
    {
        var call = Interlocked.Increment(ref _calls);
        if (call > _failAfter)
        {
            throw new InvalidOperationException(
                $"FAULT INJECTION: display pipeline call {call} failed on purpose " +
                $"(--automation-fault {AutomationArgs.DisplayPipelineFaultPrefix}{_failAfter}).");
        }

        return _inner.ApplyDisplayPipeline(rawFrame, displayInput, settings);
    }

    public BackendRuntimeInfo Initialize(AppSettings settings) => _inner.Initialize(settings);

    public string GetVersion() => _inner.GetVersion();

    public LoadedImageFrame LoadRawImage(string path, AppSettings settings) => _inner.LoadRawImage(path, settings);

    public ChainResult RunChain(LoadedImageFrame rawFrame, IReadOnlyList<StageRequest> stages, AppSettings settings) =>
        _inner.RunChain(rawFrame, stages, settings);

    public bool SupportsPreprocessing => _inner.SupportsPreprocessing;

    // GUI-C-185: the wrapper adds no fault of its own to the AI session; it passes the question to the backend it wraps.
    AiWorkerStatus IAiSessionBackend.GetAiWorkerStatus() =>
        _aiWorkerSilentAfter is not null ? SilentWorkerStatus(_aiWorkerSilentAfter.Value)
        : _aiWorkerDisabled
            ? new AiWorkerStatus(AiWorkerState.Disabled, 3, 3)   // the injected fault: the worker is "switched off after 3 of 3"
            : _inner is IAiSessionBackend session ? session.GetAiWorkerStatus() : AiWorkerStatus.Unknown;

    /// <summary>GUI-C-192c: the first <c>answers</c> reads answer "active, 0 of 3" (none for ai-worker-silent:0); every later one waits (a worker that stopped replying) until the process ends.</summary>
    private AiWorkerStatus SilentWorkerStatus(int answers)
    {
        if (Interlocked.Increment(ref _statusReads) <= answers)
        {
            return new AiWorkerStatus(AiWorkerState.Active, 0, 3);
        }

        Thread.Sleep(Timeout.Infinite);
        return AiWorkerStatus.Unknown;
    }

    AiRestartResult IAiSessionBackend.RestartAiSession(string modelDirectory) =>
        (_inner as IAiSessionBackend)?.RestartAiSession(modelDirectory)
        ?? new AiRestartResult(false, "AI session restart needs the native backend.");

    public string GetDisplayVersion() => _inner.GetDisplayVersion();

    public VoiPreset CreateVoiPreset(XpeBodyPartEnum bodyPart) => _inner.CreateVoiPreset(bodyPart);

    public TelemetrySnapshot GetTelemetrySince(int logsSeen, int alertsSeen) => _inner.GetTelemetrySince(logsSeen, alertsSeen);

    public BackendRuntimeInfo GetRuntimeInfo() => _inner.GetRuntimeInfo();

    public void Shutdown() => _inner.Shutdown();
}
#endif
