using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;

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
            IReadOnlyList<StageModeSnapshot> stageModes,
            IReadOnlyList<ModuleReadinessSnapshot> moduleReadiness)
        {
            var timestamp = DateTimeOffset.UtcNow;
            var report = new
            {
                schema = "xpe-gui-e2e-scaffold-v1",
                timestampUtc = timestamp,
                selectedCase = selectedCase is null ? null : new
                {
                    selectedCase.Name,
                    selectedCase.RootPath,
                    imageCount = selectedCase.Images.Count,
                    calibrationCount = selectedCase.CalibrationFiles.Count,
                    calibrationFiles = selectedCase.CalibrationFiles.Select(file => new
                    {
                        file.Name,
                        file.Path,
                        file.Length
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
                moduleReadiness,
                stageModes,
                beforeAfter = new
                {
                    mode = "identity-mock",
                    nativeProcessingEnabled = false,
                    reason = "xpe_preprocess.dll readiness gates have not passed."
                }
            };

            var directory = Path.Combine(AppContext.BaseDirectory, "gui-e2e-reports");
            Directory.CreateDirectory(directory);

            var name = $"gui-e2e-{timestamp:yyyyMMdd-HHmmss}";
            var jsonPath = Path.Combine(directory, $"{name}.json");
            var markdownPath = Path.Combine(directory, $"{name}.md");

            File.WriteAllText(jsonPath, JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }));
            File.WriteAllText(markdownPath, RenderMarkdown(selectedCase, selectedRaw, preview, backendHealth, readinessReportPath, preprocessHealth, stageModes, moduleReadiness, timestamp));

            return new GuiE2eReportWriteResult(jsonPath, markdownPath);
        }

        private static string RenderMarkdown(
            FixtureCaseInfo? selectedCase,
            RawFileDescriptor? selectedRaw,
            RawPreviewResult? preview,
            BackendHealthResult? backendHealth,
            string? readinessReportPath,
            PreprocessHealthResult? preprocessHealth,
            IReadOnlyList<StageModeSnapshot> stageModes,
            IReadOnlyList<ModuleReadinessSnapshot> moduleReadiness,
            DateTimeOffset timestamp)
        {
            var builder = new StringBuilder();
            builder.AppendLine("# GUI E2E Scaffold Report");
            builder.AppendLine();
            builder.AppendLine($"- Timestamp UTC: `{timestamp:O}`");
            builder.AppendLine($"- Fixture case: `{selectedCase?.Name ?? "none"}`");
            builder.AppendLine($"- Raw file: `{selectedRaw?.Path ?? "none"}`");
            builder.AppendLine($"- Readiness report: `{readinessReportPath ?? "none"}`");
            builder.AppendLine($"- Backend mode: `{backendHealth?.Mode ?? "unknown"}`");
            builder.AppendLine($"- Native processing enabled: `false`");
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
            builder.AppendLine("## Stage Modes");
            foreach (var mode in stageModes)
            {
                builder.AppendLine($"- `{mode.Stage}`: `{mode.Mode}` - {mode.Reason}");
            }

            builder.AppendLine();
            builder.AppendLine("## Before/After Scaffold");
            builder.AppendLine("- Current after image is identity-mock output.");
            builder.AppendLine("- Native preprocess execution remains disabled until readiness gates pass.");
            return builder.ToString();
        }
    }
}
