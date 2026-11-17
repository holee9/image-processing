namespace ImageProcTest.Services.Comparison;

/// <summary>Display-sample regions, not image-quality verdicts. ROI uses source coordinates; area uses sample pixels.</summary>
public static class ComparisonMarkers
{
    public const int DisplayLimit = 20;

    public sealed record Region(int X, int Y, int Width, int Height, int Area, double Score, double MeanDifference, int Step)
    {
        public int SourceX => checked(X * Step);
        public int SourceY => checked(Y * Step);
        public override string ToString() => $"({SourceX},{SourceY}) · {Area} sample px · mean {MeanDifference:G6}";
    }
    public sealed record Prepared(double[] Differences, bool[] Eligible, int Width, int Height, int Step, double Median, double Scale, int ValidPixels);
    public sealed record Analysis(double Scale, int CandidateCount, IReadOnlyList<Region> Regions, int[] CandidateAreas);

    // @MX:ANCHOR: [AUTO] One scalar preparation shared by UI and the 180-pair probe.
    // @MX:REASON: Both paths must use the same sample-grid ROI, defect exclusion and MAD definition.
    public static Prepared Prepare(double[] a, double[] b, int width, int height, int step,
        int roiX, int roiY, int roiWidth, int roiHeight, bool[]? excluded, CancellationToken cancel)
    {
        ArgumentNullException.ThrowIfNull(a); ArgumentNullException.ThrowIfNull(b);
        if (width <= 0 || height <= 0 || step <= 0 || (long)width * height != a.Length || a.Length != b.Length ||
            (excluded is not null && excluded.Length != a.Length) || roiX < 0 || roiY < 0 || roiWidth <= 0 || roiHeight <= 0)
            throw new ArgumentException("Invalid sample dimensions, ROI or exclusion mask.");
        var differences = new double[a.Length]; var eligible = new bool[a.Length]; var values = new double[a.Length]; var count = 0;
        var right = (long)roiX + roiWidth; var bottom = (long)roiY + roiHeight;
        for (var y = 0; y < height; y++)
        {
            cancel.ThrowIfCancellationRequested();
            for (var x = 0; x < width; x++)
            {
                var i = y * width + x; var d = a[i] - b[i]; differences[i] = d;
                if ((long)x * step < roiX || (long)x * step >= right || (long)y * step < roiY || (long)y * step >= bottom ||
                    !double.IsFinite(d) || excluded?[i] == true) continue;
                eligible[i] = true; values[count++] = d;
            }
        }
        if (count == 0) return new Prepared(differences, eligible, width, height, step, 0, 0, 0);
        Array.Sort(values, 0, count); var median = Median(values, count);
        cancel.ThrowIfCancellationRequested();
        for (var i = 0; i < count; i++) values[i] = Math.Abs(values[i] - median);
        Array.Sort(values, 0, count); var scale = 1.4826 * Median(values, count);
        cancel.ThrowIfCancellationRequested();
        return new Prepared(differences, eligible, width, height, step, median, scale, count);
    }

    private static double Median(double[] sorted, int n) => n % 2 == 1 ? sorted[n / 2] : sorted[n / 2 - 1] / 2 + sorted[n / 2] / 2;

    // @MX:ANCHOR: [AUTO] Count all eligible regions before applying the display cap.
    // @MX:REASON: Tuning must never mistake the 20 displayed boxes for the uncapped candidate count.
    // @MX:WARN: [AUTO] Eight-neighbor flood fill touches each active sample once.
    // @MX:REASON: Bounds and cancellation checks belong inside the scan, not a recursive traversal.
    public static Analysis Analyze(Prepared prepared, double k, int minimumArea, CancellationToken cancel)
    {
        if (!double.IsFinite(k) || k <= 0 || minimumArea <= 0) throw new ArgumentException("k must be finite and positive; N must be a positive integer.");
        var active = new bool[prepared.Differences.Length]; var queue = new int[active.Length];
        var threshold = k * prepared.Scale;
        for (var i = 0; i < active.Length; i++) active[i] = prepared.Eligible[i] && Math.Abs(prepared.Differences[i]) > threshold;
        var regions = new List<Region>(DisplayLimit + 1); var areas = new List<int>(); var width = prepared.Width; var height = prepared.Height;
        for (var seed = 0; seed < active.Length; seed++)
        {
            if ((seed & 16383) == 0) cancel.ThrowIfCancellationRequested();
            if (!active[seed]) continue;
            var head = 0; var tail = 1; queue[0] = seed; active[seed] = false;
            var minX = seed % width; var maxX = minX; var minY = seed / width; var maxY = minY;
            double score = 0, sum = 0;
            while (head < tail)
            {
                if ((head & 16383) == 0) cancel.ThrowIfCancellationRequested();
                var i = queue[head++]; var x = i % width; var y = i / width; var d = prepared.Differences[i];
                score += Math.Abs(d); sum += d; minX = Math.Min(minX, x); maxX = Math.Max(maxX, x); minY = Math.Min(minY, y); maxY = Math.Max(maxY, y);
                for (var yy = Math.Max(0, y - 1); yy <= Math.Min(height - 1, y + 1); yy++)
                    for (var xx = Math.Max(0, x - 1); xx <= Math.Min(width - 1, x + 1); xx++)
                    { var next = yy * width + xx; if (active[next]) { active[next] = false; queue[tail++] = next; } }
            }
            if (tail < minimumArea) continue;
            areas.Add(tail);
            regions.Add(new Region(minX, minY, maxX - minX + 1, maxY - minY + 1, tail, score, sum / tail, prepared.Step));
            regions.Sort((x, y) => { var c = y.Score.CompareTo(x.Score); if (c != 0) return c; c = x.Y.CompareTo(y.Y); return c != 0 ? c : x.X.CompareTo(y.X); });
            if (regions.Count > DisplayLimit) regions.RemoveAt(DisplayLimit);
        }
        cancel.ThrowIfCancellationRequested();
        return new Analysis(prepared.Scale, areas.Count, regions, areas.ToArray());
    }
}
