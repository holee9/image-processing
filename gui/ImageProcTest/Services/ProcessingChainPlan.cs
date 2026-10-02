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
        return
        [
            new StageRequest(StageIds.Preprocess, settings.PreprocessInChain),
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
