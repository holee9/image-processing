using System.IO;
using System.Runtime.InteropServices;

namespace ImageProcTest
{
    internal static class XpeGsvgReadinessProbe
    {
        private const string DllName = "gsvg.dll";

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr VersionDelegate();

        public static GsvgHealthResult Check()
        {
            foreach (var candidate in NativeModuleLibraryLocator.GetDllCandidates(
                         DllName,
                         "gsvg",
                         "image-processing",
                         "xpe-post"))
            {
                if (!File.Exists(candidate))
                {
                    continue;
                }

                NativeDependencyLoader.TryLoadFor(candidate);
                if (!NativeLibrary.TryLoad(candidate, out var handle))
                {
                    continue;
                }

                try
                {
                    if (!NativeLibrary.TryGetExport(handle, "xpe_gsvg_version", out var symbol))
                    {
                        return new GsvgHealthResult(
                            "Entry point mismatch",
                            "Unavailable",
                            candidate,
                            "gsvg.dll exists but xpe_gsvg_version export was not found.",
                            IsVersionReady: false);
                    }

                    var versionPtr = Marshal.GetDelegateForFunctionPointer<VersionDelegate>(symbol)();
                    return new GsvgHealthResult(
                        "Version health ready",
                        Marshal.PtrToStringAnsi(versionPtr) ?? "<empty>",
                        candidate,
                        "gsvg.dll is discoverable under its canonical output name.",
                        IsVersionReady: true);
                }
                finally
                {
                    NativeLibrary.Free(handle);
                }
            }

            return new GsvgHealthResult(
                "DLL not found",
                "Unavailable",
                DllName,
                "gsvg.dll is not available in known GUI/build output locations.",
                IsVersionReady: false);
        }
    }
}
