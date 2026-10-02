// #225 row 9 (GUI-C-196 M5): the Deterministic Baseline driven through the real menu with UI Automation patterns ONLY (Expand / Invoke / IsChecked / Name /
// HelpText). No Keyboard, no Mouse: a global input event goes to whatever window is in front, which on a developer machine has been the lead's terminal.
using System.Diagnostics;
using System.Text.RegularExpressions;
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;
using static ImageProcTest.E2ETests.Scenarios.Workflows.WorkbenchObservation;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// B-01 reads the menu item's state; B-02 invokes it on the native modules and checks what it must NOT touch: the pixels the viewport drew, the chain status, the
/// two stage switches in the Parameters tab. B-02 prints the wall time from the click to the pass line and does not assert it against the 3000 ms budget.
/// </summary>
[Collection(WorkflowApplicationCollection.Name)]
public sealed class DeterministicBaselineScenarios(WorkflowApplicationFixture app, ITestOutputHelper output)
{
    private const string ItemId = "RunDeterministicBaselineMenuItem";

    [SkippableFact]
    public void B01_TheMenuItem_IsEnabledOnlyWhereTheBackendCanRunIt()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        var window = app.MainWindow!;

        var item = FindItem(window);
        output.WriteLine($"B01 backend={app.BackendMode} enabled={item.IsEnabled} name='{item.Name}'");

        Assert.Contains("Deterministic Baseline", item.Name, StringComparison.Ordinal);
        Assert.Equal(app.BackendMode == "Native", item.IsEnabled);
    }

    [SkippableFact]
    public void B02_Invoking_OnNative_PassesAndLeavesTheScreenSettingsAndChainAlone()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        Skip.If(app.BackendMode != "Native", "The Deterministic Baseline needs the native backend; on Mock the item is disabled (B01).");
        Skip.If(app.CalibrationDirectory is null, app.CalibrationNote);
        var window = app.MainWindow!;
        CloseDetached(window);

        // A render first, so there is a drawn frame and a chain status to compare against.
        ApplyDisplayPipeline(window);
        var chainBefore = WaitForAnyChain(window);
        var hashBefore = DrawnHash(window);
        var preprocessBefore = Checked(window, "PreprocessInChainCheckBox");
        var aiBefore = Checked(window, "AiBoneSuppressionInChainCheckBox");
        output.WriteLine($"B02 before: chain='{chainBefore}' hash={hashBefore} preprocess={preprocessBefore} ai={aiBefore}");
        Assert.NotEqual("-", hashBefore);

        var wall = Stopwatch.StartNew();
        Invoke(window);
        var status = WaitForStatus(window, text => text.StartsWith("Deterministic Baseline ", StringComparison.Ordinal) && !text.Contains("Running", StringComparison.Ordinal), TimeSpan.FromSeconds(90));
        wall.Stop();
        output.WriteLine($"B02 status: '{status}'");
        output.WriteLine($"B02 MEASURED click to pass line: {wall.Elapsed.TotalMilliseconds:0} ms (budget 3000 ms, not asserted; this includes the UI Automation polling interval)");

        Assert.StartsWith("Deterministic Baseline PASS: two runs bit-identical", status, StringComparison.Ordinal);
        Assert.Contains("DICOM valid", status, StringComparison.Ordinal);

        // What the command must not touch.
        Assert.Equal(hashBefore, DrawnHash(window));
        Assert.Equal(chainBefore, ChainText(window));
        Assert.Equal(preprocessBefore, Checked(window, "PreprocessInChainCheckBox"));
        Assert.Equal(aiBefore, Checked(window, "AiBoneSuppressionInChainCheckBox"));
    }

    // ---- helpers: UI Automation patterns only -----------------------------------------------------------------------------------------------

    private static AutomationElement FindItem(Window window)
    {
        var menu = UiaMenu.Open(window, "PipelineMenu");
        AutomationElement? item = null;
        for (var attempt = 0; attempt < 20 && item is null; attempt++)
        {
            item = window.FindFirstDescendant(cf => cf.ByAutomationId(ItemId));
            if (item is null) Thread.Sleep(100);
        }

        Assert.True(item is not null, $"{ItemId} did not appear under PipelineMenu.");
        _ = menu;
        return item!;
    }

    private static void Invoke(Window window)
    {
        var item = FindItem(window);
        Assert.True(item.IsEnabled, $"{ItemId} is disabled, so it cannot be invoked.");
        item.AsMenuItem().Invoke();
        UiaMenu.Close();
    }

    private static string ChainText(Window window) =>
        window.FindFirstDescendant(cf => cf.ByAutomationId("ChainStatusText"))?.Name ?? string.Empty;

    private static string WaitForAnyChain(Window window)
    {
        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(30);
        while (DateTime.UtcNow < deadline)
        {
            var text = ChainText(window);
            if (Regex.IsMatch(text, @"\w+=\w+")) return text;
            Thread.Sleep(200);
        }

        Assert.Fail($"The chain status never showed a stage; it reads '{ChainText(window)}'.");
        return string.Empty;
    }

    private static string DrawnHash(Window window)
    {
        var help = window.FindFirstDescendant(cf => cf.ByAutomationId("WorkbenchViewport"))?.HelpText ?? string.Empty;
        var m = Regex.Match(help, @"processed=([0-9a-f]{16}|-)");
        return m.Success ? m.Groups[1].Value : "(unreadable: " + help + ")";
    }

    private static bool? Checked(Window window, string automationId)
    {
        OpenParameters(window);
        var box = window.FindFirstDescendant(cf => cf.ByAutomationId(automationId));
        Assert.True(box is not null, $"{automationId} is not in the Parameters tab.");
        return box!.AsCheckBox().IsChecked;
    }

    private static string WaitForStatus(Window window, Func<string, bool> condition, TimeSpan timeout)
    {
        var deadline = DateTime.UtcNow + timeout;
        var last = string.Empty;
        while (DateTime.UtcNow < deadline)
        {
            last = window.FindFirstDescendant(cf => cf.ByAutomationId("StatusBarText"))?.Name ?? string.Empty;
            if (condition(last)) return last;
            Thread.Sleep(100);
        }

        Assert.Fail($"The status line never reached the expected state within {timeout.TotalSeconds:0} s; it reads '{last}'.");
        return last;
    }
}
