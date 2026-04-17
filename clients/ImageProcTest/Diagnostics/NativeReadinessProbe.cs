using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text.Json;

namespace ImageProcTest
{
    internal sealed record NativeReadinessReportWriteResult(
        string ReportPath,
        string DisplaySummary,
        string PreprocessSummary,
        PreprocessHealthResult PreprocessHealth);

    internal sealed record DisplayHealthResult(
        string Status,
        string Version,
        string DllPath,
        string Details,
        bool IsReady);

    internal sealed record PreprocessHealthResult(
        string Status,
        string Version,
        string DllPath,
        string Details,
        IReadOnlyList<string> PresentExports,
        IReadOnlyList<string> MissingExports,
        IReadOnlyList<string> MissingExecutionExports,
        PreprocessSyntheticOracleResult SyntheticOracle,
        bool IsVersionReady,
        bool IsExportReady,
        bool IsSyntheticOracleReady);

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

    internal static class NativeReadinessProbe
    {
        public static NativeReadinessReportWriteResult WriteReport(BackendHealthResult commonResult)
        {
            var display = XpeDisplayVersionProbe.Check();
            var preprocess = XpePreprocessReadinessProbe.Check();
            var report = new
            {
                schema = "xpe-native-readiness-v1",
                timestampUtc = DateTimeOffset.UtcNow,
                common = commonResult,
                abi = new
                {
                    imageBufferSize = Marshal.SizeOf<XpeCommonApi.XpeImageBuffer>(),
                    imageMetadataSize = Marshal.SizeOf<XpeCommonApi.XpeImageMetadata>()
                },
                display,
                preprocess
            };

            var path = Path.Combine(AppContext.BaseDirectory, "native-readiness-report.json");
            var json = JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true });
            File.WriteAllText(path, json);

            return new NativeReadinessReportWriteResult(
                path,
                $"{display.Status} ({display.Version})",
                $"{preprocess.Status} ({preprocess.Version}); smoke={preprocess.SyntheticOracle.Status}",
                preprocess);
        }
    }
}
