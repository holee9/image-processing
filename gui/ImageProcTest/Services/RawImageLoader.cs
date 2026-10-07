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
    /// <summary>
    /// GUI-C-232 (B): the file's length decides the size, never the other way round. A length that is exactly width x height x 2 opens as the settings say. Any other length used to open
    /// silently (a longer file lost its end, a shorter one failed with a size nobody asked for); now it either opens at the one size that fits (a square image, the usual detector) and the
    /// notice says so, or it is refused with the numbers, because guessing between several rectangles would show a plausible wrong picture.
    /// </summary>
    internal static (int Width, int Height, string? Notice) DecideSize(long fileBytes, int settingsWidth, int settingsHeight)
    {
        var expected = checked((long)settingsWidth * settingsHeight * 2);
        if (fileBytes == expected)
        {
            return (settingsWidth, settingsHeight, null);
        }

        if (fileBytes > 0 && fileBytes % 2 == 0)
        {
            var pixels = fileBytes / 2;
            var side = (long)Math.Round(Math.Sqrt(pixels));
            if (side * side == pixels && side <= int.MaxValue)
            {
                return ((int)side, (int)side, $"size from the file length: {fileBytes} bytes = {side}x{side}x2, the settings said {settingsWidth}x{settingsHeight} ({expected} bytes)");
            }
        }

        var advice = "Set the right width and height (rawWidth/rawHeight in the settings, or --automation-width/--automation-height) and open it again.";
        throw new InvalidDataException(fileBytes < expected
            ? $"Raw file is too small. Expected at least {expected} bytes, got {fileBytes}. The length is not a square 16-bit image either. {advice}"
            : $"Raw file has {fileBytes} bytes, more than the {expected} bytes of {settingsWidth}x{settingsHeight} 16-bit, and the length is not a square image either. {advice}");
    }

    private static LoadedImageFrame LoadRaw(string path, AppSettings settings)
    {
        if (settings.RawWidth <= 0 || settings.RawHeight <= 0)
        {
            throw new InvalidOperationException("Raw width and height must be positive.");
        }

        var data = File.ReadAllBytes(path);
        var (width, height, sizeNotice) = DecideSize(data.Length, settings.RawWidth, settings.RawHeight);

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
            Summary = $"RAW {width}x{height}, min={minValue}, max={maxValue}, bytes={data.Length}" + (sizeNotice is null ? string.Empty : $" [{sizeNotice}]"),
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
            BitsStored = 16,
            SizeNotice = sizeNotice
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
