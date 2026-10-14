using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;

namespace ImageProcTest
{
    internal static class NativeDependencyLoader
    {
        private static readonly object Sync = new();
        private static readonly HashSet<string> LoadedPaths = new(StringComparer.OrdinalIgnoreCase);
        private static readonly List<IntPtr> LoadedHandles = [];

        // Per-module dependency map.
        // Modules not listed fall back to the common baseline (fmt + spdlog).
        private static readonly Dictionary<string, string[]> ModuleDependencies = new(StringComparer.OrdinalIgnoreCase)
        {
            ["xpe_common.dll"]           = ["fmt.dll", "spdlog.dll"],
            ["xpe_enhance_basic.dll"]    = ["fmt.dll", "spdlog.dll", "xpe_common.dll"],
            ["xpe_display.dll"]          = ["fmt.dll", "spdlog.dll", "xpe_common.dll"],
            ["xpe_enhance_advanced.dll"] = ["fmt.dll", "spdlog.dll"],
            ["gsvg.dll"]                 = ["fmt.dll", "spdlog.dll", "xpe_common.dll"],
            ["xpe_gsvg.dll"]             = ["fmt.dll", "spdlog.dll", "xpe_common.dll"],
            ["xpe_ai.dll"]               = ["fmt.dll", "spdlog.dll"],
            ["xpe_dicom.dll"]            = ["fmt.dll", "spdlog.dll", "xpe_common.dll",
                                            "dcmnet.dll", "dcmdata.dll",
                                            "ofstd.dll", "oflog.dll",
                                            "openjp2.dll"],
        };

        private static readonly string[] BaselineDependencies = ["fmt.dll", "spdlog.dll"];

        /// <summary>The dependency DLLs this loader names for a module (the common baseline for a module it does not list).</summary>
        internal static IReadOnlyList<string> DependenciesOf(string dllName) =>
            ModuleDependencies.TryGetValue(dllName, out var d) ? d : BaselineDependencies;

        public static void TryLoadFor(string nativeDllPath)
        {
            var dllName = Path.GetFileName(nativeDllPath);
            var deps = DependenciesOf(dllName);

            foreach (var directory in GetDependencyDirectories(nativeDllPath))
            {
                foreach (var dependency in deps)
                {
                    TryLoad(Path.Combine(directory, dependency));
                }
            }
        }

        /// <summary>The folders this loader looks for a module's dependencies in, in order (GUI-C-219c: the oracle snapshot copies from exactly these places).</summary>
        internal static IEnumerable<string> DependencyDirectoriesOf(string nativeDllPath) => GetDependencyDirectories(nativeDllPath);

        private static IEnumerable<string> GetDependencyDirectories(string nativeDllPath)
        {
            var nativeDirectory = Path.GetDirectoryName(nativeDllPath);
            if (string.IsNullOrWhiteSpace(nativeDirectory))
            {
                yield break;
            }

            yield return nativeDirectory;

            // GUI-C-238b (Codex #193 B-3): a pinned search (XPE_NATIVE_DIR_EXCLUSIVE=1) takes the dependencies from the pinned folder and nowhere else. The vcpkg_installed folder found above it loaded a SECOND
            // copy of fmt.dll next to the pinned one (measured by the module audit in the app log, 2026-10-14).
            if (NativeSearchPolicy.StopAtInjectedDirectory)
            {
                yield break;
            }

            var directory = new DirectoryInfo(nativeDirectory);
            while (directory is not null)
            {
                var vcpkgBin = Path.Combine(directory.FullName, "vcpkg_installed", "x64-windows", "bin");
                if (Directory.Exists(vcpkgBin))
                {
                    yield return vcpkgBin;
                    yield break;
                }

                directory = directory.Parent;
            }
        }

        private static void TryLoad(string dependencyPath)
        {
            if (!File.Exists(dependencyPath))
            {
                return;
            }

            lock (Sync)
            {
                if (!LoadedPaths.Add(dependencyPath))
                {
                    return;
                }

                // GUI-C-243 (Codex #195): with the search pinned, a dependency is loaded with the SAME restricted search as the module that imports it (its own folder + System32), never through the default
                // search: a copy of fmt.dll / spdlog.dll / dcm*.dll in the application folder must not stand in for one that is missing from the pinned folder.
                IntPtr handle;
                var loaded = NativeSearchPolicy.StopAtInjectedDirectory
                    ? OracleModuleConfinement.TryLoad(dependencyPath, out handle, out _)
                    : NativeLibrary.TryLoad(dependencyPath, out handle);
                if (loaded)
                {
                    LoadedHandles.Add(handle);
                }
                else
                {
                    LoadedPaths.Remove(dependencyPath);
                }
            }
        }
    }
}
