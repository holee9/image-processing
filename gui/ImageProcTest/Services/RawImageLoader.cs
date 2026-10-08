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
    /// GUI-C-232b (leader decision, Codex #165): the size is never guessed. The file must be exactly width x height x 2 bytes; any other length is refused, with the size that was needed, the bytes the file has,
    /// and how to set the size. A matching length does not prove the shape or the absence of a header (a 3072x3072 file and a 2048x4608 file are the same length): that is the operator's to know.
    /// </summary>
    internal static void CheckLength(long fileBytes, int width, int height)
    {
        var expected = checked((long)width * height * 2);
        if (fileBytes == expected)
        {
            return;
        }

        throw new InvalidDataException(
            (fileBytes < expected
                ? $"Raw file is too small. Expected at least {expected} bytes, got {fileBytes}."
                : $"Raw file has {fileBytes} bytes, more than the {expected} bytes of {width}x{height} 16-bit.") +
            $" The size is set in the settings as rawWidth/rawHeight ({width}x{height} now), or with --automation-width/--automation-height; the file is not opened until its length is exactly width x height x 2.");
    }

    /// <summary>
    /// Maps loaded from the calibration folders carry their own size in the header (xcal_format.h, width and height at byte 16 and 20). When a map is there and its size differs from the image's, the image is not
    /// opened: the stage would refuse the pair later with only an error code. A folder without a readable map is not an error here (the stage reports that itself).
    /// </summary>
    internal static void CheckMapSizes(int width, int height, params (string Kind, string Directory)[] maps)
    {
        foreach (var (kind, directory) in maps)
        {
            var path = System.IO.Path.Combine(directory ?? string.Empty, kind.ToLowerInvariant() + ".xcal");
            if (!File.Exists(path))
            {
                continue;
            }

            Span<byte> header = stackalloc byte[24];
            using (var stream = File.OpenRead(path))
            {
                if (stream.Read(header) < 24 || header[0] != (byte)'X' || header[1] != (byte)'C' || header[2] != (byte)'A' || header[3] != (byte)'L')
                {
                    continue;
                }
            }

            var mapWidth = BinaryPrimitives.ReadUInt32LittleEndian(header[16..]);
            var mapHeight = BinaryPrimitives.ReadUInt32LittleEndian(header[20..]);
            if (mapWidth != (uint)width || mapHeight != (uint)height)
            {
                throw new InvalidDataException($"The {kind.ToLowerInvariant()} calibration map {path} is {mapWidth}x{mapHeight}, but the raw image is {width}x{height}. Open an image of the map's size, or point the calibration folder at maps made for {width}x{height}.");
            }
        }
    }

    private static LoadedImageFrame LoadRaw(string path, AppSettings settings)
    {
        if (settings.RawWidth <= 0 || settings.RawHeight <= 0)
        {
            throw new InvalidOperationException("Raw width and height must be positive.");
        }

        var data = File.ReadAllBytes(path);
        var width = settings.RawWidth;
        var height = settings.RawHeight;
        CheckLength(data.Length, width, height);
        CheckMapSizes(width, height, ("Offset", settings.OffsetCalibrationDirectory), ("Gain", settings.GainCalibrationDirectory), ("Defect", settings.DefectCalibrationDirectory));

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
