namespace ImageProcTest
{
    internal sealed record BackendHealthResult(
        string BackendName,
        string Mode,
        string Status,
        string Version,
        string DllPath,
        string Init,
        string ParamRange,
        string MemoryAbi,
        string Alerts,
        string Details,
        bool IsNativeReady);
}
