// #225 row 9 (GUI-C-196 M7): the Deterministic Baseline's display step apart from the native calls, so the non-finite count of its float intermediates is made in
// code the integration tests link. Free of WPF and of native code.
using ImageProcTest.Models;

namespace ImageProcTest.Services;

/// <summary>A float32 working image of the display module: loaded from 16-bit pixels, changed in place by the LUT stages, read back as 16-bit pixels at the end.</summary>
internal interface IBaselineDisplayImage : IDisposable
{
    /// <summary>xpe_apply_modality_lut (rescale slope / intercept). Returns the module's return code (negative = failure).</summary>
    int ApplyModality(float slope, float intercept);

    /// <summary>xpe_apply_voi_lut, linear mode. Returns the module's return code.</summary>
    int ApplyVoi(float center, float width);

    /// <summary>xpe_apply_presentation_lut. After it the buffer holds 16-bit pixels. Returns the module's return code.</summary>
    int ApplyPresentation(bool gsdfEnabled);

    /// <summary>A copy of the pixels as float (valid while the image is still float: before <see cref="ApplyPresentation"/>).</summary>
    float[] ReadFloats();

    /// <summary>A copy of the pixels as 16-bit values (valid after <see cref="ApplyPresentation"/>).</summary>
    ushort[] ReadUInt16();
}

internal interface IBaselineDisplayBackend
{
    IBaselineDisplayImage Open(ushort[] input, int width, int height);
}

/// <summary>The display step's output pixels and the NaN/Inf values seen in its float intermediates. When the count is not zero the pixels are empty: the step stopped.</summary>
public sealed record BaselineDisplayResult(ushort[] Pixels, long NonFiniteCount);

/// <summary>
/// The display step of one baseline run: modality LUT, VOI LUT, presentation LUT with the FIXED parameters of <see cref="BaselineParameters"/> (no user setting is an
/// input of this method, and the signature is the proof). The float image is inspected after the modality LUT and after the VOI LUT, the two points where it still is
/// float; a non-finite value there ends the step at once (nothing after it is run on a poisoned image) and is returned as a count, not thrown, so it reaches the verdict
/// the way the stages' counts do (Codex #73, finding 1, closed for this step in M7). The presentation LUT's output is 16-bit and cannot hold one.
/// </summary>
internal static class BaselineDisplayStage
{
    public static BaselineDisplayResult Run(ushort[] input, int width, int height, IBaselineDisplayBackend backend)
    {
        ArgumentNullException.ThrowIfNull(input);
        ArgumentNullException.ThrowIfNull(backend);

        var count = checked(width * height);
        if (width <= 0 || height <= 0 || input.Length < count)
        {
            throw new InvalidOperationException("Baseline display input is smaller than width x height.");
        }

        using var image = backend.Open(input, width, height);

        Check(image.ApplyModality(BaselineParameters.ModalityRescaleSlope, BaselineParameters.ModalityRescaleIntercept), "xpe_apply_modality_lut");
        var afterModality = BaselineStageAdapters.CountNonFinite(image.ReadFloats());
        if (afterModality != 0)
        {
            return new BaselineDisplayResult([], afterModality);
        }

        Check(image.ApplyVoi(BaselineParameters.VoiWindowCenter, BaselineParameters.VoiWindowWidth), "xpe_apply_voi_lut");
        var afterVoi = BaselineStageAdapters.CountNonFinite(image.ReadFloats());
        if (afterVoi != 0)
        {
            return new BaselineDisplayResult([], afterVoi);
        }

        Check(image.ApplyPresentation(BaselineParameters.GsdfEnabled), "xpe_apply_presentation_lut");
        return new BaselineDisplayResult(image.ReadUInt16(), 0);
    }

    /// <summary>
    /// One baseline run from its chain: when every stage of the chain was applied the display step runs on the chain's last output and its count becomes the run's own
    /// <see cref="BaselineSingleRun.NaNInfCount"/>; otherwise (a refused stage is a failed baseline, D5) the display is not run and the output is empty.
    /// </summary>
    public static BaselineSingleRun ComposeRun(ChainResult chain, int width, int height, IBaselineDisplayBackend backend)
    {
        ArgumentNullException.ThrowIfNull(chain);

        if (chain.Stages.Any(s => s.Status is not (StageStatus.Applied or StageStatus.AppliedNoChange)))
        {
            return new BaselineSingleRun(chain, []);
        }

        var display = Run(chain.DisplayInput, width, height, backend);
        return new BaselineSingleRun(chain, display.Pixels, display.NonFiniteCount);
    }

    private static void Check(int code, string functionName)
    {
        if (code < 0)
        {
            throw new InvalidOperationException($"{functionName} failed with XPE error code {code}.");
        }
    }
}
