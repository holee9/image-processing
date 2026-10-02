// #225 row 9 (GUI-C-196 M2): the native working image of the enhance_basic stage. The logic is in EnhanceBasicStage; this holds the buffer and makes the calls.
using System.Runtime.InteropServices;

namespace ImageProcTest.Services.Native;

/// <summary>Opens a float32 buffer from xpe_common (xpe_alloc_image) and runs the module's functions on it in place.</summary>
internal sealed class NativeEnhanceBasicBackend : IEnhanceBasicBackend
{
    public IEnhanceImage Open(ushort[] input, int width, int height) => new NativeEnhanceImage(input, width, height);

    private sealed class NativeEnhanceImage : IEnhanceImage
    {
        private XpeImageBufferNative _buffer;
        private readonly int _count;
        private bool _allocated;

        public NativeEnhanceImage(ushort[] input, int width, int height)
        {
            _count = checked(width * height);
            var code = XpeCommonNative.xpe_alloc_image((uint)width, (uint)height, XpePixelFormatNative.Float32, out _buffer);
            if (code != 0)
            {
                throw new InvalidOperationException($"xpe_alloc_image(float32) returned {code}.");
            }

            _allocated = true;
            var floats = new float[_count];
            for (var i = 0; i < _count; i++)
            {
                floats[i] = input[i];
            }

            Marshal.Copy(floats, 0, _buffer.Data, _count);
        }

        public int LogTransform(float normFactor) => XpeEnhanceBasicNative.xpe_log_transform(ref _buffer, normFactor);

        public int NoiseReduceBilateral(float sigmaSpace, float sigmaRange)
        {
            var parameters = new XpeNoiseReduceParamsNative
            {
                Mode = 0,
                SigmaSpace = sigmaSpace,
                SigmaRange = sigmaRange,
                SearchWindow = 21,   // the module's NLM defaults; unused by the bilateral mode but a valid struct
                PatchSize = 7,
                HParam = 10.0f,
            };
            return XpeEnhanceBasicNative.xpe_noise_reduce(ref _buffer, ref parameters);
        }

        public int ContrastEnhance(float clipLimit, int tileWidth, int tileHeight)
        {
            var parameters = new XpeClaheParamsNative { ClipLimit = clipLimit, TileWidth = tileWidth, TileHeight = tileHeight };
            return XpeEnhanceBasicNative.xpe_contrast_enhance(ref _buffer, ref parameters);
        }

        public int EdgeEnhance(float amount, float radius, float threshold)
        {
            var parameters = new XpeUsmParamsNative { Amount = amount, Radius = radius, Threshold = threshold };
            return XpeEnhanceBasicNative.xpe_edge_enhance(ref _buffer, ref parameters);
        }

        public long CountNonFinite()
        {
            var floats = ReadFloats();
            long bad = 0;
            foreach (var value in floats)
            {
                // exponent bits all ones: NaN or infinity
                if ((BitConverter.SingleToInt32Bits(value) & 0x7F800000) == 0x7F800000)
                {
                    bad++;
                }
            }

            return bad;
        }

        public float[] ReadFloats()
        {
            var floats = new float[_count];
            Marshal.Copy(_buffer.Data, floats, 0, _count);
            return floats;
        }

        public void Dispose()
        {
            if (_allocated)
            {
                XpeCommonNative.xpe_free_image(ref _buffer);
                _allocated = false;
            }
        }
    }
}
