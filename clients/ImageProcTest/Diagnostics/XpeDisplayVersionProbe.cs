using System;
using System.IO;
using System.Runtime.InteropServices;

namespace ImageProcTest
{
    internal static class XpeDisplayVersionProbe
    {
        private const string DllName = "xpe_display.dll";

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr VersionDelegate();

        public static DisplayHealthResult Check()
        {
            foreach (var candidate in GetDllCandidates())
            {
                if (!File.Exists(candidate))
                {
                    continue;
                }

                if (!NativeLibrary.TryLoad(candidate, out var handle))
                {
                    continue;
                }

                try
                {
                    if (!NativeLibrary.TryGetExport(handle, "xpe_display_version", out var symbol))
                    {
                        return new DisplayHealthResult(
                            Status: "Entry point mismatch",
                            Version: "Unavailable",
                            DllPath: candidate,
                            Details: "xpe_display.dll exists but xpe_display_version export was not found.",
                            IsReady: false);
                    }

                    var version = Marshal.GetDelegateForFunctionPointer<VersionDelegate>(symbol)();
                    return new DisplayHealthResult(
                        Status: "Version-only health ready",
                        Version: Marshal.PtrToStringAnsi(version) ?? "<empty>",
                        DllPath: candidate,
                        Details: "Display module is only verified for version/status. Native display processing remains gated.",
                        IsReady: true);
                }
                finally
                {
                    NativeLibrary.Free(handle);
                }
            }

            return new DisplayHealthResult(
                Status: "DLL not found",
                Version: "Unavailable",
                DllPath: DllName,
                Details: "xpe_display.dll is not available in known GUI/build output locations.",
                IsReady: false);
        }

        private static IEnumerable<string> GetDllCandidates()
        {
            yield return Path.Combine(AppContext.BaseDirectory, DllName);

            var repoRoot = FindRepositoryRoot(AppContext.BaseDirectory);
            if (repoRoot is null)
            {
                yield break;
            }

            var candidates = new[]
            {
                Path.Combine(repoRoot, "build", "readiness-display-vs", "bin", "Debug", DllName),
                Path.Combine(repoRoot, "build", "ci-common", "bin", "Debug", DllName),
                Path.Combine(repoRoot, "build", "default", "bin", "Debug", DllName)
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
                    Directory.Exists(Path.Combine(directory.FullName, "modules", "display")))
                {
                    return directory.FullName;
                }

                directory = directory.Parent;
            }

            return null;
        }
    }
}
