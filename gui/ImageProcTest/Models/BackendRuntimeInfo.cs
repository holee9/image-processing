namespace ImageProcTest.Models;

public sealed class BackendRuntimeInfo
{
    public string BackendName { get; init; } = "Mock Backend";

    public string Version { get; init; } = "mock-0.1.0";

    public string State { get; init; } = "Uninitialized";

    public bool SupportsNativeRuntime { get; init; }

    public bool NativeDllDetected { get; init; }

    public string NativeDllPath { get; init; } = string.Empty;

    /// <summary>
    /// #129: which directory the native library actually came from (last segment), or
    /// <c>loader</c> when the DllImport resolver did not supply it. Empty for a Mock run.
    /// </summary>
    public string NativeSource { get; init; } = string.Empty;

    public string DisplayVersion { get; init; } = string.Empty;

    public bool DisplayDllDetected { get; init; }

    public string DisplayDllPath { get; init; } = string.Empty;
}
