using System;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.Json.Serialization;

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
        IReadOnlyList<PreprocessParameterRangeResult> ParameterRanges,
        bool IsVersionReady,
        bool IsExportReady,
        bool IsSyntheticOracleReady);

    internal sealed record PreprocessParameterRangeResult(
        string ParamName,
        string ErrorCode,
        float MinValue,
        float MaxValue,
        bool Passed,
        string Details);

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
            var json = JsonSerializer.Serialize(report, new JsonSerializerOptions
            {
                WriteIndented = true,
                NumberHandling = JsonNumberHandling.AllowNamedFloatingPointLiterals
            });
            File.WriteAllText(path, json);

            return new NativeReadinessReportWriteResult(
                path,
                $"{display.Status} ({display.Version})",
                $"{preprocess.Status} ({preprocess.Version}); smoke={preprocess.SyntheticOracle.Status}; params={FormatPreprocessParameterRanges(preprocess.ParameterRanges)}",
                preprocess);
        }

        public static string FormatPreprocessParameterRanges(IReadOnlyList<PreprocessParameterRangeResult> ranges)
        {
            if (ranges.Count == 0)
            {
                return "not available";
            }

            var passed = 0;
            foreach (var range in ranges)
            {
                if (range.Passed)
                {
                    passed++;
                }
            }

            var summary = string.Join(", ", ranges.Select(range =>
                range.Passed
                    ? $"{range.ParamName}={range.MinValue:0.###}..{range.MaxValue:0.###}"
                    : $"{range.ParamName}={range.ErrorCode}"));

            return $"{passed}/{ranges.Count} ok; {summary}";
        }
    }
}
