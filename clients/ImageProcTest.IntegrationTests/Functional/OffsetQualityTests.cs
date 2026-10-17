// GUI-C-253 (#273): the signed offset quality measurement (xpe_verify_offset_signed) shown as "measured" rows next to the existing dark metrics.
using System.Runtime.InteropServices;
using Xunit.Abstractions;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// The layout of <see cref="XpeOffsetQuality"/> against preprocess_api.h, what a row may say, and one call of the real module on a real dark frame. The existing dark rows (DarkBias, DSNU_ADU, DarkReduction_dB,
/// ClampRate, their limits and their pass column) are not code of this change; <see cref="OffsetQualityRows.Append"/> is the only place they meet the new rows, and it never reads them.
/// </summary>
public sealed class OffsetQualityTests(ITestOutputHelper output)
{
    private static XpeOffsetQuality Full() => new()
    {
        StructSize = 120, WrittenSize = 120, ValidMask = 0x1F, RoiPixels = 943718, PixelCount = 9437184, MapWidth = 3072, MapHeight = 3072,
        MapSha256 = Enumerable.Range(1, 32).Select(i => (byte)i).ToArray(),
        DarkBiasSigned = -4.0542, DsnuSigned = 2.5, DarkReductionDb = 21.7, ClampRatePct = 83.1, ZeroOutputRatePct = 83.4, DarkBiasOutput = 0.1875, DsnuOutput = 0.4,
    };

    private static void AssertNear(double expected, double actual, double tolerance, string what) =>
        Assert.True(Math.Abs(actual - expected) <= tolerance, $"{what}: {actual:R} is not within {tolerance:R} of {expected:R} (difference {Math.Abs(actual - expected):R})");

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

    // ---- GUI-C-253b (Codex #240 B-3): only a frame the user confirmed as a dark is measured as one

    [Fact]
    public void AConfirmedDarkWithAnOriginalSizeMap_IsCalled()
    {
        Assert.Null(OffsetQualityPlan.WhyNotCalled(RawInputKind.Dark, 3072, 3072, 3072, 3072));
    }

    [Fact]
    public void AnUnconfirmedInput_IsNeverCalled_AndSaysSo_WhateverTheMap()
    {
        foreach (var (mapW, mapH) in new (int?, int?)[] { (3072, 3072), (1024, 1024), (null, null) })
        {
            var refusal = OffsetQualityPlan.WhyNotCalled(RawInputKind.Unconfirmed, 3072, 3072, mapW, mapH);
            Assert.NotNull(refusal);
            Assert.Equal("다크 프레임 미확인 — 다크 품질을 측정하지 않음", refusal!.NotMeasuredReason);
            var rows = OffsetQualityRows.Build(refusal);
            Assert.All(rows.Where(r => r.Metric != OffsetQualityRows.BasisMetric), r => { Assert.Equal("N/A", r.Status); Assert.Equal(OffsetQualityRows.NotMeasuredText, r.Value); Assert.Equal(refusal.NotMeasuredReason, r.Gate); });
        }
    }

    [Theory]
    [InlineData((int)RawInputKind.Flat, "밝은 영상(평탄)")]
    [InlineData((int)RawInputKind.Clinical, "임상·기타 영상")]
    public void ANonDarkInput_IsNeverCalled_AndNamesWhatTheUserSaid(int kindValue, string kindText)
    {
        var refusal = OffsetQualityPlan.WhyNotCalled((RawInputKind)kindValue, 3072, 3072, 3072, 3072);   // an original-size map exists: the input kind still decides
        Assert.NotNull(refusal);
        Assert.Contains("다크 프레임이 아님", refusal!.NotMeasuredReason, StringComparison.Ordinal);
        Assert.Contains(kindText, refusal.NotMeasuredReason, StringComparison.Ordinal);
        Assert.Contains("다크 품질을 측정하지 않음", refusal.NotMeasuredReason, StringComparison.Ordinal);
    }

    [Fact]
    public void AConfirmedDarkWithoutAnOriginalSizeMap_HasItsOwnReason_NotTheInputKindOne()
    {
        var none = OffsetQualityPlan.WhyNotCalled(RawInputKind.Dark, 3072, 3072, null, null);
        Assert.Contains("offset 맵 요청이 없음", none!.NotMeasuredReason, StringComparison.Ordinal);

        var smaller = OffsetQualityPlan.WhyNotCalled(RawInputKind.Dark, 5000, 5000, 2500, 2500);
        Assert.Contains("원본 크기(5000x5000) offset 맵이 없음", smaller!.NotMeasuredReason, StringComparison.Ordinal);
        Assert.Contains("미리보기 기준", smaller.NotMeasuredReason, StringComparison.Ordinal);
        Assert.DoesNotContain("다크 프레임", smaller.NotMeasuredReason, StringComparison.Ordinal);   // it is a map problem, not an input-kind one
    }

    [Fact]
    public void TheDefaultInputKind_IsUnconfirmed()
    {
        Assert.Equal(RawInputKind.Unconfirmed, default(RawInputKind));
    }

    // ---- GUI-C-253c (Codex #241 R241-2): the base row says where "dark" comes from and what really happened

    [Fact]
    public void TheBaseRow_OfAMeasurement_NamesTheUserAsTheSourceOfTheDarkClaim()
    {
        var basis = RowOf(OffsetQualityRows.Build(OffsetQualityOutcome.FromModule(0, Full())), OffsetQualityRows.BasisMetric);
        Assert.Equal("입력 종류: 다크 프레임(사용자 선택, 취득 메타데이터로 확인하지 않음)", basis.Value);
        Assert.StartsWith("측정 조건:", basis.Gate, StringComparison.Ordinal);
        Assert.Equal("INFO", basis.Status);
    }

    [Theory]
    [InlineData(0)]   // unconfirmed
    [InlineData(2)]   // flat
    [InlineData(3)]   // clinical
    public void TheBaseRow_OfAnUnmeasuredRun_SaysItWasNotMeasured_AndWhy_NeverThatItWasADark(int kindValue)
    {
        var refusal = OffsetQualityPlan.WhyNotCalled((RawInputKind)kindValue, 3072, 3072, 3072, 3072)!;
        var basis = RowOf(OffsetQualityRows.Build(refusal), OffsetQualityRows.BasisMetric);
        Assert.Equal("미측정 — " + refusal.NotMeasuredReason, basis.Value);
        Assert.Equal("N/A", basis.Status);
        Assert.StartsWith("측정 조건:", basis.Gate, StringComparison.Ordinal);   // what a measurement needs, not a statement that it was met
        Assert.DoesNotContain("사용자 선택, 취득 메타데이터로 확인하지 않음", basis.Value, StringComparison.Ordinal);
    }

    // ---- GUI-C-253c (Codex #241 R241-3): changing the kind invalidates the measured rows of the run on screen, and only those

    private static readonly DetectorMetricRow[] FourExisting =
    [
        new("DarkBias", "0.19 ADU", "<= 5 ADU", "PASS"),
        new("DSNU_ADU", "2.1 ADU", "<= 20 ADU", "PASS"),
        new("DarkReduction_dB", "21.7 dB", ">= 10 dB", "PASS"),
        new("ClampRate", "83.1 %", "<= 1%", "REVIEW"),
    ];

    private static IReadOnlyList<DetectorMetricRow> RunRows(OffsetQualityOutcome outcome) => OffsetQualityRows.Append(FourExisting, outcome);

    private static IEnumerable<DetectorMetricRow> MeasuredOf(IReadOnlyList<DetectorMetricRow> rows) => rows.Where(OffsetQualityRows.IsMeasuredRow);

    private static void AssertInvalidated(IReadOnlyList<DetectorMetricRow> rows, string expectedReasonStart)
    {
        Assert.Equal(FourExisting, rows.Take(4).ToArray());   // the four existing rows: values, limits, verdicts, order
        Assert.DoesNotContain(rows.Skip(4), r => r.Status == "REPORTED");
        Assert.All(MeasuredOf(rows).Where(r => r.Metric != OffsetQualityRows.BasisMetric), r => { Assert.Equal("N/A", r.Status); Assert.Equal(OffsetQualityRows.NotMeasuredText, r.Value); Assert.StartsWith(expectedReasonStart, r.Gate, StringComparison.Ordinal); });
        Assert.StartsWith("미측정 — " + expectedReasonStart, RowOf(rows, OffsetQualityRows.BasisMetric).Value, StringComparison.Ordinal);
        var text = string.Join("\n", rows.Skip(4).Select(r => r.Value + " " + r.Gate));
        Assert.DoesNotContain("-4.0542", text, StringComparison.Ordinal);   // no old number comes back
        Assert.DoesNotContain("0.1875", text, StringComparison.Ordinal);
    }

    [Theory]
    [InlineData(0, "다크 프레임 미확인")]
    [InlineData(2, "다크 프레임이 아님(입력 종류: 밝은 영상(평탄))")]
    [InlineData(3, "다크 프레임이 아님(입력 종류: 임상·기타 영상)")]
    public void AfterADarkRun_ChoosingAnotherKind_InvalidatesTheMeasuredRows_WithThatKindsReason(int kindValue, string reasonStart)
    {
        var onScreen = RunRows(OffsetQualityOutcome.FromModule(0, Full()));
        Assert.Contains(onScreen, r => r.Status == "REPORTED");   // control: there were measured numbers to invalidate
        var after = OffsetQualityRows.Invalidate(onScreen, (RawInputKind)kindValue);
        AssertInvalidated(after, reasonStart);
    }

    [Fact]
    public void AfterAnUnconfirmedRun_ChoosingDark_SaysARerunIsNeeded_AndMeasuresNothing()
    {
        var onScreen = RunRows(OffsetQualityPlan.WhyNotCalled(RawInputKind.Unconfirmed, 3072, 3072, 3072, 3072)!);
        var after = OffsetQualityRows.Invalidate(onScreen, RawInputKind.Dark);
        AssertInvalidated(after, "입력 종류 변경 — 다시 실행 필요");
    }

    [Fact]
    public void ChoosingDarkAgain_DoesNotBringTheOldNumbersBack_ItNeedsANewRun()
    {
        var measured = RunRows(OffsetQualityOutcome.FromModule(0, Full()));
        var clinical = OffsetQualityRows.Invalidate(measured, RawInputKind.Clinical);
        var darkAgain = OffsetQualityRows.Invalidate(clinical, RawInputKind.Dark);
        AssertInvalidated(darkAgain, "입력 종류 변경 — 다시 실행 필요");
        Assert.Equal(measured.Count, darkAgain.Count);
    }

    [Fact]
    public void AfterARunThatFoundNoOriginalSizeMap_ChoosingDark_StaysUnmeasured()
    {
        var onScreen = RunRows(OffsetQualityPlan.WhyNotCalled(RawInputKind.Dark, 5000, 5000, 2500, 2500)!);
        Assert.Contains("원본 크기(5000x5000)", RowOf(onScreen, "DarkBias 부호").Gate, StringComparison.Ordinal);
        AssertInvalidated(OffsetQualityRows.Invalidate(onScreen, RawInputKind.Dark), "입력 종류 변경 — 다시 실행 필요");
    }

    [Fact]
    public void WhenNoMeasuredRowIsOnScreen_NothingChanges_NotEvenTheListInstance()
    {
        // the offset stage did not run (or nothing ran yet): the existing rows alone; choosing a kind must leave them as they are
        IReadOnlyList<DetectorMetricRow> onlyExisting = FourExisting;
        Assert.Same(onlyExisting, OffsetQualityRows.Invalidate(onlyExisting, RawInputKind.Dark));
        Assert.Same(onlyExisting, OffsetQualityRows.Invalidate(onlyExisting, RawInputKind.Clinical));
        Assert.Same(onlyExisting, OffsetQualityRows.Invalidate(onlyExisting, RawInputKind.Unconfirmed));
    }

    [Fact]
    public void EveryMeasuredRowTheBuilderMakes_IsRecognisedAsMeasured_AndNoExistingRowIs()
    {
        var built = OffsetQualityRows.Build(OffsetQualityOutcome.FromModule(0, Full()));
        Assert.All(built, r => Assert.True(OffsetQualityRows.IsMeasuredRow(r), r.Metric));
        Assert.All(FourExisting, r => Assert.False(OffsetQualityRows.IsMeasuredRow(r), r.Metric));
        Assert.All(OffsetQualityRows.Build(OffsetQualityOutcome.NotMeasured("x")), r => Assert.True(OffsetQualityRows.IsMeasuredRow(r), r.Metric));
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
            output.WriteLine($"DLL {dll}; validMask 0x{q.ValidMask:X}; roiPixels {q.RoiPixels}; pixelCount {q.PixelCount}; map {q.MapWidth}x{q.MapHeight}; map sha256 {Convert.ToHexString(q.MapSha256).ToLowerInvariant()}");
            output.WriteLine($"signed bias {q.DarkBiasSigned:R}; signed DSNU {q.DsnuSigned:R}; DarkReduction {q.DarkReductionDb:R} dB; clamp rate {q.ClampRatePct:R} %; zero-output rate {q.ZeroOutputRatePct:R} %; output bias {q.DarkBiasOutput:R}; output DSNU {q.DsnuOutput:R}");
            Assert.Equal(0x1Fu, q.ValidMask);
            // GUI-C-253b (Codex #240 B-2): the expected numbers are the QA-A-266 report's four-digit summaries, so each is compared with an explicit absolute tolerance of half its last digit,
            // not by rounding the expectation again (Assert.Equal(0.1875, x, 3) compares 0.188 with x rounded to 0.187 and failed on a correct 0.18748707914735363, measured with the local DLL).
            AssertNear(-4.0542, q.DarkBiasSigned, 5e-5, "signed bias");
            AssertNear(0.1875, q.DarkBiasOutput, 5e-5, "output bias");
            AssertNear(83.1, q.ClampRatePct, 0.05, "clamp rate (percent, one decimal in the report)");
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
            Assert.Equal(q.DarkBiasOutput, viaMap.Quality.DarkBiasOutput);
            Assert.Equal(q.ClampRatePct, viaMap.Quality.ClampRatePct);
            output.WriteLine($"in-memory map variant: signed bias {viaMap.Quality.DarkBiasSigned:R}, same map sha256 {q.MapSha256.SequenceEqual(viaMap.Quality.MapSha256)}");
        }
        finally
        {
            NativeLibrary.Free(module);
        }
    }
}
