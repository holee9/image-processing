using System.Runtime.InteropServices;
using System.Text;

namespace ImageProcTest.PInvokeWrappers
{
    internal static class XpeDicomWrapper
    {
        public const string DllName = "xpe_dicom.dll";

        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal delegate XpeCommonApi.XpeErrorCode OpenDelegate(
            [MarshalAs(UnmanagedType.LPStr)] string? filePath,
            out IntPtr handle);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode ReadImageDelegate(
            IntPtr handle,
            out XpeCommonApi.XpeImageBuffer image);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode GetMetadataDelegate(
            IntPtr handle,
            out XpeCommonApi.XpeImageMetadata metadata);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate void CloseDelegate(IntPtr handle);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal delegate XpeCommonApi.XpeErrorCode WriteDelegate(
            [MarshalAs(UnmanagedType.LPStr)] string? filePath,
            ref XpeCommonApi.XpeImageBuffer image,
            ref XpeCommonApi.XpeImageMetadata metadata);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal delegate XpeCommonApi.XpeErrorCode WriteJ2kDelegate(
            [MarshalAs(UnmanagedType.LPStr)] string? filePath,
            ref XpeCommonApi.XpeImageBuffer image,
            ref XpeCommonApi.XpeImageMetadata metadata);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal delegate XpeCommonApi.XpeErrorCode ValidateDelegate(
            [MarshalAs(UnmanagedType.LPStr)] string? filePath,
            StringBuilder reportJson,
            uint reportBufferLength);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal delegate XpeCommonApi.XpeErrorCode CStoreDelegate(
            [MarshalAs(UnmanagedType.LPStr)] string? host,
            ushort port,
            [MarshalAs(UnmanagedType.LPStr)] string? aet,
            [MarshalAs(UnmanagedType.LPStr)] string? filePath,
            uint timeoutMs);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal delegate XpeCommonApi.XpeErrorCode CFindMwlDelegate(
            [MarshalAs(UnmanagedType.LPStr)] string? host,
            ushort port,
            [MarshalAs(UnmanagedType.LPStr)] string? aet,
            [MarshalAs(UnmanagedType.LPStr)] string? queryJson,
            StringBuilder outJson,
            uint outBufferLength,
            uint timeoutMs);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate void CancelDelegate();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode RawPointerDelegate(
            IntPtr first,
            IntPtr second);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode RawPointer3Delegate(
            IntPtr first,
            IntPtr second,
            IntPtr third);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode RawPointerUIntDelegate(
            IntPtr first,
            IntPtr second,
            uint third);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode RawCStoreDelegate(
            IntPtr host,
            ushort port,
            IntPtr aet,
            IntPtr filePath,
            uint timeoutMs);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode RawCFindMwlDelegate(
            IntPtr host,
            ushort port,
            IntPtr aet,
            IntPtr queryJson,
            IntPtr outJson,
            uint outBufferLength,
            uint timeoutMs);

        internal static TDelegate GetRequiredDelegate<TDelegate>(
            IntPtr handle,
            string exportName)
            where TDelegate : Delegate
        {
            if (!NativeLibrary.TryGetExport(handle, exportName, out var symbol))
            {
                throw new EntryPointNotFoundException($"{exportName} was not found in {DllName}.");
            }

            return Marshal.GetDelegateForFunctionPointer<TDelegate>(symbol);
        }
    }
}
