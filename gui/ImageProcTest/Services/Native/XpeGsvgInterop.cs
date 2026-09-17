// #180 (GUI-C-101): the gsvg entry points the gui needs, declared where the shared resolver sees them.
using System.Runtime.InteropServices;

namespace ImageProcTest.Services.Native;

/// <summary>
/// Why a gsvg correction step did or did not change the image (<c>XpeGsvgReason</c>, gsvg_api.h).
/// Values are the module's; new ones are only added, so an unknown value is possible and is handled.
/// </summary>
internal enum XpeGsvgReasonNative
{
    Applied = 0,
    NotConfigured = 1,
    ImageTooSmall = 2,
    NoGridDetected = 3,
    GridNotInSubbands = 4,
    VirtualGridRefused = 5,
}

/// <summary>
/// What one <c>xpe_gsvg_process_ex</c> call did (<c>XpeGsvgResult</c>, gsvg_api.h, 24 bytes).
/// <see cref="StructSize"/> is set by the caller so a later, larger struct stays compatible.
/// </summary>
[StructLayout(LayoutKind.Sequential, Pack = 4)]
internal struct XpeGsvgResultNative
{
    public uint StructSize;
    public int VignetteApplied;
    public int GridSuppressed;
    public int VirtualGridApplied;
    public int RestoredOriginal;
    public int Reason;

    public static XpeGsvgResultNative Create() => new()
    {
        StructSize = (uint)Marshal.SizeOf<XpeGsvgResultNative>(),
    };
}

/// <summary>
/// P/Invoke surface for <c>gsvg.dll</c>. Declared here for the same reason as the preprocess surface:
/// this assembly owns exactly one DllImport resolver (GUI-C-32), so the declarations live beside it.
/// </summary>
internal static class XpeGsvgNative
{
    private const string DllName = "gsvg.dll";

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr xpe_gsvg_version();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    internal static extern int xpe_gsvg_init(out IntPtr handle, string? configJson);

    /// <summary>
    /// <c>xpe_gsvg_process_ex</c> — process plus what was done. The mask is NULL here: the GUI has no
    /// collimation field mask yet, and the module then warns once per handle (gsvg_api.h:229-232).
    /// </summary>
    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_gsvg_process_ex(
        IntPtr handle,
        ushort[] src,
        nuint srcCount,
        ushort[] dst,
        nuint dstCount,
        int width,
        int height,
        float[]? gainMap,
        nuint gainCount,
        byte[]? fieldMask,
        nuint maskCount,
        ref XpeGsvgResultNative result);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_gsvg_shutdown(IntPtr handle);
}
