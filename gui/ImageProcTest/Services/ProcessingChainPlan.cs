// #180 / #173 (GUI-C-99): which stages the chain runs, derived from the settings.
using ImageProcTest.Models;

namespace ImageProcTest.Services;

/// <summary>
/// The ordered stage list for one chain run. Kept apart from the view model so the settings it reads are
/// visible to the processing survey (SettingsProcessingConnectionTests) as processing reads.
/// </summary>
public static class ProcessingChainPlan
{
    public static IReadOnlyList<StageRequest> BuildStages(AppSettings settings)
    {
        ArgumentNullException.ThrowIfNull(settings);

        // Order (#180, GUI-C-101): preprocess, then GSVG, then (#225 row 10, GUI-C-184) AI bone suppression, then the
        // display pipeline.
        // GUI-C-234 (pipeline-spec 6.1, release mode): (5) log, (6) bilateral noise, (7) CLAHE and (8) USM are mandatory and cannot be skipped, so EnhanceBasic is in the ordinary chain with no setting that
        // turns it off. It follows preprocess (the corrected image is what is enhanced) and precedes the assistive stages.
        return
        [
            new StageRequest(StageIds.Preprocess, settings.PreprocessInChain),
            new StageRequest(StageIds.EnhanceBasic, true),
            new StageRequest(StageIds.Gsvg, GsvgModes.Normalize(settings.GsvgMode) != GsvgModes.None),
            new StageRequest(StageIds.AiBoneSuppression, settings.AiBoneSuppressionInChain),
        ];
    }

    /// <summary>
    /// The Deterministic Baseline's chain (#225 row 9, GUI-C-196, design D3): a FIXED list, the stages of pipeline-spec 1b that the chain carries
    /// (preprocess, then basic enhancement). It reads no setting, so the user's toggles cannot add or drop a stage, and the assistive stages
    /// (GSVG, AI) are never in it (pipeline-spec 5.3: an assistive output must not replace the deterministic baseline image).
    /// </summary>
    public static IReadOnlyList<StageRequest> BuildBaselineStages() =>
    [
        new StageRequest(StageIds.Preprocess, true),
        new StageRequest(StageIds.EnhanceBasic, true),
    ];
}
