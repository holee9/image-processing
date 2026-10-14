using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace ImageProcTest
{
    internal static class XpeEnhanceBasicLibraryLocator
    {
        // #128: spelled out rather than taken from XpeEnhanceBasicWrapper.DllName. Referencing
        // that const drags the wrapper — and through its delegate signatures, XpeCommonApi — into
        // anything that compiles this file, which blocks the test project from linking it
        // (a second DllImport resolver on one assembly throws). The wrapper keeps its own const;
        // both must name the same DLL.
        public const string DllName = "xpe_enhance_basic.dll";

        public static string? TryFindDll()
        {
            return GetDllCandidates().FirstOrDefault(File.Exists);
        }

        public static IEnumerable<string> GetDllCandidates()
        {
            // GUI-C-238 (#128): a search pinned to one directory looks THERE and nowhere else - the application directory included. It used to look at the application
            // directory first and stop only after the pinned one, so an old copy of the DLL beside the exe won over the folder the caller named.
            if (NativeSearchPolicy.ExclusiveDirectory is { } onlyDirectory)
            {
                yield return Path.Combine(onlyDirectory, DllName);
                yield break;
            }

            yield return Path.Combine(AppContext.BaseDirectory, DllName);

            var envDir = NativeSearchPolicy.InjectedDirectory;
            if (envDir is not null)
            {
                yield return Path.Combine(envDir, DllName);
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

            foreach (var root in GetRepositoryAndSiblingRoots(repoRoot, "image-processing", "xpe-post"))
            {
                var candidates = new[]
                {
                    Path.Combine(root, "build", "local-vs2022-common", "bin", DllName),
                    Path.Combine(root, "build", "local-vs2022-common", "bin", "RelWithDebInfo", DllName),
                    Path.Combine(root, "build", "enh01_release", "bin", DllName),
                    Path.Combine(root, "build", "enh01_release", "bin", "Release", DllName),
                    Path.Combine(root, "build", "ci-enhance-basic", "bin", DllName),
                    Path.Combine(root, "build", "ci-enhance-basic", "bin", "Release", DllName),
                    Path.Combine(root, "build", "ci", "bin", DllName),
                    Path.Combine(root, "build", "ci", "bin", "RelWithDebInfo", DllName),
                    Path.Combine(root, "build", "release", "bin", DllName),
                    Path.Combine(root, "build", "release", "bin", "Release", DllName),
                    Path.Combine(root, "build", "default", "bin", DllName),
                    Path.Combine(root, "build", "default", "bin", "Debug", DllName),
                    Path.Combine(root, "build", "enhance_test", "bin", DllName),
                    Path.Combine(root, "build", "enhance_test", "bin", "Release", DllName),
                    Path.Combine(root, "build", "enhance_test", "bin", "Debug", DllName)
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
                    Directory.Exists(Path.Combine(directory.FullName, "modules", "enhance_basic")))
                {
                    return directory.FullName;
                }

                directory = directory.Parent;
            }

            return null;
        }
    }
}
