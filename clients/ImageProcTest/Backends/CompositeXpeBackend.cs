using System;

namespace ImageProcTest
{
    internal sealed class CompositeXpeBackend : IXpeBackend
    {
        private readonly IXpeBackend nativeBackend;
        private readonly IXpeBackend fallbackBackend;

        public CompositeXpeBackend(IXpeBackend nativeBackend, IXpeBackend fallbackBackend)
        {
            this.nativeBackend = nativeBackend;
            this.fallbackBackend = fallbackBackend;
        }

        public BackendHealthResult CheckHealth()
        {
            var nativeResult = nativeBackend.CheckHealth();
            if (nativeResult.IsNativeReady)
            {
                return nativeResult;
            }

            var fallbackResult = fallbackBackend.CheckHealth();
            return nativeResult with
            {
                Details = nativeResult.Details + Environment.NewLine + fallbackResult.Details
            };
        }

        public void Shutdown()
        {
            try
            {
                nativeBackend.Shutdown();
            }
            finally
            {
                fallbackBackend.Shutdown();
            }
        }
    }
}
