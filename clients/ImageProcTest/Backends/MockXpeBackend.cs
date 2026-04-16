namespace ImageProcTest
{
    internal sealed class MockXpeBackend : IXpeBackend
    {
        public BackendHealthResult CheckHealth()
        {
            return new BackendHealthResult(
                BackendName: "MockXpeBackend",
                Mode: "Mock",
                Status: "Mock backend available. Native image-processing modules remain gated.",
                Version: "mock-0.1",
                DllPath: "Mock backend",
                Init: "Skipped",
                ParamRange: "Param range: Mock fallback available",
                MemoryAbi: "Memory ABI: Mock fallback available",
                Alerts: "Alerts: Mock fallback available",
                Details: "Mock mode is only for GUI wiring and safe fallback. It must not be treated as evidence that native processing is working.",
                IsNativeReady: false);
        }

        public void Shutdown()
        {
        }
    }
}
