using System.Collections.Generic;

namespace ImageProcTest
{
    internal sealed record PreprocessSyntheticStageResult(
        string Stage,
        string ErrorCode,
        double LatencyMs,
        double MaxAbsError,
        bool Passed);

    internal sealed record PreprocessSyntheticOracleResult(
        string Status,
        string Details,
        bool Executed,
        bool Passed,
        double TotalLatencyMs,
        bool InputPreserved,
        string RawSha256Before,
        string RawSha256After,
        string OutputSha256,
        int NaNInfCount,
        double DeterminismRmse,
        double OutputMin,
        double OutputMax,
        IReadOnlyList<PreprocessSyntheticStageResult> Stages,
        string? TempCleanupWarning = null)
    {
        /// <summary>
        /// GUI-C-219: the oracle is running in the background and has not answered. Neither "ready" nor "not ready": <c>Passed</c> is false (so every gate that asks "did it pass" stays closed,
        /// as it does for an unconfirmed module) and <c>Status</c> says it is being checked.
        /// </summary>
        public static PreprocessSyntheticOracleResult Checking() =>
            new(
                Status: "Checking",
                Details: "The synthetic oracle is running in the background; its answer replaces this.",
                Executed: false,
                Passed: false,
                TotalLatencyMs: 0,
                InputPreserved: false,
                RawSha256Before: "",
                RawSha256After: "",
                OutputSha256: "",
                NaNInfCount: 0,
                DeterminismRmse: double.NaN,
                OutputMin: double.NaN,
                OutputMax: double.NaN,
                Stages: []);

        /// <summary>The run happened and failed (as opposed to <see cref="NotRun"/>, where it never started).</summary>
        public static PreprocessSyntheticOracleResult Failed(string status, string details) =>
            new(
                Status: status,
                Details: details,
                Executed: true,
                Passed: false,
                TotalLatencyMs: 0,
                InputPreserved: false,
                RawSha256Before: "",
                RawSha256After: "",
                OutputSha256: "",
                NaNInfCount: 0,
                DeterminismRmse: double.NaN,
                OutputMin: double.NaN,
                OutputMax: double.NaN,
                Stages: []);

        public static PreprocessSyntheticOracleResult NotRun(string details)
        {
            return new PreprocessSyntheticOracleResult(
                Status: "Not run",
                Details: details,
                Executed: false,
                Passed: false,
                TotalLatencyMs: 0,
                InputPreserved: false,
                RawSha256Before: "",
                RawSha256After: "",
                OutputSha256: "",
                NaNInfCount: 0,
                DeterminismRmse: double.NaN,
                OutputMin: double.NaN,
                OutputMax: double.NaN,
                Stages: []);
        }
    }
}
