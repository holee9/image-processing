// Optional — gated on xpe_enhance_basic.dll availability at runtime.
using System.Runtime.InteropServices;

namespace ImageProcTest.IntegrationTests.PInvoke;

/// <summary>
/// Dynamic delegate signatures for xpe_enhance_basic.dll.
/// Loaded via <see cref="NativeLibrary.TryGetExport"/> only when the DLL is staged,
/// mirroring <see cref="XpePreprocessNative"/>. No static [DllImport] is used so an
/// absent DLL skips instead of failing the suite.
/// </summary>
internal static class XpeEnhanceBasicNative
{
    private const string DllName = "xpe_enhance_basic.dll";

    // -- Delegate types matching the native ABI --

    [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public delegate IntPtr VersionDelegate();

    /// <summary>xpe_log_transform(XpeImageBuffer* image, float normFactor) — in-place FLOAT32.</summary>
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate XpeCommonNative.XpeErrorCode LogTransformDelegate(
        ref XpeCommonNative.XpeImageBuffer image,
        float normFactor);

    /// <summary>
    /// Tries to locate xpe_enhance_basic.dll under the repo build tree.
    /// Returns null when not found.
    /// </summary>
    public static string? TryFindDll()
    {
        var envDir = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
        if (!string.IsNullOrEmpty(envDir))
        {
            var p = Path.Combine(envDir, DllName);
            if (File.Exists(p)) return p;
        }

        var repoRoot = FindRepositoryRoot(AppContext.BaseDirectory);
        if (repoRoot is null) return null;

        var candidates = new[]
        {
            Path.Combine(repoRoot, "build", "ci-common", "bin", "Debug", DllName),
            Path.Combine(repoRoot, "build", "ci-common", "bin", DllName),
            Path.Combine(repoRoot, "build", "ci-post", "bin", "Debug", DllName),
            Path.Combine(repoRoot, "build", "ci-post", "bin", DllName),
            Path.Combine(repoRoot, "build", "default", "bin", "Debug", DllName),
            Path.Combine(repoRoot, "build", "default", "bin", DllName),
        };

        return Array.Find(candidates, File.Exists);
    }

    private static string? FindRepositoryRoot(string start)
    {
        var dir = new DirectoryInfo(start);
        while (dir is not null)
        {
            if (Directory.Exists(Path.Combine(dir.FullName, ".git")) ||
                Directory.Exists(Path.Combine(dir.FullName, "modules", "common")))
                return dir.FullName;
            dir = dir.Parent;
        }
        return null;
    }
}
