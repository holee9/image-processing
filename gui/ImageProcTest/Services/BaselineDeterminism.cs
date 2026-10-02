// #225 row 9 (GUI-C-196 M1): the comparison the Deterministic Baseline decides by. Free of WPF so the integration tests link it.
using System.Security.Cryptography;

namespace ImageProcTest.Services;

/// <summary>What <see cref="BaselineDeterminism.Compare"/> found when it laid two pixel arrays side by side.</summary>
/// <param name="Identical">True only when the lengths are equal and every pixel is equal.</param>
/// <param name="LengthA">Pixels in the first array.</param>
/// <param name="LengthB">Pixels in the second array.</param>
/// <param name="FirstIndex">Index of the first pixel that differs; when only the lengths differ, the shorter length; -1 when identical.</param>
/// <param name="DifferentCount">Number of indexes below the shorter length whose pixels differ.</param>
/// <param name="MaxAbsDifference">The largest absolute difference among those pixels (0 when none differ).</param>
public sealed record PixelDifference(bool Identical, int LengthA, int LengthB, int FirstIndex, long DifferentCount, int MaxAbsDifference);

/// <summary>
/// Bit-exact comparison and hashing of 16-bit pixel arrays (the leader's definition of "deterministic": two runs, DeterminismRMSE = 0).
/// The detail it returns when the arrays differ (where they first differ, how many pixels, by how much) is DIAGNOSTIC: it helps find the
/// stage that is not deterministic and never relaxes the verdict.
/// </summary>
public static class BaselineDeterminism
{
    public static PixelDifference Compare(ushort[] a, ushort[] b)
    {
        ArgumentNullException.ThrowIfNull(a);
        ArgumentNullException.ThrowIfNull(b);

        var common = Math.Min(a.Length, b.Length);
        var first = -1;
        long different = 0;
        var maxAbs = 0;
        for (var i = 0; i < common; i++)
        {
            if (a[i] == b[i])
            {
                continue;
            }

            if (first < 0)
            {
                first = i;
            }

            different++;
            var delta = Math.Abs(a[i] - b[i]);
            if (delta > maxAbs)
            {
                maxAbs = delta;
            }
        }

        if (a.Length != b.Length && first < 0)
        {
            first = common;   // the common part is equal; the arrays diverge where the shorter one ends
        }

        return new PixelDifference(a.Length == b.Length && different == 0, a.Length, b.Length, first, different, maxAbs);
    }

    /// <summary>SHA-256 of the pixels as little-endian 16-bit words, lower-case hex. The byte order is fixed so a hash means the same on every machine.</summary>
    public static string Sha256Hex(ushort[] pixels)
    {
        ArgumentNullException.ThrowIfNull(pixels);

        var bytes = new byte[pixels.Length * 2];
        for (var i = 0; i < pixels.Length; i++)
        {
            bytes[2 * i] = (byte)(pixels[i] & 0xFF);
            bytes[(2 * i) + 1] = (byte)(pixels[i] >> 8);
        }

        return Convert.ToHexString(SHA256.HashData(bytes)).ToLowerInvariant();
    }
}
