using ImageProcTest.Models;

namespace ImageProcTest.Services;

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
    /// Applies the display pipeline to a loaded raw frame and returns an updated frame.
    /// </summary>
    LoadedImageFrame ApplyDisplayPipeline(LoadedImageFrame rawFrame, AppSettings settings);

    /// <summary>
    /// Returns the display module version string shown in the runtime panel.
    /// </summary>
    string GetDisplayVersion();

    /// <summary>
    /// Creates clinically validated VOI parameters for a supported body part.
    /// </summary>
    VoiPreset CreateVoiPreset(XpeBodyPartEnum bodyPart);

    /// <summary>
    /// Returns the number of queued alerts available for the GUI alert panel.
    /// </summary>
    int GetAlertCount();

    /// <summary>
    /// Returns a queued alert by index.
    /// </summary>
    AlertEntry? GetAlert(int index);

    /// <summary>
    /// Returns the number of queued log entries available for the GUI log panel.
    /// </summary>
    int GetLogCount();

    /// <summary>
    /// Returns a queued log entry by index.
    /// </summary>
    string? GetLog(int index);

    /// <summary>
    /// Returns the current runtime state used by the GUI runtime panel.
    /// </summary>
    BackendRuntimeInfo GetRuntimeInfo();

    /// <summary>
    /// Shuts the backend down and releases runtime resources owned by the adapter.
    /// </summary>
    void Shutdown();
}
