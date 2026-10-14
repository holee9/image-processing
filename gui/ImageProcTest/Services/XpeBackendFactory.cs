using System.IO;
using ImageProcTest.Models;

namespace ImageProcTest.Services;

public static class XpeBackendFactory
{
    // @MX:ANCHOR: [AUTO] Backend routing: Native requires BackendMode="Native", both DLLs present, and all required exports; any failure → MockXpeBackend
    // @MX:REASON: Single factory entry point; Func<AppSettings, IXpeBackend> in MainWindowViewModel constructor captures this method as a delegate
    public static IXpeBackend Create(AppSettings settings)
    {
        // #129: resolve through the shared search policy, not the app directory alone. The resolver
        // (GuiNativeLibraryResolver) uses the same candidate lists for the actual P/Invoke, so the
        // path this factory reports is the path that will be loaded. Before GUI-C-32 both were
        // hard-coded to AppContext.BaseDirectory, which made XPE_NATIVE_DIR inert for gui even
        // after a resolver was installed — the factory fell back to Mock before any P/Invoke ran.
        var commonDllPath = XpeCommonLibraryLocator.TryFindDll()
            ?? Path.Combine(AppContext.BaseDirectory, "xpe_common.dll");
        var displayDllPath = NativeModuleLibraryLocator.TryFindDll("xpe_display.dll", "image-processing")
            ?? Path.Combine(AppContext.BaseDirectory, "xpe_display.dll");
        var commonDllDetected = File.Exists(commonDllPath);
        var displayDllDetected = File.Exists(displayDllPath);
        var rawImageLoader = new RawImageLoader();

        string? refusal = null;
        if (string.Equals(settings.BackendMode, "Native", StringComparison.OrdinalIgnoreCase))
        {
            // GUI-C-238: the reason is asked for, not guessed from the flags - a DLL that exists can still be the wrong one (an old copy, missing exports).
            refusal = RealXpeBackend.WhyNativeUnusable(commonDllPath, displayDllPath);
            if (refusal is null)
            {
                return new RealXpeBackend(rawImageLoader, commonDllPath, displayDllPath);
            }
        }

        return new MockXpeBackend(rawImageLoader, commonDllPath, commonDllDetected, displayDllPath, displayDllDetected, refusal);
    }
}
