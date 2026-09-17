// #171 (GUI-C-79): a test fault, armed only from the command line.
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
public sealed class FaultInjectingBackend : IXpeBackend
{
    private readonly IXpeBackend _inner;
    private readonly int _failAfter;
    private int _calls;

    private FaultInjectingBackend(IXpeBackend inner, int failAfter)
    {
        _inner = inner;
        _failAfter = failAfter;
    }

    /// <summary>The last wrapper created in this process, or null when fault injection is off.</summary>
    public static FaultInjectingBackend? Armed { get; private set; }

    /// <summary>Returns <paramref name="backend"/> itself unless a fault was requested.</summary>
    public static IXpeBackend Wrap(IXpeBackend backend, int? failAfter)
    {
        if (failAfter is null)
        {
            return backend;
        }

        Armed = new FaultInjectingBackend(backend, failAfter.Value);
        return Armed;
    }

    /// <summary>What the automation tree reports: <c>off</c>, or the armed fault and its call count.</summary>
    public static string Describe() =>
        Armed is null
            ? "faultInjection=off"
            : $"faultInjection={AutomationArgs.DisplayPipelineFaultPrefix}{Armed._failAfter} calls={Armed._calls}";

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

    public string GetDisplayVersion() => _inner.GetDisplayVersion();

    public VoiPreset CreateVoiPreset(XpeBodyPartEnum bodyPart) => _inner.CreateVoiPreset(bodyPart);

    public int GetAlertCount() => _inner.GetAlertCount();

    public AlertEntry? GetAlert(int index) => _inner.GetAlert(index);

    public int GetLogCount() => _inner.GetLogCount();

    public string? GetLog(int index) => _inner.GetLog(index);

    public BackendRuntimeInfo GetRuntimeInfo() => _inner.GetRuntimeInfo();

    public void Shutdown() => _inner.Shutdown();
}
