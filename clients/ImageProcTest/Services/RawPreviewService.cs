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

            var (min, max) = ScanMinMax(path, width, height, stride, previewWidth, previewHeight);
            var pixels = RenderPreview(path, width, height, stride, previewWidth, previewHeight, min, max);
            var bitmap = new WriteableBitmap(previewWidth, previewHeight, 96, 96, PixelFormats.Gray8, null);
            bitmap.WritePixels(new Int32Rect(0, 0, previewWidth, previewHeight), pixels, previewWidth, 0);
            bitmap.Freeze();

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

        private static (ushort Min, ushort Max) ScanMinMax(
            string path,
            int width,
            int height,
            int sampleStride,
            int previewWidth,
            int previewHeight)
        {
            ushort min = ushort.MaxValue;
            ushort max = ushort.MinValue;
            var rowBytes = checked(width * 2);
            var row = new byte[rowBytes];

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
                    min = Math.Min(min, value);
                    max = Math.Max(max, value);
                }
            }

            return (min, max);
        }

        private static byte[] RenderPreview(
            string path,
            int width,
            int height,
            int sampleStride,
            int previewWidth,
            int previewHeight,
            ushort min,
            ushort max)
        {
            var rowBytes = checked(width * 2);
            var row = new byte[rowBytes];
            var pixels = new byte[checked(previewWidth * previewHeight)];
            var range = Math.Max(1, max - min);

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
                    pixels[(py * previewWidth) + px] = (byte)Math.Clamp(((value - min) * 255) / range, 0, 255);
                }
            }

            return pixels;
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
