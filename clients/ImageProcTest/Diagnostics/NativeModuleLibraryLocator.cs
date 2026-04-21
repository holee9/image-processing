using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace ImageProcTest
{
    internal static class NativeModuleLibraryLocator
    {
        public static string? TryFindDll(string dllName, params string[] siblingRoots) =>
            GetDllCandidates(dllName, siblingRoots).FirstOrDefault(File.Exists);

        public static IEnumerable<string> GetDllCandidates(string dllName, params string[] siblingRoots)
        {
            yield return Path.Combine(AppContext.BaseDirectory, dllName);

            var envDir = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
            if (!string.IsNullOrWhiteSpace(envDir))
            {
                yield return Path.Combine(envDir, dllName);
            }

            var repoRoot = FindRepositoryRoot(AppContext.BaseDirectory);
            if (repoRoot is null)
            {
                yield break;
            }

            foreach (var root in GetRepositoryAndSiblingRoots(repoRoot, siblingRoots))
            {
                foreach (var relativeDirectory in CandidateBuildDirectories)
                {
                    yield return Path.Combine(root, relativeDirectory, dllName);
                }
            }
        }

        private static readonly string[] CandidateBuildDirectories =
        [
            Path.Combine("build", "local-vs2022-common", "bin"),
            Path.Combine("build", "local-vs2022-common", "bin", "RelWithDebInfo"),
            Path.Combine("build", "ci-common", "bin"),
            Path.Combine("build", "ci-common", "bin", "Debug"),
            Path.Combine("build", "ci", "bin"),
            Path.Combine("build", "ci", "bin", "RelWithDebInfo"),
            Path.Combine("build", "default", "bin"),
            Path.Combine("build", "default", "bin", "Debug"),
            Path.Combine("build", "release", "bin"),
            Path.Combine("build", "release", "bin", "Release"),
            Path.Combine("build", "readiness-display-vs", "bin", "Debug"),
            Path.Combine("build", "readiness-preprocess-vs", "bin", "Debug"),
            Path.Combine("build", "enh01_release", "bin"),
            Path.Combine("build", "enh01_release", "bin", "Release")
        ];

        private static IEnumerable<string> GetRepositoryAndSiblingRoots(
            string repoRoot,
            IReadOnlyList<string> siblingRoots)
        {
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            if (seen.Add(repoRoot))
            {
                yield return repoRoot;
            }

            var parent = Directory.GetParent(repoRoot);
            if (parent is null)
            {
                yield break;
            }

            foreach (var siblingName in siblingRoots)
            {
                var siblingRoot = Path.Combine(parent.FullName, siblingName);
                if (Directory.Exists(siblingRoot) && seen.Add(siblingRoot))
                {
                    yield return siblingRoot;
                }
            }
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
