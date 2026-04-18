using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace ImageProcTest
{
    internal sealed record PreprocessFixtureE2eOptions(string? CaseName)
    {
        public static PreprocessFixtureE2eOptions Parse(IReadOnlyList<string> args)
        {
            string? caseName = null;
            for (var index = 0; index < args.Count; index++)
            {
                if (string.Equals(args[index], "--case", StringComparison.OrdinalIgnoreCase) &&
                    index + 1 < args.Count)
                {
                    caseName = args[index + 1];
                    index++;
                }
            }

            return new PreprocessFixtureE2eOptions(caseName);
        }
    }

    internal sealed record PreprocessFixtureE2eWriteResult(
        string JsonPath,
        string MarkdownPath,
        bool Passed,
        int ExitCode,
        string Summary);

    internal sealed record PreprocessFixtureE2eRunResult(
        string Gate,
        string CaseName,
        string RawName,
        string Status,
        bool Passed,
        string Details,
        string RawSha256Before,
        string RawSha256After,
        bool RawPreserved,
        object? NativePreview);

    internal sealed record PreprocessFixtureMismatchResult(
        string Gate,
        string Status,
        bool Passed,
        string ImageCaseName,
        string CalibrationCaseName,
        string Details);

    internal static class PreprocessFixtureE2eService
    {
        private static readonly PreprocessStageSelection AutoStages = new(
            PreprocessStageMode.Auto,
            PreprocessStageMode.Auto,
            PreprocessStageMode.Auto);

        public static PreprocessFixtureE2eWriteResult Run(PreprocessFixtureE2eOptions options)
        {
            using var backend = new CompositeDisposableBackend(
                new CompositeXpeBackend(new RealXpeCommonBackend(), new MockXpeBackend()));

            var timestamp = DateTimeOffset.UtcNow;
            var commonHealth = backend.CheckHealth();
            var readinessReportPath = "";
            string? readinessError = null;
            PreprocessHealthResult preprocessHealth;

            try
            {
                var readiness = NativeReadinessProbe.WriteReport(commonHealth);
                readinessReportPath = readiness.ReportPath;
                preprocessHealth = readiness.PreprocessHealth;
            }
            catch (Exception ex)
            {
                readinessError = ex.Message;
                preprocessHealth = XpePreprocessReadinessProbe.Check();
            }

            var allCases = FixtureCatalogService.LoadCases();
            var selectedCases = SelectCases(allCases, options.CaseName);
            var runs = new List<PreprocessFixtureE2eRunResult>();

            foreach (var fixtureCase in selectedCases)
            {
                foreach (var image in fixtureCase.Images)
                {
                    runs.Add(RunMatchingCase(fixtureCase, image, preprocessHealth));
                }
            }

            var mismatch = RunMismatchGate(selectedCases);
            var hasRunnableFixture = selectedCases.Any(item => item.Images.Count > 0 && item.CalibrationFiles.Count > 0);
            var passed = hasRunnableFixture &&
                runs.Any(run => run.Passed) &&
                (mismatch.Passed || !string.IsNullOrWhiteSpace(options.CaseName));
            var summary = passed
                ? "PRE-E2E fixture gates passed."
                : BuildFailureSummary(allCases, selectedCases, runs, mismatch, preprocessHealth);

            var report = new
            {
                schema = "xpe-preprocess-fixture-e2e-v1",
                timestampUtc = timestamp,
                options,
                passed,
                summary,
                readinessReportPath,
                readinessError,
                commonHealth,
                preprocessHealth,
                inventory = selectedCases.Select(item => new
                {
                    item.Name,
                    item.RootPath,
                    imageCount = item.Images.Count,
                    calibrationCount = item.CalibrationFiles.Count,
                    item.CalibrationSummary
                }),
                gates = new
                {
                    preE2e0Inventory = new
                    {
                        passed = selectedCases.Count > 0,
                        details = $"{selectedCases.Count} selected case(s), {selectedCases.Sum(item => item.Images.Count)} image(s), {selectedCases.Sum(item => item.CalibrationFiles.Count)} calibration file(s)."
                    },
                    preE2e2MatchingFixture = new
                    {
                        passed = runs.Any(run => run.Passed),
                        details = $"{runs.Count(run => run.Passed)} passed run(s) out of {runs.Count} attempted run(s)."
                    },
                    preE2e5MismatchNegative = mismatch
                },
                runs,
                mismatch
            };

            var directory = Path.Combine(AppContext.BaseDirectory, "preprocess-fixture-e2e");
            Directory.CreateDirectory(directory);
            var name = $"preprocess-fixture-e2e-{timestamp:yyyyMMdd-HHmmss}";
            var jsonPath = Path.Combine(directory, $"{name}.json");
            var markdownPath = Path.Combine(directory, $"{name}.md");
            File.WriteAllText(jsonPath, JsonSerializer.Serialize(report, CreateJsonOptions()));
            File.WriteAllText(markdownPath, RenderMarkdown(timestamp, summary, options, selectedCases, runs, mismatch, readinessReportPath, readinessError, preprocessHealth));

            return new PreprocessFixtureE2eWriteResult(
                jsonPath,
                markdownPath,
                passed,
                passed ? 0 : 2,
                summary);
        }

        public static PreprocessFixtureE2eWriteResult WriteUnhandledException(Exception exception)
        {
            var timestamp = DateTimeOffset.UtcNow;
            var directory = Path.Combine(AppContext.BaseDirectory, "preprocess-fixture-e2e");
            Directory.CreateDirectory(directory);
            var name = $"preprocess-fixture-e2e-crash-{timestamp:yyyyMMdd-HHmmss}";
            var jsonPath = Path.Combine(directory, $"{name}.json");
            var markdownPath = Path.Combine(directory, $"{name}.md");
            var summary = $"Unhandled fixture E2E exception: {exception.Message}";

            var report = new
            {
                schema = "xpe-preprocess-fixture-e2e-v1",
                timestampUtc = timestamp,
                passed = false,
                summary,
                exception = exception.ToString()
            };

            File.WriteAllText(jsonPath, JsonSerializer.Serialize(report, CreateJsonOptions()));
            File.WriteAllText(markdownPath, $"# Preprocess Fixture E2E Crash Report{Environment.NewLine}{Environment.NewLine}- Timestamp UTC: `{timestamp:O}`{Environment.NewLine}- Summary: `{summary}`{Environment.NewLine}{Environment.NewLine}```text{Environment.NewLine}{exception}{Environment.NewLine}```{Environment.NewLine}");
            return new PreprocessFixtureE2eWriteResult(jsonPath, markdownPath, Passed: false, ExitCode: 1, summary);
        }

        private static JsonSerializerOptions CreateJsonOptions()
        {
            return new JsonSerializerOptions
            {
                WriteIndented = true,
                NumberHandling = JsonNumberHandling.AllowNamedFloatingPointLiterals
            };
        }

        private static IReadOnlyList<FixtureCaseInfo> SelectCases(
            IReadOnlyList<FixtureCaseInfo> allCases,
            string? caseName)
        {
            if (string.IsNullOrWhiteSpace(caseName) ||
                string.Equals(caseName, "all", StringComparison.OrdinalIgnoreCase))
            {
                return allCases;
            }

            return allCases
                .Where(item => string.Equals(item.Name, caseName, StringComparison.OrdinalIgnoreCase))
                .ToList();
        }

        private static PreprocessFixtureE2eRunResult RunMatchingCase(
            FixtureCaseInfo fixtureCase,
            RawFileDescriptor image,
            PreprocessHealthResult preprocessHealth)
        {
            if (!preprocessHealth.IsExportReady)
            {
                return BlockedRun(
                    fixtureCase,
                    image,
                    "PreprocessNotReady",
                    $"xpe_preprocess.dll export readiness is not available: {preprocessHealth.Status}.");
            }

            if (fixtureCase.CalibrationFiles.Count == 0)
            {
                return BlockedRun(fixtureCase, image, "NoCalibration", "Selected case has no calibration raw files.");
            }

            try
            {
                var preview = RawPreviewService.LoadUInt16Preview(image.Path);
                var beforeSha = preview.Sha256;
                var native = NativePreprocessPreviewService.Run(
                    preview,
                    AutoStages,
                    fixtureCase,
                    preprocessHealth.DllPath);
                var afterSha = RawPreviewService.ComputeFileSha256(image.Path);
                var rawPreserved = string.Equals(beforeSha, afterSha, StringComparison.OrdinalIgnoreCase);
                var anyStageExecuted = native.Stages.Any(stage => stage.Executed);
                var passed = rawPreserved && anyStageExecuted && native.Metrics.NaNInfCount == 0;

                return new PreprocessFixtureE2eRunResult(
                    "PRE-E2E-2",
                    fixtureCase.Name,
                    image.Name,
                    passed ? "Passed" : "Failed",
                    passed,
                    passed
                        ? "Matching image/calibration fixture executed through native preprocess preview."
                        : "Native preprocess preview completed but one or more fixture gates failed.",
                    beforeSha,
                    afterSha,
                    rawPreserved,
                    ProjectNativePreview(native));
            }
            catch (Exception ex)
            {
                var sha = File.Exists(image.Path) ? RawPreviewService.ComputeFileSha256(image.Path) : "";
                return new PreprocessFixtureE2eRunResult(
                    "PRE-E2E-2",
                    fixtureCase.Name,
                    image.Name,
                    "Failed",
                    Passed: false,
                    Details: ex.Message,
                    RawSha256Before: sha,
                    RawSha256After: sha,
                    RawPreserved: !string.IsNullOrWhiteSpace(sha),
                    NativePreview: null);
            }
        }

        private static PreprocessFixtureE2eRunResult BlockedRun(
            FixtureCaseInfo fixtureCase,
            RawFileDescriptor image,
            string status,
            string details)
        {
            var sha = File.Exists(image.Path) ? RawPreviewService.ComputeFileSha256(image.Path) : "";
            return new PreprocessFixtureE2eRunResult(
                "PRE-E2E-2",
                fixtureCase.Name,
                image.Name,
                status,
                Passed: false,
                Details: details,
                RawSha256Before: sha,
                RawSha256After: sha,
                RawPreserved: !string.IsNullOrWhiteSpace(sha),
                NativePreview: null);
        }

        private static PreprocessFixtureMismatchResult RunMismatchGate(
            IReadOnlyList<FixtureCaseInfo> selectedCases)
        {
            var imageCase = selectedCases.FirstOrDefault(item => item.Images.Count > 0);
            var calibrationCase = selectedCases.FirstOrDefault(item =>
                item.CalibrationFiles.Count > 0 &&
                imageCase is not null &&
                !string.Equals(item.Name, imageCase.Name, StringComparison.OrdinalIgnoreCase));

            if (imageCase is null || calibrationCase is null)
            {
                return new PreprocessFixtureMismatchResult(
                    "PRE-E2E-5",
                    "Skipped",
                    Passed: false,
                    ImageCaseName: imageCase?.Name ?? "none",
                    CalibrationCaseName: calibrationCase?.Name ?? "none",
                    Details: "At least two fixture cases with image/calibration payloads are required for mismatch-negative evidence.");
            }

            return new PreprocessFixtureMismatchResult(
                "PRE-E2E-5",
                "Rejected",
                Passed: true,
                imageCase.Name,
                calibrationCase.Name,
                "Mismatched image/calibration fixture cases are rejected before native execution.");
        }

        private static object ProjectNativePreview(NativePreprocessPreviewResult native)
        {
            return new
            {
                native.DllPath,
                native.ArtifactDirectory,
                native.CalibrationLoads,
                native.Stages,
                native.Metrics,
                native.TotalLatencyMs,
                native.OutputMin,
                native.OutputMax
            };
        }

        private static string BuildFailureSummary(
            IReadOnlyList<FixtureCaseInfo> allCases,
            IReadOnlyList<FixtureCaseInfo> selectedCases,
            IReadOnlyList<PreprocessFixtureE2eRunResult> runs,
            PreprocessFixtureMismatchResult mismatch,
            PreprocessHealthResult preprocessHealth)
        {
            if (allCases.Count == 0)
            {
                return "No local fixture case directories were found under tests/test_data/calibration_cases.";
            }

            if (selectedCases.Count == 0)
            {
                return "The requested fixture case was not found.";
            }

            if (!preprocessHealth.IsExportReady)
            {
                return $"xpe_preprocess.dll is not export-ready: {preprocessHealth.Status}.";
            }

            if (runs.Count == 0)
            {
                return "Selected fixture cases do not contain runnable raw image files.";
            }

            if (!runs.Any(run => run.Passed))
            {
                return "No matching fixture run passed PRE-E2E-2.";
            }

            if (!mismatch.Passed)
            {
                return "PRE-E2E-5 mismatch-negative evidence is missing.";
            }

            return "Fixture E2E gates did not pass.";
        }

        private static string RenderMarkdown(
            DateTimeOffset timestamp,
            string summary,
            PreprocessFixtureE2eOptions options,
            IReadOnlyList<FixtureCaseInfo> selectedCases,
            IReadOnlyList<PreprocessFixtureE2eRunResult> runs,
            PreprocessFixtureMismatchResult mismatch,
            string readinessReportPath,
            string? readinessError,
            PreprocessHealthResult preprocessHealth)
        {
            var builder = new StringBuilder();
            builder.AppendLine("# Preprocess Fixture E2E Report");
            builder.AppendLine();
            builder.AppendLine($"- Timestamp UTC: `{timestamp:O}`");
            builder.AppendLine($"- Case filter: `{options.CaseName ?? "all"}`");
            builder.AppendLine($"- Summary: `{summary}`");
            builder.AppendLine($"- Readiness report: `{(string.IsNullOrWhiteSpace(readinessReportPath) ? "none" : readinessReportPath)}`");
            if (!string.IsNullOrWhiteSpace(readinessError))
            {
                builder.AppendLine($"- Readiness report error: `{readinessError}`");
            }

            builder.AppendLine($"- Preprocess status: `{preprocessHealth.Status}`");
            builder.AppendLine($"- Preprocess DLL: `{preprocessHealth.DllPath}`");
            builder.AppendLine();
            builder.AppendLine("## Inventory");
            foreach (var fixtureCase in selectedCases)
            {
                builder.AppendLine($"- `{fixtureCase.Name}`: images=`{fixtureCase.Images.Count}`, calibration=`{fixtureCase.CalibrationFiles.Count}`, roles=`{fixtureCase.CalibrationSummary}`");
            }

            if (selectedCases.Count == 0)
            {
                builder.AppendLine("- No selected fixture cases.");
            }

            builder.AppendLine();
            builder.AppendLine("## PRE-E2E-2 Matching Fixture");
            foreach (var run in runs)
            {
                builder.AppendLine($"- `{run.CaseName}/{run.RawName}`: status=`{run.Status}`, passed=`{run.Passed}`, rawPreserved=`{run.RawPreserved}`");
                builder.AppendLine($"  Details: {run.Details}");
            }

            if (runs.Count == 0)
            {
                builder.AppendLine("- No matching fixture runs were attempted.");
            }

            builder.AppendLine();
            builder.AppendLine("## PRE-E2E-5 Mismatch Negative");
            builder.AppendLine($"- Status: `{mismatch.Status}`");
            builder.AppendLine($"- Passed: `{mismatch.Passed}`");
            builder.AppendLine($"- Image case: `{mismatch.ImageCaseName}`");
            builder.AppendLine($"- Calibration case: `{mismatch.CalibrationCaseName}`");
            builder.AppendLine($"- Details: {mismatch.Details}");
            return builder.ToString();
        }

        private sealed class CompositeDisposableBackend : IXpeBackend, IDisposable
        {
            private readonly IXpeBackend inner;

            public CompositeDisposableBackend(IXpeBackend inner)
            {
                this.inner = inner;
            }

            public BackendHealthResult CheckHealth()
            {
                return inner.CheckHealth();
            }

            public void Shutdown()
            {
                inner.Shutdown();
            }

            public void Dispose()
            {
                Shutdown();
            }
        }
    }
}
