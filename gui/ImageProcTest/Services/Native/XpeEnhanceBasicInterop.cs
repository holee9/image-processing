// #225 row 9 (GUI-C-196 M2): the xpe_enhance_basic.dll entry points the Deterministic Baseline needs, declared where the shared resolver sees them.
using System.Runtime.InteropServices;

namespace ImageProcTest.Services.Native;

/// <summary><c>XpeNoiseReduceParams</c> (enhance_basic_api.h): mode, then the bilateral and NLM parameters. Pack = 8, 24 bytes.</summary>
[StructLayout(LayoutKind.Sequential, Pack = 8)]
internal struct XpeNoiseReduceParamsNative
{
    /// <summary>0 = bilateral, 1 = non-local means.</summary>
    public int Mode;
    public float SigmaSpace;
    public float SigmaRange;
    public int SearchWindow;
    public int PatchSize;
    public float HParam;
}

/// <summary><c>XpeClaheParams</c>: clip limit and the tile grid. Pack = 8, 12 bytes.</summary>
[StructLayout(LayoutKind.Sequential, Pack = 8)]
internal struct XpeClaheParamsNative
{
    public float ClipLimit;
    public int TileWidth;
    public int TileHeight;
}

/// <summary><c>XpeUsmParams</c>: unsharp masking amount, radius and threshold. Pack = 8, 12 bytes.</summary>
[StructLayout(LayoutKind.Sequential, Pack = 8)]
internal struct XpeUsmParamsNative
{
    public float Amount;
    public float Radius;
    public float Threshold;
}

/// <summary>
/// P/Invoke surface for <c>xpe_enhance_basic.dll</c> (the same single-resolver reason as <see cref="XpePreprocessNative"/>: declared here, beside the
/// resolver of this assembly). All of these work in place on a FLOAT32 <see cref="XpeImageBufferNative"/>.
/// </summary>
internal static class XpeEnhanceBasicNative
{
    private const string DllName = "xpe_enhance_basic.dll";

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_log_transform(ref XpeImageBufferNative img, float normFactor);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_noise_reduce(ref XpeImageBufferNative img, ref XpeNoiseReduceParamsNative parameters);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_contrast_enhance(ref XpeImageBufferNative img, ref XpeClaheParamsNative parameters);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_edge_enhance(ref XpeImageBufferNative img, ref XpeUsmParamsNative parameters);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr xpe_enhance_basic_version();

    internal static string GetVersion() => Marshal.PtrToStringAnsi(xpe_enhance_basic_version()) ?? "unknown";
}
