using System.Runtime.InteropServices;

namespace ImageProcTest.PInvokeWrappers
{
    /// <summary>
    /// P/Invoke stub scaffold for xpe_enhance_basic.dll.
    /// Phase 1b: function bodies are stubs only; the DLL is not yet linked.
    /// XpeImageMetadata marshaling matches XpeCommonApi.XpeImageMetadata (Pack=8, CharSet=Ansi).
    /// </summary>
    internal static class XpeEnhanceBasicWrapper
    {
        private const string DllName = "xpe_enhance_basic.dll";

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
    }
}
