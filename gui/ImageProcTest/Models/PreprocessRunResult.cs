namespace ImageProcTest.Models;

/// <summary>
/// What a preprocess attempt produced, or why it did not run (#141).
///
/// A refusal is a value, not an exception: a missing calibration set is an expected state (the
/// fixtures are generated at run time by xpe_calib_fixture_gen, not committed), and the Mock backend
/// has no preprocess module at all.
/// </summary>
/// <param name="Ran">True only when every stage returned XPE_OK.</param>
/// <param name="Summary">One line for the log and the status bar.</param>
/// <param name="Pixels">Corrected pixels, scaled to UInt16 for the preview.</param>
/// <param name="ProcessedPreview">Preview built from those pixels, or null when the run refused.</param>
public sealed record PreprocessRunResult(
    bool Ran,
    string Summary,
    ushort[]? Pixels,
    System.Windows.Media.Imaging.BitmapSource? ProcessedPreview = null);
