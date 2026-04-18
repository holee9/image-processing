using System.Runtime.InteropServices;

namespace ImageProcTest.Services.Native;

internal enum XpePixelFormatNative
{
    UInt16 = 0,
    Float32 = 1
}

internal enum XpeErrorCodeNative
{
    Ok = 0
}

[StructLayout(LayoutKind.Sequential, Pack = 8)]
internal struct XpeImageBufferNative
{
    public uint Width;
    public uint Height;
    public uint BitsAllocated;
    public uint BitsStored;
    public int Format;
    public IntPtr Data;
    public UIntPtr DataSize;
}

[StructLayout(LayoutKind.Sequential, Pack = 8)]
internal struct XpeModalityLutParamsNative
{
    public int Mode;
    public float RescaleSlope;
    public float RescaleIntercept;
    public IntPtr LutData;
    public uint LutLength;
    public int LutFirstMapped;
    public uint LutBitsStored;
}

[StructLayout(LayoutKind.Sequential, Pack = 8)]
internal struct XpeVoiLutParamsNative
{
    public int Mode;
    public float Center;
    public float Width;
    public float MinOut;
    public float MaxOut;
}

[StructLayout(LayoutKind.Sequential, Pack = 8)]
internal struct XpePresentationLutParamsNative
{
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 1024)]
    public ushort[] LutData;

    public int GsdfEnabled;

    public static XpePresentationLutParamsNative CreateLinear(bool gsdfEnabled)
    {
        var lut = new ushort[1024];
        for (var i = 0; i < lut.Length; i++)
        {
            lut[i] = (ushort)Math.Clamp((int)MathF.Round((i / 1023.0f) * ushort.MaxValue), 0, ushort.MaxValue);
        }

        return new XpePresentationLutParamsNative
        {
            LutData = lut,
            GsdfEnabled = gsdfEnabled ? 1 : 0
        };
    }
}

internal static class XpeCommonNative
{
    [DllImport("xpe_common.dll", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_alloc_image(
        uint width,
        uint height,
        XpePixelFormatNative format,
        out XpeImageBufferNative output);

    [DllImport("xpe_common.dll", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_free_image(ref XpeImageBufferNative buffer);
}

internal static class XpeDisplayNative
{
    [DllImport("xpe_display.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr xpe_display_version();

    [DllImport("xpe_display.dll", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_apply_modality_lut(
        ref XpeImageBufferNative img,
        ref XpeModalityLutParamsNative parameters);

    [DllImport("xpe_display.dll", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_apply_voi_lut(
        ref XpeImageBufferNative img,
        ref XpeVoiLutParamsNative parameters);

    [DllImport("xpe_display.dll", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_voi_preset_create(
        ref XpeVoiLutParamsNative parameters,
        XpeBodyPartEnumNative bodyPart);

    [DllImport("xpe_display.dll", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_apply_presentation_lut(
        ref XpeImageBufferNative img,
        ref XpePresentationLutParamsNative parameters);

    [DllImport("xpe_display.dll", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_gsdf_calibrate(
        [In] float[] luminanceValues,
        uint count,
        ref XpePresentationLutParamsNative outParams);

    internal static string GetVersion()
    {
        var versionPtr = xpe_display_version();
        return Marshal.PtrToStringAnsi(versionPtr) ?? "unknown";
    }
}

internal enum XpeBodyPartEnumNative
{
    Bone = 0,
    Lung = 1,
    Abdomen = 2,
    Head = 3
}
