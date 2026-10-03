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
        string DicomSummary,
        string GsvgSummary,
        string PreprocessSummary,
        PreprocessHealthResult PreprocessHealth);

    internal sealed record DisplaySmokeResult(
        string Status,
        bool Passed,
        string Details)
    {
        public static DisplaySmokeResult NotRun(string reason) =>
            new("Not run", false, reason);
    }

    internal sealed record DisplayHealthResult(
        string Status,
        string Version,
        string DllPath,
        string Details,
        IReadOnlyList<string> PresentExports,
        IReadOnlyList<string> MissingExports,
        DisplaySmokeResult Smoke,
        bool IsVersionReady,
        bool IsExportReady,
        bool IsSmokeReady)
    {
        public bool IsReady => IsVersionReady;
    }

    internal sealed record DicomSmokeResult(
        string Status,
        bool Passed,
        string Details)
    {
        public static DicomSmokeResult NotRun(string reason) =>
            new("Not run", false, reason);
    }

    internal sealed record DicomHealthResult(
        string Status,
        string DllPath,
        string Details,
        IReadOnlyList<string> PresentExports,
        IReadOnlyList<string> MissingExports,
        DicomSmokeResult Smoke,
        bool IsExportReady,
        bool IsSmokeReady);

    internal sealed record GsvgHealthResult(
        string Status,
        string Version,
        string DllPath,
        string Details,
        bool IsVersionReady);

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
        bool IsSyntheticOracleReady,
        bool IsSyntheticOracleChecking = false);

    internal sealed record PreprocessParameterRangeResult(
        string ParamName,
        string ErrorCode,
        float MinValue,
        float MaxValue,
        bool Passed,
        string Details);

    internal static class NativeReadinessProbe
    {
        /// <param name="commonResult">The common backend's health.</param>
        /// <param name="waitForOracle">
        /// True (headless callers): wait for the synthetic oracle. False (the window, GUI-C-219): never wait; a verdict not yet in is reported as "checking" and the window is told when it arrives.
        /// </param>
        public static NativeReadinessReportWriteResult WriteReport(BackendHealthResult commonResult, bool waitForOracle = true)
        {
            var display = XpeDisplayVersionProbe.Check();
            var dicom = XpeDicomReadinessProbe.Check();
            var gsvg = XpeGsvgReadinessProbe.Check();
            var preprocess = XpePreprocessReadinessProbe.Check(waitForOracle);
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
                dicom,
                gsvg,
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
                $"{display.Status} ({display.Version}); smoke={display.Smoke.Status}",
                $"{dicom.Status}; smoke={dicom.Smoke.Status}",
                $"{gsvg.Status} ({gsvg.Version})",
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
