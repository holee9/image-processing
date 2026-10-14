using System.IO;

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
