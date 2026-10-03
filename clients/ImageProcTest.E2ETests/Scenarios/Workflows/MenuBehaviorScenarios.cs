// #225 (GUI-C-204): the menu items GUI-C-203 found that no test observed the BEHAVIOUR of — Zoom In / Zoom Out, Native DLL Diagnostics, About, and the three Help pages.
using System.Text.RegularExpressions;
using FlaUI.Core.AutomationElements;
using FlaUI.Core.Definitions;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// Every item is invoked through its UI Automation peer and read back from what the app shows. No key and no mouse event is sent: those go to whichever window is in front.
///
/// <para>What each handler does (cited from the code, with the condition that wraps it):</para>
/// <list type="bullet">
/// <item><c>ZoomIn</c> (<c>MainWindowViewModel.ZoomIn</c>): <c>current = scale &lt;= 0 ? 1.0 : scale</c>, then <c>scale = Math.Min(16.0, current * 1.25)</c>. Fit is scale 0, so the first Zoom In from Fit is 125%.
/// <c>ZoomOut</c>: <c>Math.Max(0.05, current / 1.25)</c>. Both write the log line "Comparison viewport zoomed in/out. Mode=…, Zoom=NNN%, …" (<c>RefreshComparisonStatus</c>).</item>
/// <item><c>ShowNativeDiagnostics</c>: the status bar says "Display DLL detected at '&lt;path&gt;' (&lt;version&gt;)." when <c>RuntimeInfo.DisplayDllDetected</c>, else "Display DLL not detected; mock display pipeline remains active.",
/// and the log gets "Native diagnostics: backend=…, version=…, …".</item>
/// <item><c>AboutBuildInfoMenuItem_OnClick</c>: a modal message box "About ImageProcTest": "ImageProcTest GUI-S0 / Backend: &lt;RuntimeInfo.Version&gt; / Help bundle: packaged offline HTML".</item>
/// <item><c>OpenHelpPage</c>: opens an in-app help window titled "ImageProcTest Help - …" whose path line is the packaged page; if the page is missing it shows a warning message box instead.</item>
/// </list>
///
/// <para>Backend split: nothing here needs a native library to be PRESENT except the diagnostics' detected branch, which is asserted against the file on disk in both backends, so a Mock run that finds
/// no DLL and a Native run that finds one are each checked against what the disk says (never loosened to accept either).</para>
/// </summary>
[Collection(Wrist1024SliceApplicationCollection.Name)]
public sealed class MenuBehaviorScenarios(Wrist1024SliceApplicationFixture app, ITestOutputHelper output)
{
    // ---------------------------------------------------------------- Zoom In / Zoom Out

    [SkippableFact]
    public void Z01_ZoomIn_And_ZoomOut_MoveTheScaleBy125Percent_AndStopAtTheBounds()
    {
        Measure("Z-01", window =>
        {
            OpenLogs(window);

            Invoke(window, "ViewMenu", "ZoomFitMenuItem");
            Assert.Equal("Fit", WaitForZoom(window, "Fit"));

            // 125 % per step, from Fit (scale 0 counts as 1.0). 1.25^2 = 1.5625 -> shown as 156 %.
            Invoke(window, "ViewMenu", "ZoomInMenuItem");
            Assert.Equal("125%", WaitForZoom(window, "125%"));
            Invoke(window, "ViewMenu", "ZoomInMenuItem");
            Assert.Equal("156%", WaitForZoom(window, "156%"));

            // And back: 156.25 / 1.25 = 125, / 1.25 = 100.
            Invoke(window, "ViewMenu", "ZoomOutMenuItem");
            Assert.Equal("125%", WaitForZoom(window, "125%"));
            Invoke(window, "ViewMenu", "ZoomOutMenuItem");
            Assert.Equal("100%", WaitForZoom(window, "100%"));

            // Upper bound: 16.0 = 1600 %. 100 % * 1.25^13 passes 1600, so 13 steps reach the cap and the 14th changes nothing.
            for (var step = 0; step < 13; step++) Invoke(window, "ViewMenu", "ZoomInMenuItem", settle: 120);
            Assert.Equal("1600%", WaitForZoom(window, "1600%"));
            Invoke(window, "ViewMenu", "ZoomInMenuItem", settle: 120);
            Assert.Equal("1600%", WaitForZoom(window, "1600%"));

            // Lower bound: 0.05 = 5 %. From Fit (1.0) 14 steps of /1.25 pass 5 %, so the cap is reached and one more changes nothing.
            Invoke(window, "ViewMenu", "ZoomFitMenuItem");
            Assert.Equal("Fit", WaitForZoom(window, "Fit"));
            for (var step = 0; step < 14; step++) Invoke(window, "ViewMenu", "ZoomOutMenuItem", settle: 120);
            Assert.Equal("5%", WaitForZoom(window, "5%"));
            Invoke(window, "ViewMenu", "ZoomOutMenuItem", settle: 120);
            Assert.Equal("5%", WaitForZoom(window, "5%"));

            // Leave the shared app as the other scenarios found it.
            Invoke(window, "ViewMenu", "ZoomFitMenuItem");
            Assert.Equal("Fit", WaitForZoom(window, "Fit"));
        });
    }

    /// <summary>
    /// The zoom value the newest "Comparison viewport …" log line reports (the log list is newest-first: Log inserts at index 0), waited for until it equals <paramref name="expected"/> (or the deadline). A stale line shows the previous value, so a handler that
    /// does nothing leaves the wait unsatisfied and the assertion fails with the value that WAS shown.
    /// </summary>
    private static string WaitForZoom(Window window, string expected)
    {
        var last = "(no comparison log line)";
        for (var attempt = 0; attempt < 50; attempt++)
        {
            var line = LogText(window).Split('\n').FirstOrDefault(l => l.Contains("Comparison viewport", StringComparison.Ordinal));
            var match = line is null ? null : Regex.Match(line, @"Zoom=(Fit|\d+%)");
            if (match is { Success: true })
            {
                last = match.Groups[1].Value;
                if (last == expected) return last;
            }

            Thread.Sleep(100);
        }

        return last;
    }

    // ---------------------------------------------------------------- Native DLL Diagnostics

    [SkippableFact]
    public void D01_NativeDllDiagnostics_NamesTheDisplayLibraryTheDiskHolds_AndTheBackendThatIsRunning()
    {
        Measure("D-01", window =>
        {
            OpenLogs(window);
            Invoke(window, "BackendMenu", "NativeDllDiagnosticsMenuItem");

            var status = WaitForStatus(window, text => text.StartsWith("Display DLL", StringComparison.Ordinal));
            output.WriteLine($"D-01 status: {status}");
            Assert.StartsWith("Display DLL", status, StringComparison.Ordinal);

            var logLine = WaitForLogLine(window, "Native diagnostics:");
            output.WriteLine($"D-01 log: {logLine}");
            Assert.False(logLine is null, "The diagnostics command wrote no 'Native diagnostics:' log line.");

            // The backend named in the log is the one that is running (the fixture was told which one it launched).
            var expectedBackend = app.BackendMode == "Native" ? "RealXpeBackend" : "MockXpeBackend";
            Assert.Contains($"backend={expectedBackend}", logLine!, StringComparison.Ordinal);

            var detected = Regex.Match(status, @"^Display DLL detected at '(?<path>[^']*)' \((?<version>[^)]*)\)\.$");
            if (detected.Success)
            {
                // The path it names is a file on the disk, and the log's own displayPath says the same.
                var path = detected.Groups["path"].Value;
                Assert.True(File.Exists(path), $"The status names '{path}' as the detected display library but no such file exists.");
                Assert.Contains($"displayDetected=True", logLine, StringComparison.Ordinal);
                Assert.Contains($"displayPath='{path}'", logLine, StringComparison.Ordinal);

                var version = detected.Groups["version"].Value;
                Assert.False(string.IsNullOrWhiteSpace(version), "The status names no display library version.");
                if (app.BackendMode == "Native")
                {
                    Assert.DoesNotContain("mock", version, StringComparison.OrdinalIgnoreCase);
                    Assert.Matches(@"\d+\.\d+\.\d+", version);

                    // Where the run was pinned to a directory, the library named is the one in it.
                    var pinned = Environment.GetEnvironmentVariable(ApplicationFixture.NativeDirVariable);
                    if (!string.IsNullOrWhiteSpace(pinned))
                    {
                        var actual = Path.GetFullPath(Path.GetDirectoryName(path)!).TrimEnd('\\', '/');
                        var expected = Path.GetFullPath(pinned).TrimEnd('\\', '/');
                        Assert.Equal(expected, actual, ignoreCase: true);
                    }

                    // The version shown in the status bar's runtime summary is the same one.
                    var summary = Text(window, "RuntimeCommonVersionText");
                    Assert.Contains(version, summary, StringComparison.Ordinal);
                }
                else
                {
                    Assert.Contains("mock", version, StringComparison.OrdinalIgnoreCase);
                }
            }
            else
            {
                // The other branch: the backend found no display library. Then the status says exactly that, and the log agrees.
                Assert.Equal("Display DLL not detected; mock display pipeline remains active.", status);
                Assert.Contains("displayDetected=False", logLine, StringComparison.Ordinal);
                Assert.True(app.BackendMode != "Native", "A Native run that reports no display library is a broken run (the backend fell back or the library is missing), not a diagnostics result: see S05.");
            }
        });
    }

    // ---------------------------------------------------------------- About

    [SkippableFact]
    public void A01_About_ShowsTheBackendVersionTheStatusBarShows()
    {
        Measure("A-01", window =>
        {
            Invoke(window, "HelpMenu", "AboutBuildInfoMenuItem", settle: 0);

            var dialog = WaitForDialog(window, "About ImageProcTest");
            Assert.True(dialog is not null, "No 'About ImageProcTest' dialog appeared.");
            try
            {
                var text = string.Join("\n", dialog!.FindAllDescendants(cf => cf.ByControlType(ControlType.Text)).Select(t => t.Name));
                output.WriteLine($"A-01 dialog text: {text.Replace("\n", " | ")}");

                Assert.Contains("ImageProcTest GUI-S0", text, StringComparison.Ordinal);
                Assert.Contains("Help bundle: packaged offline HTML", text, StringComparison.Ordinal);

                var backend = Regex.Match(text, @"Backend: (?<v>[^\r\n]+)");
                Assert.True(backend.Success, "The About text has no 'Backend:' line.");
                var version = backend.Groups["v"].Value.Trim();
                Assert.NotEqual("unknown", version);

                // The same version as the status bar's runtime summary (a separate element bound to the same backend report) and, for Native, a real one.
                var summary = Text(window, "RuntimeCommonVersionText");
                Assert.Contains(version, summary, StringComparison.Ordinal);
                if (app.BackendMode == "Native")
                {
                    Assert.DoesNotContain("mock", version, StringComparison.OrdinalIgnoreCase);
                    Assert.Matches(@"\d+\.\d+\.\d+", version);
                }
                else
                {
                    Assert.Contains("mock", version, StringComparison.OrdinalIgnoreCase);
                }

                AssertBuildLine(text);

                // The first line is the window's own base title.
                Assert.Contains("ImageProcTest GUI-S0", window.Title, StringComparison.Ordinal);
            }
            finally
            {
                dialog!.FindFirstDescendant(cf => cf.ByControlType(ControlType.Button))?.AsButton().Invoke();
                Thread.Sleep(300);
            }

            Assert.True(WaitForDialog(window, "About ImageProcTest", seconds: 1) is null, "The About dialog did not close after OK.");
        });
    }

    /// <summary>
    /// GUI-C-206: the "Build: &lt;sha&gt; (&lt;configuration&gt;)" line is checked against things the About code did not compute: the product version stamped into the
    /// executable FILE, the configuration folder the executable sits in, and git itself. A build is allowed to be OLDER than the checkout (a commit made after the build), so
    /// the commit is asserted to be an ancestor of HEAD, and — where CI names the commit (GITHUB_SHA) — to be that commit.
    /// </summary>
    private void AssertBuildLine(string aboutText)
    {
        var line = Regex.Match(aboutText, @"Build: (?<sha>\S+) \((?<config>[^)]*)\)");
        Assert.True(line.Success, $"The About text has no 'Build: <sha> (<configuration>)' line: '{aboutText.Replace('\n', '|')}'");
        var sha = line.Groups["sha"].Value;
        var config = line.Groups["config"].Value;

        var exe = app.ExecutablePath!;
        var productVersion = System.Diagnostics.FileVersionInfo.GetVersionInfo(exe).ProductVersion ?? string.Empty;
        output.WriteLine($"A-01 build line: sha={sha} config={config}; exe ProductVersion='{productVersion}'");
        Assert.EndsWith("+" + sha, productVersion, StringComparison.Ordinal);

        var folder = Path.GetFileName(Path.GetDirectoryName(Path.GetDirectoryName(exe)!)!);
        Assert.Equal(folder, config, ignoreCase: true);

        var head = Git(Path.GetDirectoryName(exe)!, "rev-parse", "HEAD");
        if (head is null)
        {
            output.WriteLine("A-01 git is not usable from here: the About revision was checked against the executable's product version only.");
            return;
        }

        Assert.NotEqual("unknown", sha);
        Assert.Matches("^[0-9a-f]{10}$", sha);
        Assert.NotNull(Git(Path.GetDirectoryName(exe)!, "rev-parse", "--verify", "--quiet", sha + "^{commit}"));
        Assert.NotNull(Git(Path.GetDirectoryName(exe)!, "merge-base", "--is-ancestor", sha, "HEAD"));
        var ci = Environment.GetEnvironmentVariable("GITHUB_SHA");
        if (!string.IsNullOrWhiteSpace(ci))
        {
            Assert.StartsWith(sha, ci, StringComparison.OrdinalIgnoreCase);
        }
    }

    /// <summary>Runs git; the output when it exits 0, null when git is absent, the folder is no repository, or the command fails.</summary>
    private static string? Git(string workingDirectory, params string[] arguments)
    {
        try
        {
            var info = new System.Diagnostics.ProcessStartInfo("git") { WorkingDirectory = workingDirectory, RedirectStandardOutput = true, RedirectStandardError = true, UseShellExecute = false, CreateNoWindow = true };
            foreach (var argument in arguments) info.ArgumentList.Add(argument);
            using var process = System.Diagnostics.Process.Start(info)!;
            var text = process.StandardOutput.ReadToEnd().Trim();
            process.StandardError.ReadToEnd();
            process.WaitForExit(10000);
            return process.ExitCode == 0 ? text : null;
        }
        catch (Exception)
        {
            return null;
        }
    }

    // ---------------------------------------------------------------- Help pages

    [SkippableTheory]
    [InlineData("OpenQuickStartHelpMenuItem", "ImageProcTest Help - Quick Start", "quick-start.html")]
    [InlineData("OpenScopeHelpMenuItem", "ImageProcTest Help - Scope and Limitations", "scope.html")]
    [InlineData("OpenCurrentWorkflowHelpMenuItem", "ImageProcTest Help - Quick Start", "quick-start.html")]
    public void H01_HelpItem_OpensAHelpWindowOnAPackagedPageWithContent(string itemId, string expectedTitle, string expectedFile)
    {
        Measure("H-01 " + itemId, window =>
        {
            Invoke(window, "HelpMenu", itemId, settle: 0);

            var help = WaitForTopLevel(window, expectedTitle);
            Assert.True(help is not null,
                $"No help window titled '{expectedTitle}' appeared. (A missing page shows a warning message box instead.) Windows of the app seen: [{string.Join(" | ", WindowsOfTheApp(window))}]");
            try
            {
                var path = help!.FindFirstDescendant(cf => cf.ByAutomationId("HelpPathTextBlock"))?.Name ?? string.Empty;
                output.WriteLine($"H-01 {itemId}: path={path}");
                Assert.Equal(expectedFile, Path.GetFileName(path));

                // The page the window names is a file of the app's own packaged help folder, and it has content.
                Assert.True(File.Exists(path), $"The help window names '{path}', which does not exist.");
                var helpRoot = Path.Combine(Path.GetDirectoryName(app.ExecutablePath!)!, "help");
                Assert.Equal(Path.GetFullPath(helpRoot).TrimEnd('\\'), Path.GetDirectoryName(Path.GetFullPath(path))!.TrimEnd('\\'), ignoreCase: true);
                var html = File.ReadAllText(path);
                Assert.Matches(@"<h1[^>]*>\s*\S", html);

                // The window did not fall back to its error overlay.
                var error = help.FindFirstDescendant(cf => cf.ByAutomationId("HelpErrorTextBlock"))?.Name ?? string.Empty;
                Assert.Equal(string.Empty, error);
            }
            finally
            {
                help!.Patterns.Window.PatternOrDefault?.Close();
                Thread.Sleep(300);
            }
        });
    }

    // ---------------------------------------------------------------- helpers

    private void Measure(string scenario, Action<Window> body)
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        var window = app.MainWindow;
        Assert.True(window is not null, "The main window was not available.");
        output.WriteLine($"{scenario} backend={app.BackendMode}");
        body(window!);
    }

    private static void Invoke(Window window, string menuId, string itemId, int settle = 300)
    {
        var menu = window.FindFirstDescendant(cf => cf.ByAutomationId(menuId));
        Assert.True(menu is not null, $"{menuId} was not found.");
        menu!.AsMenuItem().Expand();

        AutomationElement? item = null;
        for (var attempt = 0; attempt < 20 && item is null; attempt++)
        {
            item = window.FindFirstDescendant(cf => cf.ByAutomationId(itemId));
            if (item is null) Thread.Sleep(100);
        }

        Assert.True(item is not null, $"{itemId} did not appear under {menuId}.");
        item!.AsMenuItem().Invoke();
        try { menu.AsMenuItem().Collapse(); } catch (Exception) { /* already closed */ }
        if (settle > 0) Thread.Sleep(settle);
    }

    private static string Text(Window window, string automationId) =>
        window.FindFirstDescendant(cf => cf.ByAutomationId(automationId))?.Name ?? string.Empty;

    private static string WaitForStatus(Window window, Func<string, bool> accept)
    {
        var last = string.Empty;
        for (var attempt = 0; attempt < 80; attempt++)
        {
            last = Text(window, "StatusBarText");
            if (accept(last)) return last;
            Thread.Sleep(100);
        }

        return last;
    }

    private static string? WaitForLogLine(Window window, string startsWithAfterTimestamp)
    {
        for (var attempt = 0; attempt < 50; attempt++)
        {
            var line = LogText(window).Split('\n').FirstOrDefault(l => l.Contains(startsWithAfterTimestamp, StringComparison.Ordinal));
            if (line is not null) return line;
            Thread.Sleep(100);
        }

        return null;
    }

    /// <summary>The message box (a modal child window of the main window) with this title, or null.</summary>
    private static Window? WaitForDialog(Window window, string title, int seconds = 10)
    {
        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(seconds);
        while (DateTime.UtcNow < deadline)
        {
            var found = window.ModalWindows.FirstOrDefault(w => w.Title == title);
            if (found is not null) return found;
            Thread.Sleep(100);
        }

        return null;
    }

    /// <summary>
    /// An owned, non-modal window of the app. UI Automation lists an owned window under its OWNER, not under the desktop (measured: the desktop's children for
    /// the app's process were only the main window while the help window was open), so the owner's descendants are searched, and the desktop's as well.
    /// </summary>
    private Window? WaitForTopLevel(Window window, string title)
    {
        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(10);
        while (DateTime.UtcNow < deadline)
        {
            var found = window.FindAllDescendants(cf => cf.ByControlType(ControlType.Window))
                .Concat(app.Automation.GetDesktop().FindAllChildren(cf => cf.ByControlType(ControlType.Window)))
                .FirstOrDefault(e => e.Name == title);
            if (found is not null) return found.AsWindow();
            Thread.Sleep(100);
        }

        return null;
    }

    private IEnumerable<string> WindowsOfTheApp(Window window)
    {
        var pid = window.Properties.ProcessId.Value;
        return window.FindAllDescendants(cf => cf.ByControlType(ControlType.Window)).Select(e => $"in-main:'{e.Name}'")
            .Concat(app.Automation.GetDesktop().FindAllChildren()
                .Where(e => e.Properties.ProcessId.ValueOrDefault == pid)
                .Select(e => $"desktop:{e.Properties.ControlType.ValueOrDefault}:'{e.Name}'"));
    }

    private static string LogText(Window window)
    {
        var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));
        if (list is null) return string.Empty;
        return string.Join("\n", list.FindAllChildren().Select(item => item.Name ?? string.Empty));
    }

    /// <summary>Copied from MenuCommandScenarios (itself copied from ClearAlertsObservationScenarios): the verified sequence that makes the log list readable.</summary>
    private static void OpenLogs(Window window)
    {
        var view = window.FindFirstDescendant(cf => cf.ByAutomationId("ViewMenu"));
        Assert.True(view is not null, "ViewMenu is not in the tree.");
        view!.Patterns.ExpandCollapse.Pattern.Expand();
        Thread.Sleep(300);

        var toggle = window.FindFirstDescendant(cf => cf.ByAutomationId("ShowLogsPanelMenuItem"));
        Assert.True(toggle is not null, "ShowLogsPanelMenuItem is not in the tree.");
        if (toggle!.Patterns.Toggle.Pattern.ToggleState != ToggleState.On)
        {
            toggle.Patterns.Toggle.Pattern.Toggle();
            Thread.Sleep(200);
        }

        view.Patterns.ExpandCollapse.Pattern.Collapse();
        Thread.Sleep(200);
        window.FindFirstDescendant(cf => cf.ByName("Log"))?.AsButton().Invoke();
        Thread.Sleep(600);
    }
}
