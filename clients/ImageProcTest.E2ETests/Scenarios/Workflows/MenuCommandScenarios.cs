// #225 (GUI-C-155): the three commands GUI-C-154 wired, asserted rather than only recorded.
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// Rows 6, 11 and 12 of the #225 table, each driven through the menu a user has and asserted on what
/// the app then says.
///
/// <para><b>Why these needed scenarios at all.</b> GUI-C-154 recorded all three in the automation
/// report and asserted none of them. A value that is written but never compared fills a report and
/// stops no regression — #205, #207 and #212 were each that shape. The report fields stay; these
/// scenarios are what make them load-bearing.</para>
///
/// <para><b>What is NOT here, and why.</b> Row 11's in-flight case — stop a render while it runs — is
/// asserted in <c>AutomationReportBackendTests</c>, not through this UI. Two attempts here failed and
/// both taught the same thing: the render is tens of milliseconds, and opening a menu takes longer.
/// The second attempt turned the GSVG stage on first, on the strength of GUI-C-153's "2.3-2.5 s" —
/// but that figure is the TEST's round trip (invoke, wait for a new processed version), not the
/// background work, which the status bar reports as <c>work=20 ms</c>. There is no UI timing that
/// wins that race, so the observation belongs where it is deterministic: in-process, in the automation
/// run. Row 11b — the empty case, which is the one that was broken — is here, because it needs no race.</para>
///
/// <para><b>Backend split.</b> Anything that needs a native library asserts on the Native backend and
/// skips under Mock with the reason stated at the skip. ci.yml:454-455 says the Mock job runs with "no
/// native DLLs" — there, a red smoke is CORRECT, and passing it anyway would make "the DLLs are
/// absent" and "the DLLs are broken" the same green (#214). What needs no native library — the stop
/// command's empty-queue path — runs in both.</para>
/// </summary>
[Collection(Wrist1024SliceApplicationCollection.Name)]
public sealed class MenuCommandScenarios(Wrist1024SliceApplicationFixture app, ITestOutputHelper output)
{
    /// <summary>
    /// Row 6: the smoke answers from the real libraries.
    ///
    /// <para>Asserted on the VERSION rather than on the word "passed": the mock display reports
    /// <c>v0.0.0-mock-display</c>, so a run that silently fell back to it would still say "passed"
    /// if the assertion only read the verdict. The version is what separates the two.</para>
    /// </summary>
    [SkippableFact]
    public void R06_PInvokeSmoke_AnswersFromTheNativeLibraries()
    {
        SkipUnlessNative();

        Measure("R-06", window =>
        {
            InvokeToolsMenuItem(window, "BackendMenu", "PInvokeSmokeTestMenuItem");
            var status = WaitForStatus(window, text => text.StartsWith("P/Invoke smoke", StringComparison.Ordinal));
            output.WriteLine($"R-06 status: {status}");

            Assert.DoesNotContain("FAILED", status, StringComparison.Ordinal);

            // Measured (GUI-C-155): reading the log without opening its panel first returned an empty
            // string, and the assertion below then failed for the wrong reason. The panel is opened the
            // way ClearAlertsObservationScenarios does it — a verified sequence, not a second guess.
            OpenLogs(window);
            var log = LogText(window);
            Assert.Contains("xpe_display_version", log, StringComparison.Ordinal);
            Assert.DoesNotContain("mock-display", log, StringComparison.OrdinalIgnoreCase);
            Assert.DoesNotContain("threw", log, StringComparison.OrdinalIgnoreCase);
        });
    }

    /// <summary>
    /// Row 11b: pressing Stop with nothing running says so, and discards nothing.
    ///
    /// <para><b>This is the case that was broken.</b> GUI-C-154's first implementation answered "the
    /// render in flight will be discarded" here, because the cancellation source from the previous
    /// render was never cleared. The observation caught it; without a scenario it comes back.</para>
    ///
    /// <para>Runs in BOTH backends: no native library is needed to press a command and read a status
    /// line, and the empty-queue answer must not depend on which backend is loaded.</para>
    /// </summary>
    [SkippableFact]
    public void R11b_StopWithNothingRunning_SaysSoAndDiscardsNothing()
    {
        Measure("R-11b", window =>
        {
            // Let any render in flight finish first, so "nothing running" is the state under test.
            InvokeToolsMenuItem(window, "PipelineMenu", "ApplyDisplayPipelineMenuItem", settle: 6000);

            InvokeToolsMenuItem(window, "PipelineMenu", "StopProcessingMenuItem");
            var first = WaitForStatus(window, text => text.StartsWith("Stop:", StringComparison.Ordinal));
            output.WriteLine($"R-11b first: {first}");
            Assert.Equal("Stop: no render is in flight.", first);

            // Twice, because the defect was a stale marker: a second press must give the same answer.
            InvokeToolsMenuItem(window, "PipelineMenu", "StopProcessingMenuItem");
            var second = WaitForStatus(window, text => text.StartsWith("Stop:", StringComparison.Ordinal));
            output.WriteLine($"R-11b second: {second}");
            Assert.Equal("Stop: no render is in flight.", second);

            Assert.DoesNotContain("discarded", LogTail(window), StringComparison.OrdinalIgnoreCase);
        });
    }

    /// <summary>
    /// Row 12: the timings shown are the ones the status bar already carries.
    ///
    /// <para>Asserted by comparing against <c>ChainStatusText</c> rather than against a shape like
    /// "contains ms": the point of row 12 is that it re-measures nothing, and only a comparison with
    /// the render's own value can show that. A second measurement would pass a shape check while
    /// disagreeing with the bar (#201).</para>
    /// </summary>
    [SkippableFact]
    public void R12_StageTiming_ShowsTheValuesTheRenderAlreadyReported()
    {
        Measure("R-12", window =>
        {
            InvokeToolsMenuItem(window, "PipelineMenu", "ApplyDisplayPipelineMenuItem", settle: 6000);

            var chainStatus = Text(window, "ChainStatusText");
            Assert.False(string.IsNullOrWhiteSpace(chainStatus), "ChainStatusText was empty after a render.");

            InvokeToolsMenuItem(window, "PipelineMenu", "StageTimingMenuItem");
            var status = WaitForStatus(window, text => text.StartsWith("Stage timing", StringComparison.Ordinal));
            output.WriteLine($"R-12 chain='{chainStatus}'");
            output.WriteLine($"R-12 status='{status}'");

            Assert.DoesNotContain("no render has run yet", status, StringComparison.Ordinal);
            Assert.Contains(chainStatus, status, StringComparison.Ordinal);
        });
    }

    // -- helpers --

    private void SkipUnlessNative() =>
        Skip.If(app.BackendMode != "Native",
            "The Mock job runs with no native DLLs (ci.yml:454-455), so a red smoke is CORRECT there. " +
            "Asserting it anyway would make 'the DLLs are absent' and 'the DLLs are broken' the same green.");

    private void Measure(string scenario, Action<Window> body)
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        var window = app.MainWindow;
        Assert.True(window is not null, "The main window was not available.");
        output.WriteLine($"{scenario} backend={app.BackendMode}");
        body(window!);
    }

    /// <summary>Opens a top-level menu and invokes a child, then waits <paramref name="settle"/> ms.</summary>
    private static void InvokeToolsMenuItem(Window window, string menuId, string itemId, int settle = 400)
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

    /// <summary>The log panel's lines. The list's automation id is <c>LogListBox</c>.</summary>
    private static string LogText(Window window)
    {
        var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));
        if (list is null) return string.Empty;
        return string.Join("\n", list.FindAllChildren().Select(item => item.Name ?? string.Empty));
    }

    /// <summary>
    /// Opens the log panel and selects its tab. Copied from ClearAlertsObservationScenarios rather than
    /// re-derived: GUI-C-139 lost a run to a re-invented version of this sequence that read 0 lines.
    /// </summary>
    private static void OpenLogs(Window window)
    {
        var view = window.FindFirstDescendant(cf => cf.ByAutomationId("ViewMenu"));
        Assert.True(view is not null, "ViewMenu is not in the tree.");
        view!.Patterns.ExpandCollapse.Pattern.Expand();
        Thread.Sleep(300);

        var toggle = window.FindFirstDescendant(cf => cf.ByAutomationId("ShowLogsPanelMenuItem"));
        Assert.True(toggle is not null, "ShowLogsPanelMenuItem is not in the tree.");
        if (toggle!.Patterns.Toggle.Pattern.ToggleState != FlaUI.Core.Definitions.ToggleState.On)
        {
            toggle.Patterns.Toggle.Pattern.Toggle();
            Thread.Sleep(200);
        }

        view.Patterns.ExpandCollapse.Pattern.Collapse();
        Thread.Sleep(200);
        window.FindFirstDescendant(cf => cf.ByName("Log"))?.AsButton().Invoke();
        Thread.Sleep(600);
    }


    private static string LogTail(Window window)
    {
        var lines = LogText(window).Split('\n');
        return string.Join("\n", lines.Skip(Math.Max(0, lines.Length - 6)));
    }
}
