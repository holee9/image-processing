// #225 row 10 (GUI-C-184): what the AI bone suppression stage makes of the module's answer. No native call here.
using System;
using System.Linq;
using ImageProcTest.Models;

namespace ImageProcTest.Services;

/// <summary>How the module's answer to one <c>xpe_bone_suppress</c> call is read.</summary>
internal enum AiCallClass
{
    /// <summary>Return code 0: the module says it produced a soft-tissue image.</summary>
    Succeeded,

    /// <summary>
    /// The module refused the input before it tried (invalid input, not initialised, unsupported format). The output
    /// buffer was not written, so it holds nothing to read, and the worker's failure count is not touched.
    /// </summary>
    NotAttempted,

    /// <summary>
    /// The call was made and did not succeed (return code -3 and the like). On the worker path the module then copies the
    /// INPUT into the output and returns non-zero (ai_api.h, xpe_bone_suppress).
    /// </summary>
    Failed,
}

/// <summary>
/// Interprets the module's return code and converts pixels either way (GUI-C-184, #225 row 10).
///
/// <para><b>The rule this class exists for.</b> A failed call leaves the output equal to the input, byte for byte
/// (ai_api.h). The pixel chain decides Applied versus AppliedNoChange by comparing pixels
/// (<see cref="ProcessingChainRunner"/>), so a failure whose output were taken as a result would read as "ran, nothing to
/// change". The success of this stage is therefore decided by the RETURN CODE alone: a non-zero code is returned as
/// <c>Ran = false</c> with no pixels, and the runner never compares anything (it records RequestedNotApplied).</para>
///
/// <para><b>Assumption, not a measurement: the input scale.</b> The header requires float32 and says nothing about the
/// range; the SDD says "normalize input to [0, 1]" without saying who does it. This stage divides 16-bit pixels by 65535
/// before the call and multiplies by 65535 after it, clamping to [0, 1]. Whether the module expects that scale is
/// asked of the module's owner (QA-B-173) and is not confirmed. In a stub build no inference runs, so nothing here has
/// been observed on a real model.</para>
///
/// Free of WPF and of native calls so the integration tests link it.
/// </summary>
internal static class AiBoneSuppressionStage
{
    public const int Ok = 0;
    public const int InvalidInput = -1;
    public const int ProcessingFailed = -3;
    public const int NotInitialized = -6;
    public const int UnsupportedFormat = -7;

    /// <summary>Full scale of the 16-bit pixels, the divisor and multiplier of the assumed [0, 1] contract.</summary>
    public const float FullScale = 65535f;

    /// <summary>The label for a frame the module really processed. Shown only for a stage whose status is Applied.</summary>
    public const string ProcessedLabel = "AI-processed: bone suppression";

    /// <summary>
    /// The label for a chain: <see cref="ProcessedLabel"/> when the AI stage's status is Applied, otherwise empty.
    /// Applied means the module returned 0 AND the pixels changed (the runner's rule), so a refused or failed stage, and
    /// a stage that returned 0 with an unchanged image, never carry it. The label is read from the status, not from an
    /// alert text or a message.
    /// </summary>
    public static string LabelFor(ChainResult chain) =>
        chain.Stages.Any(stage => stage.StageId == StageIds.AiBoneSuppression && stage.Status == StageStatus.Applied)
            ? ProcessedLabel
            : string.Empty;

    public static AiCallClass Classify(int code) =>
        code == Ok ? AiCallClass.Succeeded
        : code is InvalidInput or NotInitialized or UnsupportedFormat ? AiCallClass.NotAttempted
        : AiCallClass.Failed;

    public static float[] ToFloat(ushort[] pixels)
    {
        var result = new float[pixels.Length];
        for (var i = 0; i < pixels.Length; i++)
        {
            result[i] = pixels[i] / FullScale;
        }

        return result;
    }

    /// <summary>False when any value is not finite: a number the 16-bit chain cannot carry is not a result.</summary>
    public static bool TryToUInt16(float[] values, out ushort[] pixels)
    {
        pixels = new ushort[values.Length];
        for (var i = 0; i < values.Length; i++)
        {
            if (!float.IsFinite(values[i]))
            {
                pixels = [];
                return false;
            }

            pixels[i] = (ushort)Math.Round(Math.Clamp(values[i], 0f, 1f) * FullScale);
        }

        return true;
    }

    /// <summary>The stage's answer for one <c>xpe_bone_suppress</c> call. <paramref name="output"/> is read only for code 0.</summary>
    public static StageExecution Interpret(int code, float[]? output)
    {
        switch (Classify(code))
        {
            case AiCallClass.Succeeded:
                if (output is null || !TryToUInt16(output, out var pixels))
                {
                    return new StageExecution(false, null,
                        "AI bone suppression NOT applied: xpe_bone_suppress returned 0 but its output holds values this chain cannot use; the original image is shown.");
                }

                return new StageExecution(true, pixels, "xpe_bone_suppress returned 0 (worker path).");

            case AiCallClass.NotAttempted:
                return new StageExecution(false, null,
                    $"AI bone suppression not attempted (code {code}): {RefusalMeaning(code)} The module refused the input before trying; the original image is shown.");

            default:
                return new StageExecution(false, null,
                    $"AI bone suppression NOT applied (code {code}): the original image is shown. A failed call returns the input unchanged; " +
                    "3 failures in a row switch the AI worker off for this session (see the alerts). " +
                    "A build without an inference runtime always ends here.");
        }
    }

    /// <summary>The stage's answer when <c>xpe_ai_init</c> did not return 0.</summary>
    public static StageExecution InterpretInit(int code) =>
        new(false, null, $"AI bone suppression not started: xpe_ai_init refused the configuration (code {code}); the original image is shown.");

    private static string RefusalMeaning(int code) => code switch
    {
        InvalidInput => "the input or its size was rejected.",
        NotInitialized => "the AI module is not initialised.",
        UnsupportedFormat => "the pixel format or dimensions are not supported.",
        _ => "the input was rejected.",
    };
}
