using System.IO;

namespace ImageProcTest.Services;

/// <summary>#277: installed launcher supplies a separate user-data root; developer behavior is preserved.</summary>
public static class GuiDataPaths
{
    public static string Root
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
