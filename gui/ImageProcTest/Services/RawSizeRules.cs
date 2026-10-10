// GUI-C-232b: the rules that decide whether a raw file may be opened at the size the settings give. Pure (no WPF), so the integration tests run them without any external data.
using System.Buffers.Binary;
using System.IO;

namespace ImageProcTest.Services;

public static class RawSizeRules
{
    /// <summary>
    /// GUI-C-232b (leader decision, Codex #165): the size is never guessed. The file must be exactly width x height x 2 bytes; any other length is refused, with the size that was needed, the bytes the file has,
    /// and how to set the size. A matching length does not prove the shape or the absence of a header (a 3072x3072 file and a 2048x4608 file are the same length): that is the operator's to know.
    /// </summary>
    public static void CheckLength(long fileBytes, int width, int height)
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
    public static void CheckMapSizes(int width, int height, params (string Kind, string Directory)[] maps)
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
}
