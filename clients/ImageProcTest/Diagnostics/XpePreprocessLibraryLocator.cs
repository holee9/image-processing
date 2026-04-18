using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace ImageProcTest
{
    internal static class XpePreprocessLibraryLocator
    {
        public const string DllName = "xpe_preprocess.dll";

        public static string? TryFindDll()
        {
            return GetDllCandidates().FirstOrDefault(File.Exists);
        }

        public static IEnumerable<string> GetDllCandidates()
        {
            yield return Path.Combine(AppContext.BaseDirectory, DllName);

            var envDir = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
            if (!string.IsNullOrWhiteSpace(envDir))
            {
                yield return Path.Combine(envDir, DllName);
            }

            var repoRoot = FindRepositoryRoot(AppContext.BaseDirectory);
            if (repoRoot is null)
            {
                yield break;
            }

            var candidates = new[]
            {
                Path.Combine(repoRoot, "build", "readiness-preprocess-only-vs2", "bin", "Debug", DllName),
                Path.Combine(repoRoot, "build", "readiness-preprocess-vs", "bin", "Debug", DllName),
                Path.Combine(repoRoot, "build", "ci-common", "bin", "Debug", DllName),
                Path.Combine(repoRoot, "build", "ci-common", "bin", DllName),
                Path.Combine(repoRoot, "build", "default", "bin", "Debug", DllName),
                Path.Combine(repoRoot, "build", "default", "bin", DllName)
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
