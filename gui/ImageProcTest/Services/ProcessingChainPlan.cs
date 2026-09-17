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
        return [new StageRequest(StageIds.Preprocess, settings.PreprocessInChain)];
    }
}
