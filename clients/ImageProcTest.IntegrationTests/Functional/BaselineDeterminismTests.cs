// #225 row 9 (GUI-C-196 M1): the Deterministic Baseline's pure core: the comparison, the fixed chain, the fixed parameters and the pass/fail decision.
using System.Security.Cryptography;
using ImageProcTest.Models;
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// No native code runs here: the runs are fakes. What is tested is what COUNTS as a pass: two runs, a bit-identical output, the exact baseline chain with every
/// stage applied, the input unchanged, no non-finite value. Each rule has a case that must fail when the rule is removed (the falsification arms of the card).
/// </summary>
[Trait("Category", "Functional")]
public sealed class BaselineDeterminismTests
{
    private static ushort[] Raw() => [10, 20, 30, 40, 50, 60];

    private static ChainResult Chain(ushort[] raw, StageStatus preprocess = StageStatus.Applied, StageStatus enhance = StageStatus.Applied, string reason = "ok")
    {
        var pixels = new ushort[] { 1, 2, 3, 4, 5, 6 };
        return new ChainResult(raw,
        [
            new StageOutcome(StageIds.Preprocess, preprocess, preprocess is StageStatus.Applied or StageStatus.AppliedNoChange ? pixels : null, reason),
            new StageOutcome(StageIds.EnhanceBasic, enhance, enhance is StageStatus.Applied or StageStatus.AppliedNoChange ? pixels : null, reason),
        ]);
    }

    private static BaselineSingleRun Run(ushort[] raw, ushort[] output, long nanInf = 0) => new(Chain(raw), output, nanInf);

    // ---- the comparison ----------------------------------------------------------------------------------------------------------

    [Fact]
    public void Compare_SaysIdentical_OnlyWhenEveryPixelAndTheLengthAreEqual()
    {
        Assert.True(BaselineDeterminism.Compare([1, 2, 3], [1, 2, 3]).Identical);
        Assert.True(BaselineDeterminism.Compare([], []).Identical);

        var oneLast = BaselineDeterminism.Compare([1, 2, 3], [1, 2, 4]);
        Assert.False(oneLast.Identical);                      // the LAST pixel counts too
        Assert.Equal(2, oneLast.FirstIndex);
        Assert.Equal(1, oneLast.DifferentCount);
        Assert.Equal(1, oneLast.MaxAbsDifference);

        var lowBit = BaselineDeterminism.Compare([0x1000], [0x1001]);
        Assert.False(lowBit.Identical);                       // one bit is a difference
    }

    [Fact]
    public void Compare_ReportsWhereTheArraysFirstDiffer_HowManyDiffer_AndByHowMuch()
    {
        var d = BaselineDeterminism.Compare([5, 5, 9, 5, 100], [5, 5, 7, 5, 70]);
        Assert.False(d.Identical);
        Assert.Equal(2, d.FirstIndex);
        Assert.Equal(2, d.DifferentCount);
        Assert.Equal(30, d.MaxAbsDifference);                 // |100 - 70|, not the first difference
    }

    [Fact]
    public void Compare_TreatsADifferentLengthAsADifference_EvenWhenTheCommonPartMatches()
    {
        var d = BaselineDeterminism.Compare([1, 2, 3], [1, 2, 3, 4]);
        Assert.False(d.Identical);
        Assert.Equal(3, d.FirstIndex);                        // they diverge where the shorter one ends
        Assert.Equal(0, d.DifferentCount);
        Assert.Equal((3, 4), (d.LengthA, d.LengthB));
    }

    [Fact]
    public void Sha256Hex_IsOverLittleEndianWords_AndSeesEveryBit()
    {
        var independent = Convert.ToHexString(SHA256.HashData([0x02, 0x01, 0xFF, 0x00])).ToLowerInvariant();
        Assert.Equal(independent, BaselineDeterminism.Sha256Hex([0x0102, 0x00FF]));        // bytes 02 01 FF 00: derived apart from the code under test
        Assert.NotEqual(BaselineDeterminism.Sha256Hex([0x0001]), BaselineDeterminism.Sha256Hex([0x0100]));   // the byte order is part of the hash
        Assert.NotEqual(BaselineDeterminism.Sha256Hex([0x1000]), BaselineDeterminism.Sha256Hex([0x1001]));
    }

    // ---- the fixed chain and the fixed parameters ----------------------------------------------------------------------------------

    [Fact]
    public void TheBaselineChain_IsFixed_AndNeverCarriesAnAssistiveStage()
    {
        var baseline = ProcessingChainPlan.BuildBaselineStages();
        Assert.Equal([StageIds.Preprocess, StageIds.EnhanceBasic], baseline.Select(s => s.StageId));
        Assert.All(baseline, s => Assert.True(s.Enabled));

        // The control: the ordinary plan DOES carry the assistive stages when the user turns them on, so their absence above is the baseline's own.
        var everything = new AppSettings { PreprocessInChain = true, AiBoneSuppressionInChain = true, GsvgMode = GsvgModes.GridSuppression };
        var ordinary = ProcessingChainPlan.BuildStages(everything);
        Assert.Contains(ordinary, s => s.StageId == StageIds.Gsvg && s.Enabled);
        Assert.Contains(ordinary, s => s.StageId == StageIds.AiBoneSuppression && s.Enabled);
        Assert.DoesNotContain(ordinary, s => s.StageId == StageIds.EnhanceBasic);   // the ordinary Apply does not run the enhance stage (D3)
    }

    [Fact]
    public void TheLogNormFactor_MapsTheSixteenBitFullScaleToTheFullScale()
    {
        // normFactor * log10(x + 1) at x = 65535, the formula xpe_log_transform documents.
        var top = BaselineParameters.LogNormFactor * Math.Log10(65535.0 + 1.0);
        Assert.InRange(top, 65534.9, 65535.1);
        Assert.InRange(BaselineParameters.LogNormFactor, 13606.0, 13607.0);
    }

    [Fact]
    public void ForBaseline_NeverReadsTheUsersDisplaySettings_AndKeepsWhatPreprocessNeeds()
    {
        var a = new AppSettings();
        var b = new AppSettings
        {
            VoiWindowCenter = 1000f,
            VoiWindowWidth = 500f,
            VoiLutMode = "Sigmoid",
            GsdfEnabled = true,
            ModalityRescaleSlope = 2.5f,
            ModalityRescaleIntercept = -1024f,
        };
        var fixedA = BaselineParameters.ForBaseline(a);
        var fixedB = BaselineParameters.ForBaseline(b);

        // Two users with very different display settings get the same display settings.
        Assert.Equal((fixedA.VoiWindowCenter, fixedA.VoiWindowWidth, fixedA.VoiLutMode, fixedA.GsdfEnabled, fixedA.ModalityRescaleSlope, fixedA.ModalityRescaleIntercept),
                     (fixedB.VoiWindowCenter, fixedB.VoiWindowWidth, fixedB.VoiLutMode, fixedB.GsdfEnabled, fixedB.ModalityRescaleSlope, fixedB.ModalityRescaleIntercept));
        Assert.Equal((32768f, 65535f, "Linear", false, 1f, 0f),
                     (fixedB.VoiWindowCenter, fixedB.VoiWindowWidth, fixedB.VoiLutMode, fixedB.GsdfEnabled, fixedB.ModalityRescaleSlope, fixedB.ModalityRescaleIntercept));
        Assert.Equal(1000f, b.VoiWindowCenter);               // the user's own object is untouched

        var preprocessInputs = new AppSettings { SelectedBodyPart = "Chest", ExposureKvp = 85f, PixelPitchMm = 0.2f, OffsetCalibrationDirectory = "x/offset" };
        var kept = BaselineParameters.ForBaseline(preprocessInputs);
        Assert.Equal(("Chest", 85f, 0.2f, "x/offset"), (kept.SelectedBodyPart, kept.ExposureKvp, kept.PixelPitchMm, kept.OffsetCalibrationDirectory));
    }

    // ---- the decision ----------------------------------------------------------------------------------------------------------------

    [Fact]
    public void TwoIdenticalRunsOverAnUnchangedInput_Pass_AfterExactlyTwoRuns()
    {
        var raw = Raw();
        var calls = 0;
        var verdict = BaselineRunner.Run(raw, () =>
        {
            calls++;
            return Run(raw, [7, 8, 9, 10, 11, 12]);
        });

        Assert.Equal(BaselineStatus.Pass, verdict.Status);
        Assert.Equal(2, calls);                               // not one, not three: the same input is run TWICE
        Assert.Equal(2, verdict.RunsExecuted);
        Assert.True(verdict.BitIdentical);
        Assert.True(verdict.InputPreserved);
        Assert.Equal(verdict.InputSha256Before, verdict.InputSha256After);
        Assert.Equal(BaselineDeterminism.Sha256Hex([7, 8, 9, 10, 11, 12]), verdict.OutputSha256);
        Assert.Equal(2, verdict.StageHashesRun1.Count);
        Assert.Equal(verdict.StageHashesRun1, verdict.StageHashesRun2);
        Assert.Equal(string.Empty, verdict.FailureReason);
    }

    [Fact]
    public void OneDifferentPixelBetweenTheRuns_Fails_AndSaysWhere()
    {
        var raw = Raw();
        var n = 0;
        var verdict = BaselineRunner.Run(raw, () => Run(raw, ++n == 1 ? [7, 8, 9, 10, 11, 12] : [7, 8, 9, 10, 11, 13]));

        Assert.Equal(BaselineStatus.Fail, verdict.Status);
        Assert.False(verdict.BitIdentical);
        Assert.Equal(5, verdict.Difference!.FirstIndex);      // the LAST pixel: a comparison that stops one short passes this
        Assert.Contains("first at pixel 5", verdict.FailureReason, StringComparison.Ordinal);
    }

    [Fact]
    public void AStageThatDoesNotApply_EndsTheCommandAsAFailure_AndNothingIsCompared()
    {
        var raw = Raw();
        var calls = 0;
        var verdict = BaselineRunner.Run(raw, () =>
        {
            calls++;
            return new BaselineSingleRun(Chain(raw, enhance: StageStatus.RequestedNotApplied, reason: "noise_reduce returned INVALID_INPUT"), [7, 8, 9, 10, 11, 12]);
        });

        Assert.Equal(BaselineStatus.Fail, verdict.Status);
        Assert.Equal(1, calls);                               // the second run is not made once the first is not a baseline
        Assert.Null(verdict.Difference);
        Assert.Contains("enhance_basic", verdict.FailureReason, StringComparison.Ordinal);
        Assert.Contains("RequestedNotApplied", verdict.FailureReason, StringComparison.Ordinal);
        Assert.Contains("noise_reduce returned INVALID_INPUT", verdict.FailureReason, StringComparison.Ordinal);
    }

    [Fact]
    public void AStageThatDoesNotApplyOnTheSecondRun_AlsoFails()
    {
        var raw = Raw();
        var n = 0;
        var verdict = BaselineRunner.Run(raw, () => ++n == 1
            ? Run(raw, [7, 8, 9, 10, 11, 12])
            : new BaselineSingleRun(Chain(raw, preprocess: StageStatus.RequestedNotApplied, reason: "no calibration"), [7, 8, 9, 10, 11, 12]));

        Assert.Equal(BaselineStatus.Fail, verdict.Status);
        Assert.Equal(2, verdict.RunsExecuted);
        Assert.Contains("run 2", verdict.FailureReason, StringComparison.Ordinal);
    }

    [Fact]
    public void AStageThatWasNotRequested_IsNotAnAppliedStage()
    {
        var raw = Raw();
        var verdict = BaselineRunner.Run(raw, () => new BaselineSingleRun(Chain(raw, preprocess: StageStatus.NotRequested), [7, 8, 9, 10, 11, 12]));
        Assert.Equal(BaselineStatus.Fail, verdict.Status);
    }

    [Fact]
    public void AStageThatAppliedWithNoChange_StillCounts()
    {
        var raw = Raw();
        var verdict = BaselineRunner.Run(raw, () => new BaselineSingleRun(Chain(raw, enhance: StageStatus.AppliedNoChange), [7, 8, 9, 10, 11, 12]));
        Assert.Equal(BaselineStatus.Pass, verdict.Status);
    }

    [Fact]
    public void AChainThatIsNotExactlyTheBaselineChain_Fails()
    {
        var raw = Raw();
        var pixels = new ushort[] { 1, 2, 3, 4, 5, 6 };

        var withAssistive = new ChainResult(raw,
        [
            new StageOutcome(StageIds.Preprocess, StageStatus.Applied, pixels, "ok"),
            new StageOutcome(StageIds.EnhanceBasic, StageStatus.Applied, pixels, "ok"),
            new StageOutcome(StageIds.AiBoneSuppression, StageStatus.Applied, pixels, "ok"),
        ]);
        var extra = BaselineRunner.Run(raw, () => new BaselineSingleRun(withAssistive, [7, 8, 9, 10, 11, 12]));
        Assert.Equal(BaselineStatus.Fail, extra.Status);
        Assert.Contains("ai_bone_suppress", extra.FailureReason, StringComparison.Ordinal);

        var missing = new ChainResult(raw, [new StageOutcome(StageIds.Preprocess, StageStatus.Applied, pixels, "ok")]);
        var short_ = BaselineRunner.Run(raw, () => new BaselineSingleRun(missing, [7, 8, 9, 10, 11, 12]));
        Assert.Equal(BaselineStatus.Fail, short_.Status);

        var wrongOrder = new ChainResult(raw,
        [
            new StageOutcome(StageIds.EnhanceBasic, StageStatus.Applied, pixels, "ok"),
            new StageOutcome(StageIds.Preprocess, StageStatus.Applied, pixels, "ok"),
        ]);
        Assert.Equal(BaselineStatus.Fail, BaselineRunner.Run(raw, () => new BaselineSingleRun(wrongOrder, [7, 8, 9, 10, 11, 12])).Status);
    }

    [Fact]
    public void ARunThatChangesTheLoadedFrame_FailsEvenIfTheOutputsAgree()
    {
        var raw = Raw();
        var n = 0;
        var verdict = BaselineRunner.Run(raw, () =>
        {
            if (++n == 2)
            {
                raw[0]++;                                      // a stage that wrote into the frame it was given
            }

            return Run(raw, [7, 8, 9, 10, 11, 12]);
        });

        Assert.Equal(BaselineStatus.Fail, verdict.Status);
        Assert.False(verdict.InputPreserved);
        Assert.True(verdict.BitIdentical);                    // the outputs agree; the failure is the input's
        Assert.NotEqual(verdict.InputSha256Before, verdict.InputSha256After);
    }

    [Fact]
    public void ANonFiniteValueInAFloatIntermediate_Fails()
    {
        var raw = Raw();
        var verdict = BaselineRunner.Run(raw, () => Run(raw, [7, 8, 9, 10, 11, 12], nanInf: 3));
        Assert.Equal(BaselineStatus.Fail, verdict.Status);
        Assert.Equal(6, verdict.NaNInfCount);                 // 3 per run, two runs
    }

    [Fact]
    public void ARunThatThrows_IsAFailureWithItsMessage_NotAnException()
    {
        var raw = Raw();
        var verdict = BaselineRunner.Run(raw, () => throw new InvalidOperationException("the display pipeline is gone"));
        Assert.Equal(BaselineStatus.Fail, verdict.Status);
        Assert.Contains("run 1 threw: the display pipeline is gone", verdict.FailureReason, StringComparison.Ordinal);
        Assert.Equal(1, verdict.RunsExecuted);
    }

    [Fact]
    public void AnOutputOfAnotherLength_Fails()
    {
        var raw = Raw();
        var n = 0;
        var verdict = BaselineRunner.Run(raw, () => Run(raw, ++n == 1 ? [7, 8, 9] : [7, 8, 9, 10]));
        Assert.Equal(BaselineStatus.Fail, verdict.Status);
        Assert.Contains("different length", verdict.FailureReason, StringComparison.Ordinal);
    }
}
