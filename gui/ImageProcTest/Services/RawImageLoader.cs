using System.Buffers.Binary;
using System.IO;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using ImageProcTest.Models;

namespace ImageProcTest.Services;

public sealed class RawImageLoader
{
    // @MX:ANCHOR: [AUTO] Entry point for all raw image loading; routes by file extension; unsupported types return placeholder preview
    // @MX:REASON: Called by MockXpeBackend.LoadRawImage and RealXpeBackend.LoadRawImage; fan_in >= 2 backends
    public LoadedImageFrame Load(string path, AppSettings settings)
    {
        var extension = Path.GetExtension(path).ToLowerInvariant();
        return extension switch
        {
            ".raw" => LoadRaw(path, settings),
            _ => LoadUnsupported(path)
        };
    }

    // @MX:NOTE: [AUTO] Two-pass pixel scan: pass 1 collects min/max for normalization range, pass 2 maps to 8-bit preview; merging passes would require a full pixel buffer copy
    private static LoadedImageFrame LoadRaw(string path, AppSettings settings)
    {
        if (settings.RawWidth <= 0 || settings.RawHeight <= 0)
        {
            throw new InvalidOperationException("Raw width and height must be positive.");
        }

        var data = File.ReadAllBytes(path);
        var width = settings.RawWidth;
        var height = settings.RawHeight;
        RawSizeRules.CheckLength(data.Length, width, height);
        RawSizeRules.CheckMapSizes(width, height, ("Offset", settings.OffsetCalibrationDirectory), ("Gain", settings.GainCalibrationDirectory), ("Defect", settings.DefectCalibrationDirectory));

        ushort minValue = ushort.MaxValue;
        ushort maxValue = ushort.MinValue;
        var rawPixels = new ushort[width * height];
        var grayscale = new byte[width * height];

        for (var i = 0; i < grayscale.Length; i++)
        {
            var sample = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(i * 2, 2));
            rawPixels[i] = sample;
            if (sample < minValue)
            {
                minValue = sample;
            }

            if (sample > maxValue)
            {
                maxValue = sample;
            }
        }

        var scale = Math.Max(1, maxValue - minValue);
        for (var i = 0; i < grayscale.Length; i++)
        {
            var sample = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(i * 2, 2));
            grayscale[i] = (byte)(((sample - minValue) * 255) / scale);
        }

        var preview = BitmapSource.Create(
            width,
            height,
            96,
            96,
            PixelFormats.Gray8,
            null,
            grayscale,
            width);
        preview.Freeze();

        return new LoadedImageFrame
        {
            Preview = preview,
            ProcessedPreview = preview,
            Summary = $"RAW {width}x{height}, min={minValue}, max={maxValue}, bytes={data.Length}",
            MetadataText =
                $"Source: {path}{Environment.NewLine}" +
                $"Kind: Raw frame{Environment.NewLine}" +
                $"Pixel format: {settings.RawPixelFormat}{Environment.NewLine}" +
                $"Dimensions: {width}x{height}{Environment.NewLine}" +
                $"Min/Max: {minValue}/{maxValue}{Environment.NewLine}" +
                $"Offset calibration dir: {settings.OffsetCalibrationDirectory}{Environment.NewLine}" +
                $"Gain calibration dir: {settings.GainCalibrationDirectory}{Environment.NewLine}" +
                $"Defect calibration dir: {settings.DefectCalibrationDirectory}",
            RawPixels = rawPixels,
            Width = width,
            Height = height,
            BitsStored = 16
        };
    }

    private static LoadedImageFrame LoadUnsupported(string path)
    {
        const int size = 256;
        var pixels = new byte[size * size];
        for (var y = 0; y < size; y++)
        {
            for (var x = 0; x < size; x++)
            {
                pixels[(y * size) + x] = (byte)(((x / 16) + (y / 16)) % 2 == 0 ? 48 : 112);
            }
        }

        var preview = BitmapSource.Create(
            size,
            size,
            96,
            96,
            PixelFormats.Gray8,
            null,
            pixels,
            size);
        preview.Freeze();

        return new LoadedImageFrame
        {
            Preview = preview,
            ProcessedPreview = preview,
            Summary = $"Unsupported extension '{Path.GetExtension(path)}'. Showing placeholder preview only.",
            MetadataText =
                $"Source: {path}{Environment.NewLine}" +
                "GUI-S0 scope accepts raw binary image inputs only (*.raw)." + Environment.NewLine +
                "Real DICOM read/write remains owned by xpe_dicom.dll in Phase 1b.",
            Width = size,
            Height = size,
            BitsStored = 8
        };
    }
}
