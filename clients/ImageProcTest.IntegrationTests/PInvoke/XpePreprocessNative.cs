// Optional — gated on xpe_preprocess.dll availability at runtime.
using System.Runtime.InteropServices;

namespace ImageProcTest.IntegrationTests.PInvoke;

/// <summary>
/// Dynamic delegate signatures for xpe_preprocess.dll.
/// Loaded via <see cref="NativeLibrary.TryGetExport"/> only when the DLL is staged.
/// No static [DllImport] is used to avoid hard failures on absent DLL.
/// </summary>
internal static class XpePreprocessNative
{
    private const string DllName = "xpe_preprocess.dll";

    // -- Delegate types matching the native ABI --

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate XpeCommonNative.XpeErrorCode InitDelegate(IntPtr configOrNull);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void ShutdownDelegate();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public delegate IntPtr VersionDelegate();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate XpeCommonNative.XpeErrorCode CorrectionDelegate(
        ref XpeCommonNative.XpeImageBuffer input,
        ref XpeCommonNative.XpeImageBuffer output,
        ref XpeCommonNative.XpeImageMetadata metadata);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public delegate XpeCommonNative.XpeErrorCode CalibLoadDelegate(
        [MarshalAs(UnmanagedType.LPStr)] string path);

    // -- Required export names --
    public static readonly string[] RequiredExports =
    {
        "xpe_preprocess_version",
        "xpe_preprocess_init",
        "xpe_preprocess_shutdown",
        "xpe_offset_correct",
        "xpe_gain_correct",
        "xpe_defect_correct",
        "xpe_calib_load_offset",
        "xpe_calib_load_gain",
        "xpe_calib_load_defect_map",
        "xpe_calib_generate_offset",
        "xpe_calib_check_expiry",
        "xpe_calib_save",
        "xpe_validate_readout_artifact",
        "xpe_defect_detect_runtime",
        "xpe_preprocess_get_param_range",
    };

    /// <summary>
    /// Tries to locate xpe_preprocess.dll under the repo build tree.
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
            Path.Combine(repoRoot, "build", "readiness-preprocess-only-vs2", "bin", "Debug", DllName),
            Path.Combine(repoRoot, "build", "default", "bin", "Debug", DllName),
            Path.Combine(repoRoot, "build", "default", "bin", DllName),
            Path.Combine(repoRoot, "build", "readiness-preprocess-vs", "bin", "Debug", DllName),
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
