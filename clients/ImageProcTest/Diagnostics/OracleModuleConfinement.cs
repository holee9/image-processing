using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;

namespace ImageProcTest
{
    /// <summary>
    /// GUI-C-219d (Codex #116, high): the oracle worker loads the preprocess DLL from the SNAPSHOT folder and must find the DLLs that one imports in that folder and nowhere else. A full path for the
    /// main DLL does not do that by itself: Windows resolves the DLLs it imports BY NAME, through the search order, and the first place in that order is the folder of the process's own executable.
    /// A <c>xpe_common.dll</c> beside the application's executable would be loaded in place of the copy that was hashed.
    ///
    /// <see cref="TryLoad"/> loads with <c>LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32</c>: the folder of the DLL being loaded, then System32, and neither the executable's folder, the
    /// current directory nor PATH. This is the per-call form of the restriction (no process-wide <c>SetDefaultDllDirectories</c>, which cannot be undone and would also change what the rest of the
    /// worker can load). <see cref="Offenders"/> is the check that it worked: after the oracle has run, every module in the process that shares a name with a file of the snapshot must be that file.
    ///
    /// Scope (decided with GUI-C-219d): an accidental change of the DLLs while the app is running is in scope and is caught by the identity of the files; a deliberate race between the check and
    /// the load, by someone who can already write the application folder, is not: this application is not a security boundary against a user with that access.
    /// </summary>
    internal static class OracleModuleConfinement
    {
        private const uint LoadLibrarySearchDllLoadDir = 0x00000100;
        private const uint LoadLibrarySearchSystem32 = 0x00000800;

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true, EntryPoint = "LoadLibraryExW")]
        private static extern IntPtr LoadLibraryEx(string fileName, IntPtr reserved, uint flags);

        /// <summary>Loads <paramref name="dllPath"/> (an absolute path) with the search restricted to its own folder and System32.</summary>
        public static bool TryLoad(string dllPath, out IntPtr handle, out string? error)
        {
            handle = LoadLibraryEx(dllPath, IntPtr.Zero, LoadLibrarySearchDllLoadDir | LoadLibrarySearchSystem32);
            if (handle == IntPtr.Zero)
            {
                error = new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error()).Message;
                return false;
            }

            error = null;
            return true;
        }

        /// <summary>The full paths of the modules loaded in this process now.</summary>
        public static IReadOnlyList<string> LoadedModulePaths()
        {
            using var process = Process.GetCurrentProcess();
            var paths = new List<string>();
            foreach (ProcessModule module in process.Modules)
            {
                paths.Add(module.FileName);
            }

            return paths;
        }

        /// <summary>
        /// The loaded modules that share a name with a file of the snapshot and are not that file. <paramref name="snapshotFolder"/> is where the snapshot is; <paramref name="snapshotNames"/> are the
        /// file names in it. Pure on its inputs, so a test can say what a decoy looks like.
        /// </summary>
        public static IReadOnlyList<string> Offenders(IEnumerable<string> loadedPaths, string snapshotFolder, IReadOnlyCollection<string> snapshotNames)
        {
            var folder = Path.GetFullPath(snapshotFolder).TrimEnd('\\', '/') + Path.DirectorySeparatorChar;
            var offenders = new List<string>();
            foreach (var path in loadedPaths)
            {
                var name = Path.GetFileName(path);
                if (!snapshotNames.Contains(name, StringComparer.OrdinalIgnoreCase))
                {
                    continue;
                }

                if (!Path.GetFullPath(path).StartsWith(folder, StringComparison.OrdinalIgnoreCase))
                {
                    offenders.Add(path);
                }
            }

            return offenders;
        }

        /// <summary>The audit of the running process against its snapshot folder (the names are the files that are in it).</summary>
        public static IReadOnlyList<string> AuditProcess(string snapshotFolder) =>
            Offenders(LoadedModulePaths(), snapshotFolder, Directory.GetFiles(snapshotFolder).Select(Path.GetFileName).Select(n => n!).ToList());
    }
}
