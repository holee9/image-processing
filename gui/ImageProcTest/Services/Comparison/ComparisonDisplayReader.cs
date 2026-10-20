using System.Buffers.Binary;
using System.IO;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace ImageProcTest.Services.Comparison;

/// <summary>
/// DISPLAY-ONLY reader (QA-A-273/273b, Refs #278). It is used after the contract script has said "comparable" and reads the layout fields of the manifest SNAPSHOT (path, dtype, width, height, offset_bytes, sha256)
/// to draw a decimated copy of an output file. The numbers of a comparison never come from here: the script streams the ROI rows of the original files itself. Window/level, zoom and decimation therefore cannot
/// change a statistic. A manifest this reader cannot interpret simply draws nothing. Every read can be cancelled; nothing read is accepted after a cancellation.
///
/// Memory budget (arithmetic, not a promise for every shape). Limits: width and height each at most 2^24 = 16,777,216 (larger shapes get no preview), preview pixels P at most 4,000,000.
///   step = smallest integer with ceil(W/step) * ceil(H/step) &lt;= 4,000,000 (integer arithmetic in long; W*H &lt;= 2^48 cannot overflow a long).
///   Allocations made here: two double[P] preview arrays = 2 * 8 * 4,000,000 = 64,000,000 B; two byte[P] display copies = 8,000,000 B; one row buffer W * itemSize &lt;= 2^24 * 4 = 67,108,864 B at the limit.
///   Total at the limits &lt;= 139,108,864 B (about 133 MiB), for the arrays this class allocates only. The WPF render surface at a high zoom and the runtime's own overhead are NOT included.
/// </summary>
public static class ComparisonDisplayReader
{
    public const long MaxPreviewPixels = 4_000_000;
    public const long MaxSide = 1L << 24;

    public sealed record Layout(string Path, string Dtype, int ItemSize, long Width, long Height, long OffsetBytes, string? Sha256);

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
        if (rel is null || item == 0 || w is null or <= 0 || h is null or <= 0 || w > MaxSide || h > MaxSide) return null;
        var offset = o.ContainsKey("offset_bytes") ? Int("offset_bytes") : 0;
        if (offset is null or < 0) return null;
        var b = System.IO.Path.GetFullPath(baseDir).TrimEnd('\\', '/');
        var p = System.IO.Path.GetFullPath(System.IO.Path.Combine(b, rel));
        if (!p.StartsWith(b + System.IO.Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase)) return null;   // never read outside the manifest folder
        return new Layout(p, dtype!, item, w.Value, h.Value, offset.Value, Str("sha256"));
    }

    /// <summary>The smallest decimation step whose preview has at most <see cref="MaxPreviewPixels"/> pixels (ceil on both axes, long arithmetic).</summary>
    public static int ChooseStep(long width, long height)
    {
        var step = Math.Max(1L, (long)Math.Ceiling(Math.Sqrt((double)width * height / MaxPreviewPixels)));
        while (CeilDiv(width, step) * CeilDiv(height, step) > MaxPreviewPixels) step++;   // the square root is an estimate; this loop makes the bound exact
        return checked((int)step);
    }

    private static long CeilDiv(long a, long b) => (a + b - 1) / b;

    /// <summary>SHA-256 of the whole file, streamed, cancellable. The preview is only drawn when this equals the manifest's recorded value.</summary>
    public static async Task<string> Sha256Async(string path, CancellationToken cancel)
    {
        await using var f = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read, 1 << 16, useAsync: true);
        return Convert.ToHexString(await SHA256.HashDataAsync(f, cancel)).ToLowerInvariant();
    }

    /// <summary>Every <paramref name="step"/>-th pixel of every <paramref name="step"/>-th row, as float64 (each supported format converts without rounding). Checks the token once per row.</summary>
    public static (double[] Pixels, int Width, int Height) ReadDecimated(Layout l, int step, CancellationToken cancel)
    {
        var w = checked((int)CeilDiv(l.Width, step));
        var h = checked((int)CeilDiv(l.Height, step));
        var pixels = new double[(long)w * h];
        var row = new byte[l.Width * l.ItemSize];
        using var f = new FileStream(l.Path, FileMode.Open, FileAccess.Read, FileShare.Read);
        for (var yy = 0; yy < h; yy++)
        {
            cancel.ThrowIfCancellationRequested();
#if XPE_TEST_FAULTS
            // test build only (GUI-C-193): a per-row delay so a cancellation can be made to land inside the preview read. Not compiled into a shipped build.
            if (int.TryParse(Environment.GetEnvironmentVariable("XPE_COMPARISON_TEST_ROW_DELAY_MS"), out var delayMs) && delayMs > 0) Thread.Sleep(delayMs);
#endif
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
        cancel.ThrowIfCancellationRequested();
        return (pixels, w, h);
    }
}
