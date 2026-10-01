// #218 side finding (GUI-C-182): the defect metric pass lines are the canonical protocol's, not a second document's.
using System.Globalization;
using System.Text.RegularExpressions;
using ImageProcTest;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Two kinds of check. The values are read out of the canonical document's text and compared with the constants, so a
/// change on either side is red (the code had drifted from the document once, silently). And the verdicts are exercised
/// at the boundaries, so the label printed on screen and the PASS/REVIEW beside it come from the same line.
/// </summary>
[Trait("Category", "Functional")]
public sealed class DefectMetricGatesTests
{
    private const string CanonicalProtocol = "docs/project/Preprocessing-E2E-Automated-Evaluation-Protocol.md";

    internal static string Section55()
    {
        var text = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile(CanonicalProtocol));
        var start = text.IndexOf("### 5.5 Defect metrics", StringComparison.Ordinal);
        Assert.True(start >= 0, "The canonical protocol no longer has a '### 5.5 Defect metrics' section; this test names it.");
        var end = text.IndexOf("### 5.6", start, StringComparison.Ordinal);
        Assert.True(end > start, "Could not find where section 5.5 ends ('### 5.6').");
        return text[start..end];
    }

    private static double NumberAfter(string section, string pattern)
    {
        var match = Regex.Match(section, pattern);
        Assert.True(match.Success, $"Section 5.5 no longer contains text matching /{pattern}/.");
        return double.Parse(match.Groups[1].Value, CultureInfo.InvariantCulture);
    }

    [Fact]
    public void RecallLine_IsTheProtocols()
    {
        var protocol = NumberAfter(Section55(), @"DefectRecall\s*=\s*([0-9.]+)\s*%");
        Assert.Equal(protocol, DefectMetricGates.RecallMinPercent);
    }

    [Fact]
    public void FalsePositiveLine_IsTheProtocols_AndStrict()
    {
        // The protocol writes '<', not '<=': the regex requires the strict form, so a text change to '<=' is red here too.
        var protocol = NumberAfter(Section55(), @"DefectFPR\s*<\s*([0-9.]+)\s*%");
        Assert.Equal(protocol, DefectMetricGates.FprMaxPercent);
    }

    [Fact]
    public void GoodPixelLine_IsTheProtocols()
    {
        var protocol = NumberAfter(Section55(), @"GoodPixelDeltaP99\s*<=\s*([0-9.]+)\s*ADU");
        Assert.Equal(protocol, DefectMetricGates.GoodPixelDeltaP99MaxAdu);
    }

    [Fact]
    public void TheProtocolStillHasNoResidualLine_SoTheResidualStaysUngated()
    {
        // If the canonical text ever gains a residual threshold, this goes red on purpose: the residual row must then get
        // a gate from it (today the row is 'reported, no gate' and cannot produce REVIEW).
        Assert.DoesNotMatch(@"DefectResidualADU\s*(<=|<|≤)\s*[0-9]", Section55());
    }

    [Fact]
    public void TheProtocolDefinitions_AreTheOnesTheRowsImplement()
    {
        // The two definitions GUI-C-182 moved to: the residual is measured against a neighbour model, the good-pixel delta
        // against the image without the defect stage. If the document changes either, the implementation must be revisited.
        var section = Section55();
        Assert.Contains("DefectResidualADU = mean(abs(Y(defect_pixels) - neighbor_model(defect_pixels)))", section, StringComparison.Ordinal);
        Assert.Contains("GoodPixelDeltaP99 = percentile99(abs(Y(good_pixels) - Y_no_defect_stage(good_pixels)))", section, StringComparison.Ordinal);
    }

    [Theory]
    [InlineData(100.0, true)]
    [InlineData(99.9, false)]
    [InlineData(95.0, false)] // the old line: passed before GUI-C-182
    [InlineData(0.0, false)]
    public void Recall_Verdict(double percent, bool expected) =>
        Assert.Equal(expected, DefectMetricGates.RecallPasses(percent));

    [Theory]
    [InlineData(0.0, true)]
    [InlineData(1.0, true)]    // on the line
    [InlineData(1.0001, false)]
    [InlineData(0.5, true)]    // failed before GUI-C-182 (old line was <= 0)
    public void GoodPixelDeltaP99_Verdict(double adu, bool expected) =>
        Assert.Equal(expected, DefectMetricGates.GoodPixelDeltaP99Passes(adu));

    [Theory]
    [InlineData(0.0005, true)]
    [InlineData(0.00099, true)]
    [InlineData(0.001, false)] // strict '<': exactly on the line does not pass (the code said '<=' before)
    [InlineData(0.002, false)]
    public void Fpr_Verdict(double percent, bool expected) =>
        Assert.Equal(expected, DefectMetricGates.FprPasses(percent));

    [Fact]
    public void NaN_NeverPasses()
    {
        // The service shows 'N/A' for non-finite values before it looks at the verdict; this keeps the verdict itself honest.
        Assert.False(DefectMetricGates.RecallPasses(double.NaN));
        Assert.False(DefectMetricGates.FprPasses(double.NaN));
        Assert.False(DefectMetricGates.GoodPixelDeltaP99Passes(double.NaN));
    }

    [Fact]
    public void TheLabelsOnScreen_AreBuiltFromTheSameConstants()
    {
        Assert.Equal(">= 100%", DefectMetricGates.RecallGate);
        Assert.Equal("< 0.001%", DefectMetricGates.FprGate);
        Assert.Equal("<= 1 ADU", DefectMetricGates.GoodPixelDeltaP99Gate);
        Assert.Equal("reported, no gate", DefectMetricGates.ResidualGate);
    }
}
