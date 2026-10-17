// GUI-C-253 (#273): the signed offset quality measurement (xpe_verify_offset_signed) shown as "measured" rows next to the existing dark metrics.
using System.Runtime.InteropServices;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// The layout of <see cref="XpeOffsetQuality"/> against preprocess_api.h, what a row may say, and one call of the real module on a real dark frame. The existing dark rows (DarkBias, DSNU_ADU, DarkReduction_dB,
/// ClampRate, their limits and their pass column) are not code of this change; <see cref="OffsetQualityRows.Append"/> is the only place they meet the new rows, and it never reads them.
/// </summary>
public sealed class OffsetQualityTests
{
    private static XpeOffsetQuality Full() => new()
    {
        StructSize = 120, WrittenSize = 120, ValidMask = 0x1F, RoiPixels = 943718, PixelCount = 9437184, MapWidth = 3072, MapHeight = 3072,
        MapSha256 = Enumerable.Range(1, 32).Select(i => (byte)i).ToArray(),
        DarkBiasSigned = -4.0542, DsnuSigned = 2.5, DarkReductionDb = 21.7, ClampRatePct = 83.1, ZeroOutputRatePct = 83.4, DarkBiasOutput = 0.1875, DsnuOutput = 0.4,
    };

    private static DetectorMetricRow RowOf(IReadOnlyList<DetectorMetricRow> rows, string metricStart) => rows.Single(r => r.Metric.StartsWith(metricStart, StringComparison.Ordinal));

    // ---- layout: 120 bytes and every field's offset (header: preprocess_api.h XpeOffsetQuality; offsets of pre's draft table)

    [Fact]
    public void TheStruct_Is120Bytes_AndEveryFieldSitsWhereTheHeaderPutsIt()
    {
        Assert.Equal(120, Marshal.SizeOf<XpeOffsetQuality>());
        Assert.Equal(OffsetQualityBits.StructBytes, Marshal.SizeOf<XpeOffsetQuality>());
        var expected = new (string Field, int Offset)[]
        {
            ("StructSize", 0), ("WrittenSize", 4), ("ValidMask", 8), ("RoiPixels", 12), ("PixelCount", 16), ("MapWidth", 24), ("MapHeight", 28), ("MapSha256", 32),
            ("DarkBiasSigned", 64), ("DsnuSigned", 72), ("DarkReductionDb", 80), ("ClampRatePct", 88), ("ZeroOutputRatePct", 96), ("DarkBiasOutput", 104), ("DsnuOutput", 112),
        };
        foreach (var (field, offset) in expected) Assert.True(Marshal.OffsetOf<XpeOffsetQuality>(field).ToInt32() == offset, $"{field} is at {Marshal.OffsetOf<XpeOffsetQuality>(field)}, the header puts it at {offset}");
        Assert.Equal(15, typeof(XpeOffsetQuality).GetFields().Length);   // no field the table above does not name
    }

    [Fact]
    public void TheMaskBits_AreTheHeadersFive()
    {
        Assert.Equal([0x01u, 0x02u, 0x04u, 0x08u, 0x10u], [OffsetQualityBits.RoiSigned, OffsetQualityBits.DarkReduction, OffsetQualityBits.RoiOutput, OffsetQualityBits.ClampRate, OffsetQualityBits.MapIdent]);
    }

    // ---- reading a result: code, then writtenSize, then the bit

    [Fact]
    public void AFailedReturnCode_ReadsNoField_WhateverTheStructHolds()
    {
        var outcome = OffsetQualityOutcome.FromModule(-1, Full());   // the struct still carries a "result"; an error means it is not one
        Assert.False(outcome.Measured);
        var rows = OffsetQualityRows.Build(outcome);
        Assert.All(rows.Where(r => r.Metric != OffsetQualityRows.BasisMetric), r => { Assert.Equal(OffsetQualityRows.NotMeasuredText, r.Value); Assert.Equal("N/A", r.Status); Assert.Contains("-1", r.Gate, StringComparison.Ordinal); });
    }

    [Fact]
    public void AStructWrittenShortOf120Bytes_IsNotRead()
    {
        var shorter = Full();
        shorter.WrittenSize = 64;
        var outcome = OffsetQualityOutcome.FromModule(0, shorter);
        Assert.False(outcome.Measured);
        Assert.Contains("64", outcome.NotMeasuredReason, StringComparison.Ordinal);
    }

    // ---- what a row may say

    [Fact]
    public void AFullyMeasuredResult_ShowsEachValue_AsReported_WithNoVerdict()
    {
        var rows = OffsetQualityRows.Build(OffsetQualityOutcome.FromModule(0, Full()));
        Assert.Equal("-4.0542 화소값", RowOf(rows, "DarkBias 부호").Value);
        Assert.Equal("0.1875 화소값", RowOf(rows, "DarkBias 출력").Value);
        Assert.Equal("83.1 %", RowOf(rows, "ClampRate x<0").Value);
        Assert.Equal("83.4 %", RowOf(rows, "출력 0 비율").Value);
        Assert.StartsWith("0102030405060708", RowOf(rows, "Offset 맵 SHA-256").Value, StringComparison.Ordinal);
        // no verdict anywhere: the status column of a new row is REPORTED, INFO or N/A, never the existing rows' PASS / REVIEW
        Assert.All(rows, r => Assert.Contains(r.Status, new[] { "REPORTED", "INFO", "N/A" }));
        Assert.DoesNotContain(rows, r => r.Status is "PASS" or "REVIEW");
    }

    [Fact]
    public void TheRows_SayThePopulationDiffers_AndDoNotPresentTheOldLimitsAsApproved()
    {
        var rows = OffsetQualityRows.Build(OffsetQualityOutcome.FromModule(0, Full()));
        var basis = RowOf(rows, OffsetQualityRows.BasisMetric);
        Assert.Contains("미리보기 기준", basis.Gate, StringComparison.Ordinal);
        Assert.Contains("모집단과 단계가 다르다", basis.Gate, StringComparison.Ordinal);
        // no limit appears in a new row: the DSNU <= 20 / ClampRate <= 1 % lines belong to the old rows only
        var text = string.Join("\n", rows.Select(r => r.Value + " " + r.Gate));
        Assert.DoesNotContain("<= 20", text, StringComparison.Ordinal);
        Assert.DoesNotContain("<= 1%", text, StringComparison.Ordinal);
        Assert.DoesNotContain("≤", text, StringComparison.Ordinal);
        // the unit of a new value is "화소값", not the old "ADU" (the basis row names the old rows, DSNU_ADU among them, so it is left out of this one check)
        Assert.DoesNotContain("ADU", string.Join("\n", rows.Where(r => r.Metric != OffsetQualityRows.BasisMetric).Select(r => r.Value + " " + r.Gate)), StringComparison.Ordinal);
        Assert.Contains("판정 없음", RowOf(rows, "DarkBias 부호").Gate, StringComparison.Ordinal);
        Assert.Contains("적용 가능한 맵이라는 뜻이 아니다", RowOf(rows, "Offset 맵 SHA-256").Gate, StringComparison.Ordinal);
    }

    [Fact]
    public void AGroupWhoseBitIsClear_ReadsNotMeasured_EvenWhenItsFieldHoldsAnImpressiveNumber()
    {
        var partial = Full();
        partial.ValidMask = OffsetQualityBits.ClampRate | OffsetQualityBits.MapIdent;   // the ROI groups were not measured (a tiny ROI, a flat raw)
        partial.DarkBiasSigned = 3.0;
        partial.DarkBiasOutput = 0;   // a legal measured value when the bit is set; here it is only the module's zero fill
        var rows = OffsetQualityRows.Build(OffsetQualityOutcome.FromModule(0, partial));

        foreach (var metric in new[] { "DarkBias 부호", "DSNU 부호", "DarkReduction 부호", "DarkBias 출력", "DSNU 출력" })
        {
            var row = RowOf(rows, metric);
            Assert.Equal(OffsetQualityRows.NotMeasuredText, row.Value);
            Assert.Equal("N/A", row.Status);
            Assert.Contains("validMask", row.Gate, StringComparison.Ordinal);
            Assert.DoesNotContain("3", row.Value, StringComparison.Ordinal);
        }

        Assert.Equal("83.1 %", RowOf(rows, "ClampRate x<0").Value);   // the groups that WERE measured still show
        Assert.NotEqual(OffsetQualityRows.NotMeasuredText, RowOf(rows, "Offset 맵 SHA-256").Value);
    }

    [Fact]
    public void AMeasuredZero_IsShownAsZero_NotAsNotMeasured()
    {
        var zero = Full();
        zero.ClampRatePct = 0;
        zero.ZeroOutputRatePct = 0;
        var rows = OffsetQualityRows.Build(OffsetQualityOutcome.FromModule(0, zero));
        Assert.Equal("0 %", RowOf(rows, "ClampRate x<0").Value);
        Assert.Equal("REPORTED", RowOf(rows, "ClampRate x<0").Status);
    }

    [Fact]
    public void ANonFiniteValue_IsNotShownAsAMeasurement()
    {
        var bad = Full();
        bad.DarkReductionDb = double.PositiveInfinity;
        var row = RowOf(OffsetQualityRows.Build(OffsetQualityOutcome.FromModule(0, bad)), "DarkReduction 부호");
        Assert.Equal(OffsetQualityRows.NotMeasuredText, row.Value);
        Assert.Equal("N/A", row.Status);
    }

    [Fact]
    public void NothingCalled_ReadsNotMeasured_WithThePreviewReason()
    {
        var reason = "원본 크기(3072x3072) offset 맵이 없음: 이 화면의 맵은 미리보기 크기(1024x1024). 위 칸은 미리보기 기준";
        var rows = OffsetQualityRows.Build(OffsetQualityOutcome.NotMeasured(reason));
        Assert.All(rows.Where(r => r.Metric != OffsetQualityRows.BasisMetric), r => { Assert.Equal(OffsetQualityRows.NotMeasuredText, r.Value); Assert.Equal(reason, r.Gate); Assert.Equal("N/A", r.Status); });
    }

    // ---- the existing rows are not touched

    [Fact]
    public void Append_KeepsTheExistingRowsExactlyAsTheyWere_ThenAddsTheMeasuredOnes()
    {
        DetectorMetricRow[] existing =
        [
            new("DarkBias", "0.19 ADU", "<= 5 ADU", "PASS"),
            new("DSNU_ADU", "2.1 ADU", "<= 20 ADU", "PASS"),
            new("DarkReduction_dB", "21.7 dB", ">= 10 dB", "PASS"),
            new("ClampRate", "83.1 %", "<= 1%", "REVIEW"),
        ];
        var rows = OffsetQualityRows.Append(existing, OffsetQualityOutcome.FromModule(0, Full()));
        Assert.Equal(existing, rows.Take(existing.Length).ToArray());   // same values, same limits, same verdicts, same order
        Assert.True(rows.Count > existing.Length);
        Assert.All(rows.Skip(existing.Length), r => Assert.DoesNotContain(r.Status, new[] { "PASS", "REVIEW" }));
    }

    // ---- the real module on a real dark frame (needs the DLL of dev/preprocess 026ea384 and the files; skipped with a reason otherwise)

    /// <summary>
    /// Dark_16 of cyan_test against <c>clean_set_v1/offset.xcal</c> (QA-A-266 report §4): signed bias -4.0542, output bias 0.1875, clamp rate 83.1 %. Needs <c>XPE_C253_NATIVE_DIR</c> (the folder of the CI build of
    /// 026ea384 or later), <c>XPE_C253_DARK</c> (Dark_16.raw) and <c>XPE_C253_OFFSET</c> (offset.xcal); the data is not in the repository, so CI cannot run it (XPE-SKIP-ALLOWED:273).
    /// </summary>
    [SkippableFact]
    public void TheRealModule_OnDark16AndTheCleanSetMap_MeasuresTheSignedBiasThePaperReports()
    {
        var native = Environment.GetEnvironmentVariable("XPE_C253_NATIVE_DIR");
        var dark = Environment.GetEnvironmentVariable("XPE_C253_DARK");
        var map = Environment.GetEnvironmentVariable("XPE_C253_OFFSET");
        Skip.If(string.IsNullOrEmpty(native) || string.IsNullOrEmpty(dark) || string.IsNullOrEmpty(map),
            "XPE_C253_NATIVE_DIR/XPE_C253_DARK/XPE_C253_OFFSET not set - the dark frame and the map are not in the repository. XPE-SKIP-ALLOWED:273");
        var dll = Path.Combine(native!, "xpe_preprocess.dll");
        Assert.True(File.Exists(dll), "xpe_preprocess.dll is not in " + native);
        Assert.True(File.Exists(dark!), "the dark frame does not exist: " + dark);
        Assert.True(File.Exists(map!), "the offset map does not exist: " + map);

        Environment.SetEnvironmentVariable("PATH", native + ";" + Environment.GetEnvironmentVariable("PATH"));   // the module's own DLLs are found beside it
        var module = NativeLibrary.Load(dll);
        try
        {
            var raw = OffsetQualityNative.ReadRaw(dark!, 3072, 3072, out var problem);
            Assert.True(raw is not null, problem);
            var outcome = OffsetQualityNative.MeasureWithFile(module, raw!, 3072, 3072, map!);
            Assert.True(outcome.Measured, outcome.NotMeasuredReason);
            var q = outcome.Quality;
            Assert.Equal(0x1Fu, q.ValidMask);
            Assert.Equal(-4.0542, q.DarkBiasSigned, 3);
            Assert.Equal(0.1875, q.DarkBiasOutput, 3);
            Assert.Equal(83.1, q.ClampRatePct, 1);
            Assert.Equal(3072u, q.MapWidth);
            Assert.Equal(9437184ul, q.PixelCount);

            // the in-memory variant on the same pair gives the same numbers when it is handed the same map (the payload of the file: float32 after the 152-byte header and the config)
            var bytes = File.ReadAllBytes(map!);
            var configLength = checked((int)BitConverter.ToUInt64(bytes, 104));
            var payload = bytes.AsSpan(152 + configLength).ToArray();
            var floats = new float[3072 * 3072];
            Buffer.BlockCopy(payload, 0, floats, 0, payload.Length);
            var viaMap = OffsetQualityNative.MeasureWithMap(module, raw!, floats, 3072, 3072);
            Assert.True(viaMap.Measured, viaMap.NotMeasuredReason);
            Assert.Equal(q.DarkBiasSigned, viaMap.Quality.DarkBiasSigned);
            Assert.Equal(q.MapSha256, viaMap.Quality.MapSha256);
        }
        finally
        {
            NativeLibrary.Free(module);
        }
    }
}
