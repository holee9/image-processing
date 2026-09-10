using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;

namespace ImageProcTest
{
    /// <summary>
    /// P/Invoke wrapper for xpe_common.dll.
    /// </summary>
    internal static class XpeCommonApi
    {
        private const string DllName = "xpe_common.dll";
        private static string? resolvedDllPath;

        static XpeCommonApi()
        {
            NativeLibrary.SetDllImportResolver(typeof(XpeCommonApi).Assembly, ResolveNativeLibrary);
        }

        public static string ResolvedDllPath => resolvedDllPath ?? DllName;

        #region Enums

        public enum XpePixelFormat : uint
        {
            UInt16 = 0,
            Float32 = 1,
            UInt8 = 2,
        }

        public enum XpeErrorCode : int
        {
            OK = 0,
            INVALID_INPUT = -1,
            OUT_OF_MEMORY = -2,
            PROCESSING_FAILED = -3,
            CONFIG_INVALID = -4,
            CALIBRATION_EXPIRED = -5,
            NOT_INITIALIZED = -6,
            UNSUPPORTED_FORMAT = -7,
            BUFFER_TOO_SMALL = -8,
            IO_FAILED = -9,
            NETWORK_FAILED = -10,
            SAFETY_VIOLATION = -11,
            INTERNAL = -12,
            DICOM_INVALID = -13,
            DICOM_CONFORMANCE = -14,
            NOT_IMPLEMENTED = -15,
            CALIB_NOT_LOADED = -16,
        }

        public enum XpeAlertSeverity : int
        {
            Info = 0,
            Warning = 1,
            Error = 2,
        }

        #endregion

        #region Structs

        [StructLayout(LayoutKind.Sequential, Pack = 8)]
        public struct XpeImageBuffer
        {
            public uint Width;
            public uint Height;
            public uint BitsAllocated;
            public uint BitsStored;
            public XpePixelFormat Format;
            public IntPtr Data;
            public nuint DataSize;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 8, CharSet = CharSet.Ansi)]
        public struct XpeImageMetadata
        {
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
            public string BodyPart;
            public float KVp;
            public float MAs;
            public float SID_mm;
            public float PixelPitch_mm;
            public ulong AcquisitionTime;
            public uint Flags;
        }

        #endregion

        #region Lifecycle Functions

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern XpeErrorCode xpe_init([MarshalAs(UnmanagedType.LPStr)] string? configJsonOrNull);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern void xpe_shutdown();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr xpe_version();

        #endregion

        #region Configuration Functions

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern XpeErrorCode xpe_configure([MarshalAs(UnmanagedType.LPStr)] string configJson);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern XpeErrorCode xpe_get_param_range(
            [MarshalAs(UnmanagedType.LPStr)] string bodyPart,
            [MarshalAs(UnmanagedType.LPStr)] string paramName,
            out float minValue,
            out float maxValue,
            out float defaultValue);

        #endregion

        #region Error Handling Functions

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr xpe_error_string(XpeErrorCode code);

        #endregion

        #region Alert Queue Functions

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int xpe_get_pending_alert_count();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern XpeErrorCode xpe_get_pending_alert(
            int index,
            StringBuilder message,
            UIntPtr messageLength,
            out int severity);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern void xpe_clear_alerts();

        #endregion

        #region Memory Functions

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern XpeErrorCode xpe_alloc_image(uint width, uint height, XpePixelFormat format, out XpeImageBuffer buffer);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern XpeErrorCode xpe_free_image(ref XpeImageBuffer buffer);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern XpeErrorCode xpe_copy_image(ref XpeImageBuffer src, ref XpeImageBuffer dst);

        #endregion

        #region Logging Functions

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern XpeErrorCode xpe_log_set_level(int level);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern XpeErrorCode xpe_log_set_file([MarshalAs(UnmanagedType.LPStr)] string filePath);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern void xpe_log_flush();

        #endregion

        private static IntPtr ResolveNativeLibrary(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
        {
            if (!string.Equals(libraryName, DllName, StringComparison.OrdinalIgnoreCase))
            {
                return IntPtr.Zero;
            }

            foreach (var candidate in XpeCommonLibraryLocator.GetDllCandidates())
            {
                if (!File.Exists(candidate))
                {
                    continue;
                }

                NativeDependencyLoader.TryLoadFor(candidate);
                if (NativeLibrary.TryLoad(candidate, out var handle))
                {
                    resolvedDllPath = candidate;
                    return handle;
                }
            }

            if (NativeLibrary.TryLoad(libraryName, assembly, searchPath, out var fallbackHandle))
            {
                resolvedDllPath = libraryName;
                return fallbackHandle;
            }

            return IntPtr.Zero;
        }

    }
}
