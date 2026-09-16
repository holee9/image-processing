using System.Text.RegularExpressions;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// #166 (GUI-C-71): when the detached comparison viewer cannot be opened, the user is told.
///
/// <para>Before #166 the command threw at its first line and said nothing — the status bar kept its
/// previous value, the log gained no entry, and no dialog appeared (GUI-C-70 measured all three).
/// Pressing the command was indistinguishable from not pressing it.</para>
///
/// <para><b>This is a source guard, not a runtime observation, and that is a real weakness.</b> The
/// failure path cannot be produced from outside the app: the exception that used to trigger it is
/// the very thing this card fixed, and neither test project references the WPF assembly — both link
/// individual source files, so the view model cannot be constructed in a unit test without pulling
/// in the backends. The scenario that IS a runtime observation is <c>S11</c>, and it covers the
/// success path only. So this test can be satisfied by code that looks right and misbehaves; what
/// it does catch is the reporting being deleted, which is the regression that actually happened.</para>
///
/// <para>Kept separate from S11 on purpose: one assertion covering both would pass if either half
/// were later undone.</para>
/// </summary>
public sealed class DetachFailureIsReportedTests
{
    [Fact]
    public void TheDetachCommand_ReportsAFailureOnBothSurfacesTheUserReads()
    {
        var body = WithoutLineComments(MethodBody(ViewModelSource(), "private void OpenDetachedComparisonViewer()"));

        var catchIndex = body.IndexOf("catch", StringComparison.Ordinal);
        Assert.True(
            catchIndex >= 0,
            "OpenDetachedComparisonViewer has no catch. An exception there reaches no user-visible " +
            "surface: GUI-C-70 measured the status bar unchanged, the log empty and no dialog (#166).");

        var handler = body[catchIndex..];

        Assert.True(
            handler.Contains("StatusText", StringComparison.Ordinal),
            "The failure path does not write StatusText, so the status bar keeps whatever the " +
            "previous command left there (#166).");

        Assert.True(
            Regex.IsMatch(handler, @"\bLog\s*\("),
            "The failure path does not write the log, so nothing survives the next status message (#166).");
    }

    /// <summary>
    /// The normal path still claims success — and only after the window has been shown.
    ///
    /// Without this, "report the failure" could be satisfied by reporting failure always.
    /// </summary>
    [Fact]
    public void TheSuccessClaim_StillFollowsShow()
    {
        var body = WithoutLineComments(MethodBody(ViewModelSource(), "private void OpenDetachedComparisonViewerCore()"));

        var show = body.IndexOf("window.Show()", StringComparison.Ordinal);
        var claim = body.IndexOf("Detached comparison viewer opened.", StringComparison.Ordinal);

        Assert.True(show >= 0, "The command no longer calls window.Show().");
        Assert.True(claim >= 0, "The success message is gone; a command that opens a window says so.");
        Assert.True(
            claim > show,
            "The success message is written before window.Show(), so it would claim a window that " +
            "the very next line could fail to produce (#166).");
    }

    private static string WithoutLineComments(string text) =>
        Regex.Replace(text, @"//[^\r\n]*", string.Empty);

    /// <summary>The balanced body that follows a signature (same reason as GUI-C-63: lambdas).</summary>
    private static string MethodBody(string source, string signature)
    {
        var start = source.IndexOf(signature, StringComparison.Ordinal);
        Assert.True(start >= 0, $"'{signature}' was not found in MainWindowViewModel.cs.");

        var open = source.IndexOf('{', start);
        Assert.True(open >= 0, $"'{signature}' has no body.");

        var depth = 0;
        for (var i = open; i < source.Length; i++)
        {
            if (source[i] == '{') depth++;
            else if (source[i] == '}')
            {
                depth--;
                if (depth == 0) return source[open..i];
            }
        }

        throw new InvalidOperationException($"'{signature}' has an unbalanced body.");
    }

    private static string ViewModelSource()
    {
        var directory = new DirectoryInfo(AppContext.BaseDirectory);
        while (directory is not null)
        {
            var candidate = Path.Combine(
                directory.FullName, "gui", "ImageProcTest", "ViewModels", "MainWindowViewModel.cs");
            if (File.Exists(candidate)) return File.ReadAllText(candidate);
            directory = directory.Parent;
        }

        throw new FileNotFoundException(
            "gui/ImageProcTest/ViewModels/MainWindowViewModel.cs was not found above " + AppContext.BaseDirectory);
    }
}
