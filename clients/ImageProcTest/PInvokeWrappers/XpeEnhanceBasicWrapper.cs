using System.Runtime.InteropServices;

namespace ImageProcTest.PInvokeWrappers
{
    /// <summary>
    /// P/Invoke stub scaffold for xpe_enhance_basic.dll.
    /// Phase 1b: function bodies are stubs only; the DLL is not yet linked.
    /// XpeImageMetadata marshaling matches XpeCommonApi.XpeImageMetadata (Pack=8, CharSet=Ansi).
    ///
    /// Module independence: if the DLL is absent, IsAvailable is false and all
    /// Safe* helpers return XPE_ERR_NOT_INITIALIZED (-6) instead of throwing.
    /// </summary>
    internal static class XpeEnhanceBasicWrapper
    {
        private const string DllName = "xpe_enhance_basic.dll";

        // ------------------------------------------------------------------ //
        // Availability probe (set once in static constructor)
        // ------------------------------------------------------------------ //

        /// <summary>True when xpe_enhance_basic.dll loaded and version entry-point resolved.</summary>
        public static bool IsAvailable { get; private set; }

        /// <summary>Human-readable reason when IsAvailable is false; null when available.</summary>
        public static string? UnavailableReason { get; private set; }

        static XpeEnhanceBasicWrapper()
        {
            try
            {
                var ptr = xpe_enhance_basic_version();
                IsAvailable = ptr != IntPtr.Zero;
            }
            catch (DllNotFoundException ex)
            {
                IsAvailable = false;
                UnavailableReason = ex.Message;
            }
            catch (EntryPointNotFoundException ex)
            {
                IsAvailable = false;
                UnavailableReason = ex.Message;
            }
            catch (BadImageFormatException ex)
            {
                IsAvailable = false;
                UnavailableReason = ex.Message;
            }
        }

        // ------------------------------------------------------------------ //
        // Raw P/Invoke declarations
        // ------------------------------------------------------------------ //

        /// <summary>Returns the DLL version string (const char*).</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr xpe_enhance_basic_version();

        /// <summary>Applies logarithmic transform in-place.</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int xpe_log_transform(
            IntPtr img,
            ref XpeCommonApi.XpeImageMetadata meta,
            IntPtr config);

        /// <summary>Applies inverse logarithmic transform in-place.</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int xpe_log_inverse(
            IntPtr img,
            ref XpeCommonApi.XpeImageMetadata meta,
            IntPtr config);

        /// <summary>Applies noise reduction in-place.</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int xpe_noise_reduce(
            IntPtr img,
            ref XpeCommonApi.XpeImageMetadata meta,
            IntPtr config);

        /// <summary>Estimates noise sigma for the image.</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int xpe_noise_estimate_sigma(
            IntPtr img,
            ref XpeCommonApi.XpeImageMetadata meta,
            out float sigma);

        /// <summary>Applies contrast enhancement in-place.</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int xpe_contrast_enhance(
            IntPtr img,
            ref XpeCommonApi.XpeImageMetadata meta,
            IntPtr config);

        /// <summary>Applies edge enhancement in-place.</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int xpe_edge_enhance(
            IntPtr img,
            ref XpeCommonApi.XpeImageMetadata meta,
            IntPtr config);

        /// <summary>Calculates exposure index (EI) and deviation index (DI).</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int xpe_calc_exposure_index(
            IntPtr img,
            ref XpeCommonApi.XpeImageMetadata meta,
            out float ei,
            out float di);

        // ------------------------------------------------------------------ //
        // Safe wrappers — return XPE_ERR_NOT_INITIALIZED (-6) when unavailable
        // ------------------------------------------------------------------ //

        public static int SafeLogTransform(IntPtr img, ref XpeCommonApi.XpeImageMetadata meta, IntPtr cfg)
        {
            if (!IsAvailable) return -6;
            try { return xpe_log_transform(img, ref meta, cfg); }
            catch (Exception) { return -3; }
        }

        public static int SafeLogInverse(IntPtr img, ref XpeCommonApi.XpeImageMetadata meta, IntPtr cfg)
        {
            if (!IsAvailable) return -6;
            try { return xpe_log_inverse(img, ref meta, cfg); }
            catch (Exception) { return -3; }
        }

        public static int SafeNoiseReduce(IntPtr img, ref XpeCommonApi.XpeImageMetadata meta, IntPtr cfg)
        {
            if (!IsAvailable) return -6;
            try { return xpe_noise_reduce(img, ref meta, cfg); }
            catch (Exception) { return -3; }
        }

        public static int SafeNoiseEstimateSigma(IntPtr img, ref XpeCommonApi.XpeImageMetadata meta, out float sigma)
        {
            if (!IsAvailable) { sigma = 0f; return -6; }
            try { return xpe_noise_estimate_sigma(img, ref meta, out sigma); }
            catch (Exception) { sigma = 0f; return -3; }
        }

        public static int SafeContrastEnhance(IntPtr img, ref XpeCommonApi.XpeImageMetadata meta, IntPtr cfg)
        {
            if (!IsAvailable) return -6;
            try { return xpe_contrast_enhance(img, ref meta, cfg); }
            catch (Exception) { return -3; }
        }

        public static int SafeEdgeEnhance(IntPtr img, ref XpeCommonApi.XpeImageMetadata meta, IntPtr cfg)
        {
            if (!IsAvailable) return -6;
            try { return xpe_edge_enhance(img, ref meta, cfg); }
            catch (Exception) { return -3; }
        }

        public static int SafeCalcExposureIndex(IntPtr img, ref XpeCommonApi.XpeImageMetadata meta, out float ei, out float di)
        {
            if (!IsAvailable) { ei = 0f; di = 0f; return -6; }
            try { return xpe_calc_exposure_index(img, ref meta, out ei, out di); }
            catch (Exception) { ei = 0f; di = 0f; return -3; }
        }
    }
}
