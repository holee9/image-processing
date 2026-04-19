namespace ImageProcTest
{
    internal sealed record AlgorithmValidationItem(
        string ModuleName,
        string SwuId,
        string AlgorithmName,
        string RequirementIds,
        string TestIds,
        string Gate,
        string Adapter,
        string Level,
        string Status,
        string Evidence,
        string NextAction,
        bool CanRun,
        string? StageKey);

    internal sealed record AlgorithmValidationRunSnapshot(
        string SwuId,
        string AlgorithmName,
        string Status,
        string Details,
        string? ArtifactDirectory,
        double? LatencyMs);

    internal sealed record UserEvaluationSnapshot(
        string AlgorithmKey,
        string Evaluator,
        string Verdict,
        string Notes,
        string EvidenceSummary);
}
