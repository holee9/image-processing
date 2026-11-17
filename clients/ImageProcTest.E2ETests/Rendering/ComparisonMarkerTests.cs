using System.Reflection;
using Xunit;

namespace ImageProcTest.E2ETests.Rendering;

public sealed class ComparisonMarkerTests
{
    [Fact]
    public void C271_ThreeKnownRectangles_AreTheOnlyRegionsAboveBoundedNoise()
    {
        var (a, b) = Noise(128, 96);
        Rectangle(a, 128, 10, 12, 6, 5, 20);
        Rectangle(a, 128, 50, 30, 8, 6, -30);
        Rectangle(a, 128, 90, 65, 7, 7, 40);
        dynamic result = Analyze(Prepare(a, b, 128, 96), 4, 4);
        Assert.Equal(3, (int)result.CandidateCount);
        var regions = ((System.Collections.IEnumerable)result.Regions).Cast<dynamic>().ToArray();
        Assert.Equal(3, regions.Length);
        Assert.Equal(new[] { 90, 50, 10 }, regions.Select(r => (int)r.X).ToArray());
        Assert.Equal(new[] { 49, 48, 30 }, regions.Select(r => (int)r.Area).ToArray());
        Assert.True((double)regions[0].MeanDifference > 39);
        Assert.True((double)regions[1].MeanDifference < -29);
    }

    [Fact]
    public void C271_BoundedNoiseAlone_HasNoRegions()
    {
        var (a, b) = Noise(128, 96);
        dynamic result = Analyze(Prepare(a, b, 128, 96), 4, 4);
        Assert.Equal(0, (int)result.CandidateCount);
        Assert.Empty(((System.Collections.IEnumerable)result.Regions).Cast<object>());
    }

    [Fact]
    public void C271_TwentyFiveCandidates_AreCountedBeforeTheTwentyDisplayLimit()
    {
        var (a, b) = Noise(128, 128);
        for (var i = 0; i < 25; i++) Rectangle(a, 128, 4 + i % 5 * 24, 4 + i / 5 * 24, 3, 3, 20 + i);
        dynamic result = Analyze(Prepare(a, b, 128, 128), 4, 4);
        Assert.Equal(25, (int)result.CandidateCount);
        var regions = ((System.Collections.IEnumerable)result.Regions).Cast<dynamic>().ToArray();
        Assert.Equal(20, regions.Length);
        Assert.Equal(100, (int)regions[0].X);
        Assert.Equal(100, (int)regions[0].Y);
        Assert.True(regions.Zip(regions.Skip(1), (dynamic x, dynamic y) => (double)x.Score >= (double)y.Score).All(x => x));
    }

    [Fact]
    public void C271_KnownDefectUnion_IsExcludedFromScaleAndCandidates()
    {
        var (a, b) = Noise(80, 80); var excluded = new bool[a.Length];
        Rectangle(a, 80, 5, 5, 6, 6, 100);
        Rectangle(a, 80, 40, 40, 6, 6, 20);
        for (var y = 5; y < 11; y++) for (var x = 5; x < 11; x++) excluded[y * 80 + x] = true;
        dynamic result = Analyze(Prepare(a, b, 80, 80, excluded: excluded), 4, 4);
        Assert.Equal(1, (int)result.CandidateCount);
        dynamic region = ((System.Collections.IEnumerable)result.Regions).Cast<dynamic>().Single();
        Assert.Equal(40, (int)region.X); Assert.Equal(36, (int)region.Area);
    }

    [Fact]
    public void C271_SourceRoi_UsesTheSameDecimationGrid_AndZeroMadIsDefined()
    {
        var a = new double[20 * 20]; var b = new double[a.Length];
        Rectangle(a, 20, 0, 0, 2, 2, 100); // outside source ROI
        Rectangle(a, 20, 4, 4, 2, 2, -10);
        a[19 * 20 + 19] = double.NaN;
        dynamic prepared = Prepare(a, b, 20, 20, step: 2, roi: new[] { 5, 3, 20, 18 });
        dynamic result = Analyze(prepared, 4, 4);
        Assert.Equal(0, (double)result.Scale);
        Assert.Equal(1, (int)result.CandidateCount);
        dynamic region = ((System.Collections.IEnumerable)result.Regions).Cast<dynamic>().Single();
        Assert.Equal(4, (int)region.X); Assert.Equal(8, (int)region.SourceX);
        Assert.Equal(-10, (double)region.MeanDifference);
    }

    [Fact]
    public void C271_ThresholdIsAbsoluteRawDifference_NotDifferenceMinusItsMedian()
    {
        var a = Enumerable.Repeat(10.0, 16).ToArray(); var b = new double[16];
        dynamic result = Analyze(Prepare(a, b, 4, 4), 4, 4);
        Assert.Equal(1, (int)result.CandidateCount); // MAD=0, raw |10|>0, not |10-median|.
        Assert.ThrowsAny<ArgumentException>(() => Call("Analyze", Prepare(a, b, 4, 4), -1.0, 4, CancellationToken.None));
    }

    [Fact]
    public void C271_ExplicitBpmAndXcal_AreHashBound_AndUnionXorAreSeparate()
    {
        var dir = Path.Combine(Path.GetTempPath(), "xpe-c271-mask-" + Guid.NewGuid().ToString("N")); Directory.CreateDirectory(dir);
        try
        {
            var a = new byte[64]; var b = new byte[64]; a[2 * 8 + 2] = 1; b[4 * 8 + 4] = 1;
            File.WriteAllBytes(Path.Combine(dir, "a.map"), a);
            var header = new byte[152]; System.Text.Encoding.ASCII.GetBytes("XCAL").CopyTo(header, 0);
            void U32(int offset, uint value) => System.Buffers.Binary.BinaryPrimitives.WriteUInt32LittleEndian(header.AsSpan(offset), value);
            U32(4, 1); U32(8, 2); U32(12, 2); U32(16, 8); U32(20, 8);
            System.Buffers.Binary.BinaryPrimitives.WriteUInt64LittleEndian(header.AsSpan(112), 64);
            System.Security.Cryptography.SHA256.HashData(b).CopyTo(header, 120);
            File.WriteAllBytes(Path.Combine(dir, "b.xcal"), header.Concat(b).ToArray());
            System.Text.Json.Nodes.JsonObject Entry(string file) => new() { ["path"] = file, ["sha256"] = Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(File.ReadAllBytes(Path.Combine(dir, file)))) };
            var manifest = new System.Text.Json.Nodes.JsonObject { ["engines"] = new System.Text.Json.Nodes.JsonObject {
                ["A"] = new System.Text.Json.Nodes.JsonObject { ["maps"] = new System.Text.Json.Nodes.JsonObject { ["bpm"] = Entry("a.map") } },
                ["B"] = new System.Text.Json.Nodes.JsonObject { ["maps"] = new System.Text.Json.Nodes.JsonObject { ["defect.xcal"] = Entry("b.xcal") } } } };
            dynamic result = MaskCall(manifest, dir, 8L, 8L, 2, CancellationToken.None);
            Assert.True((bool)result.DisagreementAvailable);
            Assert.Equal(2, ((bool[])result.Excluded).Count(x => x));
            Assert.Equal(2, ((bool[])result.Disagreement).Count(x => x));
            manifest["engines"]!["B"]!["maps"]!["defect.xcal"]!["sha256"] = new string('0', 64);
            result = MaskCall(manifest, dir, 8L, 8L, 2, CancellationToken.None);
            Assert.False((bool)result.DisagreementAvailable);
            Assert.Contains("SHA", (string)result.Status);
        }
        finally { foreach (var f in Directory.EnumerateFiles(dir)) File.Delete(f); Directory.Delete(dir); }
    }
    private static object MaskCall(params object?[] args)
    {
        var type = typeof(ImportedComparisonTests).Assembly.GetType("ImageProcTest.Services.Comparison.ComparisonDefectMasks");
        Assert.NotNull(type); return type.GetMethod("Read")!.Invoke(null, args)!;
    }

    private static object Prepare(double[] a, double[] b, int width, int height, int step = 1, int[]? roi = null, bool[]? excluded = null)
    {
        roi ??= new[] { 0, 0, width * step, height * step };
        return Call("Prepare", a, b, width, height, step, roi[0], roi[1], roi[2], roi[3], excluded, CancellationToken.None);
    }
    private static object Analyze(object prepared, double k, int n) => Call("Analyze", prepared, k, n, CancellationToken.None);
    private static object Call(string name, params object?[] args)
    {
        var type = typeof(ImportedComparisonTests).Assembly.GetType("ImageProcTest.Services.Comparison.ComparisonMarkers");
        Assert.NotNull(type);
        try { return type.GetMethod(name, BindingFlags.Public | BindingFlags.Static)!.Invoke(null, args)!; }
        catch (TargetInvocationException e) when (e.InnerException is not null) { System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(e.InnerException).Throw(); throw; }
    }
    private static (double[] A, double[] B) Noise(int width, int height)
    {
        var rng = new Random(271); var a = new double[width * height]; var b = new double[a.Length];
        for (var i = 0; i < a.Length; i++) a[i] = rng.NextDouble() - 0.5;
        return (a, b);
    }
    private static void Rectangle(double[] a, int width, int x, int y, int w, int h, double delta)
    { for (var yy = y; yy < y + h; yy++) for (var xx = x; xx < x + w; xx++) a[yy * width + xx] += delta; }
}
