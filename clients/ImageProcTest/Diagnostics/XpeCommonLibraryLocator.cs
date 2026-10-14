using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace ImageProcTest
{
    /// <summary>
    /// Where <c>xpe_common.dll</c> may be looked for (#129).
    ///
    /// This list used to live inside <c>XpeCommonApi</c>, next to the DllImport resolver.
    /// That put it out of reach of the search-policy regression: a test project cannot compile
    /// <c>PInvokeWrapper.cs</c>, because <c>XpeCommonApi</c>'s static constructor registers a
    /// DllImport resolver and one assembly may only have one (measured in GUI-C-13). So xpe_common —
    /// the module every other one depends on — was the single search whose "no sibling checkout by
    /// default" property nothing verified.
    ///
    /// Split out here it is plain path arithmetic: no P/Invoke, no resolver, no static constructor.
    /// The resolver now calls <see cref="GetDllCandidates"/> and does nothing else about paths.
    /// </summary>
    internal static class XpeCommonLibraryLocator
    {
        /// <summary>
        /// Spelled out rather than taken from <c>XpeCommonApi.DllName</c>: referencing that const
        /// would pull the resolver-owning type back in and undo the split. The two literals must
        /// name the same DLL — <c>DllNameParityTests</c> compares them.
        /// </summary>
        public const string DllName = "xpe_common.dll";

        public static string? TryFindDll() => GetDllCandidates().FirstOrDefault(File.Exists);

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

            foreach (var root in GetRepositoryAndSiblingRoots(repoRoot, "xpe-pre"))
            {
                var candidates = new[]
                {
                    Path.Combine(root, "build", "local-vs2022-common", "bin", DllName),
                    Path.Combine(root, "build", "local-vs2022-common", "bin", "RelWithDebInfo", DllName),
                    Path.Combine(root, "build", "gui-preprocess-link", "bin", DllName),
                    Path.Combine(root, "build", "gui-preprocess-link", "bin", "Debug", DllName),
                    Path.Combine(root, "build", "ci-common", "bin", DllName),
                    Path.Combine(root, "build", "ci-common", "bin", "Debug", DllName),
                    Path.Combine(root, "build", "ci", "bin", DllName),
                    Path.Combine(root, "build", "ci", "bin", "RelWithDebInfo", DllName),
                    Path.Combine(root, "build", "default", "bin", DllName),
                    Path.Combine(root, "build", "default", "bin", "Debug", DllName),
                    Path.Combine(root, "build", "release", "bin", DllName),
                    Path.Combine(root, "build", "release", "bin", "Release", DllName),
                    Path.Combine(root, "build", "readiness-display-vs", "bin", "Debug", DllName),
                    Path.Combine(root, "build", "readiness-preprocess-vs", "bin", "Debug", DllName),
                    Path.Combine(root, "gui", "ImageProcTest", "bin", "Debug", "net8.0-windows", DllName),
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
