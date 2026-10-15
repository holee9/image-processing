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
        PreprocessHealthResult PreprocessHealth,
        string? ReportJson = null);

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
        private static readonly object ReportFileLock = new();

        /// <param name="commonResult">The common backend's health.</param>
        /// <param name="waitForOracle">
        /// True (headless callers): wait for the synthetic oracle. False (the window, GUI-C-219): never wait; a verdict not yet in is reported as "checking" and the window is told when it arrives.
        /// </param>
        public static NativeReadinessReportWriteResult WriteReport(BackendHealthResult commonResult, bool waitForOracle = true)
        {
            var report = BuildReport(commonResult, waitForOracle);
            PublishReport(report);
            return report;
        }

        /// <summary>
        /// GUI-C-235b (Codex #186): builds the report (the probing and the JSON text) WITHOUT writing it. The window builds on the thread pool at start-up and publishes later, on the UI thread, once it knows the
        /// result is still the newest - a build that has been overtaken is simply never published, so the file cannot go back to an older answer than the screen shows.
        /// </summary>
        public static NativeReadinessReportWriteResult BuildReport(BackendHealthResult commonResult, bool waitForOracle = true)
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
            return new NativeReadinessReportWriteResult(
                path,
                $"{display.Status} ({display.Version}); smoke={display.Smoke.Status}",
                $"{dicom.Status}; smoke={dicom.Smoke.Status}",
                $"{gsvg.Status} ({gsvg.Version})",
                $"{preprocess.Status} ({preprocess.Version}); smoke={preprocess.SyntheticOracle.Status}; params={FormatPreprocessParameterRanges(preprocess.ParameterRanges)}",
                preprocess,
                json);
        }

        /// <summary>
        /// Writes a built report to its file: complete into a temporary file first, then swapped in, so a reader never sees half a report, under one lock so two publishers in this process never write at once
        /// (GUI-C-235: the second got "being used by another process" and the window said "Report generation skipped"). The ORDER of publishing is the caller's: the window publishes on the UI thread only.
        /// </summary>
        public static void PublishReport(NativeReadinessReportWriteResult report)
        {
            if (report.ReportJson is null) throw new InvalidOperationException("The report was not built with its JSON text.");
            lock (ReportFileLock)
            {
                var temporary = report.ReportPath + "." + Guid.NewGuid().ToString("N") + ".tmp";
                try
                {
                    File.WriteAllText(temporary, report.ReportJson);
                    File.Move(temporary, report.ReportPath, overwrite: true);
                }
                finally
                {
                    try { File.Delete(temporary); } catch (IOException) { }
                }
            }
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
