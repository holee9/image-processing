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

                if (NativeLibrary.TryLoad(dependencyPath, out var handle))
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
