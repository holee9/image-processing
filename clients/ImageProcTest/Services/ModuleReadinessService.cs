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
            var enhanceBasic = XpeEnhanceBasicReadinessProbe.Check();

            return
            [
                EvaluateCommon(commonHealth),
                EvaluateDisplay(display),
                EvaluatePreprocess(root),
                EvaluateEnhanceBasic(enhanceBasic),
                EvaluateDllPresence("dicom", "xpe_dicom.dll"),
                EvaluateDllPresence("enhance_advanced", "xpe_enhance_advanced.dll"),
                EvaluateDllPresence("gsvg", "gsvg.dll"),
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
            if (display.IsReady)
            {
                return new ModuleReadinessSnapshot(
                    "xpe_display",
                    "R1",
                    "Version-only health ready",
                    $"version={display.Version}; dll={display.DllPath}",
                    "Wait for real LUT/display pipeline exports before enabling image processing.",
                    ProcessingEnabled: false);
            }

            return new ModuleReadinessSnapshot(
                "xpe_display",
                "R0",
                display.Status,
                display.Details,
                "Build xpe_display.dll with required display pipeline exports.",
                ProcessingEnabled: false);
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

        /// <summary>
        /// Checks DLL presence in the application directory and well-known build output paths.
        /// Supersedes EvaluateSourceOnly for modules that have no dedicated probe yet.
        /// </summary>
        private static ModuleReadinessSnapshot EvaluateDllPresence(string moduleName, string dllName)
        {
            var found = GetDllSearchDirectories()
                .Select(d => Path.Combine(d, dllName))
                .FirstOrDefault(File.Exists);

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

        private static IEnumerable<string> GetDllSearchDirectories()
        {
            yield return AppContext.BaseDirectory;

            var dir = new DirectoryInfo(AppContext.BaseDirectory);
            while (dir != null)
            {
                foreach (var candidate in new[]
                {
                    "bin",
                    Path.Combine("build", "release", "bin"),
                    Path.Combine("build", "enh01_release", "bin"),
                    Path.Combine("build", "default", "bin", "Debug"),
                    Path.Combine("build", "readiness-display-vs", "bin", "Debug"),
                    Path.Combine("build", "ci-common", "bin", "Debug"),
                })
                {
                    var p = Path.Combine(dir.FullName, candidate);
                    if (Directory.Exists(p)) yield return p;
                }
                dir = dir.Parent;
            }
        }

        private static ModuleReadinessSnapshot EvaluateSourceOnly(string? root, string moduleName, string dllName)
        {
            var moduleRoot = root is null ? null : Path.Combine(root, "modules", moduleName);
            var sourceCount = moduleRoot is null || !Directory.Exists(moduleRoot)
                ? 0
                : Directory.EnumerateFiles(moduleRoot, "*.*", SearchOption.AllDirectories)
                    .Count(path =>
                        path.EndsWith(".cpp", StringComparison.OrdinalIgnoreCase) ||
                        path.EndsWith(".h", StringComparison.OrdinalIgnoreCase) ||
                        path.EndsWith(".hpp", StringComparison.OrdinalIgnoreCase));

            var binary = FindFirstExisting(root, dllName,
                Path.Combine("build", "default", "bin", "Debug"),
                Path.Combine("build", "readiness-display-vs", "bin", "Debug"),
                Path.Combine("build", "ci-common", "bin", "Debug"));

            if (binary is not null)
            {
                return new ModuleReadinessSnapshot(
                    moduleName,
                    "R1",
                    "Binary discoverable, no GUI adapter",
                    $"dll={binary}",
                    "Add module-specific export and ABI smoke checks before processing controls.",
                    ProcessingEnabled: false);
            }

            return new ModuleReadinessSnapshot(
                moduleName,
                "R0",
                sourceCount > 0 ? "Source scaffold only" : "No implementation evidence",
                $"sourceFiles={sourceCount}",
                "Wait for stable exports and module smoke tests.",
                ProcessingEnabled: false);
        }

        private static string? FindFirstExisting(string? root, string dllName, params string[] relativeDirectories)
        {
            if (root is null)
            {
                return null;
            }

            return relativeDirectories
                .Select(relative => Path.Combine(root, relative, dllName))
                .FirstOrDefault(File.Exists);
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
