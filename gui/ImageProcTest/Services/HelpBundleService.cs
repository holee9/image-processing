using System.Diagnostics;
using System.IO;
using ImageProcTest.Models;

namespace ImageProcTest.Services;

/// <summary>
/// Resolves packaged offline help pages for the current application output.
/// </summary>
public sealed class HelpBundleService
{
    /// <summary>
    /// Gets the root directory of the packaged help bundle.
    /// </summary>
    public string HelpRoot => Path.Combine(AppContext.BaseDirectory, "help");

    /// <summary>
    /// Gets the path of the requested help page if it exists in the packaged bundle.
    /// </summary>
    public string GetHelpPagePath(HelpPageKind pageKind)
    {
        var relativePath = pageKind switch
        {
            HelpPageKind.Index => "index.html",
            HelpPageKind.QuickStart => "quick-start.html",
            HelpPageKind.Scope => "scope.html",
            _ => throw new ArgumentOutOfRangeException(nameof(pageKind), pageKind, "Unknown help page.")
        };

        return Path.Combine(HelpRoot, relativePath);
    }

    /// <summary>
    /// Returns true when the requested help page is packaged with the app.
    /// </summary>
    public bool HelpPageExists(HelpPageKind pageKind)
    {
        return File.Exists(GetHelpPagePath(pageKind));
    }

    /// <summary>
    /// Opens the requested help page in the default external browser.
    /// </summary>
    public void OpenInBrowser(HelpPageKind pageKind)
    {
        var helpPagePath = GetHelpPagePath(pageKind);
        if (!File.Exists(helpPagePath))
        {
            throw new FileNotFoundException("Help page not found.", helpPagePath);
        }

        Process.Start(new ProcessStartInfo
        {
            FileName = helpPagePath,
            UseShellExecute = true
        });
    }
}
