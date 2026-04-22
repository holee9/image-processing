using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace ImageProcTest
{
    internal static class ModuleReadinessService
    {
        public static IReadOnlyList<ModuleReadinessSnapshot> Evaluate(BackendHealthResult? commonHealth)
        {
            var root = FindRepositoryRoot(AppContext.BaseDirectory);
            var display = XpeDisplayVersionProbe.Check();
            var dicom = XpeDicomReadinessProbe.Check();
            var gsvg = XpeGsvgReadinessProbe.Check();
            var enhanceBasic = XpeEnhanceBasicReadinessProbe.Check();

            return
            [
                EvaluateCommon(commonHealth),
                EvaluateDisplay(display),
                EvaluatePreprocess(root),
                EvaluateEnhanceBasic(enhanceBasic),
                EvaluateDicom(dicom),
                EvaluateDllPresence("enhance_advanced", "xpe_enhance_advanced.dll"),
                EvaluateGsvg(gsvg),
                EvaluateDllPresence("ai", "xpe_ai.dll")
            ];
        }

        private static ModuleReadinessSnapshot EvaluateCommon(BackendHealthResult? health)
        {
            if (health?.IsNativeReady == true)
            {
                return new ModuleReadinessSnapshot(
                    "xpe_common",
                    "R2",
                    "ABI smoke ready",
                    $"version={health.Version}; {health.MemoryAbi}",
                    "Keep as baseline dependency for all later module adapters.",
                    ProcessingEnabled: false);
            }

            return new ModuleReadinessSnapshot(
                "xpe_common",
                "R0",
                "Unavailable",
                health?.Details ?? "No backend health result.",
                "Restore xpe_common.dll before native module adapters can be trusted.",
                ProcessingEnabled: false);
        }

        private static ModuleReadinessSnapshot EvaluateDisplay(DisplayHealthResult display)
        {
            if (display.IsSmokeReady)
            {
                return new ModuleReadinessSnapshot(
                    "xpe_display",
                    "R3",
                    "ABI smoke ready",
                    $"version={display.Version}; dll={display.DllPath}; exports={display.PresentExports.Count}; smoke={display.Smoke.Details}",
                    "Use xpe_display readiness smoke as Phase 1b E2E evidence; image-domain display rendering remains separately gated.",
                    ProcessingEnabled: false,
                    RequiredLevel: "R3",
                    DegradedMode: "Display DLL is callable; clinical display rendering remains Off until a verified image-domain adapter is approved.");
            }

            if (display.IsExportReady)
            {
                return new ModuleReadinessSnapshot(
                    "xpe_display",
                    "R2",
                    "Export checklist ready",
                    $"version={display.Version}; dll={display.DllPath}; smoke={display.Smoke.Status}; {display.Smoke.Details}",
                    "Fix display ABI smoke before enabling the full Phase 1b pipeline runner.",
                    ProcessingEnabled: false,
                    RequiredLevel: "R3",
                    DegradedMode: "Display stage remains Off because ABI smoke has not passed.");
            }

            if (display.IsVersionReady)
            {
                return new ModuleReadinessSnapshot(
                    "xpe_display",
                    "R1",
                    "Version ready, export checklist incomplete",
                    $"version={display.Version}; missing={string.Join(", ", display.MissingExports)}",
                    "Complete mandatory display exports before ABI smoke.",
                    ProcessingEnabled: false,
                    RequiredLevel: "R3",
                    DegradedMode: "Display stage remains Off because required exports are incomplete.");
            }

            return new ModuleReadinessSnapshot(
                "xpe_display",
                "R0",
                display.Status,
                display.Details,
                "Build xpe_display.dll with required display pipeline exports.",
                ProcessingEnabled: false,
                RequiredLevel: "R3",
                DegradedMode: "Display stage remains Off because xpe_display.dll is unavailable.");
        }

        private static ModuleReadinessSnapshot EvaluatePreprocess(string? root)
        {
            var health = XpePreprocessReadinessProbe.Check();
            var sourceExists = root is not null && Directory.Exists(Path.Combine(root, "modules", "preprocess", "src"));

            if (health.IsExportReady)
            {
                var executionEvidence = health.MissingExecutionExports.Count == 0
                    ? "native adapter chain uses offset/gain/defect exports"
                    : $"missing execution export(s): {string.Join(", ", health.MissingExecutionExports)}";
                var paramRangeEvidence = NativeReadinessProbe.FormatPreprocessParameterRanges(health.ParameterRanges);

                if (health.IsSyntheticOracleReady)
                {
                    return new ModuleReadinessSnapshot(
                        "xpe_preprocess",
                        "R3",
                        "Synthetic oracle ready",
                        $"version={health.Version}; dll={health.DllPath}; adapter-chain smoke passed; latency={health.SyntheticOracle.TotalLatencyMs:0.###}ms; {executionEvidence}; params={paramRangeEvidence}",
                        "GUI preview execution can use the native adapter chain; fixture-calibrated clinical processing still needs R4 E2E.",
                        ProcessingEnabled: true);
                }

                return new ModuleReadinessSnapshot(
                    "xpe_preprocess",
                    "R2",
                    "Version and export checklist ready",
                    $"version={health.Version}; dll={health.DllPath}; synthetic={health.SyntheticOracle.Status}; {executionEvidence}; params={paramRangeEvidence}",
                    "Next: fix synthetic oracle gaps, then run fixture E2E before enabling GUI execution.",
                    ProcessingEnabled: false);
            }

            if (health.IsVersionReady)
            {
                return new ModuleReadinessSnapshot(
                    "xpe_preprocess",
                    "R1",
                    "Version ready, export checklist incomplete",
                    $"version={health.Version}; missing={string.Join(", ", health.MissingExports)}; params={NativeReadinessProbe.FormatPreprocessParameterRanges(health.ParameterRanges)}",
                    "Complete mandatory exports before ABI smoke or execution controls.",
                    ProcessingEnabled: false);
            }

            if (health.Status == "DLL not found")
            {
                return new ModuleReadinessSnapshot(
                    "xpe_preprocess",
                    sourceExists ? "R0" : "R0",
                    sourceExists ? "Source present, binary not ready" : "Missing",
                    sourceExists ? "preprocess source exists but readiness build has no xpe_preprocess.dll." : "No preprocess source evidence.",
                    "Fix native build, verify exports, then run synthetic oracle before GUI execution.",
                    ProcessingEnabled: false);
            }

            return new ModuleReadinessSnapshot(
                "xpe_preprocess",
                "R1",
                health.Status,
                health.Details,
                "Fix preprocess binary load/export readiness before ABI smoke.",
                ProcessingEnabled: false);
        }

        private static ModuleReadinessSnapshot EvaluateEnhanceBasic(EnhanceBasicHealthResult health)
        {
            if (health.IsSmokeReady)
            {
                return new ModuleReadinessSnapshot(
                    "xpe_enhance_basic",
                    "R3",
                    "ABI smoke ready",
                    $"version={health.Version}; dll={health.DllPath}; smoke={health.Smoke.Status}; latency={health.Smoke.LatencyMs:0.###}ms; " +
                    $"sigma={health.Smoke.SigmaBefore:0.###}->{health.Smoke.SigmaAfter:0.###}; EI={health.Smoke.ExposureIndex:0.###}; DI={health.Smoke.DeviationIndex:0.###}; {health.Smoke.Details}",
                    "GUI post-processing evaluation can execute log/noise/contrast/edge/EI on the active detector-float preview buffer.",
                    ProcessingEnabled: true);
            }

            if (health.IsExportReady)
            {
                return new ModuleReadinessSnapshot(
                    "xpe_enhance_basic",
                    "R2",
                    "Version and export checklist ready",
                    $"version={health.Version}; dll={health.DllPath}; smoke={health.Smoke.Status}; {health.Smoke.Details}",
                    "Fix enhance_basic ABI smoke before enabling GUI post-processing execution.",
                    ProcessingEnabled: false);
            }

            if (health.IsVersionReady)
            {
                return new ModuleReadinessSnapshot(
                    "xpe_enhance_basic",
                    "R1",
                    "Version ready, export checklist incomplete",
                    $"version={health.Version}; missing={string.Join(", ", health.MissingExports)}",
                    "Complete mandatory enhance_basic exports before GUI execution.",
                    ProcessingEnabled: false);
            }

            return new ModuleReadinessSnapshot(
                "xpe_enhance_basic",
                "R0",
                health.Status,
                health.Details,
                "Build xpe_enhance_basic.dll and make it discoverable through the GUI native search path or XPE_NATIVE_DIR.",
                ProcessingEnabled: false);
        }

        private static ModuleReadinessSnapshot EvaluateDicom(DicomHealthResult health)
        {
            if (health.IsSmokeReady)
            {
                return new ModuleReadinessSnapshot(
                    "xpe_dicom",
                    "R3",
                    "ABI smoke ready",
                    $"dll={health.DllPath}; exports={health.PresentExports.Count}; smoke={health.Smoke.Details}",
                    "Use xpe_dicom readiness smoke as Phase 1b E2E evidence; file export remains separately gated.",
                    ProcessingEnabled: false,
                    RequiredLevel: "R3",
                    DegradedMode: "DICOM DLL is callable; clinical file export remains Off until a verified writer path is approved.");
            }

            if (health.IsExportReady)
            {
                return new ModuleReadinessSnapshot(
                    "xpe_dicom",
                    "R2",
                    "Export checklist ready",
                    $"dll={health.DllPath}; smoke={health.Smoke.Status}; {health.Smoke.Details}",
                    "Fix DICOM ABI smoke before enabling DICOM export.",
                    ProcessingEnabled: false,
                    RequiredLevel: "R3",
                    DegradedMode: "DICOM export remains Off because ABI smoke has not passed.");
            }

            if (health.PresentExports.Count > 0)
            {
                return new ModuleReadinessSnapshot(
                    "xpe_dicom",
                    "R1",
                    "Export checklist incomplete",
                    $"dll={health.DllPath}; missing={string.Join(", ", health.MissingExports)}",
                    "Complete mandatory DICOM exports before ABI smoke.",
                    ProcessingEnabled: false,
                    RequiredLevel: "R3",
                    DegradedMode: "DICOM export remains Off because required exports are incomplete.");
            }

            return new ModuleReadinessSnapshot(
                "xpe_dicom",
                "R0",
                health.Status,
                health.Details,
                "Build xpe_dicom.dll and make it discoverable through the GUI native search path or XPE_NATIVE_DIR.",
                ProcessingEnabled: false,
                RequiredLevel: "R3",
                DegradedMode: "DICOM export remains Off because xpe_dicom.dll is unavailable.");
        }

        private static ModuleReadinessSnapshot EvaluateGsvg(GsvgHealthResult health)
        {
            if (health.IsVersionReady)
            {
                return new ModuleReadinessSnapshot(
                    "gsvg",
                    "R1",
                    "Version-only health ready",
                    $"version={health.Version}; dll={health.DllPath}",
                    "Wait for #61 processing exports and GUI adapter contract before enabling GSVG execution.",
                    ProcessingEnabled: false,
                    RequiredLevel: "R3",
                    DegradedMode: "GSVG is Phase 2 and stays Off until #61 defines stable processing exports.");
            }

            return new ModuleReadinessSnapshot(
                "gsvg",
                "R0",
                health.Status,
                health.Details,
                "Complete #61, then add GSVG processing export and GUI adapter smoke checks.",
                ProcessingEnabled: false,
                RequiredLevel: "R3",
                DegradedMode: "GSVG is unavailable or planned, so the GUI skips it without blocking Phase 1b execution.");
        }

        private static ModuleReadinessSnapshot EvaluateDllPresence(string moduleName, string dllName)
        {
            var found = NativeModuleLibraryLocator.TryFindDll(
                dllName,
                moduleName,
                "image-processing",
                "xpe-post",
                "xpe-pre");

            if (found != null)
                return new ModuleReadinessSnapshot(
                    moduleName,
                    "R1",
                    "DLL found",
                    $"path={found}",
                    "DLL present. Enable processing when R2 ABI smoke is implemented.",
                    ProcessingEnabled: false);

            return new ModuleReadinessSnapshot(
                moduleName,
                "R0",
                "DLL absent",
                $"{dllName} not found in search path",
                "Build and place the DLL to enable this module.",
                ProcessingEnabled: false);
        }

        private static string? FindRepositoryRoot(string startPath)
        {
            var directory = new DirectoryInfo(startPath);
            while (directory is not null)
            {
                if (Directory.Exists(Path.Combine(directory.FullName, ".git")) ||
                    Directory.Exists(Path.Combine(directory.FullName, "modules")))
                {
                    return directory.FullName;
                }

                directory = directory.Parent;
            }

            return null;
        }
    }
}
