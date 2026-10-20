using System.Buffers.Binary;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace ImageProcTest.Services.Comparison;

/// <summary>
/// DISPLAY-ONLY reader (QA-A-273, Refs #278). It is used after the contract script has said "comparable" and reads the layout fields of the manifest (path, dtype, width, height, offset_bytes) to draw a
/// decimated copy of an output file. The numbers of a comparison never come from here: the script streams the ROI rows of the original files itself. Window/level, zoom and decimation therefore cannot
/// change a statistic. A manifest this reader cannot interpret simply draws nothing.
/// </summary>
public static class ComparisonDisplayReader
{
    public sealed record Layout(string Path, string Dtype, int ItemSize, long Width, long Height, long OffsetBytes);

    public static Layout? ReadLayout(JsonNode? output, string baseDir)
    {
        if (output is not JsonObject o) return null;
        string? Str(string key) => o[key] is JsonValue v && v.GetValueKind() == JsonValueKind.String ? v.GetValue<string>() : null;
        long? Int(string key) => o[key] is JsonValue v && v.GetValueKind() == JsonValueKind.Number && v.TryGetValue<long>(out var l) ? l : null;
        var rel = Str("path");
        var dtype = Str("dtype");
        var item = dtype switch { "uint16" or "int16" => 2, "uint32" or "int32" or "float32" => 4, _ => 0 };
        var w = Int("width");
        var h = Int("height");
        if (rel is null || item == 0 || w is null or <= 0 || h is null or <= 0) return null;
        var offset = o.ContainsKey("offset_bytes") ? Int("offset_bytes") : 0;
        if (offset is null or < 0) return null;
        var b = System.IO.Path.GetFullPath(baseDir).TrimEnd('\\', '/');
        var p = System.IO.Path.GetFullPath(System.IO.Path.Combine(b, rel));
        if (!p.StartsWith(b + System.IO.Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase)) return null;   // never read outside the manifest folder
        return new Layout(p, dtype!, item, w.Value, h.Value, offset.Value);
    }

    /// <summary>Every <paramref name="step"/>-th pixel of every <paramref name="step"/>-th row, as float64 (each supported format converts without rounding).</summary>
    public static (double[] Pixels, int Width, int Height) ReadDecimated(Layout l, int step)
    {
        var w = (int)((l.Width + step - 1) / step);
        var h = (int)((l.Height + step - 1) / step);
        var pixels = new double[(long)w * h];
        var row = new byte[l.Width * l.ItemSize];
        using var f = new FileStream(l.Path, FileMode.Open, FileAccess.Read, FileShare.Read);
        for (var yy = 0; yy < h; yy++)
        {
            f.Seek(l.OffsetBytes + (long)yy * step * l.Width * l.ItemSize, SeekOrigin.Begin);
            f.ReadExactly(row);
            for (var xx = 0; xx < w; xx++)
            {
                var span = row.AsSpan(xx * step * l.ItemSize, l.ItemSize);
                // each arm is cast to double on its own: a switch over mixed numeric types would otherwise settle on float and lose the low bits of a uint32 (found by the v2 parity fixture)
                pixels[(long)yy * w + xx] = l.Dtype switch
                {
                    "uint16" => (double)BinaryPrimitives.ReadUInt16LittleEndian(span),
                    "int16" => (double)BinaryPrimitives.ReadInt16LittleEndian(span),
                    "uint32" => (double)BinaryPrimitives.ReadUInt32LittleEndian(span),
                    "int32" => (double)BinaryPrimitives.ReadInt32LittleEndian(span),
                    _ => (double)BinaryPrimitives.ReadSingleLittleEndian(span),
                };
            }
        }
        return (pixels, w, h);
    }
}
