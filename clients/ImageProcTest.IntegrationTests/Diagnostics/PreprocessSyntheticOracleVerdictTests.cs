// GUI-C-215 (#249): the oracle's gain verdict, held by inputs a wrong gain stage would produce.
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.Diagnostics;

/// <summary>
/// GUI-C-212b weakened the gain verdict back to its pre-212b form ("the call returned OK and some output is non-zero") as a falsification arm and NO test went red: the other tests run
/// the real module, whose gain is right and passes either condition. A verdict that no test can make say "no" is not held by anything. These tests call the verdict function directly with
/// the outputs a WRONG gain stage would give (a copy of the input, a scale error, one bad pixel, an error code) and with the right output, so the condition itself is what is measured.
/// Each wrong output is also shown to satisfy the old condition: that is what makes these inputs the ones that separate old from new.
/// </summary>
[Trait("Category", "P1AReady")]
public sealed class PreprocessSyntheticOracleVerdictTests
{
    private const XpeCommonApi.XpeErrorCode Ok = XpeCommonApi.XpeErrorCode.OK;

    /// <summary>The input of the gain stage: what the offset stage would have produced.</summary>
    private static ushort[] OffsetOut() => Enumerable.Range(0, XpePreprocessSyntheticOracle.FlatFrame().Length).Select(i => (ushort)(900 + i)).ToArray();

    /// <summary>The right gain output, computed by arithmetic from the flat frame (the same definition the oracle states, written out again here rather than called).</summary>
    private static float[] RightOutput(ushort[] offsetOut)
    {
        var flat = XpePreprocessSyntheticOracle.FlatFrame();
        var mean = flat.Average(v => (double)v);
        return offsetOut.Select((v, i) => (float)(v * mean / flat[i])).ToArray();
    }

    /// <summary>The condition the oracle had before GUI-C-212b, written out so the tests can show which wrong outputs it accepted.</summary>
    private static bool OldCondition(XpeCommonApi.XpeErrorCode code, float[] gainOut) => code == Ok && gainOut.Any(v => v != 0f);

    private static bool New(XpeCommonApi.XpeErrorCode code, ushort[] offsetOut, float[] gainOut) =>
        XpePreprocessSyntheticOracle.GainStagePassed(code, XpePreprocessSyntheticOracle.MaxAbsError(gainOut, offsetOut), offsetOut, gainOut);

    [Fact]
    public void TheRightOutput_Passes()
    {
        var offsetOut = OffsetOut();
        var right = RightOutput(offsetOut);

        Assert.True(OldCondition(Ok, right));
        Assert.True(New(Ok, offsetOut, right));
    }

    [Fact]
    public void WithinTheTolerance_Passes_AndJustBeyondIt_IsRejected()
    {
        var offsetOut = OffsetOut();
        var inside = RightOutput(offsetOut).Select(v => v * (1f + 1e-4f)).ToArray();   // 0.01 %, inside the 1e-3 relative tolerance
        var outside = RightOutput(offsetOut).Select(v => v * (1f + 5e-3f)).ToArray(); // 0.5 %, outside it

        Assert.True(New(Ok, offsetOut, inside));
        Assert.False(New(Ok, offsetOut, outside));
        Assert.True(OldCondition(Ok, outside), "the old condition accepted a gain that is half a percent off");
    }

    /// <summary>The stage the card names: it copied its input. Output non-zero, so the old condition passed it; no effect and a mismatch, so the new one does not.</summary>
    [Fact]
    public void AStageThatCopiesItsInput_IsRejected_ThoughTheOldConditionAcceptedIt()
    {
        var offsetOut = OffsetOut();
        var copy = offsetOut.Select(v => (float)v).ToArray();

        Assert.True(OldCondition(Ok, copy), "control: the copy satisfies the old condition, which is the defect");
        Assert.Equal(0.0, XpePreprocessSyntheticOracle.MaxAbsError(copy, offsetOut));
        Assert.False(New(Ok, offsetOut, copy));
    }

    /// <summary>A stage that changes its input but by the wrong amount: effect above zero (so "had an effect" alone would pass it), output not what the flat field dictates.</summary>
    [Fact]
    public void AStageWithTheWrongScale_IsRejected_ThoughItHadAnEffect()
    {
        var offsetOut = OffsetOut();
        var scaled = RightOutput(offsetOut).Select(v => v * 1.1f).ToArray();

        Assert.True(OldCondition(Ok, scaled));
        Assert.True(XpePreprocessSyntheticOracle.MaxAbsError(scaled, offsetOut) > 0, "control: it did change its input, so an effect-only condition would pass it");
        Assert.False(New(Ok, offsetOut, scaled));
    }

    [Fact]
    public void OneBadPixel_IsRejected()
    {
        var offsetOut = OffsetOut();
        var right = RightOutput(offsetOut);
        var bad = (float[])right.Clone();
        bad[137] *= 1.05f;

        Assert.True(OldCondition(Ok, bad));
        Assert.False(New(Ok, offsetOut, bad));
    }

    [Fact]
    public void ARightOutputWithAnErrorCode_IsRejected()
    {
        var offsetOut = OffsetOut();
        var right = RightOutput(offsetOut);

        Assert.False(New(XpeCommonApi.XpeErrorCode.INVALID_INPUT, offsetOut, right));
        Assert.False(OldCondition(XpeCommonApi.XpeErrorCode.INVALID_INPUT, right));
    }

    [Fact]
    public void AnAllZeroOutput_IsRejected()
    {
        var offsetOut = OffsetOut();

        Assert.False(OldCondition(Ok, new float[offsetOut.Length]));
        Assert.False(New(Ok, offsetOut, new float[offsetOut.Length]));
    }
}
