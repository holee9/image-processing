namespace ImageProcTest
{
    internal sealed record ModuleReadinessSnapshot(
        string ModuleName,
        string Level,
        string Status,
        string Evidence,
        string NextAction,
        bool ProcessingEnabled,
        string RequiredLevel = "R3",
        string DegradedMode = "Processing controls remain disabled.")
    {
        public int LevelRank => ParseLevelRank(Level);

        public string LevelDescription => LevelRank switch
        {
            0 => "R0 Not ready",
            1 => "R1 Binary/version",
            2 => "R2 ABI/export smoke",
            3 => "R3 Executable oracle",
            _ => $"{Level} Verified"
        };

        public string ExecutionState => ProcessingEnabled
            ? "Enabled"
            : "Off";

        public string DegradationReason => ProcessingEnabled
            ? "Native execution gate is open for this module."
            : $"Graceful degradation: {DegradedMode}";

        private static int ParseLevelRank(string level)
        {
            if (string.IsNullOrWhiteSpace(level) ||
                !level.StartsWith("R", StringComparison.OrdinalIgnoreCase))
            {
                return 0;
            }

            return int.TryParse(level[1..], out var rank) ? Math.Max(0, rank) : 0;
        }
    }
}
