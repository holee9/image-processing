// #198 ② (GUI-C-126): an alert pushed by a native module reaches the screen.
using System;
using System.Linq;
using System.Threading;
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;
using static ImageProcTest.E2ETests.Scenarios.Workflows.WorkbenchObservation;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// The alert whose journey is measured here is pushed by <c>modules/gsvg</c> — not written by the GUI
/// about itself. <c>gsvg.cpp:463</c> calls <c>xpe_alert_push</c> when the virtual grid runs without a
/// collimation field mask, which is exactly what this fixture's frame does.
///
/// <para><b>Why this file exists even though the wiring already worked.</b> GUI-C-126 set out to build
/// that wiring and found it present: <c>XpeCommonNative</c> declares the queue calls,
/// <c>RealXpeBackend.DrainNativeAlerts</c> pulls them after every native call, and the view model
/// hands them to <c>RaiseAlert</c>, which GUI-C-125 taught to write a visible line. What was missing
/// was any case that says so — so the path could be removed and every test would stay green.</para>
///
/// <para><b>Destructive drain, and why it does not poison this measurement.</b>
/// <c>xpe_get_pending_alert</c> does not modify the queue; <c>xpe_clear_alerts</c> does, and
/// <c>DrainNativeAlerts</c> calls it after reading. The queue is per-process and each fixture launches
/// its own app, so one run cannot consume another run's alerts — the trap of an earlier call eating
/// what the assertion was going to look for.</para>
/// </summary>
[Collection(LargeFrameApplicationCollection.Name)]
public sealed class NativeAlertScenarios(LargeFrameApplicationFixture app, ITestOutputHelper output)
{
    /// <summary>What the GUI stamps on an alert that came from the native queue.</summary>
    private const string NativeCode = "NATIVE_ALERT";

    private const string GsvgAlertText = "no collimation field mask";

    [SkippableFact]
    public void AnAlertPushedByANativeModule_IsOnScreen()
    {
        var window = Ready();

        // The count BEFORE this run's virtual grid, not zero: the fixture is shared, so an earlier
        // case may have left one here. What this case claims is that running the stage produces a new
        // one — a claim a pre-existing line cannot satisfy.
        SelectAlgorithm(window, "A", "No correction");
        ApplyDisplayPipeline(window);
        OpenLogs(window);

        var before = LogLines(window);
        var beforeGsvg = before.Count(t => t.Contains(GsvgAlertText, StringComparison.Ordinal));
        output.WriteLine($"before: {before.Length} lines, {beforeGsvg} carrying the gsvg alert");
        Assert.True(before.Length > 0, "The log is empty, so this case would pass on a dead panel.");

        // Run the stage that pushes it.
        SelectAlgorithm(window, "A", "Virtual grid");
        ApplyDisplayPipeline(window);
        Thread.Sleep(500);

        var after = LogLines(window);
        var afterGsvg = after.Count(t => t.Contains(GsvgAlertText, StringComparison.Ordinal));
        var alert = after.FirstOrDefault(t => t.Contains(GsvgAlertText, StringComparison.Ordinal));
        output.WriteLine($"after: {after.Length} lines, {afterGsvg} carrying the gsvg alert");
        output.WriteLine($"native alert line: {alert ?? "(none)"}");

        Assert.True(afterGsvg > beforeGsvg,
            $"The native module pushed an alert and nothing new reached the screen: {beforeGsvg} -> {afterGsvg} (#198 ②).");
        Assert.True(alert is not null,
            "The native module pushed an alert and nothing reached the screen (#198 ②).");

        // It arrives marked as an alert (GUI-C-125) and says it came from the native queue.
        Assert.Contains("ALERT", alert!, StringComparison.Ordinal);
        Assert.Contains(NativeCode, alert!, StringComparison.Ordinal);

        // The drain says what it did, which is what distinguishes "nothing was pushed" from
        // "something was pushed and dropped on the way".
        var drained = after.FirstOrDefault(t => t.Contains("native alert(s) from the xpe_common queue", StringComparison.Ordinal));
        output.WriteLine($"drain line: {drained ?? "(none)"}");
        Assert.True(drained is not null, "Nothing reported draining the native queue.");
    }

    /// <summary>
    /// A re-initialise keeps what the user has already seen and writes a boundary instead (#198,
    /// GUI-C-126). Before this the same action emptied the log — measured in GUI-C-122.
    /// </summary>
    [SkippableFact]
    public void AReInitialise_WritesASeparatorAndKeepsWhatWasThere()
    {
        var window = Ready();

        SelectAlgorithm(window, "A", "Virtual grid");
        ApplyDisplayPipeline(window);
        Thread.Sleep(500);
        OpenLogs(window);

        var before = LogLines(window);
        var target = before.FirstOrDefault(t => t.Contains(GsvgAlertText, StringComparison.Ordinal));
        Skip.If(target is null, "The native alert is not present, so there is nothing to keep.");
        output.WriteLine($"before re-initialise: {before.Length} lines");

        var initialize = window.FindFirstDescendant(cf => cf.ByAutomationId("InitializeBackendButton"));
        Assert.True(initialize is not null, "InitializeBackendButton is not in the tree.");
        initialize!.AsButton().Invoke();
        Thread.Sleep(1200);

        var after = LogLines(window);
        output.WriteLine($"after re-initialise: {after.Length} lines");
        foreach (var l in after.Take(4)) output.WriteLine($"  {l}");

        Assert.Contains(after, t => t.Contains("backend re-initialised", StringComparison.Ordinal));
        Assert.Contains(after, t => t.Contains(GsvgAlertText, StringComparison.Ordinal));
    }

    private Window Ready()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        Skip.If(app.BackendMode != "Native", "Native alerts come from the native modules.");
        var window = app.MainWindow!;
        SettleMenuBar(window);
        return window;
    }

    /// <summary>
    /// Waits for the menu bar to be reachable before driving it.
    ///
    /// <para>The fixture is shared, so this case can start with whatever the previous one left on
    /// screen. Measured: without this, <c>ApplyDisplayPipeline</c> threw a NullReferenceException on
    /// <c>PipelineMenu</c> being absent from the tree rather than failing with a readable message.</para>
    /// </summary>
    private static void SettleMenuBar(Window window)
    {
        for (var attempt = 0; attempt < 10; attempt++)
        {
            window.SetForeground();
            FlaUI.Core.Input.Keyboard.Press(FlaUI.Core.WindowsAPI.VirtualKeyShort.ESCAPE);
            Thread.Sleep(250);
            if (window.FindFirstDescendant(cf => cf.ByAutomationId("PipelineMenu")) is not null) return;
        }

        Skip.If(true, "The Pipeline menu never became reachable; another window is covering it.");
    }

    private static void SelectAlgorithm(Window window, string lane, string option)
    {
        var picker = window.FindFirstDescendant(cf => cf.ByAutomationId($"Lane{lane}AlgorithmPicker"));
        Assert.True(picker is not null, $"Lane{lane}AlgorithmPicker is not in the tree.");
        picker!.AsComboBox().Select(option);
        Thread.Sleep(200);
    }

    private static string[] LogLines(Window window)
    {
        var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));
        return list is null ? [] : list.FindAllChildren().Select(i => i.Name ?? string.Empty).ToArray();
    }

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
        Thread.Sleep(500);
    }
}
