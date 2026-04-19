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
            new("xpe_preprocess", "SWU-1.4", "GhostCorrector", "SRS-CALIB-FUNC-013, SRS-CALIB-FUNC-020", "UT-1.4, PRE-E2E-1, PRE-E2E-2, BP-05", "R4", "adapter-pending", null, "Add lag history fixture sequence, tier selection, bypass reason, and residual metrics.")
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
            var canRun = isFolderAudit ||
                (string.Equals(definition.Adapter, "native-preview", StringComparison.OrdinalIgnoreCase) &&
                    definition.StageKey is not null &&
                    module?.ProcessingEnabled == true);

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
