using System.IO;
using ImageProcTest.Models;

namespace ImageProcTest.Services;

public static class XpeBackendFactory
{
    public static IXpeBackend Create(AppSettings settings)
    {
        var nativeDllPath = Path.Combine(AppContext.BaseDirectory, "xpe_common.dll");
        var nativeDllDetected = File.Exists(nativeDllPath);
        return new MockXpeBackend(new RawImageLoader(), nativeDllPath, nativeDllDetected);
    }
}
