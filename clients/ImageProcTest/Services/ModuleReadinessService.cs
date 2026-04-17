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

            return
            [
                EvaluateCommon(commonHealth),
                EvaluateDisplay(display),
                EvaluatePreprocess(root),
                EvaluateSourceOnly(root, "enhance_basic", "xpe_enhance_basic.dll"),
                EvaluateSourceOnly(root, "dicom", "xpe_dicom.dll"),
                EvaluateSourceOnly(root, "enhance_advanced", "xpe_enhance_advanced.dll"),
                EvaluateSourceOnly(root, "gsvg", "gsvg.dll"),
                EvaluateSourceOnly(root, "ai", "xpe_ai.dll")
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
                var executionGap = health.MissingExecutionExports.Count == 0
                    ? "pipeline export present"
                    : $"missing execution export(s): {string.Join(", ", health.MissingExecutionExports)}";

                return new ModuleReadinessSnapshot(
                    "xpe_preprocess",
                    "R2",
                    "Version and export checklist ready",
                    $"version={health.Version}; dll={health.DllPath}; {executionGap}",
                    "Next: run ABI smoke, synthetic oracle, and fixture E2E before enabling GUI execution.",
                    ProcessingEnabled: false);
            }

            if (health.IsVersionReady)
            {
                return new ModuleReadinessSnapshot(
                    "xpe_preprocess",
                    "R1",
                    "Version ready, export checklist incomplete",
                    $"version={health.Version}; missing={string.Join(", ", health.MissingExports)}",
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
