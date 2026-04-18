namespace ImageProcTest.Models;

/// <summary>
/// Identifies the packaged offline help pages exposed by the GUI-S0 shell.
/// </summary>
public enum HelpPageKind
{
    /// <summary>
    /// Opens the main help landing page.
    /// </summary>
    Index,

    /// <summary>
    /// Opens the current quick-start guide for GUI-S0.
    /// </summary>
    QuickStart,

    /// <summary>
    /// Opens the scope and limitations page for the current build.
    /// </summary>
    Scope
}
