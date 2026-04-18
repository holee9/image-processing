using System.IO;
using ImageProcTest.Models;

namespace ImageProcTest.Services;

public static class XpeBackendFactory
{
    public static IXpeBackend Create(AppSettings settings)
    {
        var commonDllPath = Path.Combine(AppContext.BaseDirectory, "xpe_common.dll");
        var displayDllPath = Path.Combine(AppContext.BaseDirectory, "xpe_display.dll");
        var commonDllDetected = File.Exists(commonDllPath);
        var displayDllDetected = File.Exists(displayDllPath);
        var rawImageLoader = new RawImageLoader();

        if (string.Equals(settings.BackendMode, "Native", StringComparison.OrdinalIgnoreCase) &&
            commonDllDetected &&
            displayDllDetected &&
            RealXpeBackend.CanUseNative(commonDllPath, displayDllPath))
        {
            return new RealXpeBackend(rawImageLoader, commonDllPath, displayDllPath);
        }

        return new MockXpeBackend(rawImageLoader, commonDllPath, commonDllDetected, displayDllPath, displayDllDetected);
    }
}
