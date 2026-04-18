using System;
using System.Buffers.Binary;
using System.IO;
using System.Security.Cryptography;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace ImageProcTest
{
    internal static class RawPreviewService
    {
        private static readonly (int Width, int Height)[] KnownDimensions =
        [
            (4096, 4096),
            (3584, 3584),
            (3328, 3328),
            (3072, 3072),
            (2816, 2816),
            (2560, 2560),
            (2048, 2048),
            (1792, 1792),
            (1536, 1536),
            (1024, 1024),
            (4096, 3072),
            (3072, 4096),
            (3072, 2048),
            (2048, 3072),
            (2304, 1792),
            (1792, 2304)
        ];

        public static RawPreviewResult LoadUInt16Preview(string path, int maxPreviewSide = 1024)
        {
            var file = new FileInfo(path);
            if (!file.Exists)
            {
                throw new FileNotFoundException("Raw file not found.", path);
            }

            if (file.Length <= 0 || file.Length % 2 != 0)
            {
                throw new InvalidDataException("Only even-sized uint16 raw files are supported.");
            }

            var (width, height) = InferDimensions(file.Length);
            var stride = Math.Max(1, Math.Max(
                (int)Math.Ceiling(width / (double)maxPreviewSide),
                (int)Math.Ceiling(height / (double)maxPreviewSide)));
            var previewWidth = (int)Math.Ceiling(width / (double)stride);
            var previewHeight = (int)Math.Ceiling(height / (double)stride);

            var samples = SamplePreview(path, width, height, stride, previewWidth, previewHeight, out var min, out var max);
            var bitmap = CreateGray8Bitmap(samples, previewWidth, previewHeight, min, max);

            return new RawPreviewResult(
                path,
                file.Length,
                width,
                height,
                previewWidth,
                previewHeight,
                stride,
                min,
                max,
                ComputeSha256(path),
                samples,
                bitmap);
        }

        private static (int Width, int Height) InferDimensions(long byteLength)
        {
            var pixels = byteLength / 2;
            foreach (var dimension in KnownDimensions)
            {
                if ((long)dimension.Width * dimension.Height == pixels)
                {
                    return dimension;
                }
            }

            var square = (long)Math.Sqrt(pixels);
            if (square * square == pixels && square <= int.MaxValue)
            {
                return ((int)square, (int)square);
            }

            throw new InvalidDataException($"Cannot infer raw dimensions from {byteLength} bytes. Add explicit dimensions before using this file.");
        }

        internal static WriteableBitmap CreateGray8Bitmap(
            ReadOnlySpan<ushort> values,
            int width,
            int height,
            ushort min,
            ushort max)
        {
            if (values.Length != checked(width * height))
            {
                throw new ArgumentException("Pixel count does not match bitmap dimensions.", nameof(values));
            }

            var pixels = new byte[values.Length];
            var range = Math.Max(1, max - min);

            for (var i = 0; i < values.Length; i++)
            {
                pixels[i] = (byte)Math.Clamp(((values[i] - min) * 255) / range, 0, 255);
            }

            return CreateGray8Bitmap(pixels, width, height);
        }

        internal static WriteableBitmap CreateGray8Bitmap(
            ReadOnlySpan<float> values,
            int width,
            int height,
            out float min,
            out float max)
        {
            if (values.Length != checked(width * height))
            {
                throw new ArgumentException("Pixel count does not match bitmap dimensions.", nameof(values));
            }

            min = float.PositiveInfinity;
            max = float.NegativeInfinity;
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
                min = 0;
                max = 1;
            }

            var pixels = new byte[values.Length];
            var range = Math.Max(1.0f, max - min);

            for (var i = 0; i < values.Length; i++)
            {
                var value = float.IsFinite(values[i]) ? values[i] : min;
                pixels[i] = (byte)Math.Clamp(((value - min) * 255.0f) / range, 0.0f, 255.0f);
            }

            return CreateGray8Bitmap(pixels, width, height);
        }

        internal static WriteableBitmap CreateGray8Bitmap(
            ReadOnlySpan<float> values,
            int width,
            int height,
            float windowMin,
            float windowMax)
        {
            if (values.Length != checked(width * height))
            {
                throw new ArgumentException("Pixel count does not match bitmap dimensions.", nameof(values));
            }

            var pixels = new byte[values.Length];
            var range = Math.Max(1.0f, windowMax - windowMin);

            for (var i = 0; i < values.Length; i++)
            {
                var value = float.IsFinite(values[i]) ? values[i] : windowMin;
                pixels[i] = (byte)Math.Clamp(((value - windowMin) * 255.0f) / range, 0.0f, 255.0f);
            }

            return CreateGray8Bitmap(pixels, width, height);
        }

        private static WriteableBitmap CreateGray8Bitmap(byte[] pixels, int width, int height)
        {
            var bitmap = new WriteableBitmap(width, height, 96, 96, PixelFormats.Gray8, null);
            bitmap.WritePixels(new Int32Rect(0, 0, width, height), pixels, width, 0);
            bitmap.Freeze();
            return bitmap;
        }

        private static ushort[] SamplePreview(
            string path,
            int width,
            int height,
            int sampleStride,
            int previewWidth,
            int previewHeight,
            out ushort min,
            out ushort max)
        {
            min = ushort.MaxValue;
            max = ushort.MinValue;
            var rowBytes = checked(width * 2);
            var row = new byte[rowBytes];
            var samples = new ushort[checked(previewWidth * previewHeight)];

            using var stream = File.OpenRead(path);
            for (var py = 0; py < previewHeight; py++)
            {
                var sourceY = Math.Min(py * sampleStride, height - 1);
                stream.Position = checked((long)sourceY * rowBytes);
                ReadFullRow(stream, row);

                for (var px = 0; px < previewWidth; px++)
                {
                    var sourceX = Math.Min(px * sampleStride, width - 1);
                    var value = BinaryPrimitives.ReadUInt16LittleEndian(row.AsSpan(sourceX * 2, 2));
                    samples[(py * previewWidth) + px] = value;
                    min = Math.Min(min, value);
                    max = Math.Max(max, value);
                }
            }

            return samples;
        }

        private static void ReadFullRow(Stream stream, byte[] row)
        {
            var offset = 0;
            while (offset < row.Length)
            {
                var read = stream.Read(row, offset, row.Length - offset);
                if (read == 0)
                {
                    throw new EndOfStreamException("Unexpected end of raw file.");
                }

                offset += read;
            }
        }

        private static string ComputeSha256(string path)
        {
            using var stream = File.OpenRead(path);
            var hash = SHA256.HashData(stream);
            return Convert.ToHexString(hash).ToLowerInvariant();
        }
    }
}
