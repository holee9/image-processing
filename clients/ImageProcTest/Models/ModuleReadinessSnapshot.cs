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
        string DegradedMode = "Processing controls remain disabled.",
        string ResolvedDllPath = "")
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

    /// <summary>
    /// Reporting helpers for <see cref="ModuleReadinessSnapshot"/> (#129). Kept beside the record —
    /// and away from ModuleReadinessService, which reaches P/Invoke code — so a test project can
    /// link them and assert the shape of what gets logged.
    /// </summary>
    internal static class ModuleReadinessReporting
    {
        /// <summary>
        /// A rooted path means the probe actually located a file. The probes put the bare DLL name
        /// in DllPath when they found nothing, so anything unrooted is reported as "not resolved".
        /// </summary>
        public static string ResolvedPathOrEmpty(string? dllPath) =>
            !string.IsNullOrWhiteSpace(dllPath) && System.IO.Path.IsPathRooted(dllPath)
                ? dllPath
                : string.Empty;

        /// <summary>One line naming the file behind every module, for the log.</summary>
        public static string DescribeResolvedModules(
            System.Collections.Generic.IReadOnlyList<ModuleReadinessSnapshot> snapshots) =>
            "native modules: " + string.Join("; ", System.Linq.Enumerable.Select(snapshots, s =>
                $"{s.ModuleName}={(string.IsNullOrEmpty(s.ResolvedDllPath) ? "<not resolved>" : s.ResolvedDllPath)}"));
    }
}
