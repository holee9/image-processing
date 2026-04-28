using System;
using System.Collections.Generic;
using System.Linq;

namespace ImageProcTest
{
    internal static class AlgorithmValidationCatalogService
    {
        private sealed record AlgorithmDefinition(
            string ModuleName,
            string SwuId,
            string AlgorithmName,
            string RequirementIds,
            string TestIds,
            string Gate,
            string Adapter,
            string? StageKey,
            string NextAction);

        private static readonly AlgorithmDefinition[] Definitions =
        [
            new("xpe_preprocess", "SWU-1.5", "CalibFileIO and Expiry", "SRS-CALIB-FUNC-001..003, SRS-CALIB-FUNC-009, SRS-CALIB-SAFE-001..005", "UT-1.5-001..018, IT-CALIB-001..005, PRE-E2E-0, PRE-E2E-5", "PRE-E2E-0/5", "folder-audit", "calib-folder", "Select the acquired calibration folder and verify role assignment, mandatory offset/gain presence, unknown files, SHA-256 evidence, expiry, and mismatch policy."),
            new("xpe_preprocess", "SWU-1.1", "OffsetCorrector", "SRS-CALIB-FUNC-004, SRS-CALIB-FUNC-016, SRS-CALIB-SAFE-001", "UT-1.1, UT-1.5-001, PRE-E2E-1, PRE-E2E-2, BP-01", "PRE-E2E-2", "native-preview", "offset", "Select the acquired calibration folder with Offset/Dark data, load a target raw, then apply offset correction."),
            new("xpe_preprocess", "SWU-1.2", "GainCorrector", "SRS-CALIB-FUNC-005, SRS-CALIB-FUNC-017, SRS-CALIB-FUNC-018, SRS-CALIB-SAFE-002", "UT-1.2, UT-1.5-002, PRE-E2E-1, PRE-E2E-2, BP-02, BP-03", "PRE-E2E-2", "native-preview", "gain", "Select the acquired calibration folder with Offset/Dark and Gain/Flat data, load a target raw, then apply the minimum offset+gain chain."),
            new("xpe_preprocess", "SWU-1.3", "DefectCorrector", "SRS-CALIB-FUNC-007, SRS-CALIB-FUNC-010, SRS-CALIB-FUNC-019", "UT-1.3, UT-1.5-003, PRE-E2E-1, PRE-E2E-2, BP-04", "PRE-E2E-2", "native-preview", "defect", "Select the acquired calibration folder with Offset/Gain/BPM data, load a target raw, then apply offset+gain+defect correction."),
            new("xpe_preprocess", "SWU-1.11", "BpmGenerator", "SRS-CALIB-FUNC-022, SRS-CALIB-FUNC-023, SRS-CALIB-FUNC-024, SRS-CALIB-FUNC-025", "BP-06, BP-07, TDS-CALIB-001 9.4", "R4", "adapter-pending", null, "Add BPM generation P/Invoke adapter and multi-frame dark/bright input panel before enabling execution."),
            new("xpe_preprocess", "SWU-1.6", "TempCompensator", "SRS-CALIB-FUNC-008, SRS-CALIB-FUNC-016", "UT-1.5-005, IT-CALIB-002, BP-01", "R4", "adapter-pending", null, "Add detector temperature metadata and dark interpolation evidence before GUI execution."),
            new("xpe_preprocess", "SWU-1.7", "NonlinearityCorrector", "SRS-CALIB-FUNC-006", "UT-1.5-004, TDS-CALIB-001 4.6", "R4", "adapter-pending", null, "Add LUT/polynomial calibration profile input and monotonicity evidence before enabling execution."),
            new("xpe_preprocess", "SWU-1.8", "BinningCorrector", "SRS-CALIB-FUNC-012", "UT binning, detector metadata tests", "R4", "adapter-pending", null, "Add binning-mode detector metadata and gain scaling evidence."),
            new("xpe_preprocess", "SWU-1.9", "RuntimeDefectDetector", "SRS-CALIB-FUNC-010, SRS-CALIB-FUNC-019", "IT runtime defect, BP-04 runtime", "R4", "adapter-pending", null, "Add 10-frame sequence input and runtime BPM merge evidence."),
            new("xpe_preprocess", "SWU-1.10", "SessionManager", "SRS-CALIB-FUNC-011, SRS-CALIB-FUNC-014, SRS-CALIB-NFR-006", "UT-1.5-013, UT-1.5-014, IT-CALIB-005", "R4", "adapter-pending", null, "Add session-id and isolation evidence for multi-frame calibration state."),
            new("xpe_preprocess", "SWU-1.4", "GhostCorrector", "SRS-CALIB-FUNC-013, SRS-CALIB-FUNC-020", "UT-1.4, PRE-E2E-1, PRE-E2E-2, BP-05", "R4", "adapter-pending", null, "Add lag history fixture sequence, tier selection, bypass reason, and residual metrics."),
            new("xpe_enhance_basic", "SWU-2.10", "ExposureIndex", "REQ-ENH-023..030", "EI baseline tests", "R3", "native-enhance-basic", "ei-whole", "Run EI/DI on the active detector-float preview buffer and review EI, DI, and alert evidence."),
            new("xpe_enhance_basic", "SWU-2.1", "LogTransform", "REQ-ENH-001..006", "TST-100-001", "R3", "native-enhance-basic", "log", "Run log transform on the active detector-float preview buffer and review output range/histogram changes."),
            new("xpe_enhance_basic", "SWU-2.2", "NoiseReduction", "REQ-ENH-007..012", "TST-200-001", "R3", "native-enhance-basic", "basic-noise", "Run bilateral noise reduction and compare sigma/visual texture before and after."),
            new("xpe_enhance_basic", "SWU-2.3", "ContrastEnhancement", "REQ-ENH-013..017", "TST-100-003/004", "R3", "native-enhance-basic", "contrast", "Run CLAHE contrast enhancement and compare histogram distribution and visual contrast."),
            new("xpe_enhance_basic", "SWU-2.4", "EdgeEnhancement", "REQ-ENH-018..022", "TST-200-001", "R3", "native-enhance-basic", "edge", "Run USM edge enhancement and inspect edge detail, overshoot, and changed-pixel metrics."),
            new("xpe_enhance_advanced", "SWU-2.5", "Multiscale / tiered denoise", "FR-100..600", "test_gaussian_denoise, test_bilateral_filter, test_nlm_denoise, test_wavelet_shrink, test_wire_phantom_mtf", "Phase 2 C3 gate", "advanced-adapter-pending", "multiscale", "Expose the C3 advanced multiscale row and skip reason; enable execution only after xpe_enhance_advanced exports and GUI adapter smoke checks exist."),
            new("xpe_enhance_advanced", "SWU-2.6", "Fractional processing", "PR-FUNC-006, advanced fractional processing", "advanced fractional regression bundle", "Phase 2 C3 gate", "advanced-adapter-pending", "fractional", "Expose the C3 fractional row and skip reason; keep workflow blocked until the native adapter contract is available."),
            new("xpe_enhance_advanced", "SWU-2.8", "CollimationDetector", "FR-1100..1300, HAZ-ADV-003", "TC-ROI-002..004, TC-Real-004", "Phase 2 C4 workflow gate", "advanced-adapter-pending", "collimation", "Expose the C4 collimation workflow row, ROI sidecar expectation, confidence fallback, and skip reason before native execution is enabled."),
            new("xpe_enhance_advanced", "SWU-2.10", "ROI-aware EI refinement", "EI ROI correction, PR-FUNC-006", "TC-Real-005, EI_error <= 5%", "Phase 2 C4 workflow gate", "advanced-adapter-pending", "ei-roi", "Expose the C4 ROI-aware EI workflow row; keep full-image EI as the safe fallback until ROI sidecar and adapter evidence exist."),
            new("gsvg", "SI-001", "PipelineManager / ProcessingConfig", "IF-002..005, SAFE-003, SAFE-005, VG-FR-008", "IT-003, IT-004, ST-007, ST-008", "Phase 2 C1/C2 gate", "gsvg-adapter-pending", "gsvg", "Track GSVG C1/C2 in the UI, but keep execution blocked until #61 adds stable processing exports and a GUI adapter contract."),
            new("gsvg", "SI-002", "Grid Suppression (C1)", "GS-FR-001..008", "UT-GS-001..007, IT-001, ST-001, ST-002", "Phase 2 C1 gate", "gsvg-adapter-pending", "gsvg", "Expose the C1 grid suppression row and skip reason; enable execution only after #61 provides the processing export and benchmark fixture."),
            new("gsvg", "SI-003", "Virtual Grid (C2)", "VG-FR-001..010, SAFE-004", "UT-VG-001..007, IT-002, IT-003, ST-003..005", "Phase 2 C2 gate", "gsvg-adapter-pending", "gsvg", "Expose the C2 virtual grid row and skip reason; enable execution only after #61 provides LUT/config inputs and adapter smoke checks."),
            new("gsvg", "SI-004", "DICOM / ImageBuffer safety", "IF-001, SAFE-001, SAFE-002, PERF-004", "UT-CM-005, UT-CM-007, IT-004, ST-007", "Phase 2 safety gate", "gsvg-adapter-pending", "gsvg", "Keep GSVG safety and output marking visible in the GUI catalog while native execution remains blocked by #61."),
            new("xpe_ai", "SWU-2.7", "BodyPartClassifier", "FR-AI-130, FR-AI-140, SAF-AI-100, SAF-AI-130", "TC-AI-130-01..04, sidecar JSON validation", "Phase 3 C5 AI assistive gate", "ai-adapter-pending", "ai-bodypart", "Expose the C5 body-part advisory row, UNKNOWN fallback, and sidecar-only rule; enable execution only after xpe_ai_worker heartbeat and adapter smoke checks exist."),
            new("xpe_ai", "SWU-2.8-AI", "CollimationRefiner", "FR-AI-150, SAF-AI-100, SAF-AI-130", "TC-AI-150-01..03, confidence fallback", "Phase 3 C5/C6 ROI refinement gate", "ai-adapter-pending", "ai-collimation", "Expose AI ROI refinement as a non-blocking proposal branch; keep the original baseline ROI when confidence is low or the worker is unavailable."),
            new("xpe_ai", "SWU-2.9", "ImageStitcher", "FR-AI-160, PERF-AI-120, SAF-AI-100", "TC-AI-160-01..03, multi-frame rejection cases", "Phase 3 C6 sequence gate", "ai-adapter-pending", "stitching", "Expose stitching as a sequence-only branch; keep the single-image path when overlap, worker, or adapter evidence is unavailable."),
            new("xpe_ai", "SWU-2.11", "BoneSuppressionEngine", "FR-AI-170, PERF-AI-130, SAF-AI-130", "TC-AI-170-01..04, observer/task evidence", "Phase 3 C6 assistive-image gate", "ai-adapter-pending", "bone-suppression", "Expose bone suppression as an assistive secondary output; keep the deterministic baseline image when confidence or worker readiness is insufficient."),
            new("xpe_ai", "SWU-2.12", "DlDenoiser", "FR-AI-190, SAF-AI-130", "research DL degraded-mode tests", "Phase 3 C6 research gate", "ai-adapter-pending", "dl-denoise", "Expose DL denoising as a research-gated optional branch; keep classical noise reduction as the safe fallback."),
            new("xpe_display", "SWU-3.1", "ModalityLUT", "REQ-DISP-001..008", "display LUT smoke", "R3", "native-display", "modality-lut", "Run xpe_display readiness smoke and include modality LUT in the Phase 1b E2E evidence chain."),
            new("xpe_display", "SWU-3.2", "VOILUT", "REQ-DISP-009..018", "display VOI smoke", "R3", "native-display", "voi-lut", "Run xpe_display readiness smoke and include VOI/window LUT in the Phase 1b E2E evidence chain."),
            new("xpe_display", "SWU-3.3", "PresentationLUT", "REQ-DISP-019..028", "display presentation smoke", "R3", "native-display", "presentation-lut", "Run xpe_display readiness smoke and include presentation LUT/GSDF in the Phase 1b E2E evidence chain."),
            new("xpe_dicom", "SWU-4.2", "DicomWriterValidator", "REQ-DICOM-013..028", "DICOM write/validate smoke", "R3", "native-dicom", "dicom-write", "Run xpe_dicom readiness smoke and include DICOM write/validate readiness in the Phase 1b E2E evidence chain.")
        ];

        public static IReadOnlyList<AlgorithmValidationItem> Build(IReadOnlyList<ModuleReadinessSnapshot> readiness)
        {
            var byModule = readiness.ToDictionary(
                item => item.ModuleName,
                item => item,
                StringComparer.OrdinalIgnoreCase);

            return Definitions
                .Select(definition => BuildItem(definition, byModule))
                .ToArray();
        }

        private static AlgorithmValidationItem BuildItem(
            AlgorithmDefinition definition,
            IReadOnlyDictionary<string, ModuleReadinessSnapshot> readiness)
        {
            readiness.TryGetValue(definition.ModuleName, out var module);
            var level = module?.Level ?? "R0";
            var moduleStatus = module?.Status ?? "Module not registered";
            var isFolderAudit = string.Equals(definition.Adapter, "folder-audit", StringComparison.OrdinalIgnoreCase);
            var isNativeAdapter =
                string.Equals(definition.Adapter, "native-preview", StringComparison.OrdinalIgnoreCase) ||
                string.Equals(definition.Adapter, "native-enhance-basic", StringComparison.OrdinalIgnoreCase);
            var isReadinessSmokeAdapter =
                string.Equals(definition.Adapter, "native-display", StringComparison.OrdinalIgnoreCase) ||
                string.Equals(definition.Adapter, "native-dicom", StringComparison.OrdinalIgnoreCase);
            var canRun = isFolderAudit ||
                (isNativeAdapter &&
                    definition.StageKey is not null &&
                    module?.ProcessingEnabled == true) ||
                (isReadinessSmokeAdapter &&
                    definition.StageKey is not null &&
                    module?.LevelRank >= 3);

            var status = canRun
                ? "Runnable"
                : $"{level} {moduleStatus}";
            var evidence = canRun
                ? $"{module?.Evidence}; stage={definition.StageKey}"
                : module?.Evidence ?? "No module readiness evidence.";
            var nextAction = canRun
                ? definition.NextAction
                : $"{definition.NextAction} Module gate: {module?.NextAction ?? "Add module readiness probe."}";

            return new AlgorithmValidationItem(
                definition.ModuleName,
                definition.SwuId,
                definition.AlgorithmName,
                definition.RequirementIds,
                definition.TestIds,
                definition.Gate,
                definition.Adapter,
                level,
                status,
                evidence,
                nextAction,
                canRun,
                definition.StageKey);
        }
    }
}
