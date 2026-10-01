using ImageProcTest.Models;

namespace ImageProcTest.Services;

// @MX:ANCHOR: [AUTO] Public contract between GUI shell and backend (Mock or Native); all backend operations route through this interface
// @MX:REASON: Changing any method signature requires updating both MockXpeBackend and RealXpeBackend; fan_in: MainWindowViewModel + integration tests
/// <summary>
/// Defines the runtime contract used by the GUI shell to communicate with either
/// a mock backend or a future native XPE adapter.
/// </summary>
public interface IXpeBackend
{
    /// <summary>
    /// Initializes the backend with the current GUI settings and returns runtime info.
    /// </summary>
    BackendRuntimeInfo Initialize(AppSettings settings);

    /// <summary>
    /// Returns the backend version string shown in the runtime panel.
    /// </summary>
    string GetVersion();

    /// <summary>
    /// Loads a raw image frame and returns the preview plus metadata required by the GUI.
    /// </summary>
    LoadedImageFrame LoadRawImage(string path, AppSettings settings);

    /// <summary>
    /// Applies the display pipeline and returns an updated frame.
    /// <paramref name="displayInput"/> is the pixel chain's result (<see cref="ChainResult.DisplayInput"/>),
    /// width × height long; the raw pixels of <paramref name="rawFrame"/> are not read (#180, GUI-C-99).
    /// </summary>
    LoadedImageFrame ApplyDisplayPipeline(LoadedImageFrame rawFrame, ushort[] displayInput, AppSettings settings);

    /// <summary>
    /// Runs the pixel chain (#180, GUI-C-99, contract B) over the loaded raw frame: the stages in order,
    /// each on the previous result, the raw frame untouched, a refused stage falling back to its input.
    /// Backends without a stage report it as <see cref="StageStatus.RequestedNotApplied"/>, not as an error.
    /// </summary>
    ChainResult RunChain(LoadedImageFrame rawFrame, IReadOnlyList<StageRequest> stages, AppSettings settings);

    /// <summary>True when this backend can actually run preprocessing (#141: native only).</summary>
    bool SupportsPreprocessing { get; }

    /// <summary>
    /// Returns the display module version string shown in the runtime panel.
    /// </summary>
    string GetDisplayVersion();

    /// <summary>
    /// Creates clinically validated VOI parameters for a supported body part.
    /// </summary>
    VoiPreset CreateVoiPreset(XpeBodyPartEnum bodyPart);

    /// <summary>
    /// The queued log lines and alerts after the first <paramref name="logsSeen"/> / <paramref name="alertsSeen"/> of each, with the
    /// totals, read as ONE step under the backend's own lock (GUI-C-186f, Codex #36 finding 4). Replaces the four count-and-index
    /// reads: a count and the items it counts can disagree when another thread writes between two calls, and the lists are written
    /// from pool threads (chain and display calls, a background shutdown) while the UI thread drains them.
    /// </summary>
    TelemetrySnapshot GetTelemetrySince(int logsSeen, int alertsSeen);

    /// <summary>
    /// Returns the current runtime state used by the GUI runtime panel.
    /// </summary>
    BackendRuntimeInfo GetRuntimeInfo();

    /// <summary>
    /// Shuts the backend down and releases runtime resources owned by the adapter.
    /// </summary>
    void Shutdown();
}
