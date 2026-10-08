// #225 row 9 (GUI-C-196 M1): the fixed parameters of the Deterministic Baseline. Free of WPF so the integration tests link it.
using ImageProcTest.Models;

namespace ImageProcTest.Services;

/// <summary>
/// Everything the baseline command uses that a user could otherwise change, fixed in one place (D2/D3/D7 of the GUI-C-196 design, approved by the lead).
/// Two runs on the same input must give the same output, so nothing here comes from the user's settings, and the same values go to every run.
///
/// <para><b>The enhance parameters are the module's own documented defaults</b> (enhance_basic_api.h, the "default" in each parameter comment):
/// bilateral noise reduction sigma_space 3.0 and sigma_range 50.0; CLAHE clip limit 3.0 with an 8 x 8 tile grid; unsharp masking amount 0.5, radius 2.0,
/// threshold 10.0. Whether those defaults suit an image in the 0..65535 range after the log transform has NOT been checked; that is image quality,
/// which this command does not judge.</para>
///
/// <para><b>The log normalisation factor is an ASSUMPTION with no basis in any specification, valid for the baseline command only.</b> The module computes
/// <c>normFactor * log10(x + 1)</c> (xpe_log_transform); no document gives a value (docs/enhance-basic/README.md shows the second argument as "epsilon", which
/// contradicts the API). <c>65535 / log10(65536)</c> maps the 16-bit full scale to the 16-bit full scale, so the display stage that follows keeps the pixel range
/// its default window assumes. A different value, if the lead sets one, changes this constant and nothing else.</para>
/// </summary>
public static class BaselineParameters
{
    /// <summary>ASSUMPTION (see the type remarks): 65535 / log10(65536), about 13652.</summary>
    public static readonly float LogNormFactor = (float)(65535.0 / Math.Log10(65536.0));

    public const float NoiseSigmaSpace = 3.0f;
    public const float NoiseSigmaRange = 50.0f;

    public const float ClaheClipLimit = 3.0f;
    public const int ClaheTileWidth = 8;
    public const int ClaheTileHeight = 8;

    public const float UsmAmount = 0.5f;
    public const float UsmRadius = 2.0f;
    public const float UsmThreshold = 10.0f;

    /// <summary>The display settings of the baseline: linear modality (slope 1, intercept 0), the default linear VOI window, no GSDF.</summary>
    public const float ModalityRescaleSlope = 1.0f;
    public const float ModalityRescaleIntercept = 0.0f;
    public const string VoiLutMode = "Linear";
    public const float VoiWindowCenter = 32768.0f;
    public const float VoiWindowWidth = 65535.0f;
    public const bool GsdfEnabled = false;

    /// <summary>
    /// A copy of <paramref name="user"/> whose DISPLAY settings are the fixed ones above (D7): a user's window, LUT mode or GSDF choice is never read by the
    /// baseline. What stays is what the preprocess stage needs to find its inputs (calibration directories and modes, body part, kVp, pixel pitch).
    /// The input is not modified.
    /// </summary>
    public static AppSettings ForBaseline(AppSettings user)
    {
        ArgumentNullException.ThrowIfNull(user);

        var settings = user.Snapshot();
        settings.ModalityRescaleSlope = ModalityRescaleSlope;
        settings.ModalityRescaleIntercept = ModalityRescaleIntercept;
        settings.VoiLutMode = VoiLutMode;
        settings.VoiWindowCenter = VoiWindowCenter;
        settings.VoiWindowWidth = VoiWindowWidth;
        settings.VoiWindowAuto = false;   // GUI-C-233: the Baseline's window is fixed, never the automatic one
        settings.GsdfEnabled = GsdfEnabled;
        return settings;
    }
}
