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
        IReadOnlyList<PreprocessSyntheticStageResult> Stages)
    {
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
