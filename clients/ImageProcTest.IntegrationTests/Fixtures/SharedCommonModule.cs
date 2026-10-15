// #225 / #202 (GUI-C-199): native tests that load a module by full path must not add a SECOND copy of xpe_common.dll to the process.
using System.Diagnostics;
using System.Runtime.InteropServices;
using ImageProcTest;   // NativeModuleLibraryLocator (linked)

namespace ImageProcTest.IntegrationTests.Fixtures;

/// <summary>
/// The alert queue lives in xpe_common.dll. Windows treats the same file name at two paths as two modules, so a test that loads
/// <c>build/ci-dicom/bin/xpe_common.dll</c> by full path before another test loads the copy in the test output directory leaves one process with two queues: the gain stage of
/// <c>GainPolyClampAlertTests</c> then pushes into the first (its xpe_preprocess binds to the module already loaded under that name) while the test reads the second, and the alert
/// "never arrives" (GUI-C-199, the CI signature of 70aac662: "Alerts seen: 0"). A native test therefore never adds a copy: when xpe_common.dll is already in the process it loads
/// nothing, and otherwise it loads the copy the module locator finds first (the test output directory, which is where <c>XpePreprocessNative.TryFindDll()</c> and so
/// <c>GainPolyClampAlertTests</c> take theirs). The module it needs (xpe_dicom, xpe_display, xpe_enhance_basic) is loaded by full path afterwards and binds its xpe_common
/// dependency by name to that copy.
/// </summary>
internal static class SharedCommonModule
{
    /// <summary>Loads nothing when an xpe_common.dll is already in the process; otherwise the locator's copy, or <paramref name="fallbackDirectory"/>'s when the locator finds none.</summary>
    public static void Load(string fallbackDirectory)
    {
        if (IsLoaded())
        {
            return;
        }

        NativeLibrary.Load(NativeModuleLibraryLocator.TryFindDll("xpe_common.dll", "image-processing") ?? Path.Combine(fallbackDirectory, "xpe_common.dll"));
    }

    private static bool IsLoaded() =>
        Process.GetCurrentProcess().Modules.Cast<ProcessModule>().Any(m => string.Equals(m.ModuleName, "xpe_common.dll", StringComparison.OrdinalIgnoreCase));
}
