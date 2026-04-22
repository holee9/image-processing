using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace ImageProcTest
{
    internal sealed record Phase1bFixtureE2eOptions(string? CaseName, int MaxRuns)
    {
        public static Phase1bFixtureE2eOptions Parse(IReadOnlyList<string> args)
        {
            string? caseName = null;
            var maxRuns = 3;
            for (var index = 0; index < args.Count; index++)
            {
                if (string.Equals(args[index], "--case", StringComparison.OrdinalIgnoreCase) &&
                    index + 1 < args.Count)
                {
                    caseName = args[index + 1];
                    index++;
                }
                else if (string.Equals(args[index], "--max-runs", StringComparison.OrdinalIgnoreCase) &&
                    index + 1 < args.Count &&
                    int.TryParse(args[index + 1], out var parsedMaxRuns))
                {
                    maxRuns = Math.Max(1, parsedMaxRuns);
                    index++;
                }
            }

            return new Phase1bFixtureE2eOptions(caseName, maxRuns);
        }
    }

    internal sealed record Phase1bFixtureE2eWriteResult(
        string JsonPath,
        string MarkdownPath,
        bool Passed,
        int ExitCode,
        string Summary);

    internal sealed record Phase1bFixtureE2eRunResult(
        string Gate,
        string CaseName,
        string RawName,
        string Status,
        bool Passed,
        string Details,
        string RawSha256Before,
        string RawSha256After,
        bool RawPreserved,
        object? Preprocess,
        object? EnhanceBasic,
        object? PresentationExport);

    internal static class Phase1bFixtureE2eService
    {
        private static readonly PreprocessStageSelection AutoPreprocessStages = new(
            PreprocessStageMode.Auto,
            PreprocessStageMode.Auto,
            PreprocessStageMode.Auto);

        private static readonly EnhanceBasicStageSelection FullEnhanceBasicStages = new(
            ExposureIndex: true,
            Log: true,
            Noise: true,
            Contrast: true,
            Edge: true);

        private static readonly string[] PreprocessStageOrder = ["offset", "gain", "defect"];
        private static readonly string[] EnhanceBasicStageOrder = ["ei-whole", "log", "basic-noise", "contrast", "edge"];
        private static readonly string[] DisplayStageOrder = ["modality-lut", "voi-lut", "presentation-lut"];
        private static readonly string[] DicomStageOrder = ["dicom-write"];

        public static Phase1bFixtureE2eWriteResult Run(Phase1bFixtureE2eOptions options)
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

            var enhanceHealth = XpeEnhanceBasicReadinessProbe.Check();
            var displayHealth = XpeDisplayVersionProbe.Check();
            var dicomHealth = XpeDicomReadinessProbe.Check();
            var allCases = FixtureCatalogService.LoadCases();
            var selectedCases = SelectCases(allCases, options.CaseName);
            var runs = new List<Phase1bFixtureE2eRunResult>();

            foreach (var fixtureCase in selectedCases)
            {
                foreach (var image in fixtureCase.Images)
                {
                    if (runs.Count >= options.MaxRuns)
                    {
                        break;
                    }

                    runs.Add(RunMatchingCase(
                        fixtureCase,
                        image,
                        preprocessHealth,
                        enhanceHealth,
                        displayHealth,
                        dicomHealth));
                }

                if (runs.Count >= options.MaxRuns)
                {
                    break;
                }
            }

            var passed = runs.Any(run => run.Passed);
            var summary = passed
                ? $"PHASE1B-E2E full pipeline passed for {runs.Count(run => run.Passed)}/{runs.Count} fixture run(s)."
                : BuildFailureSummary(allCases, selectedCases, runs, preprocessHealth, enhanceHealth, displayHealth, dicomHealth);

            var report = new
            {
                schema = "xpe-phase1b-fixture-e2e-v1",
                timestampUtc = timestamp,
                options,
                passed,
                summary,
                readinessReportPath,
                readinessError,
                commonHealth,
                preprocessHealth,
                enhanceHealth,
                displayHealth,
                dicomHealth,
                stageOrder = new
                {
                    preprocess = PreprocessStageOrder,
                    enhanceBasic = EnhanceBasicStageOrder,
                    display = DisplayStageOrder,
                    dicom = DicomStageOrder
                },
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
                    phase1b0Inventory = new
                    {
                        passed = selectedCases.Count > 0,
                        details = $"{selectedCases.Count} selected case(s), {selectedCases.Sum(item => item.Images.Count)} image(s), {selectedCases.Sum(item => item.CalibrationFiles.Count)} calibration file(s)."
                    },
                    phase1b1NativeReadiness = new
                    {
                        passed = preprocessHealth.IsExportReady &&
                            enhanceHealth.IsSmokeReady &&
                            displayHealth.IsSmokeReady &&
                            dicomHealth.IsSmokeReady,
                        preprocess = preprocessHealth.Status,
                        enhanceBasic = enhanceHealth.Status,
                        display = displayHealth.Status,
                        dicom = dicomHealth.Status
                    },
                    phase1b4FullPipeline = new
                    {
                        passed,
                        details = $"{runs.Count(run => run.Passed)} passed run(s) out of {runs.Count} attempted run(s)."
                    }
                },
                runs
            };

            var directory = Path.Combine(AppContext.BaseDirectory, "phase1b-fixture-e2e");
            Directory.CreateDirectory(directory);
            var name = $"phase1b-fixture-e2e-{timestamp:yyyyMMdd-HHmmss}";
            var jsonPath = Path.Combine(directory, $"{name}.json");
            var markdownPath = Path.Combine(directory, $"{name}.md");
            File.WriteAllText(jsonPath, JsonSerializer.Serialize(report, CreateJsonOptions()));
            File.WriteAllText(markdownPath, RenderMarkdown(
                timestamp,
                summary,
                options,
                selectedCases,
                runs,
                readinessReportPath,
                readinessError,
                preprocessHealth,
                enhanceHealth,
                displayHealth,
                dicomHealth));

            return new Phase1bFixtureE2eWriteResult(
                jsonPath,
                markdownPath,
                passed,
                passed ? 0 : 2,
                summary);
        }

        public static Phase1bFixtureE2eWriteResult WriteUnhandledException(Exception exception)
        {
            var timestamp = DateTimeOffset.UtcNow;
            var directory = Path.Combine(AppContext.BaseDirectory, "phase1b-fixture-e2e");
            Directory.CreateDirectory(directory);
            var name = $"phase1b-fixture-e2e-crash-{timestamp:yyyyMMdd-HHmmss}";
            var jsonPath = Path.Combine(directory, $"{name}.json");
            var markdownPath = Path.Combine(directory, $"{name}.md");
            var summary = $"Unhandled Phase 1b fixture E2E exception: {exception.Message}";

            var report = new
            {
                schema = "xpe-phase1b-fixture-e2e-v1",
                timestampUtc = timestamp,
                passed = false,
                summary,
                exception = exception.ToString()
            };

            File.WriteAllText(jsonPath, JsonSerializer.Serialize(report, CreateJsonOptions()));
            File.WriteAllText(markdownPath, $"# Phase 1b Fixture E2E Crash Report{Environment.NewLine}{Environment.NewLine}- Timestamp UTC: `{timestamp:O}`{Environment.NewLine}- Summary: `{summary}`{Environment.NewLine}{Environment.NewLine}```text{Environment.NewLine}{exception}{Environment.NewLine}```{Environment.NewLine}");
            return new Phase1bFixtureE2eWriteResult(jsonPath, markdownPath, Passed: false, ExitCode: 1, summary);
        }

        private static Phase1bFixtureE2eRunResult RunMatchingCase(
            FixtureCaseInfo fixtureCase,
            RawFileDescriptor image,
            PreprocessHealthResult preprocessHealth,
            EnhanceBasicHealthResult enhanceHealth,
            DisplayHealthResult displayHealth,
            DicomHealthResult dicomHealth)
        {
            if (!preprocessHealth.IsExportReady)
            {
                return BlockedRun(fixtureCase, image, "PreprocessNotReady", $"xpe_preprocess.dll export readiness is not available: {preprocessHealth.Status}.");
            }

            if (!enhanceHealth.IsSmokeReady)
            {
                return BlockedRun(fixtureCase, image, "EnhanceBasicNotReady", $"xpe_enhance_basic.dll smoke readiness is not available: {enhanceHealth.Status}.");
            }

            if (!displayHealth.IsSmokeReady)
            {
                return BlockedRun(fixtureCase, image, "DisplayNotReady", $"xpe_display.dll smoke readiness is not available: {displayHealth.Status}.");
            }

            if (!dicomHealth.IsSmokeReady)
            {
                return BlockedRun(fixtureCase, image, "DicomNotReady", $"xpe_dicom.dll smoke readiness is not available: {dicomHealth.Status}.");
            }

            if (fixtureCase.CalibrationFiles.Count == 0)
            {
                return BlockedRun(fixtureCase, image, "NoCalibration", "Selected case has no calibration raw files.");
            }

            try
            {
                var preview = RawPreviewService.LoadUInt16Preview(image.Path);
                var beforeSha = preview.Sha256;
                var preprocess = NativePreprocessPreviewService.Run(
                    preview,
                    AutoPreprocessStages,
                    fixtureCase,
                    preprocessHealth.DllPath,
                    PreprocessStageOrder);
                var enhance = NativeEnhanceBasicPreviewService.Run(
                    preview,
                    preprocess.OutputPixels,
                    "preprocess-output",
                    FullEnhanceBasicStages,
                    EnhanceBasicStageParameters.Default,
                    enhanceHealth.DllPath,
                    EnhanceBasicStageOrder);
                var artifactDirectory = Path.Combine(
                    preprocess.ArtifactDirectory,
                    "phase1b-display-dicom");
                var presentation = NativePresentationExportService.Run(
                    preview,
                    enhance.OutputPixels,
                    "enhance-basic-output",
                    DisplayStageOrder,
                    DicomStageOrder,
                    artifactDirectory);
                var afterSha = RawPreviewService.ComputeFileSha256(image.Path);
                var rawPreserved = string.Equals(beforeSha, afterSha, StringComparison.OrdinalIgnoreCase);
                var passed = rawPreserved &&
                    preprocess.Stages.Any(stage => stage.Executed) &&
                    enhance.Stages.Any(stage => stage.Executed) &&
                    presentation.Stages.Any(stage => stage.Executed) &&
                    presentation.DicomPassed &&
                    preprocess.Metrics.NaNInfCount == 0 &&
                    enhance.Metrics.NaNInfCount == 0 &&
                    presentation.Metrics.NaNInfCount == 0;

                return new Phase1bFixtureE2eRunResult(
                    "PHASE1B-E2E-4",
                    fixtureCase.Name,
                    image.Name,
                    passed ? "Passed" : "Failed",
                    passed,
                    passed
                        ? "Native preprocess, enhance_basic, display LUT, and DICOM write/validate executed on one fixture preview."
                        : "Full native pipeline completed but one or more gates failed.",
                    beforeSha,
                    afterSha,
                    rawPreserved,
                    ProjectPreprocess(preprocess),
                    ProjectEnhanceBasic(enhance),
                    ProjectPresentation(presentation));
            }
            catch (Exception ex)
            {
                var sha = File.Exists(image.Path) ? RawPreviewService.ComputeFileSha256(image.Path) : "";
                return new Phase1bFixtureE2eRunResult(
                    "PHASE1B-E2E-4",
                    fixtureCase.Name,
                    image.Name,
                    "Failed",
                    Passed: false,
                    Details: ex.Message,
                    RawSha256Before: sha,
                    RawSha256After: sha,
                    RawPreserved: !string.IsNullOrWhiteSpace(sha),
                    Preprocess: null,
                    EnhanceBasic: null,
                    PresentationExport: null);
            }
        }

        private static Phase1bFixtureE2eRunResult BlockedRun(
            FixtureCaseInfo fixtureCase,
            RawFileDescriptor image,
            string status,
            string details)
        {
            var sha = File.Exists(image.Path) ? RawPreviewService.ComputeFileSha256(image.Path) : "";
            return new Phase1bFixtureE2eRunResult(
                "PHASE1B-E2E-4",
                fixtureCase.Name,
                image.Name,
                status,
                Passed: false,
                Details: details,
                RawSha256Before: sha,
                RawSha256After: sha,
                RawPreserved: !string.IsNullOrWhiteSpace(sha),
                Preprocess: null,
                EnhanceBasic: null,
                PresentationExport: null);
        }

        private static object ProjectPreprocess(NativePreprocessPreviewResult native)
        {
            return new
            {
                native.DllPath,
                native.ArtifactDirectory,
                native.CalibrationLoads,
                native.Stages,
                native.Metrics,
                native.DetectorMetrics,
                native.TotalLatencyMs,
                native.OutputMin,
                native.OutputMax
            };
        }

        private static object ProjectEnhanceBasic(NativeEnhanceBasicPreviewResult native)
        {
            return new
            {
                native.DllPath,
                native.InputSource,
                native.Stages,
                native.Metrics,
                native.TotalLatencyMs,
                native.OutputMin,
                native.OutputMax,
                native.ExposureIndex,
                native.DeviationIndex,
                native.SigmaBefore,
                native.SigmaAfter
            };
        }

        private static object ProjectPresentation(NativePresentationExportResult native)
        {
            return new
            {
                native.DisplayDllPath,
                native.DicomDllPath,
                native.CommonDllPath,
                native.InputSource,
                native.ArtifactDirectory,
                native.Stages,
                native.Metrics,
                native.TotalLatencyMs,
                native.OutputMin,
                native.OutputMax,
                native.DicomValidation
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

        private static string BuildFailureSummary(
            IReadOnlyList<FixtureCaseInfo> allCases,
            IReadOnlyList<FixtureCaseInfo> selectedCases,
            IReadOnlyList<Phase1bFixtureE2eRunResult> runs,
            PreprocessHealthResult preprocessHealth,
            EnhanceBasicHealthResult enhanceHealth,
            DisplayHealthResult displayHealth,
            DicomHealthResult dicomHealth)
        {
            if (allCases.Count == 0)
            {
                return "No local fixture case directories were found under tests/test_data.";
            }

            if (selectedCases.Count == 0)
            {
                return "The requested fixture case was not found.";
            }

            if (!preprocessHealth.IsExportReady)
            {
                return $"xpe_preprocess.dll is not export-ready: {preprocessHealth.Status}.";
            }

            if (!enhanceHealth.IsSmokeReady)
            {
                return $"xpe_enhance_basic.dll is not smoke-ready: {enhanceHealth.Status}.";
            }

            if (!displayHealth.IsSmokeReady)
            {
                return $"xpe_display.dll is not smoke-ready: {displayHealth.Status}.";
            }

            if (!dicomHealth.IsSmokeReady)
            {
                return $"xpe_dicom.dll is not smoke-ready: {dicomHealth.Status}.";
            }

            if (runs.Count == 0)
            {
                return "Selected fixture cases do not contain runnable raw image files.";
            }

            return "No Phase 1b fixture run passed preprocess->enhance->display->dicom gates.";
        }

        private static string RenderMarkdown(
            DateTimeOffset timestamp,
            string summary,
            Phase1bFixtureE2eOptions options,
            IReadOnlyList<FixtureCaseInfo> selectedCases,
            IReadOnlyList<Phase1bFixtureE2eRunResult> runs,
            string readinessReportPath,
            string? readinessError,
            PreprocessHealthResult preprocessHealth,
            EnhanceBasicHealthResult enhanceHealth,
            DisplayHealthResult displayHealth,
            DicomHealthResult dicomHealth)
        {
            var builder = new StringBuilder();
            builder.AppendLine("# Phase 1b Fixture E2E Report");
            builder.AppendLine();
            builder.AppendLine($"- Timestamp UTC: `{timestamp:O}`");
            builder.AppendLine($"- Case filter: `{options.CaseName ?? "all"}`");
            builder.AppendLine($"- Max runs: `{options.MaxRuns}`");
            builder.AppendLine($"- Summary: `{summary}`");
            builder.AppendLine($"- Readiness report: `{(string.IsNullOrWhiteSpace(readinessReportPath) ? "none" : readinessReportPath)}`");
            if (!string.IsNullOrWhiteSpace(readinessError))
            {
                builder.AppendLine($"- Readiness report error: `{readinessError}`");
            }

            builder.AppendLine();
            builder.AppendLine("## Native Readiness");
            builder.AppendLine($"- Preprocess: `{preprocessHealth.Status}`, dll=`{preprocessHealth.DllPath}`");
            builder.AppendLine($"- Enhance basic: `{enhanceHealth.Status}`, dll=`{enhanceHealth.DllPath}`, smoke=`{enhanceHealth.Smoke.Status}`");
            builder.AppendLine($"- Display: `{displayHealth.Status}`, dll=`{displayHealth.DllPath}`, smoke=`{displayHealth.Smoke.Status}`");
            builder.AppendLine($"- DICOM: `{dicomHealth.Status}`, dll=`{dicomHealth.DllPath}`, smoke=`{dicomHealth.Smoke.Status}`");
            builder.AppendLine();
            builder.AppendLine("## Stage Order");
            builder.AppendLine($"- Preprocess: `{string.Join(" -> ", PreprocessStageOrder)}`");
            builder.AppendLine($"- Enhance basic: `{string.Join(" -> ", EnhanceBasicStageOrder)}`");
            builder.AppendLine($"- Display: `{string.Join(" -> ", DisplayStageOrder)}`");
            builder.AppendLine($"- DICOM: `{string.Join(" -> ", DicomStageOrder)}`");
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
            builder.AppendLine("## PHASE1B-E2E-4 Full Pipeline");
            foreach (var run in runs)
            {
                builder.AppendLine($"- `{run.CaseName}/{run.RawName}`: status=`{run.Status}`, passed=`{run.Passed}`, rawPreserved=`{run.RawPreserved}`");
                builder.AppendLine($"  Details: {run.Details}");
            }

            if (runs.Count == 0)
            {
                builder.AppendLine("- No fixture runs were attempted.");
            }

            return builder.ToString();
        }

        private static JsonSerializerOptions CreateJsonOptions()
        {
            return new JsonSerializerOptions
            {
                WriteIndented = true,
                NumberHandling = JsonNumberHandling.AllowNamedFloatingPointLiterals
            };
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
