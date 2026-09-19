// #161 (GUI-C-63): the rejection is said AFTER the thing that clears the log, not before.
using System.Text.RegularExpressions;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Guards the order of two calls in <c>MainWindowViewModel</c>'s constructor.
///
/// <para><b>What was measured</b> (GUI-C-62, GUI-C-63, by launching the app with
/// <c>comparisonMode: "NotAMode"</c> in <c>appsettings.json</c> and reading the log list):</para>
///
/// <list type="table">
/// <item><term>report after InitializeBackend</term><description>7 log lines, the rejection among
/// them</description></item>
/// <item><term>report before it (the old order)</term><description>6 lines, <b>no rejection</b> — and
/// no "GUI-S0 initialized." either; the whole of that moment was gone</description></item>
/// <item><term>report before it, with <c>Logs.Clear()</c> removed</term><description>8 lines, the
/// rejection among them</description></item>
/// </list>
///
/// <para><b>Why this is a source guard and not a scenario.</b> Two reasons, both measured rather than
/// assumed. First, the run above needs a doctored <c>appsettings.json</c>, and a committed test that
/// rewrites the shipped settings file overwrites a developer's own settings (GUI-C-46) — the probe
/// that produced these numbers was deleted, as in GUI-C-59 and GUI-C-61. Second, the third row shows
/// that an outcome assertion ("the line survives start-up") <b>cannot tell the ordering from the
/// clearing</b>: removing <c>Logs.Clear()</c> satisfies it just as well. An outcome test would pass
/// while the ordering silently went back.</para>
///
/// <para>So the order is guarded here, together with the reason it matters — and the premise
/// assertion did its job: GUI-C-126 removed <c>Logs.Clear()</c> (lead decision on #198, which replaced
/// it with a <c>--- backend re-initialised ---</c> separator) and this file went red, rather than
/// keeping a rule alive whose reason had evaporated. Both assertions were then rewritten to the new
/// contract instead of being deleted: the ordering still matters, for the different reason stated in
/// its own message, and the premise now pins what replaced the clear.</para>
///
/// <para><b>What this cannot see</b>: whether the line is readable on screen. That was observed by
/// hand (GUI-C-62 measured the list at 356x156 px after pressing the panel's Log button) and is not
/// re-observed here.</para>
/// </summary>
[Trait("Category", "Functional")]
public sealed class StartupRejectionSurvivesTests
{
    [Fact]
    public void TheRejectionIsReported_AfterTheBackendClearsTheLog()
    {
        var source = ViewModelSource();
        var constructor = ConstructorBody(source);

        var report = constructor.IndexOf("ReportRejectedComparisonMode();", StringComparison.Ordinal);
        var initialize = constructor.IndexOf("InitializeBackend();", StringComparison.Ordinal);

        Assert.True(report >= 0, "The constructor no longer reports a rejected comparison mode (#161).");
        Assert.True(initialize >= 0, "The constructor no longer calls InitializeBackend.");

        Assert.True(
            report > initialize,
            "ReportRejectedComparisonMode() runs BEFORE InitializeBackend(). It no longer gets erased " +
            "(GUI-C-126 replaced the clear with a separator), but it would land ABOVE the " +
            "'--- backend re-initialised ---' boundary and so read as belonging to a previous backend " +
            "that, on the first initialise, does not exist. Move the call back after it.");
    }

    /// <summary>
    /// The premise, restated after GUI-C-126: start-up marks a boundary instead of erasing the record.
    ///
    /// <para>This assertion used to require <c>Logs.Clear();</c> here, and it fired the moment the
    /// clear was removed — which is what it was for. The rule did not disappear with the clear, it
    /// changed shape, so the assertion changed with it: what must not come back is the erasure, and
    /// what must stay is the separator. A run that clears again silently loses what the user has
    /// already read (GUI-C-122 measured 7 lines carrying a rescued settings path becoming 6 without
    /// it).</para>
    ///
    /// <para>The drain cursors are a different thing and are expected to keep resetting: they are
    /// backend state, not a record anybody read.</para>
    /// </summary>
    [Fact]
    public void InitializeBackend_MarksABoundaryRatherThanErasingTheRecord()
    {
        var body = WithoutLineComments(MethodBody(ViewModelSource(), "private void InitializeBackend()"));

        Assert.DoesNotContain("Logs.Clear();", body, StringComparison.Ordinal);
        Assert.DoesNotContain("Alerts.Clear();", body, StringComparison.Ordinal);
        Assert.Contains("backend re-initialised", body, StringComparison.Ordinal);

        // The control: the cursors still reset, so "nothing clears any more" is not what was measured.
        Assert.Contains("_drainedBackendLogCount = 0;", body, StringComparison.Ordinal);
        Assert.Contains("_drainedBackendAlertCount = 0;", body, StringComparison.Ordinal);
    }

    /// <summary>
    /// The text with <c>//</c> comments removed.
    ///
    /// The first version of the premise assertion searched the raw body, and the falsification caught
    /// it: commenting the call out left the text in place and the guard still passed. A guard that a
    /// pair of slashes defeats is not a guard.
    /// </summary>
    private static string WithoutLineComments(string text) =>
        Regex.Replace(text, @"//[^\r\n]*", string.Empty);

    /// <summary>The constructor body, brace-balanced from its signature.</summary>
    private static string ConstructorBody(string source) =>
        MethodBody(source, "public MainWindowViewModel(");

    /// <summary>
    /// The balanced body that follows a signature.
    ///
    /// Balanced rather than "up to the next blank line": the constructor is long and contains lambdas
    /// with their own braces. GUI-C-38 and GUI-C-39 both had to replace a naive scan for the same
    /// reason, and both were caught by a coverage assertion rather than by review.
    /// </summary>
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

    /// <summary>Reads the view model's source, walking up to the repository root.</summary>
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
