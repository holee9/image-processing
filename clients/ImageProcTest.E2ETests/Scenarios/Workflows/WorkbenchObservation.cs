// #172 / #171 (GUI-C-79): observation and driving helpers for the workbench viewport, shared by the
// scenarios that read what it received.
using System.Text.RegularExpressions;
using FlaUI.Core.AutomationElements;
using FlaUI.Core.Input;
using FlaUI.Core.WindowsAPI;
using Xunit;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// Reads the workbench viewport through its automation peer (<c>source=WxH vN; processed=WxH vM</c>),
/// the stale indicator, the HUD and the main window's fault-injection status, and drives the Analysis
/// panel and Pipeline menu.
/// </summary>
internal static class WorkbenchObservation
{
    /// <summary>The stale indicator's text, or null when it is not shown.</summary>
    internal static string? StaleIndicator(Window window)
    {
        var element = window.FindFirstDescendant(cf => cf.ByAutomationId("PreviewStaleIndicator"));
        if (element is null || element.IsOffscreen)
        {
            return null;
        }

        var text = element.Name;
        return string.IsNullOrEmpty(text) ? null : text;
    }

    internal static int WaitForProcessedVersionAbove(Window window, int version)
    {
        var last = Viewport(window).ProcessedVersion;
        for (var i = 0; i < 20 && last <= version; i++)
        {
            Thread.Sleep(250);
            last = Viewport(window).ProcessedVersion;
        }

        return last;
    }

    // ---- Analysis panel / menu helpers ----------------------------------------------------------

    internal static void OpenParameters(Window window)
    {
        window.SetForeground();
        window.FindFirstDescendant(cf => cf.ByName("Parameters"))!.AsButton().Invoke();
        Thread.Sleep(400);
    }

    internal static string BodyPart(Window window) =>
        window.FindFirstDescendant(cf => cf.ByAutomationId("BodyPartSelector"))!.AsComboBox().SelectedItem?.Text ?? "Abdomen";

    internal static void SelectBodyPart(Window window, string bodyPart)
    {
        var combo = window.FindFirstDescendant(cf => cf.ByAutomationId("BodyPartSelector"))!.AsComboBox();
        if (combo.SelectedItem?.Text == bodyPart)
        {
            return;
        }

        combo.Select(bodyPart);
        Thread.Sleep(1200);
    }

    internal static string CenterInput(Window window) =>
        window.FindFirstDescendant(cf => cf.ByAutomationId("VoiWindowCenterInput"))!.AsTextBox().Text;

    internal static void TypeCenter(Window window, string value)
    {
        var input = window.FindFirstDescendant(cf => cf.ByAutomationId("VoiWindowCenterInput"))!.AsTextBox();
        input.Focus();
        input.Text = value;
        Keyboard.Press(VirtualKeyShort.TAB);
        Thread.Sleep(1200);
    }

    internal static void ApplyDisplayPipeline(Window window)
    {
        window.SetForeground();
        Keyboard.Press(VirtualKeyShort.ESCAPE);
        Thread.Sleep(120);
        window.FindFirstDescendant(cf => cf.ByAutomationId("PipelineMenu"))!.AsMenuItem().Click();
        Thread.Sleep(350);
        window.FindFirstDescendant(cf => cf.ByAutomationId("ApplyDisplayPipelineMenuItem"))!.AsMenuItem().Invoke();
        Thread.Sleep(1200);
    }

    /// <summary>The centre the HUD currently names ("C  40000  · W  30000" → "40000").</summary>
    internal static string HudCenter(Window window)
    {
        var hud = window.FindFirstDescendant(cf => cf.ByAutomationId("HudVoiWindow"));
        Assert.True(hud is not null, "HudVoiWindow is not in the automation tree.");
        var match = Regex.Match(hud!.Name, @"^C\s+(\S+)\s+·");
        Assert.True(match.Success, $"The HUD reads '{hud.Name}', which does not name a centre.");
        return match.Groups[1].Value;
    }

    internal static string WaitForHudCenter(Window window, string expected)
    {
        var last = HudCenter(window);
        for (var i = 0; i < 20 && last != expected; i++)
        {
            Thread.Sleep(250);
            last = HudCenter(window);
        }

        return last;
    }

    // ---- observation helpers ------------------------------------------------------------------

    internal sealed record ViewportState(string Status, string Source, int SourceVersion, string Processed, int ProcessedVersion);

    internal static readonly Regex StatusPattern = new(
        @"^source=(?<s>\S+) v(?<sv>\d+); processed=(?<p>\S+) v(?<pv>\d+)$", RegexOptions.CultureInvariant);

    internal static ViewportState Viewport(Window window)
    {
        var element = window.FindFirstDescendant(cf => cf.ByAutomationId("WorkbenchViewport"));
        Assert.True(element is not null, "WorkbenchViewport is not in the automation tree, so nothing about the image can be said.");

        var status = element!.Properties.ItemStatus.ValueOrDefault ?? string.Empty;
        var match = StatusPattern.Match(status);
        Assert.True(match.Success, $"WorkbenchViewport reported '{status}', which is not the expected status format.");

        return new ViewportState(
            status,
            match.Groups["s"].Value,
            int.Parse(match.Groups["sv"].Value, System.Globalization.CultureInfo.InvariantCulture),
            match.Groups["p"].Value,
            int.Parse(match.Groups["pv"].Value, System.Globalization.CultureInfo.InvariantCulture));
    }

    /// <summary>The loaded frame's size as the app itself reports it ("RAW 1024x1024, …").</summary>
    internal static string LoadedSize(Window window)
    {
        var summary = TextElements(window).FirstOrDefault(t => t.StartsWith("RAW ", StringComparison.Ordinal));
        Assert.True(summary is not null, "No 'RAW WxH' summary is on screen, so there is no loaded image to compare with.");
        var match = Regex.Match(summary!, @"^RAW (\d+x\d+)");
        Assert.True(match.Success, $"The summary '{summary}' does not name a size.");
        return match.Groups[1].Value;
    }

    internal static IEnumerable<string> TextElements(Window window) =>
        window.FindAllDescendants(cf => cf.ByControlType(FlaUI.Core.Definitions.ControlType.Text))
            .Select(t => { try { return t.Name; } catch { return string.Empty; } });

    /// <summary>The main window's fault-injection status (<c>faultInjection=off</c> unless armed).</summary>
    internal static string FaultInjectionStatus(Window window) =>
        window.Properties.ItemStatus.ValueOrDefault ?? string.Empty;
}
