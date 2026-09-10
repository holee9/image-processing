// #141 (Phase 1a): the preprocess entry points the gui needs, declared where the shared resolver sees them.
using System.Runtime.InteropServices;

namespace ImageProcTest.Services.Native;

/// <summary>
/// Image metadata passed to every preprocess stage (`XpeImageMetadata`, xpe_types.h).
///
/// The fixed 64-byte <c>bodyPart</c> array is marshalled by value — a string field would marshal as
/// a pointer and shift every following offset.
/// </summary>
[StructLayout(LayoutKind.Sequential, Pack = 8)]
internal struct XpeImageMetadataNative
{
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 64)]
    public byte[] BodyPart;

    public float KVp;
    public float MAs;
    public float SidMm;
    public float PixelPitchMm;
    public ulong AcquisitionTime;
    public uint Flags;

    /// <summary>Metadata with the body part filled in and the rest left at the caller's values.</summary>
    public static XpeImageMetadataNative Create(string bodyPart, float kVp, float mAs, float sidMm, float pixelPitchMm)
    {
        var buffer = new byte[64];
        var bytes = System.Text.Encoding.ASCII.GetBytes(bodyPart ?? string.Empty);
        Array.Copy(bytes, buffer, Math.Min(bytes.Length, buffer.Length - 1));

        return new XpeImageMetadataNative
        {
            BodyPart = buffer,
            KVp = kVp,
            MAs = mAs,
            SidMm = sidMm,
            PixelPitchMm = pixelPitchMm,
            AcquisitionTime = 0,
            Flags = 0,
        };
    }
}

/// <summary>
/// P/Invoke surface for <c>xpe_preprocess.dll</c>.
///
/// Declared in gui rather than shared from clients: the clients-side implementation
/// (<c>NativePreprocessPreviewService</c>, 1116 lines) reaches into <c>XpeCommonApi</c> 49 times, and
/// that type's static constructor registers a DllImport resolver. gui already registers its own
/// (GUI-C-32) and the runtime allows exactly one per assembly, so linking that file in would throw
/// at first touch — the same wall measured in GUI-C-13. Only the entry points the gui actually calls
/// are declared here.
/// </summary>
internal static class XpePreprocessNative
{
    private const string DllName = "xpe_preprocess.dll";

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    internal static extern int xpe_preprocess_init(string? config);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void xpe_preprocess_shutdown();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    internal static extern int xpe_calib_load_offset(string filePath);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    internal static extern int xpe_calib_load_gain(string filePath);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    internal static extern int xpe_calib_load_defect_map(string filePath);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_offset_correct(
        ref XpeImageBufferNative input,
        ref XpeImageBufferNative output,
        ref XpeImageMetadataNative metadata);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_gain_correct(
        ref XpeImageBufferNative input,
        ref XpeImageBufferNative output,
        ref XpeImageMetadataNative metadata);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_defect_correct(
        ref XpeImageBufferNative input,
        ref XpeImageBufferNative output,
        ref XpeImageMetadataNative metadata);
}
