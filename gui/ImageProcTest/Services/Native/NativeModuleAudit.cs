using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;

namespace ImageProcTest.Services.Native;

/// <summary>
/// GUI-C-238b (Codex #193 B-3): after the native modules are loaded, say WHERE each one came from. Pinning the search (<c>XPE_NATIVE_DIR_EXCLUSIVE=1</c>) restricts where the app's own loader looks, but the
/// DLLs those modules import (fmt, spdlog, DCMTK, OpenJPEG ...) are found by the Windows loader by NAME, so the only way to know they came from the pinned folder is to ask the process which files it
/// loaded. This reads the loaded-module list and classifies the native ones. It changes nothing; the result is written to the log and, for a module outside the folder, to an alert.
/// </summary>
internal static class NativeModuleAudit
{
    /// <summary>Names that are native modules of this product or DLLs they import (the ones <see cref="NativeDependencyLoader"/> names, plus every <c>xpe_*.dll</c> and <c>dcm*.dll</c>).</summary>
    private static readonly HashSet<string> Imported = new(StringComparer.OrdinalIgnoreCase)
    {
        "gsvg.dll", "fmt.dll", "spdlog.dll", "ofstd.dll", "oflog.dll", "openjp2.dll",
    };

    internal static bool IsNativeModuleOfTheProduct(string fileName) =>
        fileName.StartsWith("xpe_", StringComparison.OrdinalIgnoreCase) && fileName.EndsWith(".dll", StringComparison.OrdinalIgnoreCase)
        || fileName.StartsWith("dcm", StringComparison.OrdinalIgnoreCase) && fileName.EndsWith(".dll", StringComparison.OrdinalIgnoreCase)
        || Imported.Contains(fileName);

    /// <summary>One native module: its file name, the full path it was loaded from, and whether that is inside the pinned folder.</summary>
    internal readonly record struct Module(string Name, string Path, bool InsidePinnedFolder);

    /// <summary>Pure on its inputs, so a test can say what an outsider looks like.</summary>
    internal static IReadOnlyList<Module> Classify(IEnumerable<string> loadedModulePaths, string pinnedFolder)
    {
        var folder = System.IO.Path.GetFullPath(pinnedFolder).TrimEnd('\\', '/');
        var modules = new List<Module>();
        foreach (var path in loadedModulePaths)
        {
            var name = System.IO.Path.GetFileName(path);
            if (!IsNativeModuleOfTheProduct(name))
            {
                continue;
            }

            var directory = System.IO.Path.GetDirectoryName(System.IO.Path.GetFullPath(path))?.TrimEnd('\\', '/') ?? string.Empty;
            modules.Add(new Module(name, path, string.Equals(directory, folder, StringComparison.OrdinalIgnoreCase)));
        }

        return modules;
    }

    /// <summary>The loaded native modules that are NOT from <paramref name="pinnedFolder"/>, one text each (name and path). Pure on its inputs.</summary>
    internal static IReadOnlyList<string> Outsiders(IEnumerable<string> loadedModulePaths, string pinnedFolder) =>
        Classify(loadedModulePaths, pinnedFolder).Where(m => !m.InsidePinnedFolder).Select(m => $"{m.Name} from '{m.Path}'").ToList();

    /// <summary>
    /// GUI-C-243 (Codex #195): a feature that needs ALL its modules from the pinned folder must not succeed with only a warning. When the search is pinned and any native module of the product is loaded from
    /// elsewhere, this throws, naming them; the caller is the feature's native entry (the DICOM export, the AI start), so the feature fails with that reason instead of running on a stray module.
    /// Not pinned: nothing to enforce.
    /// </summary>
    public static void ThrowIfOutsidePinnedFolder(string feature)
    {
        var pinned = NativeSearchPolicy.ExclusiveDirectory;
        if (pinned is null)
        {
            return;
        }

        var outsiders = Outsiders(OracleModuleConfinement.LoadedModulePaths(), pinned);
        if (outsiders.Count > 0)
        {
            throw new InvalidOperationException($"{feature}: native module(s) loaded from outside the pinned folder '{pinned}': {string.Join("; ", outsiders)}. The feature was not run on them (XPE_NATIVE_DIR_EXCLUSIVE=1).");
        }
    }

    /// <summary>Where the AI module looks for its worker: beside <c>xpe_ai.dll</c> (ai_api.h; the module does the search, this only says where). Null when the DLL has not been loaded through the resolver.</summary>
    internal static string? AiWorkerPathBeside(string? aiDllPath) =>
        string.IsNullOrEmpty(aiDllPath) ? null : System.IO.Path.Combine(System.IO.Path.GetDirectoryName(aiDllPath) ?? string.Empty, "xpe_ai_worker.exe");

    [DllImport("ntdll.dll")]
    private static extern int NtQueryInformationProcess(IntPtr process, int informationClass, ref ProcessBasicInformation information, int length, out int returned);

    [StructLayout(LayoutKind.Sequential)]
    private struct ProcessBasicInformation
    {
        public IntPtr Reserved1;
        public IntPtr PebBaseAddress;
        public IntPtr Reserved2a;
        public IntPtr Reserved2b;
        public IntPtr UniqueProcessId;
        public IntPtr InheritedFromUniqueProcessId;
    }

    /// <summary>The executable paths of the <c>xpe_ai_worker</c> processes that THIS process started (a worker of another session is not ours and is not listed).</summary>
    internal static IReadOnlyList<string> OwnAiWorkerPaths()
    {
        var paths = new List<string>();
        foreach (var process in Process.GetProcessesByName("xpe_ai_worker"))
        {
            using (process)
            {
                try
                {
                    var info = default(ProcessBasicInformation);
                    if (NtQueryInformationProcess(process.Handle, 0, ref info, Marshal.SizeOf<ProcessBasicInformation>(), out _) == 0
                        && info.InheritedFromUniqueProcessId.ToInt64() == Environment.ProcessId
                        && process.MainModule?.FileName is { } path)
                    {
                        paths.Add(path);
                    }
                }
                catch (Exception)
                {
                    // a process that ended, or one that cannot be asked, is not one this check can say anything about
                }
            }
        }

        return paths;
    }

    /// <summary>
    /// The sentence the AI start logs about the worker: where it is expected (beside the loaded <c>xpe_ai.dll</c>), whether that file exists, and where the workers this process already started run from.
    /// When the search is pinned, a worker of ours running from outside the pinned folder makes this throw (see <see cref="ThrowIfOutsidePinnedFolder"/>: the same rule, for the worker process).
    /// </summary>
    public static string DescribeAndCheckAiWorker(string? aiDllPath)
    {
        var expected = AiWorkerPathBeside(aiDllPath);
        var running = OwnAiWorkerPaths();
        var pinned = NativeSearchPolicy.ExclusiveDirectory;
        if (pinned is not null)
        {
            var folder = System.IO.Path.GetFullPath(pinned).TrimEnd('\\', '/');
            var outside = running.Where(p => !string.Equals(System.IO.Path.GetDirectoryName(System.IO.Path.GetFullPath(p))?.TrimEnd('\\', '/'), folder, StringComparison.OrdinalIgnoreCase)).ToList();
            if (outside.Count > 0)
            {
                throw new InvalidOperationException($"AI worker: a worker started by this application runs from outside the pinned folder '{pinned}': {string.Join("; ", outside)}. XPE_NATIVE_DIR_EXCLUSIVE=1.");
            }
        }

        return $"worker expected at '{expected ?? "(xpe_ai.dll not loaded through the resolver)"}' exists={(expected is not null && File.Exists(expected))}; running workers of this process: {(running.Count == 0 ? "none" : string.Join("; ", running))}";
    }

    /// <summary>
    /// The log lines for the native modules loaded in this process now. When the search is pinned, one summary line and one line per module that is NOT from the pinned folder (the outsiders, with their path);
    /// when it is not pinned, one line saying so and listing where each module came from.
    /// </summary>
    public static IReadOnlyList<string> Report(string stage) => Report(stage, NativeSearchPolicy.ExclusiveDirectory, OracleModuleConfinement.LoadedModulePaths());

    internal static IReadOnlyList<string> Report(string stage, string? pinnedFolder, IEnumerable<string> loadedModulePaths)
    {
        var lines = new List<string>();
        if (pinnedFolder is null)
        {
            var all = loadedModulePaths.Where(p => IsNativeModuleOfTheProduct(System.IO.Path.GetFileName(p))).ToList();
            lines.Add($"Native module audit ({stage}): the search is not pinned (XPE_NATIVE_DIR_EXCLUSIVE is not 1); {all.Count} native module(s) loaded: {string.Join("; ", all)}");
            return lines;
        }

        var modules = Classify(loadedModulePaths, pinnedFolder);
        var outside = modules.Where(m => !m.InsidePinnedFolder).ToList();
        lines.Add($"Native module audit ({stage}): pinned folder '{pinnedFolder}'; {modules.Count} native module(s) loaded, {modules.Count - outside.Count} from the pinned folder, {outside.Count} from elsewhere.");
        foreach (var module in outside)
        {
            lines.Add($"Native module audit ({stage}): {module.Name} was loaded from '{module.Path}', which is NOT the pinned folder '{pinnedFolder}'.");
        }

        return lines;
    }
}
