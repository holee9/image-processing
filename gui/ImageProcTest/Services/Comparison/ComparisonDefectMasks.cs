using System.Buffers.Binary;
using System.IO;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace ImageProcTest.Services.Comparison;

/// <summary>Only explicit, hash-bound map references. A map disagreement is not an audit of corrections actually applied.</summary>
public static class ComparisonDefectMasks
{
    public sealed record Masks(bool[]? Excluded, bool[]? Disagreement, bool DisagreementAvailable, string Status);

    // @MX:ANCHOR: [AUTO] Shared explicit-map boundary for GUI, parameter probe and synthetic checks.
    // @MX:REASON: These callers must all validate the same hashes, XCAL header and sample coordinates.
    public static Masks Read(JsonNode? manifest, string baseDir, long width, long height, int step, CancellationToken cancel)
    {
        if (width <= 0 || height <= 0 || width > ComparisonDisplayReader.MaxSide || height > ComparisonDisplayReader.MaxSide || step <= 0 ||
            ((width + step - 1) / step) * ((height + step - 1) / step) > ComparisonDisplayReader.MaxPreviewPixels)
            throw new ArgumentException("Invalid defect-map sample dimensions.");
        var reasons = new List<string>();
        bool[]? ReadOne(JsonNode? entry, bool xcal, string name)
        {
            if (entry is null) { reasons.Add(name + " map reference unavailable"); return null; }
            try
            {
                string? S(string k) => entry[k] is JsonValue v && v.GetValueKind() == JsonValueKind.String ? v.GetValue<string>() : null;
                var rel = S("path"); var expected = S("sha256");
                if (rel is null || expected is null || expected.Length != 64) throw new InvalidDataException("explicit path/SHA required");
                var root = Path.GetFullPath(baseDir).TrimEnd('\\', '/'); var path = Path.GetFullPath(Path.Combine(root, rel));
                if (!path.StartsWith(root + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("path is outside manifest folder");
                using var f = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
                if (!string.Equals(Hash(f, cancel), expected, StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("SHA-256 mismatch");
                f.Position = 0; long payload = 0;
                if (xcal)
                {
                    var header = new byte[152]; f.ReadExactly(header);
                    uint U(int off) => BinaryPrimitives.ReadUInt32LittleEndian(header.AsSpan(off));
                    var configLength = BinaryPrimitives.ReadUInt64LittleEndian(header.AsSpan(104));
                    var payloadLength = BinaryPrimitives.ReadUInt64LittleEndian(header.AsSpan(112));
                    if (!header.AsSpan(0, 4).SequenceEqual("XCAL"u8) || U(4) != 1 || U(8) != 2 || U(12) != 2 || U(16) != width || U(20) != height ||
                        configLength > (ulong)long.MaxValue - 152 || payloadLength != (ulong)(width * height) ||
                        configLength + payloadLength > (ulong)long.MaxValue - 152 || (ulong)f.Length != 152 + configLength + payloadLength)
                        throw new InvalidDataException("unsupported XCAL defect header/length");
                    if (!Convert.FromHexString(Hash(f, cancel)).AsSpan().SequenceEqual(header.AsSpan(120, 32))) throw new InvalidDataException("XCAL embedded SHA mismatch");
                    payload = checked(152 + (long)configLength);
                }
                else if (f.Length != width * height) throw new InvalidDataException("BPM length must be width*height bytes");
                var sw = checked((int)((width + step - 1) / step)); var sh = checked((int)((height + step - 1) / step));
                var mask = new bool[checked(sw * sh)]; var row = new byte[checked((int)width)];
                for (var y = 0; y < sh; y++)
                {
                    cancel.ThrowIfCancellationRequested(); f.Position = payload + (long)y * step * width; f.ReadExactly(row);
                    for (var x = 0; x < sw; x++) mask[y * sw + x] = row[x * step] != 0;
                }
                return mask;
            }
            catch (Exception e) when (e is IOException or InvalidDataException or UnauthorizedAccessException or ArgumentException or OverflowException)
            { reasons.Add(name + ": " + e.Message); return null; }
        }
        var a = ReadOne(manifest?["engines"]?["A"]?["maps"]?["bpm"], false, "A BPM");
        var b = ReadOne(manifest?["engines"]?["B"]?["maps"]?["defect.xcal"], true, "B XCAL");
        var length = a?.Length ?? b?.Length ?? 0; bool[]? union = length == 0 ? null : new bool[length];
        var both = a is not null && b is not null; bool[]? xor = both ? new bool[length] : null;
        for (var i = 0; i < length; i++) { union![i] = a?[i] == true || b?[i] == true; if (both) xor![i] = a![i] != b![i]; }
        var status = both ? $"Defect-map disagreement: {xor!.Count(x => x)} sample pixels (cyan). Known defect union excluded from region scale/candidates." :
            "Defect-map disagreement overlay off: " + string.Join("; ", reasons) + ". " + (union is null ? "Unknown defects cannot be excluded." : "Only the available known map is excluded.");
        return new Masks(union, xor, both, status + " This is map membership, not a per-pixel correction-application audit.");
    }

    private static string Hash(Stream stream, CancellationToken cancel)
    {
        using var hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256); var buffer = new byte[65536]; int n;
        while ((n = stream.Read(buffer)) > 0) { cancel.ThrowIfCancellationRequested(); hash.AppendData(buffer, 0, n); }
        cancel.ThrowIfCancellationRequested(); return Convert.ToHexString(hash.GetHashAndReset());
    }
}
