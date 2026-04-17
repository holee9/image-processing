using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;

namespace ImageProcTest
{
    internal static class XpePreprocessReadinessProbe
    {
        private const string DllName = "xpe_preprocess.dll";

        private static readonly string[] RequiredExports =
        [
            "xpe_preprocess_version",
            "xpe_preprocess_init",
            "xpe_preprocess_shutdown",
            "xpe_calib_load_offset",
            "xpe_calib_load_gain",
            "xpe_calib_load_defect_map",
            "xpe_offset_correct",
            "xpe_gain_correct",
            "xpe_defect_correct"
        ];

        private static readonly string[] ExecutionExports =
        [
            "xpe_preprocess_apply_pipeline"
        ];

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr VersionDelegate();

        public static PreprocessHealthResult Check()
        {
            foreach (var candidate in GetDllCandidates())
            {
                if (!File.Exists(candidate))
                {
                    continue;
                }

                if (!NativeLibrary.TryLoad(candidate, out var handle))
                {
                    continue;
                }

                try
                {
                    var present = RequiredExports
                        .Where(name => NativeLibrary.TryGetExport(handle, name, out _))
                        .ToArray();
                    var missing = RequiredExports.Except(present).ToArray();
                    var missingExecution = ExecutionExports
                        .Where(name => !NativeLibrary.TryGetExport(handle, name, out _))
                        .ToArray();

                    var version = "Unavailable";
                    if (NativeLibrary.TryGetExport(handle, "xpe_preprocess_version", out var versionSymbol))
                    {
                        var versionPtr = Marshal.GetDelegateForFunctionPointer<VersionDelegate>(versionSymbol)();
                        version = Marshal.PtrToStringAnsi(versionPtr) ?? "<empty>";
                    }

                    if (missing.Length > 0)
                    {
                        return new PreprocessHealthResult(
                            Status: "Export checklist incomplete",
                            Version: version,
                            DllPath: candidate,
                            Details: "xpe_preprocess.dll exists but mandatory readiness exports are missing.",
                            PresentExports: present,
                            MissingExports: missing,
                            MissingExecutionExports: missingExecution,
                            IsVersionReady: version != "Unavailable",
                            IsExportReady: false);
                    }

                    return new PreprocessHealthResult(
                        Status: "Export checklist ready",
                        Version: version,
                        DllPath: candidate,
                        Details: "Preprocess exports are discoverable. Native image execution remains gated until ABI smoke, synthetic oracle, and fixture E2E pass.",
                        PresentExports: present,
                        MissingExports: missing,
                        MissingExecutionExports: missingExecution,
                        IsVersionReady: true,
                        IsExportReady: true);
                }
                finally
                {
                    NativeLibrary.Free(handle);
                }
            }

            return new PreprocessHealthResult(
                Status: "DLL not found",
                Version: "Unavailable",
                DllPath: DllName,
                Details: "xpe_preprocess.dll is not available in known GUI/build output locations.",
                PresentExports: [],
                MissingExports: RequiredExports,
                MissingExecutionExports: ExecutionExports,
                IsVersionReady: false,
                IsExportReady: false);
        }

        private static IEnumerable<string> GetDllCandidates()
        {
            yield return Path.Combine(AppContext.BaseDirectory, DllName);

            var repoRoot = FindRepositoryRoot(AppContext.BaseDirectory);
            if (repoRoot is null)
            {
                yield break;
            }

            var candidates = new[]
            {
                Path.Combine(repoRoot, "build", "readiness-preprocess-only-vs2", "bin", "Debug", DllName),
                Path.Combine(repoRoot, "build", "readiness-preprocess-vs", "bin", "Debug", DllName),
                Path.Combine(repoRoot, "build", "default", "bin", "Debug", DllName)
            };

            foreach (var candidate in candidates)
            {
                yield return candidate;
            }
        }

        private static string? FindRepositoryRoot(string startPath)
        {
            var directory = new DirectoryInfo(startPath);
            while (directory is not null)
            {
                if (Directory.Exists(Path.Combine(directory.FullName, ".git")) ||
                    Directory.Exists(Path.Combine(directory.FullName, "modules", "preprocess")))
                {
                    return directory.FullName;
                }

                directory = directory.Parent;
            }

            return null;
        }
    }
}
