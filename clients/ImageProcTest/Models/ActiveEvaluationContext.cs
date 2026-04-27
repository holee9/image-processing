using System.Linq;

namespace ImageProcTest
{
    internal sealed record ActiveEvaluationContext(
        FixtureCaseInfo? CalibrationContext,
        string? CalibrationFolderPath,
        RawPreviewResult? RawPreview,
        AlgorithmChainPlan AlgorithmChain,
        PreprocessStageSelection StageSelection)
    {
        public bool HasCalibrationFolder => CalibrationContext is not null;

        public bool HasTargetRaw => RawPreview is not null;

        public bool HasExecutableChain => AlgorithmChain.CanExecute && !AlgorithmChain.HasHardBlocks;

        public bool RequiresCalibrationFolder =>
            AlgorithmChain.IsFolderAuditOnly ||
            AlgorithmChain.NativeStageOrder.Count > 0;

        public bool IsReady =>
            (!RequiresCalibrationFolder || HasCalibrationFolder) &&
            HasExecutableChain &&
            (AlgorithmChain.IsFolderAuditOnly || HasTargetRaw);

        public string Summary
        {
            get
            {
                var folder = CalibrationContext is null
                    ? RequiresCalibrationFolder ? "calibration folder not selected" : "calibration folder optional"
                    : $"{CalibrationContext.CalibrationFiles.Count} calibration file(s)";
                var raw = RawPreview is null
                    ? "target raw not loaded"
                    : $"{RawPreview.PreviewWidth}x{RawPreview.PreviewHeight} preview";
                var chain = AlgorithmChain.Steps.Count == 0
                    ? "no algorithm chain"
                    : AlgorithmChain.DisplayName;

                return $"{folder}; {raw}; chain={chain}";
            }
        }

        public string BlockingReason
        {
            get
            {
                if (RequiresCalibrationFolder && !HasCalibrationFolder)
                {
                    return "Select the acquired calibration folder first.";
                }

                if (!HasExecutableChain)
                {
                    var hardBlocks = AlgorithmChain.Findings
                        .Where(finding => finding.Severity == AlgorithmRuleSeverity.Hard)
                        .Select(finding => $"{finding.RuleId}: {finding.Message}")
                        .ToArray();
                    return hardBlocks.Length == 0
                        ? "Select a runnable algorithm chain."
                        : string.Join(" ", hardBlocks);
                }

                if (!AlgorithmChain.IsFolderAuditOnly && !HasTargetRaw)
                {
                    return "Load the target raw image to evaluate correction output.";
                }

                return "Ready for evaluation run.";
            }
        }

        public string Details
        {
            get
            {
                var calibrationFolder = CalibrationFolderPath ?? "none";
                var roles = CalibrationContext?.CalibrationSummary ?? "none";
                var raw = RawPreview?.FilePath ?? "none";
                return $"Calibration folder={calibrationFolder}; roles={roles}; raw={raw}; " +
                    $"stage switches=offset:{StageSelection.Offset}, gain:{StageSelection.Gain}, defect:{StageSelection.Defect}; " +
                    $"post stages={string.Join(" -> ", AlgorithmChain.EnhanceBasicStageOrder)}; " +
                    $"display stages={string.Join(" -> ", AlgorithmChain.DisplayStageOrder)}; " +
                    $"dicom stages={string.Join(" -> ", AlgorithmChain.DicomStageOrder)}; " +
                    $"rule status={AlgorithmChain.Summary}";
            }
        }
    }
}
