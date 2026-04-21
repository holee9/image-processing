using System.Runtime.InteropServices;

namespace ImageProcTest.PInvokeWrappers
{
    internal enum XpeNoiseReduceMode : int
    {
        Bilateral = 0,
        NonLocalMeans = 1
    }

    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct XpeNoiseReduceParams
    {
        public XpeNoiseReduceMode Mode;
        public float SigmaSpace;
        public float SigmaRange;
        public int SearchWindow;
        public int PatchSize;
        public float HParam;

        public static XpeNoiseReduceParams DefaultBilateral => new()
        {
            Mode = XpeNoiseReduceMode.Bilateral,
            SigmaSpace = 3.0f,
            SigmaRange = 50.0f,
            SearchWindow = 21,
            PatchSize = 7,
            HParam = 10.0f
        };
    }

    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct XpeClaheParams
    {
        public float ClipLimit;
        public int TileWidth;
        public int TileHeight;

        public static XpeClaheParams Default => new()
        {
            ClipLimit = 3.0f,
            TileWidth = 8,
            TileHeight = 8
        };
    }

    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct XpeUsmParams
    {
        public float Amount;
        public float Radius;
        public float Threshold;

        public static XpeUsmParams Default => new()
        {
            Amount = 0.5f,
            Radius = 2.0f,
            Threshold = 10.0f
        };
    }

    internal static class XpeEnhanceBasicWrapper
    {
        public const string DllName = "xpe_enhance_basic.dll";

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate IntPtr VersionDelegate();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode LogTransformDelegate(
            ref XpeCommonApi.XpeImageBuffer image,
            float normFactor);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode LogInverseDelegate(
            ref XpeCommonApi.XpeImageBuffer image,
            float normFactor);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode NoiseReduceDelegate(
            ref XpeCommonApi.XpeImageBuffer image,
            ref XpeNoiseReduceParams parameters);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode NoiseEstimateSigmaDelegate(
            ref XpeCommonApi.XpeImageBuffer image,
            out float sigma);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode ContrastEnhanceDelegate(
            ref XpeCommonApi.XpeImageBuffer image,
            ref XpeClaheParams parameters);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode EdgeEnhanceDelegate(
            ref XpeCommonApi.XpeImageBuffer image,
            ref XpeUsmParams parameters);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode CalcExposureIndexDelegate(
            ref XpeCommonApi.XpeImageBuffer image,
            ref XpeCommonApi.XpeImageMetadata metadata,
            out float ei,
            out float di);

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
