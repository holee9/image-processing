using System;
using System.Collections.Generic;
using System.Linq;

namespace ImageProcTest
{
    internal static class AlgorithmChainCatalogService
    {
        private sealed record NodeDefinition(
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
            double DefaultOrder,
            bool Mandatory,
            bool Conditional,
            bool RequiresSequence,
            bool IsBranch,
            string? CalibrationRole,
            string NextAction);

        private static readonly NodeDefinition[] Definitions =
        [
            new("calib-folder", "BOOT-0", "Calibration manifest load/audit", "xpe_preprocess", "xpe_preprocess.dll", "1a", "N/A", "detector-context", "SRS-CALIB-FUNC-001..003,009,015", "PRE-E2E-0, PRE-E2E-5", "folder context", "folder-audit", 0.0, true, false, false, false, null, "Select the acquired calibration folder and verify offset/dark, gain/flat, BPM roles, checksums, expiry, and mismatch evidence."),
            new("readout", "(0.5)", "Readout artifact validation", "xpe_preprocess", "xpe_preprocess.dll", "1a", "detector", "detector", "SRS-CALIB readout validation, ALG-SPEC 6.1", "PRE-E2E-1", "advisory", "adapter-pending", 0.5, false, false, false, false, null, "Add readout artifact validation adapter and alert-only evidence before enabling execution."),
            new("temperature", "(0.7)", "Temperature compensation", "xpe_preprocess", "xpe_preprocess.dll", "1a", "detector", "detector", "SRS-CALIB-FUNC-008,016", "UT-1.5-005, BP-01", "conditional", "adapter-pending", 0.7, false, true, false, false, null, "Add detector temperature metadata and dark interpolation evidence before enabling execution."),
            new("offset", "(1)", "Offset correction", "xpe_preprocess", "xpe_preprocess.dll", "1a", "detector", "detector", "SRS-CALIB-FUNC-004,016,SAFE-001", "UT-1.1, PRE-E2E-1/2, BP-01", "mandatory", "native-preview", 1.0, true, false, false, false, "Offset", "Select an acquired calibration folder with offset/dark data and load a target raw image."),
            new("nonlinearity", "(1.5)", "Nonlinearity correction", "xpe_preprocess", "xpe_preprocess.dll", "1a", "detector", "detector", "SRS-CALIB-FUNC-006, ALG-SPEC 6.2", "UT-1.5-004", "conditional", "adapter-pending", 1.5, false, true, false, false, null, "Add LUT/polynomial calibration profile input and monotonicity evidence before enabling execution."),
            new("gain", "(2)", "Gain correction", "xpe_preprocess", "xpe_preprocess.dll", "1a", "detector", "detector-float", "SRS-CALIB-FUNC-005,017,018,SAFE-001", "UT-1.2, PRE-E2E-1/2, BP-02/03", "mandatory", "native-preview", 2.0, true, false, false, false, "Gain", "Select an acquired calibration folder with offset/dark and gain/flat data and load a target raw image."),
            new("binning", "(2.5)", "Binning correction", "xpe_preprocess", "xpe_preprocess.dll", "1a", "detector-float", "detector-float", "SRS-CALIB-FUNC-012", "binning metadata tests", "conditional", "adapter-pending", 2.5, false, true, false, false, null, "Add binning-mode detector metadata and gain scaling evidence before enabling execution."),
            new("defect", "(3)", "Defect correction", "xpe_preprocess", "xpe_preprocess.dll", "1a", "detector-float", "detector-float", "SRS-CALIB-FUNC-007,010,019", "UT-1.3, PRE-E2E-1/2, BP-04", "conditional", "native-preview", 3.0, false, true, false, false, "Defect", "Select an acquired calibration folder with BPM/defect data after offset+gain are in the chain."),
            new("runtime-defect", "(3b)", "Runtime defect detection", "xpe_preprocess", "xpe_preprocess.dll", "1a", "detector-float-sequence", "detector-float", "SRS-CALIB-FUNC-010,019", "runtime defect integration, BP-04", "sequence", "adapter-pending", 3.5, false, true, true, false, null, "Add 10-frame sequence input and runtime BPM merge evidence before enabling execution."),
            new("lag-ghost", "(4)", "Lag / ghost correction", "xpe_preprocess", "xpe_preprocess.dll", "1a", "detector-float-sequence", "detector-float", "SRS-CALIB-FUNC-013,014,020", "UT-1.4, PRE-E2E-1/2, BP-05", "sequence", "adapter-pending", 4.0, false, true, true, false, null, "Add lag history sequence input, tier selection, reset, bypass reason, and residual metrics."),
            new("ei-whole", "(EI-0)", "Whole-image EI / DI", "xpe_enhance_basic", "xpe_enhance_basic.dll", "1b", "detector-float", "metrics", "FR-100-001..006, ALG-SPEC 6.5", "EI baseline tests", "mandatory single-irradiation", "native-enhance-basic", 4.5, true, false, false, false, null, "Build/load xpe_enhance_basic.dll and run EI/DI on the active detector-float preview buffer."),
            new("log", "(5)", "Log transform", "xpe_enhance_basic", "xpe_enhance_basic.dll", "1b", "enhancement-entry", "log-domain", "FR-200-001..005, SAF-100-002", "TST-100-001", "mandatory", "native-enhance-basic", 5.0, true, false, false, false, null, "Build/load xpe_enhance_basic.dll and run log transform on the active detector-float preview buffer."),
            new("basic-noise", "(6)", "Noise reduction", "xpe_enhance_basic", "xpe_enhance_basic.dll", "1b", "enhancement", "enhancement", "pipeline-spec (6)", "TST-200-001", "mandatory baseline", "native-enhance-basic", 6.0, true, false, false, false, null, "Build/load xpe_enhance_basic.dll and run deterministic bilateral noise reduction."),
            new("contrast", "(7)", "Contrast enhancement", "xpe_enhance_basic", "xpe_enhance_basic.dll", "1b", "enhancement", "enhancement", "FR-300, FR-400", "TST-100-003/004", "mandatory baseline", "native-enhance-basic", 7.0, true, false, false, false, null, "Build/load xpe_enhance_basic.dll and run CLAHE contrast enhancement."),
            new("edge", "(8)", "Edge enhancement", "xpe_enhance_basic", "xpe_enhance_basic.dll", "1b", "enhancement", "enhancement", "pipeline-spec (8)", "TST-200-001", "mandatory baseline", "native-enhance-basic", 8.0, true, false, false, false, null, "Build/load xpe_enhance_basic.dll and run USM edge enhancement."),
            new("collimation", "(5b)", "Baseline collimation detection", "xpe_enhance_advanced", "xpe_enhance_advanced.dll", "2", "detector-float-side-copy", "roi-sidecar", "SRS-ENHANCE-ADV ROI", "ROI confidence tests", "optional branch", "advanced-adapter-pending", 8.5, false, true, false, true, null, "Add ROI sidecar adapter; result gates EI-1 only."),
            new("ei-roi", "(EI-1)", "ROI-aware EI refinement", "orchestrator+xpe_enhance_basic", "orchestrator", "2", "detector-float-roi", "metrics", "ALG-SPEC 6.5, SAF-101", "ROI EI tests", "optional branch", "advanced-adapter-pending", 8.6, false, true, false, true, null, "Add ROI crop orchestration after baseline collimation detection."),
            new("gsvg", "(9)", "GSVG / virtual grid", "gsvg", "gsvg.dll", "2", "enhancement", "enhancement", "GSVG-SRS-001", "grid/no-grid benchmark", "optional", "gsvg-adapter-pending", 9.0, false, true, false, false, null, "Track C1/C2 readiness in the GUI and wait for #61 processing exports before enabling GSVG execution."),
            new("multiscale", "(10)", "Multiscale processing", "xpe_enhance_advanced", "xpe_enhance_advanced.dll", "2", "enhancement", "enhancement", "SRS-ENHANCE-ADV", "PSNR/SSIM/MTF gates", "optional", "advanced-adapter-pending", 10.0, false, true, false, false, null, "Add advanced enhancement adapter and quality gates."),
            new("fractional", "(11)", "Fractional processing", "xpe_enhance_advanced", "xpe_enhance_advanced.dll", "2", "enhancement", "enhancement", "pipeline-spec (11)", "advanced enhancement gates", "optional", "advanced-adapter-pending", 11.0, false, true, false, false, null, "Add fractional processing adapter and benchmark binding."),
            new("ai-bodypart", "(5a)", "Body-part recognition advisory", "xpe_ai", "xpe_ai.dll/xpe_ai_worker.exe", "3", "preview-copy", "ai-advisory", "FR-AI-130, FR-AI-140", "TC-AI-130-01..04", "optional branch", "ai-adapter-pending", 12.0, false, true, false, true, null, "Add AI worker proxy, heartbeat smoke, confidence threshold evidence, and sidecar-only fallback report."),
            new("ai-collimation", "(5c)", "AI collimation refinement advisory", "xpe_ai", "xpe_ai.dll/xpe_ai_worker.exe", "3", "preview-copy", "ai-advisory", "FR-AI-150, SAF-AI-130", "TC-AI-150-01..03", "optional branch", "ai-adapter-pending", 12.1, false, true, false, true, null, "Add non-blocking AI collimation proposal branch and original-ROI fallback evidence."),
            new("stitching", "(12)", "Image stitching", "xpe_ai", "xpe_ai.dll/xpe_ai_worker.exe", "3", "multi-frame-set", "panorama", "FR-AI-160, PERF-AI-120", "TC-AI-160-01..03", "sequence branch", "ai-adapter-pending", 12.2, false, true, true, true, null, "Add multi-frame study input, reject/fallback evidence, and tagged panorama output."),
            new("bone-suppression", "(13)", "Bone suppression", "xpe_ai", "xpe_ai.dll/xpe_ai_worker.exe", "3", "enhancement", "assistive-secondary-image", "FR-AI-170, PERF-AI-130", "TC-AI-170-01..04", "optional branch", "ai-adapter-pending", 13.0, false, true, false, true, null, "Keep as assistive secondary output; do not overwrite deterministic baseline."),
            new("dl-denoise", "(13b)", "DL denoising", "xpe_ai", "xpe_ai.dll/xpe_ai_worker.exe", "3", "enhancement", "assistive-enhanced-image", "FR-AI-190, SAF-AI-130", "DL degraded-mode tests", "optional branch", "ai-adapter-pending", 13.1, false, true, false, true, null, "Keep research-gated and use classical noise reduction fallback until task-based validation passes."),
            new("modality-lut", "(14)", "Modality LUT", "xpe_display", "xpe_display.dll", "1b", "presentation", "presentation", "SRS-DISPLAY FR-MODAL", "display LUT tests", "mandatory", "adapter-pending", 14.0, true, false, false, false, null, "Add xpe_display modality LUT adapter."),
            new("voi-lut", "(15)", "VOI LUT", "xpe_display", "xpe_display.dll", "1b", "presentation", "presentation", "SRS-DISPLAY FR-VOI", "VOI LUT tests", "mandatory", "adapter-pending", 15.0, true, false, false, false, null, "Add xpe_display VOI/window adapter."),
            new("presentation-lut", "(16)", "Presentation LUT / GSDF", "xpe_display", "xpe_display.dll", "1b", "presentation", "display-ready", "SRS-DISPLAY FR-GSDF", "GSDF tests", "mandatory", "adapter-pending", 16.0, true, false, false, false, null, "Add xpe_display GSDF adapter."),
            new("dicom-write", "(17)", "DICOM write / export", "xpe_dicom", "xpe_dicom.dll", "1b", "presentation", "exported-dicom", "SRS-DICOM-001", "DICOM conformance tests", "mandatory", "adapter-pending", 17.0, true, false, false, false, null, "Add DICOM write adapter and IOD validation.")
        ];

        private static readonly string[] ProductCanonicalOrder =
        [
            "calib-folder", "readout", "temperature", "offset", "nonlinearity", "gain", "binning", "defect",
            "lag-ghost", "ei-whole", "log", "basic-noise", "contrast", "edge", "ai-bodypart",
            "collimation", "ai-collimation", "ei-roi", "gsvg", "multiscale", "fractional",
            "stitching", "bone-suppression", "dl-denoise", "modality-lut", "voi-lut", "presentation-lut", "dicom-write"
        ];

        private static readonly string[] PreE2eProofOrder =
        [
            "calib-folder", "readout", "nonlinearity", "offset", "gain", "temperature", "defect",
            "binning", "lag-ghost"
        ];

        private static readonly string[] RunnablePreprocessOrder =
        [
            "calib-folder", "offset", "gain", "defect"
        ];

        private static readonly string[] RunnablePostBasicOrder =
        [
            "ei-whole", "log", "basic-noise", "contrast", "edge"
        ];

        private static readonly string[] RunnablePrePostBasicOrder =
        [
            "calib-folder", "offset", "gain", "defect", "ei-whole", "log", "basic-noise", "contrast", "edge",
            "modality-lut", "voi-lut", "presentation-lut", "dicom-write"
        ];

        public static IReadOnlyList<AlgorithmNode> BuildNodes(
            IReadOnlyList<ModuleReadinessSnapshot> readiness,
            IReadOnlyList<AlgorithmValidationItem> calibrationValidation)
        {
            var readinessByModule = readiness.ToDictionary(
                item => item.ModuleName,
                item => item,
                StringComparer.OrdinalIgnoreCase);
            var validationByStage = calibrationValidation
                .Where(item => !string.IsNullOrWhiteSpace(item.StageKey))
                .GroupBy(item => item.StageKey!, StringComparer.OrdinalIgnoreCase)
                .ToDictionary(group => group.Key, group => group.First(), StringComparer.OrdinalIgnoreCase);

            return Definitions.Select(definition =>
            {
                validationByStage.TryGetValue(definition.StageKey, out var validation);
                readinessByModule.TryGetValue(definition.ModuleName, out var module);
                var adapter = validation?.Adapter ?? definition.Adapter;
                var canRun = validation?.CanRun == true && IsRunnableAdapter(adapter);
                var status = validation?.Status ??
                    (module is null ? "Module not registered" : $"{module.Level} {module.Status}");
                var evidence = validation?.Evidence ?? module?.Evidence ?? "No readiness evidence.";
                var nextAction = validation?.NextAction ?? definition.NextAction;

                return new AlgorithmNode(
                    definition.StageKey,
                    definition.Label,
                    definition.AlgorithmName,
                    definition.ModuleName,
                    definition.Owner,
                    definition.Phase,
                    definition.InputDomain,
                    definition.OutputDomain,
                    validation?.RequirementIds ?? definition.RequirementIds,
                    validation?.TestIds ?? definition.TestIds,
                    validation?.Gate ?? definition.Gate,
                    adapter,
                    status,
                    evidence,
                    nextAction,
                    definition.DefaultOrder,
                    definition.Mandatory,
                    definition.Conditional,
                    canRun,
                    definition.RequiresSequence,
                    definition.IsBranch,
                    definition.CalibrationRole);
            }).ToArray();
        }

        public static IReadOnlyList<AlgorithmNode> BuildPreset(
            IReadOnlyList<AlgorithmNode> nodes,
            AlgorithmChainPreset preset)
        {
            var order = preset switch
            {
                AlgorithmChainPreset.ProductCanonical => ProductCanonicalOrder,
                AlgorithmChainPreset.PreE2eProof => PreE2eProofOrder,
                AlgorithmChainPreset.RunnablePostBasic => RunnablePostBasicOrder,
                AlgorithmChainPreset.RunnablePrePostBasic => RunnablePrePostBasicOrder,
                _ => RunnablePreprocessOrder
            };

            return order
                .Select(stageKey => nodes.FirstOrDefault(node =>
                    string.Equals(node.StageKey, stageKey, StringComparison.OrdinalIgnoreCase)))
                .Where(node => node is not null)
                .Select(node => node!)
                .ToArray();
        }

        public static AlgorithmChainPlan BuildPlan(IReadOnlyList<AlgorithmNode> selectedNodes)
        {
            var steps = selectedNodes
                .Select((node, index) => new AlgorithmChainStep(index + 1, node))
                .ToArray();
            var findings = new List<AlgorithmDependencyFinding>();

            if (steps.Length == 0)
            {
                findings.Add(Hard("CHAIN-EMPTY", "Select at least one algorithm stage.", "GUI chain editor"));
                return CreatePlan(steps, findings);
            }

            AddDuplicateFindings(steps, findings);
            AddAdapterFindings(steps, findings);
            AddFolderFindings(steps, findings);
            AddSequenceFindings(steps, findings);
            AddOrderFindings(steps, findings);
            AddDomainFindings(steps, findings);
            AddBranchFindings(steps, findings);

            return CreatePlan(steps, findings);
        }

        private static AlgorithmChainPlan CreatePlan(
            IReadOnlyList<AlgorithmChainStep> steps,
            IReadOnlyList<AlgorithmDependencyFinding> findings)
        {
            var nativeOrder = steps
                .Where(step => IsNativePreprocessStage(step.StageKey))
                .Select(step => step.StageKey)
                .ToArray();
            var enhanceBasicOrder = steps
                .Where(step => IsNativeEnhanceBasicStage(step.StageKey))
                .Select(step => step.StageKey)
                .ToArray();
            var advancedOrder = steps
                .Where(step => IsAdvancedStage(step.StageKey))
                .Select(step => step.StageKey)
                .ToArray();
            var aiOrder = steps
                .Where(step => IsAiStage(step.StageKey))
                .Select(step => step.StageKey)
                .ToArray();
            var displayOrder = steps
                .Where(step => IsDisplayStage(step.StageKey))
                .Select(step => step.StageKey)
                .ToArray();
            var dicomOrder = steps
                .Where(step => IsDicomStage(step.StageKey))
                .Select(step => step.StageKey)
                .ToArray();
            var isFolderAuditOnly = steps.Count == 1 &&
                string.Equals(steps[0].StageKey, "calib-folder", StringComparison.OrdinalIgnoreCase);
            var hasHardBlocks = findings.Any(item => item.Severity == AlgorithmRuleSeverity.Hard);
            var canExecute = !hasHardBlocks &&
                (isFolderAuditOnly ||
                 nativeOrder.Length > 0 ||
                 enhanceBasicOrder.Length > 0 ||
                 displayOrder.Length > 0 ||
                 dicomOrder.Length > 0);
            var summary = BuildSummary(
                steps,
                findings,
                nativeOrder,
                enhanceBasicOrder,
                advancedOrder,
                aiOrder,
                displayOrder,
                dicomOrder,
                isFolderAuditOnly,
                canExecute);

            return new AlgorithmChainPlan(
                steps,
                findings,
                nativeOrder,
                enhanceBasicOrder,
                advancedOrder,
                aiOrder,
                displayOrder,
                dicomOrder,
                isFolderAuditOnly,
                canExecute,
                summary);
        }

        private static void AddDuplicateFindings(
            IReadOnlyList<AlgorithmChainStep> steps,
            List<AlgorithmDependencyFinding> findings)
        {
            foreach (var duplicate in steps.GroupBy(step => step.StageKey, StringComparer.OrdinalIgnoreCase)
                         .Where(group => group.Count() > 1))
            {
                findings.Add(Hard(
                    "CHAIN-DUP",
                    $"Stage '{duplicate.Key}' appears more than once.",
                    "A stage can only contribute one deterministic output in a single evaluation chain."));
            }
        }

        private static void AddAdapterFindings(
            IReadOnlyList<AlgorithmChainStep> steps,
            List<AlgorithmDependencyFinding> findings)
        {
            foreach (var step in steps)
            {
                if (string.Equals(step.Node.Adapter, "folder-audit", StringComparison.OrdinalIgnoreCase))
                {
                    continue;
                }

                if (IsNativePreprocessStage(step.StageKey))
                {
                    if (!step.Node.CanRun)
                    {
                        findings.Add(Hard(
                            "NATIVE-NOT-READY",
                            $"{step.Node.Label} {step.Node.AlgorithmName} is selected but the native preprocess adapter is not ready.",
                            step.Node.NextAction));
                    }

                    continue;
                }

                if (IsNativeEnhanceBasicStage(step.StageKey))
                {
                    if (!step.Node.CanRun)
                    {
                        findings.Add(Hard(
                            "POST-NOT-READY",
                            $"{step.Node.Label} {step.Node.AlgorithmName} is selected but the native enhance_basic adapter is not ready.",
                            step.Node.NextAction));
                    }

                    continue;
                }

                if (IsDisplayStage(step.StageKey))
                {
                    if (!step.Node.CanRun)
                    {
                        findings.Add(Hard(
                            "DISPLAY-NOT-READY",
                            $"{step.Node.Label} {step.Node.AlgorithmName} is selected but the xpe_display readiness smoke is not ready.",
                            step.Node.NextAction));
                    }

                    continue;
                }

                if (IsAdvancedStage(step.StageKey))
                {
                    findings.Add(Hard(
                        "ADVANCED-ADAPTER-PENDING",
                        $"{step.Node.Label} {step.Node.AlgorithmName} is selected, but Advanced C3/C4 execution is waiting on the Phase 2 native adapter contract.",
                        step.Node.NextAction));
                    continue;
                }

                if (IsDicomStage(step.StageKey))
                {
                    if (!step.Node.CanRun)
                    {
                        findings.Add(Hard(
                            "DICOM-NOT-READY",
                            $"{step.Node.Label} {step.Node.AlgorithmName} is selected but the xpe_dicom readiness smoke is not ready.",
                            step.Node.NextAction));
                    }

                    continue;
                }

                if (IsGsvgStage(step.StageKey))
                {
                    findings.Add(Hard(
                        "GSVG-ADAPTER-PENDING",
                        $"{step.Node.Label} {step.Node.AlgorithmName} is selected, but GSVG C1/C2 execution is waiting on the Post-B #61 native adapter contract.",
                        step.Node.NextAction));
                    continue;
                }

                if (IsAiStage(step.StageKey))
                {
                    findings.Add(Hard(
                        "AI-ADAPTER-PENDING",
                        $"{step.Node.Label} {step.Node.AlgorithmName} is selected, but AI C5/C6 execution is waiting on xpe_ai_worker heartbeat/IPC smoke and a GUI adapter contract.",
                        step.Node.NextAction));
                    continue;
                }

                findings.Add(Hard(
                    "ADAPTER-PENDING",
                    $"{step.Node.Label} {step.Node.AlgorithmName} is selected but its GUI execution adapter is not available yet.",
                    step.Node.NextAction));
            }
        }

        private static void AddFolderFindings(
            IReadOnlyList<AlgorithmChainStep> steps,
            List<AlgorithmDependencyFinding> findings)
        {
            var folderIndex = IndexOf(steps, "calib-folder");
            if (folderIndex > 0)
            {
                findings.Add(Hard(
                    "BOOT-ORDER",
                    "Calibration manifest load/audit must be first when it is included.",
                    "pipeline-spec BOOT-0; SRS-CALIB-FUNC-001..003,015"));
            }

            if (folderIndex < 0 && steps.Any(step => IsDetectorOrLaterImageStage(step.StageKey)))
            {
                findings.Add(Soft(
                    "BOOT-MISSING",
                    "Calibration folder audit is not in the chain. Product validation should include BOOT-0 evidence before image correction.",
                    "pipeline-spec 5.1; PRE-E2E-0"));
            }
        }

        private static void AddSequenceFindings(
            IReadOnlyList<AlgorithmChainStep> steps,
            List<AlgorithmDependencyFinding> findings)
        {
            foreach (var step in steps.Where(step => step.Node.RequiresSequence))
            {
                findings.Add(Hard(
                    "SEQUENCE-INPUT",
                    $"{step.Node.Label} {step.Node.AlgorithmName} requires a multi-frame or sequence input, but the current GUI workflow loads one target raw.",
                    step.Node.NextAction));
            }
        }

        private static void AddOrderFindings(
            IReadOnlyList<AlgorithmChainStep> steps,
            List<AlgorithmDependencyFinding> findings)
        {
            RequireBefore(steps, findings, "offset", "gain", "CAL-OFFSET-GAIN",
                "Offset correction must run before gain correction.",
                "pipeline-spec (1)->(2); SRS-CALIB-FUNC-004/005; SRS-CALIB-SAFE-001");
            RequireBeforeIfBoth(steps, findings, "nonlinearity", "gain", "CAL-NONLIN-GAIN",
                "Nonlinearity correction must run before gain correction when selected.",
                "SRS-CALIB-FUNC-006; ALG-SPEC 6.2");
            RequireBefore(steps, findings, "gain", "defect", "CAL-GAIN-DEFECT",
                "Defect correction needs detector-float input after gain correction.",
                "pipeline-spec (2)->(3); SRS-CALIB-FUNC-007");
            RequireBeforeIfBoth(steps, findings, "binning", "defect", "CAL-BINNING-DEFECT",
                "Binning correction is canonical before defect correction when both are selected.",
                "pipeline-spec (2.5)->(3); SRS-CALIB-FUNC-012");
            RequireBeforeIfBoth(steps, findings, "defect", "lag-ghost", "CAL-DEFECT-GHOST",
                "Lag/ghost correction is canonical after defect correction.",
                "pipeline-spec (3)->(4); SRS-CALIB-FUNC-013/014");
            RequireBeforeIfBoth(steps, findings, "ei-whole", "log", "EI-BEFORE-LOG",
                "Whole-image EI/DI must be computed before log transform.",
                "pipeline-spec EI-0 before (5); SRS-ENHANCE-BASIC FR-100-003");
            RequireBeforeIfBoth(steps, findings, "collimation", "ei-roi", "ROI-BEFORE-EI1",
                "Baseline collimation detection must complete before ROI-aware EI refinement.",
                "pipeline-spec parallel branch rule 5.3");
            RequireBeforeIfBoth(steps, findings, "log", "multiscale", "LOG-BEFORE-ADV",
                "Advanced multiscale processing expects log-domain input after the Phase 1b log transform.",
                "SRS-ENHANCE-ADV scope: advanced module consumes log-transformed float32 images.");
            RequireBeforeIfBoth(steps, findings, "multiscale", "fractional", "ADV-MULTISCALE-BEFORE-FRACTIONAL",
                "Fractional processing is canonical after multiscale processing when both are selected.",
                "Phase 2 advanced workflow order: multiscale before fractional.");
            RequireBeforeIfBoth(steps, findings, "collimation", "ai-collimation", "AI-COLLIMATION-REQ-BASELINE",
                "AI collimation refinement expects the baseline ROI proposal before it runs.",
                "SRS-AI FR-AI-150: AI ROI is a non-blocking refinement of the baseline collimation ROI.");
            RequireBeforeIfBoth(steps, findings, "ai-bodypart", "bone-suppression", "AI-BODYPART-BEFORE-BONE",
                "Bone suppression should preserve body-part advisory context when both AI branches are selected.",
                "SRS-AI FR-AI-130/170: body-part confidence remains sidecar-only evidence for assistive AI outputs.");
            RequireBeforeIfBoth(steps, findings, "ai-bodypart", "dl-denoise", "AI-BODYPART-BEFORE-DL-DENOISE",
                "DL denoising should preserve body-part advisory context when both AI branches are selected.",
                "SRS-AI SAF-AI-130: AI confidence and fallback state must stay explicit in sidecar evidence.");
            RequireBeforeIfBoth(steps, findings, "basic-noise", "dl-denoise", "AI-DL-DENOISE-AFTER-CLASSICAL",
                "DL denoising is canonical after the deterministic noise-reduction baseline when both are selected.",
                "SRS-AI SWU-2.12: classical noise reduction is the fallback path.");

            RequirePresentBefore(steps, findings, "gain", "offset", "CAL-GAIN-REQ-OFFSET",
                "Gain correction is selected without offset correction.",
                "SRS-CALIB-SAFE-001: offset and gain are mandatory product-mode corrections.");
            RequirePresentBefore(steps, findings, "defect", "gain", "CAL-DEFECT-REQ-GAIN",
                "Defect correction is selected without gain correction.",
                "pipeline-spec (2)->(3) detector-float boundary.");
            AddStandalonePostWarning(steps, findings, "ei-whole", "EI-REQ-GAIN",
                "Whole-image EI/DI is selected without gain-corrected detector-float input. GUI will run it as standalone post validation on a float-cast raw buffer unless preprocess stages are selected before it.",
                "pipeline-spec EI-0 input domain is detector float.");
            AddStandalonePostWarning(steps, findings, "log", "LOG-REQ-GAIN",
                "Log transform is selected without gain-corrected detector-float input. GUI will run it as standalone post validation on a float-cast raw buffer unless preprocess stages are selected before it.",
                "SRS-ENHANCE-BASIC IF-200-001.");

            if (Contains(steps, "offset") && Contains(steps, "nonlinearity"))
            {
                findings.Add(Soft(
                    "CAL-OFFSET-NONLIN-ORDER",
                    "Offset/nonlinearity relative order differs between product canonical and Pre-E2E proof documents; preserve the selected order in evidence.",
                    "pipeline-spec places offset before nonlinearity; Preprocessing-E2E places nonlinearity before dark/offset. Both agree nonlinearity must precede gain."));
            }
        }

        private static void AddDomainFindings(
            IReadOnlyList<AlgorithmChainStep> steps,
            List<AlgorithmDependencyFinding> findings)
        {
            var previousRank = -1;
            AlgorithmChainStep? previous = null;
            foreach (var step in steps.Where(step => !step.Node.IsBranch && !string.Equals(step.StageKey, "calib-folder", StringComparison.OrdinalIgnoreCase)))
            {
                var rank = DomainRank(step.Node.InputDomain);
                if (previous is not null && rank < previousRank)
                {
                    findings.Add(Hard(
                        "DOMAIN-ORDER",
                        $"{step.Node.Label} {step.Node.AlgorithmName} moves data back from a later domain to {step.Node.InputDomain}.",
                        "pipeline-spec invariant: detector-domain corrections complete before enhancement/presentation work."));
                }

                previousRank = Math.Max(previousRank, rank);
                previous = step;
            }
        }

        private static void AddBranchFindings(
            IReadOnlyList<AlgorithmChainStep> steps,
            List<AlgorithmDependencyFinding> findings)
        {
            foreach (var step in steps.Where(step => step.Node.IsBranch))
            {
                findings.Add(Advisory(
                    "BRANCH-OUTPUT",
                    $"{step.Node.Label} {step.Node.AlgorithmName} is a branch/advisory output and must not silently overwrite the deterministic baseline image.",
                    "pipeline-spec 5.3 parallel branch rules; ALG-SPEC AI governance."));
            }
        }

        private static void RequireBefore(
            IReadOnlyList<AlgorithmChainStep> steps,
            List<AlgorithmDependencyFinding> findings,
            string before,
            string after,
            string ruleId,
            string message,
            string evidence)
        {
            var beforeIndex = IndexOf(steps, before);
            var afterIndex = IndexOf(steps, after);
            if (beforeIndex < 0 || afterIndex < 0)
            {
                return;
            }

            if (beforeIndex > afterIndex)
            {
                findings.Add(Hard(ruleId, message, evidence));
            }
        }

        private static void RequireBeforeIfBoth(
            IReadOnlyList<AlgorithmChainStep> steps,
            List<AlgorithmDependencyFinding> findings,
            string before,
            string after,
            string ruleId,
            string message,
            string evidence) => RequireBefore(steps, findings, before, after, ruleId, message, evidence);

        private static void RequirePresentBefore(
            IReadOnlyList<AlgorithmChainStep> steps,
            List<AlgorithmDependencyFinding> findings,
            string stage,
            string requiredBefore,
            string ruleId,
            string message,
            string evidence)
        {
            var stageIndex = IndexOf(steps, stage);
            if (stageIndex < 0)
            {
                return;
            }

            var requiredIndex = IndexOf(steps, requiredBefore);
            if (requiredIndex < 0 || requiredIndex > stageIndex)
            {
                findings.Add(Hard(ruleId, message, evidence));
            }
        }

        private static void AddStandalonePostWarning(
            IReadOnlyList<AlgorithmChainStep> steps,
            List<AlgorithmDependencyFinding> findings,
            string stage,
            string ruleId,
            string message,
            string evidence)
        {
            var stageIndex = IndexOf(steps, stage);
            if (stageIndex < 0)
            {
                return;
            }

            var gainIndex = IndexOf(steps, "gain");
            if (gainIndex < 0 || gainIndex > stageIndex)
            {
                findings.Add(Soft(ruleId, message, evidence));
            }
        }

        private static bool Contains(IReadOnlyList<AlgorithmChainStep> steps, string stageKey) =>
            IndexOf(steps, stageKey) >= 0;

        private static int IndexOf(IReadOnlyList<AlgorithmChainStep> steps, string stageKey)
        {
            for (var i = 0; i < steps.Count; i++)
            {
                if (string.Equals(steps[i].StageKey, stageKey, StringComparison.OrdinalIgnoreCase))
                {
                    return i;
                }
            }

            return -1;
        }

        private static bool IsRunnableAdapter(string adapter) =>
            string.Equals(adapter, "native-preview", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(adapter, "native-enhance-basic", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(adapter, "native-display", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(adapter, "native-dicom", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(adapter, "folder-audit", StringComparison.OrdinalIgnoreCase);

        private static bool IsNativePreprocessStage(string stageKey) =>
            string.Equals(stageKey, "offset", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(stageKey, "gain", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(stageKey, "defect", StringComparison.OrdinalIgnoreCase);

        private static bool IsNativeEnhanceBasicStage(string stageKey) =>
            string.Equals(stageKey, "ei-whole", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(stageKey, "log", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(stageKey, "basic-noise", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(stageKey, "contrast", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(stageKey, "edge", StringComparison.OrdinalIgnoreCase);

        private static bool IsDisplayStage(string stageKey) =>
            string.Equals(stageKey, "modality-lut", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(stageKey, "voi-lut", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(stageKey, "presentation-lut", StringComparison.OrdinalIgnoreCase);

        private static bool IsAdvancedStage(string stageKey) =>
            string.Equals(stageKey, "collimation", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(stageKey, "ei-roi", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(stageKey, "multiscale", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(stageKey, "fractional", StringComparison.OrdinalIgnoreCase);

        private static bool IsDicomStage(string stageKey) =>
            string.Equals(stageKey, "dicom-write", StringComparison.OrdinalIgnoreCase);

        private static bool IsGsvgStage(string stageKey) =>
            string.Equals(stageKey, "gsvg", StringComparison.OrdinalIgnoreCase);

        private static bool IsAiStage(string stageKey) =>
            string.Equals(stageKey, "ai-bodypart", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(stageKey, "ai-collimation", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(stageKey, "stitching", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(stageKey, "bone-suppression", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(stageKey, "dl-denoise", StringComparison.OrdinalIgnoreCase);

        private static bool IsDetectorOrLaterImageStage(string stageKey) =>
            !string.Equals(stageKey, "calib-folder", StringComparison.OrdinalIgnoreCase);

        private static int DomainRank(string domain)
        {
            if (domain.Contains("N/A", StringComparison.OrdinalIgnoreCase))
            {
                return 0;
            }

            if (domain.Contains("detector", StringComparison.OrdinalIgnoreCase))
            {
                return 1;
            }

            if (domain.Contains("enhancement", StringComparison.OrdinalIgnoreCase) ||
                domain.Contains("log", StringComparison.OrdinalIgnoreCase))
            {
                return 2;
            }

            if (domain.Contains("presentation", StringComparison.OrdinalIgnoreCase) ||
                domain.Contains("display", StringComparison.OrdinalIgnoreCase) ||
                domain.Contains("dicom", StringComparison.OrdinalIgnoreCase))
            {
                return 3;
            }

            return 1;
        }

        private static string BuildSummary(
            IReadOnlyList<AlgorithmChainStep> steps,
            IReadOnlyList<AlgorithmDependencyFinding> findings,
            IReadOnlyList<string> nativeOrder,
            IReadOnlyList<string> enhanceBasicOrder,
            IReadOnlyList<string> advancedOrder,
            IReadOnlyList<string> aiOrder,
            IReadOnlyList<string> displayOrder,
            IReadOnlyList<string> dicomOrder,
            bool isFolderAuditOnly,
            bool canExecute)
        {
            var hard = findings.Count(item => item.Severity == AlgorithmRuleSeverity.Hard);
            var soft = findings.Count(item => item.Severity == AlgorithmRuleSeverity.Soft);
            var advisory = findings.Count(item => item.Severity == AlgorithmRuleSeverity.Advisory);
            var order = steps.Count == 0
                ? "none"
                : string.Join(" -> ", steps.Select(step => step.Node.Label));
            var executableStages = string.Join(
                " -> ",
                nativeOrder
                    .Concat(enhanceBasicOrder)
                    .Concat(displayOrder)
                    .Concat(dicomOrder));
            var advanced = advancedOrder.Count == 0
                ? "advanced=none"
                : $"advanced selected: {string.Join(" -> ", advancedOrder)}";
            var ai = aiOrder.Count == 0
                ? "ai=none"
                : $"ai selected: {string.Join(" -> ", aiOrder)}";
            var execution = canExecute
                ? isFolderAuditOnly ? "folder audit runnable" : $"native runnable: {executableStages}"
                : "blocked";

            return $"Chain: {order}; rules hard={hard}, soft={soft}, advisory={advisory}; {advanced}; {ai}; execution={execution}.";
        }

        private static AlgorithmDependencyFinding Hard(string ruleId, string message, string evidence) =>
            new(AlgorithmRuleSeverity.Hard, ruleId, message, evidence);

        private static AlgorithmDependencyFinding Soft(string ruleId, string message, string evidence) =>
            new(AlgorithmRuleSeverity.Soft, ruleId, message, evidence);

        private static AlgorithmDependencyFinding Advisory(string ruleId, string message, string evidence) =>
            new(AlgorithmRuleSeverity.Advisory, ruleId, message, evidence);
    }
}
