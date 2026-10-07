// GUI-C-232: the corrected float image of the last Run Preprocessing, and the two files the File menu writes from it.
using System.IO;
using System.Security.Cryptography;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace ImageProcTest.Services;

/// <summary>
/// The corrected float32 image the preprocess stage produced for the frame the user has open. <paramref name="RawKey"/> is that frame's raw pixel array: the image is the answer for THAT frame only,
/// so a later load (a different array) makes it unavailable instead of letting an old result be saved under a new image's name.
/// </summary>
public sealed record CorrectedImage(float[] Floats, int Width, int Height, ushort[] RawKey);

/// <summary>A backend that can hand out the float image its last Run Preprocessing produced.</summary>
public interface ICorrectedImageSource
{
    /// <summary>The corrected image of the last preprocess run that Applied, or null.</summary>
    CorrectedImage? Corrected { get; }
}

/// <summary>The result of a save: where, how big, and the SHA-256 of the bytes written (what the user compares with a reference file).</summary>
public sealed record SavedFile(string Path, long Bytes, string Sha256);

/// <summary>Writes the corrected image. Free of the view model so it can be tested on its own.</summary>
public static class CorrectedImageWriter
{
    /// <summary>
    /// The corrected values exactly as the module produced them: raw float32, little-endian, width x height x 4 bytes, no header. This is the file to compare, byte for byte, with a first-stage
    /// reference image of the same format.
    /// </summary>
    public static SavedFile WriteFloat32(string path, float[] floats)
    {
        ArgumentNullException.ThrowIfNull(floats);
        var bytes = new byte[floats.Length * sizeof(float)];
        Buffer.BlockCopy(floats, 0, bytes, 0, bytes.Length);
        File.WriteAllBytes(path, bytes);
        return new SavedFile(path, bytes.Length, Convert.ToHexString(SHA256.HashData(bytes)).ToLowerInvariant());
    }

    /// <summary>
    /// A 16-bit grayscale PNG to LOOK at: the 1st to the 99th percentile of the corrected values stretched linearly to 0..65535. It is a display file, not data: the stretch discards the absolute
    /// values and clips the extremes (the float file is the data).
    /// </summary>
    public static SavedFile WritePng16(string path, float[] floats, int width, int height)
    {
        ArgumentNullException.ThrowIfNull(floats);
        if (floats.Length != checked(width * height))
        {
            throw new ArgumentException($"{floats.Length} values do not fill {width}x{height}.", nameof(floats));
        }

        var (low, high) = Percentiles(floats, 0.01, 0.99);
        var scale = high > low ? 65535.0 / (high - low) : 0.0;
        var pixels = new ushort[floats.Length];
        for (var i = 0; i < pixels.Length; i++)
        {
            var value = (floats[i] - low) * scale;
            pixels[i] = float.IsFinite(floats[i]) ? (value <= 0 ? (ushort)0 : value >= 65535 ? ushort.MaxValue : (ushort)value) : (ushort)0;
        }

        var bitmap = BitmapSource.Create(width, height, 96, 96, PixelFormats.Gray16, null, pixels, width * 2);
        bitmap.Freeze();
        var encoder = new PngBitmapEncoder();
        encoder.Frames.Add(BitmapFrame.Create(bitmap));
        using (var stream = File.Create(path))
        {
            encoder.Save(stream);
        }

        var written = File.ReadAllBytes(path);
        return new SavedFile(path, written.Length, Convert.ToHexString(SHA256.HashData(written)).ToLowerInvariant());
    }

    /// <summary>The values at two fractions of the sorted finite values (0.01 and 0.99 for the 1st and 99th percentile).</summary>
    public static (float Low, float High) Percentiles(float[] values, double lowFraction, double highFraction)
    {
        var finite = values.Where(float.IsFinite).ToArray();
        if (finite.Length == 0)
        {
            return (0f, 0f);
        }

        Array.Sort(finite);
        float At(double fraction) => finite[(int)Math.Clamp(Math.Round(fraction * (finite.Length - 1)), 0, finite.Length - 1)];
        return (At(lowFraction), At(highFraction));
    }
}
