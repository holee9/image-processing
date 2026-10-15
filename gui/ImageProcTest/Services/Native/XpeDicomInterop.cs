// #225 row 9 (GUI-C-196 M3): the xpe_dicom.dll entry points the baseline's export needs (write, validate, and the reader used to read the file back).
using System.Runtime.InteropServices;

namespace ImageProcTest.Services.Native;

/// <summary>
/// P/Invoke surface for <c>xpe_dicom.dll</c>, declared in this assembly beside its resolver for the same one-resolver-per-assembly reason as
/// <see cref="XpePreprocessNative"/>. Only what the baseline calls: network (C-STORE, MWL) and J2K writing are not here.
/// Paths are marshalled as ANSI, like the other modules' paths in this assembly.
/// </summary>
internal static class XpeDicomNative
{
    private const string DllName = "xpe_dicom.dll";

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    internal static extern int xpe_dicom_write(string filePath, ref XpeImageBufferNative img, ref XpeImageMetadataNative meta);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    internal static extern int xpe_dicom_validate(string filePath, byte[] outReportJson, uint reportBufLen);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    internal static extern int xpe_dicom_open(string filePath, out IntPtr handle);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_dicom_read_image(IntPtr handle, out XpeImageBufferNative image);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_dicom_get_metadata(IntPtr handle, ref XpeImageMetadataNative meta);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void xpe_dicom_close(IntPtr handle);
}
