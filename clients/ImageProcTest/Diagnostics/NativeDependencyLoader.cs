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

        private static readonly string[] KnownDependencies =
        [
            "fmt.dll",
            "spdlog.dll",
        ];

        public static void TryLoadFor(string nativeDllPath)
        {
            foreach (var directory in GetDependencyDirectories(nativeDllPath))
            {
                foreach (var dependency in KnownDependencies)
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
