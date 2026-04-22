using System.Runtime.InteropServices;

namespace ImageProcTest.PInvokeWrappers
{
    internal enum XpeModalityLutMode : int
    {
        Linear = 0,
        Table = 1
    }

    internal enum XpeVoiLutMode : int
    {
        Linear = 0,
        LinearExact = 1,
        Sigmoid = 2
    }

    internal enum XpeBodyPart : int
    {
        Bone = 0,
        Lung = 1,
        Abdomen = 2,
        Head = 3
    }

    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct XpeModalityLutParams
    {
        public XpeModalityLutMode Mode;
        public float RescaleSlope;
        public float RescaleIntercept;
        public IntPtr LutData;
        public uint LutLength;
        public int LutFirstMapped;
        public uint LutBitsStored;

        public static XpeModalityLutParams Identity => new()
        {
            Mode = XpeModalityLutMode.Linear,
            RescaleSlope = 1.0f,
            RescaleIntercept = 0.0f,
            LutData = IntPtr.Zero,
            LutLength = 0,
            LutFirstMapped = 0,
            LutBitsStored = 16
        };
    }

    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct XpeVoiLutParams
    {
        public XpeVoiLutMode Mode;
        public float Center;
        public float Width;
        public float MinOut;
        public float MaxOut;

        public static XpeVoiLutParams UnitWindow => new()
        {
            Mode = XpeVoiLutMode.Linear,
            Center = 0.5f,
            Width = 1.0f,
            MinOut = 0.0f,
            MaxOut = 1.0f
        };
    }

    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct XpePresentationLutParams
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 1024)]
        public ushort[] LutData;
        public int GsdfEnabled;

        public static XpePresentationLutParams LinearUInt16()
        {
            var lut = new ushort[1024];
            for (var i = 0; i < lut.Length; i++)
            {
                lut[i] = (ushort)Math.Round(i / 1023.0 * ushort.MaxValue);
            }

            return new XpePresentationLutParams
            {
                LutData = lut,
                GsdfEnabled = 0
            };
        }
    }

    internal static class XpeDisplayWrapper
    {
        public const string DllName = "xpe_display.dll";

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate IntPtr VersionDelegate();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode ApplyModalityLutDelegate(
            ref XpeCommonApi.XpeImageBuffer image,
            ref XpeModalityLutParams parameters);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode ApplyVoiLutDelegate(
            ref XpeCommonApi.XpeImageBuffer image,
            ref XpeVoiLutParams parameters);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode VoiPresetCreateDelegate(
            ref XpeVoiLutParams parameters,
            XpeBodyPart bodyPart);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode ApplyPresentationLutDelegate(
            ref XpeCommonApi.XpeImageBuffer image,
            ref XpePresentationLutParams parameters);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode GsdfCalibrateDelegate(
            IntPtr luminanceValues,
            uint count,
            ref XpePresentationLutParams outParameters);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode RawPointerDelegate(
            IntPtr first,
            IntPtr second);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode RawGsdfCalibrateDelegate(
            IntPtr luminanceValues,
            uint count,
            IntPtr outParameters);

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
