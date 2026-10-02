// #225 row 9 (GUI-C-196 M7): the display module behind IBaselineDisplayBackend. The step's rules and its non-finite counting are in BaselineDisplayStage; this only
// holds the float32 buffer and makes the calls, with the same native functions, in the same order, as the ordinary display pipeline.
using System.Runtime.InteropServices;

namespace ImageProcTest.Services.Native;

internal sealed class NativeBaselineDisplayBackend : IBaselineDisplayBackend
{
    public IBaselineDisplayImage Open(ushort[] input, int width, int height) => new NativeDisplayImage(input, width, height);

    private sealed class NativeDisplayImage : IBaselineDisplayImage
    {
        private XpeImageBufferNative _image;
        private readonly int _count;
        private bool _allocated;

        public NativeDisplayImage(ushort[] input, int width, int height)
        {
            _count = checked(width * height);
            var code = XpeCommonNative.xpe_alloc_image((uint)width, (uint)height, XpePixelFormatNative.Float32, out _image);
            if (code < 0)
            {
                throw new InvalidOperationException($"xpe_alloc_image failed with XPE error code {code}.");
            }

            _allocated = true;
            var floats = new float[_count];
            for (var i = 0; i < _count; i++)
            {
                floats[i] = input[i];
            }

            Marshal.Copy(floats, 0, _image.Data, _count);
        }

        public int ApplyModality(float slope, float intercept)
        {
            var modality = new XpeModalityLutParamsNative
            {
                Mode = 0,
                RescaleSlope = slope,
                RescaleIntercept = intercept,
                LutData = IntPtr.Zero,
                LutLength = 0,
                LutFirstMapped = 0,
                LutBitsStored = 16,
            };
            return XpeDisplayNative.xpe_apply_modality_lut(ref _image, ref modality);
        }

        public int ApplyVoi(float center, float width)
        {
            // Mode 0 is the linear VOI (ToNativeVoiMode("Linear")); the baseline fixes it (BaselineParameters.VoiLutMode).
            var voi = new XpeVoiLutParamsNative { Mode = 0, Center = center, Width = width, MinOut = 0.0f, MaxOut = 1.0f };
            return XpeDisplayNative.xpe_apply_voi_lut(ref _image, ref voi);
        }

        public int ApplyPresentation(bool gsdfEnabled)
        {
            var presentation = XpePresentationLutParamsNative.CreateLinear(gsdfEnabled);
            return XpeDisplayNative.xpe_apply_presentation_lut(ref _image, ref presentation);
        }

        public float[] ReadFloats()
        {
            var floats = new float[_count];
            Marshal.Copy(_image.Data, floats, 0, _count);
            return floats;
        }

        public ushort[] ReadUInt16()
        {
            var signed = new short[_count];
            Marshal.Copy(_image.Data, signed, 0, _count);
            var pixels = new ushort[_count];
            Buffer.BlockCopy(signed, 0, pixels, 0, _count * sizeof(ushort));
            return pixels;
        }

        public void Dispose()
        {
            if (_allocated)
            {
                XpeCommonNative.xpe_free_image(ref _image);
                _allocated = false;
            }
        }
    }
}
