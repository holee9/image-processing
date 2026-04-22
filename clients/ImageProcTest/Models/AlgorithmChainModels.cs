using System;
using System.Collections.Generic;
using System.Linq;

namespace ImageProcTest
{
    internal enum AlgorithmRuleSeverity
    {
        Hard,
        Soft,
        Advisory
    }

    internal enum AlgorithmChainPreset
    {
        RunnablePreprocess,
        RunnablePostBasic,
        RunnablePrePostBasic,
        ProductCanonical,
        PreE2eProof
    }

    internal sealed record AlgorithmNode(
        string StageKey,
        string Label,
        string AlgorithmName,
        string ModuleName,
        string Owner,
        string Phase,
        string InputDomain,
        string OutputDomain,
        string RequirementIds,
        string TestIds,
        string Gate,
        string Adapter,
        string Status,
        string Evidence,
        string NextAction,
        double DefaultOrder,
        bool Mandatory,
        bool Conditional,
        bool CanRun,
        bool RequiresSequence,
        bool IsBranch,
        string? CalibrationRole)
    {
        public string DisplayName =>
            $"{Label} {AlgorithmName} [{ModuleName}; {Adapter}; {Status}]";
    }

    internal sealed record AlgorithmChainStep(int Position, AlgorithmNode Node)
    {
        public string StageKey => Node.StageKey;
        public string Label => Node.Label;
        public string AlgorithmName => Node.AlgorithmName;
        public string ModuleName => Node.ModuleName;
        public string Adapter => Node.Adapter;
        public string Status => Node.Status;
        public string Domain => $"{Node.InputDomain}->{Node.OutputDomain}";
        public string ChainDisplayName => $"{Position}. {Node.Label} {Node.AlgorithmName}";
    }

    internal sealed record AlgorithmDependencyFinding(
        AlgorithmRuleSeverity Severity,
        string RuleId,
        string Message,
        string Evidence)
    {
        public string SeverityText => Severity.ToString();
    }

    internal sealed record AlgorithmChainPlan(
        IReadOnlyList<AlgorithmChainStep> Steps,
        IReadOnlyList<AlgorithmDependencyFinding> Findings,
        IReadOnlyList<string> NativeStageOrder,
        IReadOnlyList<string> EnhanceBasicStageOrder,
        IReadOnlyList<string> AdvancedStageOrder,
        IReadOnlyList<string> AiStageOrder,
        IReadOnlyList<string> DisplayStageOrder,
        IReadOnlyList<string> DicomStageOrder,
        bool IsFolderAuditOnly,
        bool CanExecute,
        string Summary)
    {
        public bool HasHardBlocks => Findings.Any(item => item.Severity == AlgorithmRuleSeverity.Hard);

        public string DisplayName => Steps.Count == 0
            ? "No chain selected"
            : string.Join(" -> ", Steps.Select(step => step.Node.Label));

        public PreprocessStageSelection ToPreprocessSelection()
        {
            var stages = NativeStageOrder.ToHashSet(StringComparer.OrdinalIgnoreCase);
            return new PreprocessStageSelection(
                stages.Contains("offset") ? PreprocessStageMode.On : PreprocessStageMode.Off,
                stages.Contains("gain") ? PreprocessStageMode.On : PreprocessStageMode.Off,
                stages.Contains("defect") ? PreprocessStageMode.On : PreprocessStageMode.Off);
        }

        public EnhanceBasicStageSelection ToEnhanceBasicSelection()
        {
            var stages = EnhanceBasicStageOrder.ToHashSet(StringComparer.OrdinalIgnoreCase);
            return new EnhanceBasicStageSelection(
                stages.Contains("ei-whole"),
                stages.Contains("log"),
                stages.Contains("basic-noise"),
                stages.Contains("contrast"),
                stages.Contains("edge"));
        }
    }
}
