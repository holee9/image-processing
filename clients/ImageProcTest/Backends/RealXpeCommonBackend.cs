using System;
using System.Runtime.InteropServices;

namespace ImageProcTest
{
    internal sealed class RealXpeCommonBackend : IXpeBackend
    {
        private bool initialized;

        public BackendHealthResult CheckHealth()
        {
            try
            {
                var initResult = XpeCommonApi.xpe_init(null);
                initialized = initResult == XpeCommonApi.XpeErrorCode.OK;

                if (!initialized)
                {
                    return Unavailable(
                        status: "Native common unavailable: init failed.",
                        init: FormatResult(initResult),
                        details: "xpe_init returned a failure code. Image-processing modules stay disabled.");
                }

                var version = PtrToString(XpeCommonApi.xpe_version());
                var details = $"xpe_error_string(INVALID_INPUT): {PtrToString(XpeCommonApi.xpe_error_string(XpeCommonApi.XpeErrorCode.INVALID_INPUT))}";

                var paramRange = GetParamRange();
                var memoryAbi = RunMemoryAbiSmoke();
                var alerts = $"Alerts: pending={XpeCommonApi.xpe_get_pending_alert_count()}";

                var aedResult = XpeCommonApi.xpe_aed_get_status(out var aedState);
                if (aedResult == XpeCommonApi.XpeErrorCode.OK)
                {
                    details += Environment.NewLine + $"Auto Exposure Detection state: {aedState}";
                }

                return new BackendHealthResult(
                    BackendName: nameof(RealXpeCommonBackend),
                    Mode: "Native",
                    Status: "Native common backend ready. Image-processing modules remain gated.",
                    Version: version,
                    DllPath: XpeCommonApi.ResolvedDllPath,
                    Init: FormatResult(initResult),
                    ParamRange: paramRange,
                    MemoryAbi: memoryAbi,
                    Alerts: alerts,
                    Details: details,
                    IsNativeReady: true);
            }
            catch (DllNotFoundException ex)
            {
                return Unavailable("Native common unavailable: xpe_common.dll not found.", "Unavailable", ex.Message);
            }
            catch (EntryPointNotFoundException ex)
            {
                return Unavailable("Native common unavailable: xpe_common.dll entry point mismatch.", "Unavailable", ex.Message);
            }
            catch (BadImageFormatException ex)
            {
                return Unavailable("Native common unavailable: xpe_common.dll architecture mismatch.", "Unavailable", ex.Message);
            }
            catch (Exception ex)
            {
                return Unavailable("Native common health check failed.", "Unavailable", ex.Message);
            }
        }

        public void Shutdown()
        {
            if (!initialized)
            {
                return;
            }

            try
            {
                XpeCommonApi.xpe_shutdown();
            }
            catch
            {
                // Shutdown happens during window close; health details are captured during refresh.
            }
        }

        private static string GetParamRange()
        {
            var result = XpeCommonApi.xpe_get_param_range("CHEST", "window_center",
                out var minValue, out var maxValue, out var defaultValue);

            return result == XpeCommonApi.XpeErrorCode.OK
                ? $"Param range: OK min={minValue:0.###}, max={maxValue:0.###}, default={defaultValue:0.###}"
                : $"Param range: {FormatResult(result)}";
        }

        private static string RunMemoryAbiSmoke()
        {
            var allocResult = XpeCommonApi.xpe_alloc_image(16, 16, XpeCommonApi.XpePixelFormat.UInt16, out var buffer);
            if (allocResult != XpeCommonApi.XpeErrorCode.OK)
            {
                return $"Memory ABI: alloc {FormatResult(allocResult)}";
            }

            try
            {
                var expectedSize = 16u * 16u * 2u;
                var ok = buffer.Width == 16 &&
                         buffer.Height == 16 &&
                         buffer.BitsAllocated == 16 &&
                         buffer.BitsStored == 16 &&
                         buffer.Format == XpeCommonApi.XpePixelFormat.UInt16 &&
                         buffer.Data != IntPtr.Zero &&
                         buffer.DataSize == expectedSize;

                return ok
                    ? $"Memory ABI: OK buffer={buffer.Width}x{buffer.Height}, bytes={buffer.DataSize}"
                    : $"Memory ABI: unexpected layout/data size, bytes={buffer.DataSize}";
            }
            finally
            {
                XpeCommonApi.xpe_free_image(ref buffer);
            }
        }

        private static BackendHealthResult Unavailable(string status, string init, string details)
        {
            return new BackendHealthResult(
                BackendName: nameof(RealXpeCommonBackend),
                Mode: "Unavailable",
                Status: status,
                Version: "Unavailable",
                DllPath: XpeCommonApi.ResolvedDllPath,
                Init: init,
                ParamRange: "Param range: Skipped",
                MemoryAbi: "Memory ABI: Skipped",
                Alerts: "Alerts: Skipped",
                Details: details,
                IsNativeReady: false);
        }

        private static string PtrToString(IntPtr ptr)
        {
            return ptr == IntPtr.Zero ? "<null>" : Marshal.PtrToStringAnsi(ptr) ?? "<empty>";
        }

        private static string FormatResult(XpeCommonApi.XpeErrorCode result)
        {
            return $"{result} ({(int)result})";
        }
    }
}
