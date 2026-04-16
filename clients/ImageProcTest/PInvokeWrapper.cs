using System.Runtime.InteropServices;

namespace ImageProcTest
{
    /// <summary>
    /// P/Invoke wrapper for xpe_common.dll
    /// </summary>
    internal static class XpeCommonApi
    {
        private const string DllName = "xpe_common.dll";

        #region Enums

        public enum XpePixelFormat : uint
        {
            UInt16 = 0,
            Float32 = 1
        }

        public enum XpeErrorCode : int
        {
            OK = 0,
            NOT_INITIALIZED = -1,
            INVALID_PARAM = -2,
            OUT_OF_MEMORY = -3,
            FILE_IO = -4,
            CONFIG_INVALID = -5
        }

        public enum XpeAlertSeverity : int
        {
            Info = 0,
            Warning = 1,
            Error = 2,
            Critical = 3
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

        [StructLayout(LayoutKind.Sequential, Pack = 8)]
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

        [StructLayout(LayoutKind.Sequential, Pack = 8)]
        public struct XpeAlertEntry
        {
            public XpeAlertSeverity Severity;
            public uint Code;
            public ulong Timestamp;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
            public string Message;
        }

        #endregion

        #region Lifecycle Functions

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern XpeErrorCode xpe_init();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern XpeErrorCode xpe_shutdown();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr xpe_version();

        #endregion

        #region Configuration Functions

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern XpeErrorCode xpe_configure([MarshalAs(UnmanagedType.LPStr)] string configJson);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern XpeErrorCode xpe_get_param_range(int paramId, out int minValue, out int maxValue);

        #endregion

        #region Error Handling Functions

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr xpe_error_string(XpeErrorCode code);

        #endregion

        #region Alert Queue Functions

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern uint xpe_get_pending_alert_count();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern XpeErrorCode xpe_get_pending_alert(out XpeAlertEntry alert);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern void xpe_clear_alerts();

        #endregion

        #region Memory Functions

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern XpeErrorCode xpe_alloc_image(uint width, uint height, XpePixelFormat format, out XpeImageBuffer buffer);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern XpeErrorCode xpe_free_image(ref XpeImageBuffer buffer);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern XpeErrorCode xpe_copy_image(in XpeImageBuffer src, ref XpeImageBuffer dst);

        #endregion

        #region Logging Functions

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern void xpe_log_set_level(int level);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern XpeErrorCode xpe_log_set_file([MarshalAs(UnmanagedType.LPStr)] string filePath);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern void xpe_log_flush();

        #endregion

        #region AED Functions

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern XpeErrorCode xpe_aed_configure([MarshalAs(UnmanagedType.LPStr)] string configJson);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern XpeErrorCode xpe_aed_poll_event(out int eventType, out ulong timestamp, out float signalLevel);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern XpeErrorCode xpe_aed_get_status(out int state);

        #endregion
    }
}
