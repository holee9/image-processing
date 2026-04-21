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
            ["xpe_enhance_basic.dll"]    = ["xpe_common.dll", "fmt.dll", "spdlog.dll"],
            ["xpe_display.dll"]          = ["xpe_common.dll", "fmt.dll", "spdlog.dll"],
            ["xpe_enhance_advanced.dll"] = ["fmt.dll", "spdlog.dll"],
            ["gsvg.dll"]                 = ["xpe_common.dll", "fmt.dll", "spdlog.dll"],
            ["xpe_gsvg.dll"]             = ["xpe_common.dll", "fmt.dll", "spdlog.dll"],
            ["xpe_ai.dll"]               = ["fmt.dll", "spdlog.dll"],
            ["xpe_dicom.dll"]            = ["xpe_common.dll", "fmt.dll", "spdlog.dll",
                                            "dcmnet.dll", "dcmdata.dll",
                                            "ofstd.dll", "oflog.dll",
                                            "openjp2.dll"],
        };

        private static readonly string[] BaselineDependencies = ["fmt.dll", "spdlog.dll"];

        public static void TryLoadFor(string nativeDllPath)
        {
            var dllName = Path.GetFileName(nativeDllPath);
            var deps = ModuleDependencies.TryGetValue(dllName, out var d) ? d : BaselineDependencies;

            foreach (var directory in GetDependencyDirectories(nativeDllPath))
            {
                foreach (var dependency in deps)
                {
                    TryLoad(Path.Combine(directory, dependency));
                }
            }
        }

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
