using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace ImageProcTest
{
    internal sealed record StageModeSnapshot(string Stage, string Mode, string Reason);

    internal sealed record GuiE2eReportWriteResult(string JsonPath, string MarkdownPath);

    internal static class GuiE2eReportService
    {
        public static GuiE2eReportWriteResult WriteReport(
            FixtureCaseInfo? selectedCase,
            RawFileDescriptor? selectedRaw,
            RawPreviewResult? preview,
            BackendHealthResult? backendHealth,
            string? readinessReportPath,
            PreprocessHealthResult? preprocessHealth,
            NativePreprocessPreviewResult? nativePreview,
            IReadOnlyList<StageModeSnapshot> stageModes,
            IReadOnlyList<ModuleReadinessSnapshot> moduleReadiness,
            IReadOnlyList<AlgorithmValidationItem> algorithmValidation,
            AlgorithmValidationRunSnapshot? latestAlgorithmRun,
            UserEvaluationSnapshot? userEvaluation)
        {
            var timestamp = DateTimeOffset.UtcNow;
            var report = new
            {
                schema = "xpe-preprocess-gui-test-v1",
                timestampUtc = timestamp,
                selectedCase = selectedCase is null ? null : new
                {
                    selectedCase.Name,
                    selectedCase.RootPath,
                    imageCount = selectedCase.Images.Count,
                    calibrationCount = selectedCase.CalibrationFiles.Count,
                    selectedCase.CalibrationSummary,
                    calibrationFiles = selectedCase.CalibrationFiles.Select(file => new
                    {
                        file.Name,
                        file.Path,
                        file.Length,
                        Role = file.Role.ToString()
                    })
                },
                selectedRaw = selectedRaw is null ? null : new
                {
                    selectedRaw.Name,
                    selectedRaw.Path,
                    selectedRaw.Length
                },
                preview = preview is null ? null : new
                {
                    preview.FilePath,
                    preview.FileSizeBytes,
                    preview.Width,
                    preview.Height,
                    preview.PreviewWidth,
                    preview.PreviewHeight,
                    preview.SampleStride,
                    preview.MinValue,
                    preview.MaxValue,
                    preview.Sha256
                },
                backendHealth,
                readinessReportPath,
                preprocessHealth,
                preprocessSyntheticOracle = preprocessHealth?.SyntheticOracle,
                nativePreview = nativePreview is null ? null : new
                {
                    nativePreview.DllPath,
                    nativePreview.ArtifactDirectory,
                    nativePreview.CalibrationLoads,
                    nativePreview.TotalLatencyMs,
                    nativePreview.OutputMin,
                    nativePreview.OutputMax,
                    nativePreview.Metrics,
                    nativePreview.DetectorMetrics,
                    nativePreview.Stages
                },
                moduleReadiness,
                algorithmValidation,
                latestAlgorithmRun,
                userEvaluation,
                stageModes,
                beforeAfter = new
                {
                    mode = nativePreview is null ? "identity-mock" : "native-preprocess-preview",
                    nativeProcessingEnabled = nativePreview is not null,
                    reason = nativePreview is null
                        ? "xpe_preprocess.dll export readiness has not passed or preprocessing has not been applied."
                        : "Fixture calibration raw files were converted to XCal, loaded into xpe_preprocess.dll, and applied to the sampled preview buffer."
                }
            };

            var directory = Path.Combine(AppContext.BaseDirectory, "preprocess-gui-reports");
            Directory.CreateDirectory(directory);

            var name = $"preprocess-gui-{timestamp:yyyyMMdd-HHmmss}";
            var jsonPath = Path.Combine(directory, $"{name}.json");
            var markdownPath = Path.Combine(directory, $"{name}.md");

            File.WriteAllText(jsonPath, JsonSerializer.Serialize(report, new JsonSerializerOptions
            {
                WriteIndented = true,
                NumberHandling = JsonNumberHandling.AllowNamedFloatingPointLiterals
            }));
            File.WriteAllText(markdownPath, RenderMarkdown(
                selectedCase,
                selectedRaw,
                preview,
                backendHealth,
                readinessReportPath,
                preprocessHealth,
                nativePreview,
                stageModes,
                moduleReadiness,
                algorithmValidation,
                latestAlgorithmRun,
                userEvaluation,
                timestamp));

            return new GuiE2eReportWriteResult(jsonPath, markdownPath);
        }

        private static string RenderMarkdown(
            FixtureCaseInfo? selectedCase,
            RawFileDescriptor? selectedRaw,
            RawPreviewResult? preview,
            BackendHealthResult? backendHealth,
            string? readinessReportPath,
            PreprocessHealthResult? preprocessHealth,
            NativePreprocessPreviewResult? nativePreview,
            IReadOnlyList<StageModeSnapshot> stageModes,
            IReadOnlyList<ModuleReadinessSnapshot> moduleReadiness,
            IReadOnlyList<AlgorithmValidationItem> algorithmValidation,
            AlgorithmValidationRunSnapshot? latestAlgorithmRun,
            UserEvaluationSnapshot? userEvaluation,
            DateTimeOffset timestamp)
        {
            var builder = new StringBuilder();
            builder.AppendLine("# Preprocess GUI Test Report");
            builder.AppendLine();
            builder.AppendLine($"- Timestamp UTC: `{timestamp:O}`");
            builder.AppendLine($"- Fixture case: `{selectedCase?.Name ?? "none"}`");
            builder.AppendLine($"- Calibration roles: `{selectedCase?.CalibrationSummary ?? "none"}`");
            builder.AppendLine($"- Raw file: `{selectedRaw?.Path ?? "none"}`");
            builder.AppendLine($"- Readiness report: `{readinessReportPath ?? "none"}`");
            builder.AppendLine($"- Backend mode: `{backendHealth?.Mode ?? "unknown"}`");
            builder.AppendLine($"- Native processing enabled: `{nativePreview is not null}`");
            builder.AppendLine();

            builder.AppendLine("## Native Preview");
            if (nativePreview is null)
            {
                builder.AppendLine("- Native preview was not applied; after image is the identity mock.");
            }
            else
            {
                builder.AppendLine($"- DLL: `{nativePreview.DllPath}`");
                builder.AppendLine($"- Artifacts: `{nativePreview.ArtifactDirectory}`");
                builder.AppendLine($"- Total latency ms: `{nativePreview.TotalLatencyMs:0.###}`");
                builder.AppendLine($"- Output min/max: `{nativePreview.OutputMin:0.###}` / `{nativePreview.OutputMax:0.###}`");
                builder.AppendLine($"- Mean absolute delta: `{nativePreview.Metrics.MeanAbsoluteDelta:0.###}`");
                builder.AppendLine($"- RMSE: `{nativePreview.Metrics.Rmse:0.###}`");
                builder.AppendLine($"- Max absolute delta: `{nativePreview.Metrics.MaxAbsoluteDelta:0.###}`");
                builder.AppendLine($"- Changed pixels: `{nativePreview.Metrics.ChangedPixels}/{nativePreview.Metrics.PixelCount}` (`{nativePreview.Metrics.ChangedPixelRatio:P2}`)");
                builder.AppendLine($"- Input preserved: `{nativePreview.Metrics.InputPreserved}`");
                builder.AppendLine($"- NaN/Inf count: `{nativePreview.Metrics.NaNInfCount}`");
                AppendDetectorMetrics(builder, nativePreview.DetectorMetrics);
                foreach (var load in nativePreview.CalibrationLoads)
                {
                    builder.AppendLine($"- Calibration `{load.Stage}`: status=`{load.Status}`, loaded=`{load.Loaded}`, source=`{load.SourceRawPath ?? "none"}`, xcal=`{load.XCalPath ?? "none"}`");
                    builder.AppendLine($"  Details: {load.Details}");
                    if (load.Expiry is not null)
                    {
                        builder.AppendLine(
                            $"  Expiry: status=`{load.Expiry.Status}`, checked=`{load.Expiry.Checked}`, expired=`{load.Expiry.Expired}`, " +
                            $"expiryUtc=`{load.Expiry.ExpiryUtc ?? "unknown"}`, remainingDays=`{FormatNullableDays(load.Expiry.RemainingDays)}`, " +
                            $"latency=`{load.Expiry.LatencyMs:0.###}` ms; {load.Expiry.Details}");
                    }
                }
                foreach (var stage in nativePreview.Stages)
                {
                    builder.AppendLine($"- `{stage.Stage}`: `{stage.ErrorCode}`, executed=`{stage.Executed}`, latency=`{stage.LatencyMs:0.###}` ms");
                    builder.AppendLine($"  Details: {stage.Details}");
                }
            }

            builder.AppendLine();

            builder.AppendLine("## Preprocess Native Gate");
            if (preprocessHealth is null)
            {
                builder.AppendLine("- Preprocess health was not checked.");
            }
            else
            {
                builder.AppendLine($"- Status: `{preprocessHealth.Status}`");
                builder.AppendLine($"- Version: `{preprocessHealth.Version}`");
                builder.AppendLine($"- DLL: `{preprocessHealth.DllPath}`");
                builder.AppendLine($"- Parameter ranges: `{NativeReadinessProbe.FormatPreprocessParameterRanges(preprocessHealth.ParameterRanges)}`");
                builder.AppendLine($"- Synthetic oracle: `{preprocessHealth.SyntheticOracle.Status}`");
                builder.AppendLine($"- Synthetic passed: `{preprocessHealth.SyntheticOracle.Passed}`");
                builder.AppendLine($"- Total latency ms: `{preprocessHealth.SyntheticOracle.TotalLatencyMs:0.###}`");
                builder.AppendLine($"- Input preserved: `{preprocessHealth.SyntheticOracle.InputPreserved}`");
                builder.AppendLine($"- NaN/Inf count: `{preprocessHealth.SyntheticOracle.NaNInfCount}`");
                builder.AppendLine($"- Determinism RMSE: `{preprocessHealth.SyntheticOracle.DeterminismRmse}`");
            }

            builder.AppendLine();

            builder.AppendLine("## Raw Preview");
            if (preview is null)
            {
                builder.AppendLine("- No raw preview loaded.");
            }
            else
            {
                builder.AppendLine($"- Source: `{preview.Width}x{preview.Height}` uint16");
                builder.AppendLine($"- File size: `{RawFileDescriptor.FormatBytes(preview.FileSizeBytes)}`");
                builder.AppendLine($"- Preview: `{preview.PreviewWidth}x{preview.PreviewHeight}`, stride `{preview.SampleStride}`");
                builder.AppendLine($"- Min/Max: `{preview.MinValue}` / `{preview.MaxValue}`");
                builder.AppendLine($"- SHA-256: `{preview.Sha256}`");
            }

            builder.AppendLine();
            builder.AppendLine("## Module Readiness Matrix");
            foreach (var module in moduleReadiness)
            {
                builder.AppendLine($"- `{module.ModuleName}`: `{module.Level}` / `{module.Status}` / exec=`{module.ProcessingEnabled}`");
                builder.AppendLine($"  Evidence: {module.Evidence}");
                builder.AppendLine($"  Next: {module.NextAction}");
            }

            builder.AppendLine();
            builder.AppendLine("## Calibration Validation Catalog");
            builder.AppendLine($"- Catalog rows: `{algorithmValidation.Count}`");
            builder.AppendLine($"- Runnable rows: `{algorithmValidation.Count(item => item.CanRun)}`");
            if (latestAlgorithmRun is null)
            {
                builder.AppendLine("- Latest SWU validation: `none`");
            }
            else
            {
                builder.AppendLine(
                    $"- Latest SWU validation: `{latestAlgorithmRun.Status}` `{latestAlgorithmRun.SwuId}` " +
                    $"`{latestAlgorithmRun.AlgorithmName}`, latency=`{FormatNullableLatency(latestAlgorithmRun.LatencyMs)}`, " +
                    $"artifacts=`{latestAlgorithmRun.ArtifactDirectory ?? "none"}`");
                builder.AppendLine($"  Details: {latestAlgorithmRun.Details}");
            }

            foreach (var item in algorithmValidation)
            {
                builder.AppendLine($"- `{item.SwuId}` `{item.AlgorithmName}`: module=`{item.ModuleName}`, level=`{item.Level}`, status=`{item.Status}`, run=`{item.CanRun}`");
                builder.AppendLine($"  Req: {item.RequirementIds}; Tests: {item.TestIds}; Gate: {item.Gate}");
                builder.AppendLine($"  Next: {item.NextAction}");
            }

            builder.AppendLine();
            builder.AppendLine("## User Evaluation");
            if (userEvaluation is null)
            {
                builder.AppendLine("- User evaluation was not recorded.");
            }
            else
            {
                builder.AppendLine($"- Calibration: `{userEvaluation.AlgorithmKey}`");
                builder.AppendLine($"- Evaluator: `{userEvaluation.Evaluator}`");
                builder.AppendLine($"- Verdict: `{userEvaluation.Verdict}`");
                builder.AppendLine($"- Evidence: {userEvaluation.EvidenceSummary}");
                builder.AppendLine($"- Notes: {userEvaluation.Notes}");
            }

            builder.AppendLine();
            builder.AppendLine("## Stage Modes");
            foreach (var mode in stageModes)
            {
                builder.AppendLine($"- `{mode.Stage}`: `{mode.Mode}` - {mode.Reason}");
            }

            builder.AppendLine();
            builder.AppendLine("## Before/After Scaffold");
            builder.AppendLine(nativePreview is null
                ? "- Current after image is identity-mock output."
                : "- Current after image is fixture-calibrated native preprocess output.");
            builder.AppendLine("- This GUI test is diagnostic preview execution on sampled buffers; clinical workflow release still needs formal fixture E2E acceptance.");
            return builder.ToString();
        }

        private static string FormatNullableDays(double? days)
        {
            return days.HasValue ? days.Value.ToString("0.###") : "unknown";
        }

        private static string FormatNullableLatency(double? latencyMs)
        {
            return latencyMs.HasValue ? $"{latencyMs.Value:0.###} ms" : "n/a";
        }

        private static void AppendDetectorMetrics(StringBuilder builder, DetectorDomainMetrics metrics)
        {
            builder.AppendLine("- Detector-domain dark metrics:");
            AppendMetricRows(builder, metrics.DarkMetrics);
            builder.AppendLine("- Detector-domain flat metrics:");
            AppendMetricRows(builder, metrics.FlatMetrics);
            builder.AppendLine("- Defect/BPM metrics:");
            AppendMetricRows(builder, metrics.DefectMetrics);
        }

        private static void AppendMetricRows(StringBuilder builder, IReadOnlyList<DetectorMetricRow> rows)
        {
            foreach (var row in rows)
            {
                builder.AppendLine($"  - `{row.Metric}`: value=`{row.Value}`, gate=`{row.Gate}`, status=`{row.Status}`");
            }
        }
    }
}
