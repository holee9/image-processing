using System.IO;

namespace ImageProcTest.Services.Comparison;

/// <summary>
/// Where the comparison window keeps its own files (remembered script path, default result folder). The same rule the app uses for its user data: XPE_GUI_DATA_DIR when set (absolute path), else the folder next to the exe.
/// Kept here, not shared, so this slice builds from committed files alone.
/// </summary>
public static class ComparisonPaths
{
    public static string DataRoot
    {
        get
        {
            var root = Environment.GetEnvironmentVariable("XPE_GUI_DATA_DIR");
            if (string.IsNullOrWhiteSpace(root)) return AppContext.BaseDirectory;
            if (!Path.IsPathFullyQualified(root)) throw new InvalidOperationException("XPE_GUI_DATA_DIR must be an absolute user-data path.");
            return Path.GetFullPath(root);
        }
    }
}
