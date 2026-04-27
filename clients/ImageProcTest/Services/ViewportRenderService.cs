using System;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace ImageProcTest
{
    internal sealed record ViewportRenderResult(
        WriteableBitmap Bitmap,
        HistogramData Histogram,
        ViewportRenderParams Params);

    internal static class ViewportRenderService
    {
        public static ViewportRenderResult Render(
            ReadOnlySpan<ushort> values,
            int width,
            int height,
            ViewportRenderParams parameters)
        {
            var floatValues = new float[values.Length];
            for (var i = 0; i < values.Length; i++)
            {
                floatValues[i] = values[i];
            }

            return Render(floatValues, width, height, parameters);
        }

        public static ViewportRenderResult Render(
            ReadOnlySpan<float> values,
            int width,
            int height,
            ViewportRenderParams parameters)
        {
            var expectedPixelCount = ValidateDimensions(width, height);
            if (values.Length != expectedPixelCount)
            {
                throw new ArgumentException("Pixel count does not match viewport dimensions.", nameof(values));
            }

            var safeParams = parameters.WithSafeWidth();
            var pixels = new byte[values.Length];
            var sourceMin = float.PositiveInfinity;
            var sourceMax = float.NegativeInfinity;

            for (var i = 0; i < values.Length; i++)
            {
                var value = values[i];
                if (!float.IsFinite(value))
                {
                    continue;
                }

                sourceMin = Math.Min(sourceMin, value);
                sourceMax = Math.Max(sourceMax, value);
            }

            if (!float.IsFinite(sourceMin) || !float.IsFinite(sourceMax))
            {
                sourceMin = 0f;
                sourceMax = 1f;
            }

            var bins = new int[256];
            var histogramRange = Math.Max(float.Epsilon, sourceMax - sourceMin);
            for (var i = 0; i < values.Length; i++)
            {
                var value = float.IsFinite(values[i]) ? values[i] : safeParams.WindowCenter;
                var pixel = ApplyWindowLevel(value, safeParams);
                pixels[i] = pixel;

                var normalized = Math.Clamp((value - sourceMin) / histogramRange, 0f, 1f);
                var binIndex = (int)Math.Clamp(
                    MathF.Floor(normalized * (bins.Length - 1)),
                    0,
                    bins.Length - 1);
                bins[binIndex]++;
            }

            var bitmap = CreateGray8Bitmap(pixels, width, height);
            return new ViewportRenderResult(
                bitmap,
                HistogramData.FromBins(bins, sourceMin, sourceMax),
                safeParams);
        }

        public static ViewportRenderParams AutoFit(ReadOnlySpan<ushort> values)
        {
            if (values.Length == 0)
            {
                return ViewportRenderParams.Default;
            }

            ushort min = ushort.MaxValue;
            ushort max = ushort.MinValue;
            for (var i = 0; i < values.Length; i++)
            {
                min = Math.Min(min, values[i]);
                max = Math.Max(max, values[i]);
            }

            return CreateAutoFit(min, max);
        }

        public static ViewportRenderParams AutoFit(ReadOnlySpan<float> values)
        {
            if (values.Length == 0)
            {
                return ViewportRenderParams.Default;
            }

            var min = float.PositiveInfinity;
            var max = float.NegativeInfinity;
            for (var i = 0; i < values.Length; i++)
            {
                var value = values[i];
                if (!float.IsFinite(value))
                {
                    continue;
                }

                min = Math.Min(min, value);
                max = Math.Max(max, value);
            }

            if (!float.IsFinite(min) || !float.IsFinite(max))
            {
                return ViewportRenderParams.Default;
            }

            return CreateAutoFit(min, max);
        }

        public static IReadOnlyDictionary<string, ViewportRenderParams> Presets { get; } =
            new Dictionary<string, ViewportRenderParams>(StringComparer.OrdinalIgnoreCase)
            {
                ["Chest PA"] = new(2048f, 3000f),
                ["Bone"] = new(1200f, 2500f),
                ["Soft Tissue"] = new(2048f, 1000f),
                ["Lung"] = new(2048f, 4096f),
                ["Pediatric"] = new(1800f, 2000f),
            };

        private static ViewportRenderParams CreateAutoFit(float min, float max)
        {
            var range = Math.Max(1f, max - min);
            return new ViewportRenderParams(min + (range / 2f), range);
        }

        private static byte ApplyWindowLevel(float value, ViewportRenderParams parameters)
        {
            var width = Math.Max(1f, parameters.WindowWidth);
            var normalized = parameters.Lut == ViewportLutType.Sigmoid
                ? 1f / (1f + MathF.Exp(-4f * (value - parameters.WindowCenter) / width))
                : ApplyLinearWindow(value, parameters.WindowCenter, width);
            var pixel = (byte)Math.Clamp(normalized * 255f, 0f, 255f);
            return parameters.Invert ? (byte)(255 - pixel) : pixel;
        }

        private static float ApplyLinearWindow(float value, float center, float width)
        {
            var lower = center - (width / 2f);
            if (value <= lower)
            {
                return 0f;
            }

            var upper = center + (width / 2f);
            if (value >= upper)
            {
                return 1f;
            }

            return (value - lower) / width;
        }

        private static WriteableBitmap CreateGray8Bitmap(byte[] pixels, int width, int height)
        {
            _ = ValidateDimensions(width, height);
            var bitmap = new WriteableBitmap(width, height, 96, 96, PixelFormats.Gray8, null);
            bitmap.WritePixels(new Int32Rect(0, 0, width, height), pixels, width, 0);
            bitmap.Freeze();
            return bitmap;
        }

        private static int ValidateDimensions(int width, int height)
        {
            if (width <= 0)
            {
                throw new ArgumentOutOfRangeException(nameof(width), width, "Viewport width must be positive.");
            }

            if (height <= 0)
            {
                throw new ArgumentOutOfRangeException(nameof(height), height, "Viewport height must be positive.");
            }

            return checked(width * height);
        }
    }
}
