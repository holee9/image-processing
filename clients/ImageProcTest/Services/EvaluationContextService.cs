namespace ImageProcTest
{
    internal sealed class EvaluationContextService
    {
        public ActiveEvaluationContext Build(
            FixtureCaseInfo? calibrationContext,
            string? calibrationFolderPath,
            RawPreviewResult? rawPreview,
            AlgorithmChainPlan algorithmChain,
            PreprocessStageSelection stageSelection)
        {
            return new ActiveEvaluationContext(
                calibrationContext,
                calibrationFolderPath,
                rawPreview,
                algorithmChain,
                stageSelection);
        }
    }
}
