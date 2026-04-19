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

            foreach (var root in GetRepositoryAndSiblingRoots(repoRoot, "xpe-pre"))
            {
                var candidates = new[]
                {
                    Path.Combine(root, "build", "local-vs2022-common", "bin", DllName),
                    Path.Combine(root, "build", "local-vs2022-common", "bin", "RelWithDebInfo", DllName),
                    Path.Combine(root, "build", "gui-preprocess-link", "bin", "Debug", DllName),
                    Path.Combine(root, "build", "gui-preprocess-link", "bin", DllName),
                    Path.Combine(root, "build", "readiness-preprocess-only-vs2", "bin", "Debug", DllName),
                    Path.Combine(root, "build", "readiness-preprocess-vs", "bin", "Debug", DllName),
                    Path.Combine(root, "build", "preprocess", "bin", "Debug", DllName),
                    Path.Combine(root, "build", "preprocess", "bin", DllName),
                    Path.Combine(root, "build", "ci", "bin", "RelWithDebInfo", DllName),
                    Path.Combine(root, "build", "ci", "bin", DllName),
                    Path.Combine(root, "build", "ci-common", "bin", "Debug", DllName),
                    Path.Combine(root, "build", "ci-common", "bin", DllName),
                    Path.Combine(root, "build", "release", "bin", "Release", DllName),
                    Path.Combine(root, "build", "release", "bin", DllName),
                    Path.Combine(root, "build", "default", "bin", "Debug", DllName),
                    Path.Combine(root, "build", "default", "bin", DllName)
                };

                foreach (var candidate in candidates)
                {
                    yield return candidate;
                }
            }
        }

        private static IEnumerable<string> GetRepositoryAndSiblingRoots(
            string repoRoot,
            params string[] siblingNames)
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

            foreach (var siblingName in siblingNames)
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
