namespace ImageProcTest
{
    internal sealed record ModuleReadinessSnapshot(
        string ModuleName,
        string Level,
        string Status,
        string Evidence,
        string NextAction,
        bool ProcessingEnabled);
}
