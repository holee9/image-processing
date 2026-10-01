// #218 side finding (GUI-C-182): the defect metric rows follow the canonical protocol's definitions (section 5.5).
using ImageProcTest;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// The rows are computed from arrays, so each definition is checked on a picture small enough to work out by hand.
/// </summary>
[Trait("Category", "Functional")]
public sealed class DefectMetricRowsTests
{
    private const int W = 5;
    private const int H = 5;

    private static DetectorMetricRow RowOf(IReadOnlyList<DetectorMetricRow> rows, string metric) =>
        rows.Single(row => row.Metric == metric);

    private static bool[] OracleAt(params (int x, int y)[] pixels)
    {
        var mask = new bool[W * H];
        foreach (var (x, y) in pixels) mask[y * W + x] = true;
        return mask;
    }

    private static float[] Flat(float level) => Enumerable.Repeat(level, W * H).ToArray();

    // ---- GoodPixelDeltaP99: measured against the image without the defect stage, not against the raw input -----------

    /// <summary>
    /// The case the old definition got wrong. Offset and gain are on, so EVERY good pixel differs from the raw input
    /// (raw 1000 becomes 1170); the defect stage then changes only the defect pixel. Against the image the defect stage was
    /// given, the good pixels did not move (delta 0, PASS). Against the raw input, as the code compared before GUI-C-182,
    /// they moved by 170 ADU and the row said REVIEW whatever the defect stage did.
    /// </summary>
    [Fact]
    public void GoodPixelDelta_IsAboutTheDefectStage_NotAboutOffsetAndGain()
    {
        var oracle = OracleAt((2, 2));
        var raw = Flat(1000f);
        var beforeDefectStage = raw.Select(v => v * 1.2f - 30f).ToArray(); // offset + gain already applied: 1170
        var afterDefectStage = (float[])beforeDefectStage.Clone();
        beforeDefectStage[2 * W + 2] = 5000f;                              // the dead pixel, as the defect stage received it
        afterDefectStage[2 * W + 2] = 1170f;                               // corrected

        var rows = DefectMetricRows.Build(oracle, null, afterDefectStage, new DefectStageImages(beforeDefectStage, afterDefectStage), W, H);
        var row = RowOf(rows, "GoodPixelDeltaP99");
        Assert.Equal("PASS", row.Status);
        Assert.Equal("0 ADU", row.Value);

        // The old definition on the same pictures: delta against the raw input, over the good pixels.
        var oldDefinition = DefectMetricRows.GoodPixelDeltaP99(oracle, raw, afterDefectStage);
        Assert.True(oldDefinition > 100, $"The scenario must separate the two definitions, but the old one gave {oldDefinition}.");
        Assert.False(DefectMetricGates.GoodPixelDeltaP99Passes(oldDefinition));
    }

    [Fact]
    public void GoodPixelDelta_SeesADefectStageThatTouchesGoodPixels()
    {
        var oracle = OracleAt((2, 2));
        var before = Flat(1000f);
        var after = Flat(1000f);
        for (var i = 0; i < after.Length; i++) after[i] += 7f; // the stage altered every pixel by 7 ADU

        var row = RowOf(DefectMetricRows.Build(oracle, null, after, new DefectStageImages(before, after), W, H), "GoodPixelDeltaP99");
        Assert.Equal("REVIEW", row.Status);
        Assert.Equal("7 ADU", row.Value);
    }

    [Fact]
    public void GoodPixelDelta_IsNotComputed_WhenTheDefectStageDidNotRun()
    {
        var rows = DefectMetricRows.Build(OracleAt((2, 2)), null, Flat(100f), defectStage: null, W, H);
        var row = RowOf(rows, "GoodPixelDeltaP99");
        Assert.Equal("N/A", row.Status);
        Assert.Equal("not computed", row.Value);
        // ...and the residual is still reported: it needs only the output.
        Assert.NotEqual("N/A", RowOf(rows, "DefectResidualADU").Status);
    }

    // ---- DefectResidualADU: against a neighbour model, reported without a gate ------------------------------------------

    [Fact]
    public void Residual_IsTheDifferenceFromTheNeighbours_AndIndependentOfTheImageLevel()
    {
        var oracle = OracleAt((2, 2));
        foreach (var level in new[] { 100f, 5000f })
        {
            var uncorrected = Flat(level);
            uncorrected[2 * W + 2] = level + 10f;
            Assert.Equal(10.0, DefectMetricRows.Residual(oracle, uncorrected, W, H), 6);

            var corrected = Flat(level);
            Assert.Equal(0.0, DefectMetricRows.Residual(oracle, corrected, W, H), 6);
        }
    }

    [Fact]
    public void Residual_NeighbourModelIgnoresOtherDefectPixels()
    {
        // Two adjacent defect pixels, both 130 against a good surround of 100: each one's neighbour mean is over the good
        // pixels only (100), so the residual is 30. Averaging the other defect pixel in would give less than 30.
        var oracle = OracleAt((2, 2), (3, 2));
        var image = Flat(100f);
        image[2 * W + 2] = 130f;
        image[2 * W + 3] = 130f;
        Assert.Equal(30.0, DefectMetricRows.Residual(oracle, image, W, H), 6);
    }

    [Fact]
    public void Residual_HasNoNeighbourToCompareWith_IsNaN_NotZero()
    {
        var everyPixelIsADefect = Enumerable.Repeat(true, W * H).ToArray();
        Assert.True(double.IsNaN(DefectMetricRows.Residual(everyPixelIsADefect, Flat(100f), W, H)));
    }

    [Fact]
    public void Residual_IsReportedWithNoGate_AndNeverReview()
    {
        var oracle = OracleAt((2, 2));
        var image = Flat(100f);
        image[2 * W + 2] = 400f; // a residual of 300 ADU: far above the 2 ADU the old row used to gate on
        var row = RowOf(DefectMetricRows.Build(oracle, null, image, null, W, H), "DefectResidualADU");
        Assert.Equal("REPORTED", row.Status);
        Assert.Equal("reported, no gate", row.Gate);
        Assert.Equal("300 ADU", row.Value);
    }

    // ---- Recall and FPR rows ----------------------------------------------------------------------------------------

    [Fact]
    public void Recall_And_Fpr_Rows()
    {
        var oracle = OracleAt((0, 0), (1, 0), (2, 0), (3, 0));
        var found = OracleAt((0, 0), (1, 0), (2, 0), (3, 0));
        var rowsPerfect = DefectMetricRows.Build(oracle, found, Flat(1f), null, W, H);
        Assert.Equal("PASS", RowOf(rowsPerfect, "DefectRecall").Status);
        Assert.Equal("PASS", RowOf(rowsPerfect, "DefectFPR").Status);

        var oneMissed = OracleAt((0, 0), (1, 0), (2, 0));
        var rowsMissed = DefectMetricRows.Build(oracle, oneMissed, Flat(1f), null, W, H);
        var recall = RowOf(rowsMissed, "DefectRecall");
        Assert.Equal("75 %", recall.Value);
        Assert.Equal("REVIEW", recall.Status); // 75% < 100%; the old >= 95% line failed it too, 99% is what separates them (gate tests)

        var oneFalseAlarm = OracleAt((0, 0), (1, 0), (2, 0), (3, 0), (4, 4));
        var fpr = RowOf(DefectMetricRows.Build(oracle, oneFalseAlarm, Flat(1f), null, W, H), "DefectFPR");
        Assert.Equal("REVIEW", fpr.Status); // 1 of 21 good pixels is far above 0.001%
    }

    [Fact]
    public void NoPredictedMap_RecallAndFpr_AreNotComputed()
    {
        var rows = DefectMetricRows.Build(OracleAt((2, 2)), null, Flat(1f), null, W, H);
        Assert.Equal("N/A", RowOf(rows, "DefectRecall").Status);
        Assert.Equal("N/A", RowOf(rows, "DefectFPR").Status);
    }
}
