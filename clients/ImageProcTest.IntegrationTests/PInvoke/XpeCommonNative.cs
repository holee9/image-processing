// @MX:NOTE Mirror of clients/ImageProcTest/PInvokeWrapper.cs — kept in sync intentionally.
// Tests call this directly to avoid WPF/WinExe compilation dependency.
using System.Runtime.InteropServices;
using System.Text;

namespace ImageProcTest.IntegrationTests.PInvoke;

/// <summary>
/// Internal P/Invoke declarations for xpe_common.dll, mirroring
/// <c>ImageProcTest.XpeCommonApi</c> for test-project isolation.
/// CharSet=Ansi and CallingConvention=Cdecl match the native ABI (Pack=8).
/// </summary>
public static class XpeCommonNative
{
    private const string DllName = "xpe_common.dll";

    #region Enums

    public enum XpePixelFormat : uint
    {
        UInt16 = 0,
        Float32 = 1,
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
    }

    public enum XpeAlertSeverity : int
    {
        Info = 0,
        Warning = 1,
        Error = 2,
    }

    #endregion

    #region Structs

    /// <summary>
    /// Image buffer passed across the C#↔C ABI boundary.
    /// Pack=8, x64: Width(4)+Height(4)+BitsAllocated(4)+BitsStored(4)+Format(4)+pad(4)+Data(8)+DataSize(8) = 40 bytes.
    /// </summary>
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

    /// <summary>
    /// Image metadata passed across the ABI boundary.
    /// Pack=8, BodyPart is ANSI fixed-length 64 bytes, x64: 64+4+4+4+4+8+4+pad(4) = 96 bytes.
    /// </summary>
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

    #region Lifecycle

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern XpeErrorCode xpe_init([MarshalAs(UnmanagedType.LPStr)] string? configJsonOrNull);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern void xpe_shutdown();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern IntPtr xpe_version();

    #endregion

    #region Configuration

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

    #region Error Handling

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern IntPtr xpe_error_string(XpeErrorCode code);

    #endregion

    #region Alert Queue

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

    #region Memory

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern XpeErrorCode xpe_alloc_image(
        uint width, uint height, XpePixelFormat format, out XpeImageBuffer buffer);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern XpeErrorCode xpe_free_image(ref XpeImageBuffer buffer);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern XpeErrorCode xpe_copy_image(ref XpeImageBuffer src, ref XpeImageBuffer dst);

    #endregion

    #region Logging

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern XpeErrorCode xpe_log_set_level(int level);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern XpeErrorCode xpe_log_set_file([MarshalAs(UnmanagedType.LPStr)] string filePath);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern void xpe_log_flush();

    #endregion

}
