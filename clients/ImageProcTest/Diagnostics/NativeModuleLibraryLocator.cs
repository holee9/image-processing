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
            // GUI-C-238 (#128): a search pinned to one directory looks THERE and nowhere else - the application directory included. It used to look at the application
            // directory first and stop only after the pinned one, so an old copy of the DLL beside the exe won over the folder the caller named.
            if (NativeSearchPolicy.ExclusiveWithoutDirectory)
            {
                yield break;   // GUI-C-238b: pinned to a folder that is not named: no candidate at all
            }

            if (NativeSearchPolicy.ExclusiveDirectory is { } onlyDirectory)
            {
                yield return Path.Combine(onlyDirectory, dllName);
                yield break;
            }

            yield return Path.Combine(AppContext.BaseDirectory, dllName);

            var envDir = NativeSearchPolicy.InjectedDirectory;
            if (envDir is not null)
            {
                yield return Path.Combine(envDir, dllName);
            }

            // #129: build directories and sibling checkouts are opt-in. A DLL found there has no
            // recorded provenance, so the default search does not reach them.
            if (!NativeSearchPolicy.DeveloperSearchEnabled)
            {
                yield break;
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
            Path.Combine("build", "build_test", "bin", "Debug"),
            Path.Combine("build", "enhance_test", "bin", "Release"),
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
