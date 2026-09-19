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

    // ---- lane readers ---------------------------------------------------------------------------

    /// <summary>One lane's drawn frame: the size it received, and the hash and mean it composed.</summary>
    internal sealed record LaneState(string Size, string Hash, double Mean);

    /// <summary>
    /// Read from that lane's own viewport peer, never from a setting or a binding name (GUI-C-112).
    /// </summary>
    internal static LaneState ReadLane(Window window, string lane)
    {
        var id = $"Lane{lane}Viewport";
        var element = window.FindFirstDescendant(cf => cf.ByAutomationId(id));
        Assert.True(element is not null, $"{id} is not in the automation tree, so this lane draws nothing.");

        var status = element!.Properties.ItemStatus.ValueOrDefault ?? string.Empty;
        var help = element.HelpText ?? string.Empty;

        var size = Regex.Match(status, @"source=(?<v>\S+) v");
        var hash = Regex.Match(help, @"processed=(?<v>[0-9a-f]{16})");
        var mean = Regex.Match(help, @"processedMean=(?<v>-?[0-9.]+)");
        Assert.True(hash.Success && mean.Success,
            $"{id} reported no drawn pixels: status='{status}' help='{help}'.");

        return new LaneState(
            size.Success ? size.Groups["v"].Value : "(none)",
            hash.Groups["v"].Value,
            double.Parse(mean.Groups["v"].Value, System.Globalization.NumberStyles.Float,
                System.Globalization.CultureInfo.InvariantCulture));
    }

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

    internal static void TypeWidth(Window window, string value)
    {
        var input = window.FindFirstDescendant(cf => cf.ByAutomationId("VoiWindowWidthInput"))!.AsTextBox();
        input.Focus();
        input.Text = value;
        Keyboard.Press(VirtualKeyShort.TAB);
        Thread.Sleep(1200);
    }

    internal static void ApplyDisplayPipeline(Window window)
    {
        // Symptom workaround (GUI-C-80, cause measured in GUI-C-81): the detached viewer is an owned window,
        // so it stays above the main window even after SetForeground. Where it covers the Pipeline menu,
        // FromPoint at the menu centre returns the viewer and the click never reaches the menu (2 runs:
        // covered → not found, moved away → found, moved back → not found). Later attempts therefore open
        // the menu through the expand pattern, which needs no pointer.
        AutomationElement? item = null;
        for (var attempt = 0; attempt < 3 && item is null; attempt++)
        {
            window.SetForeground();
            Keyboard.Press(VirtualKeyShort.ESCAPE);
            Thread.Sleep(120);
            var menu = window.FindFirstDescendant(cf => cf.ByAutomationId("PipelineMenu"))!.AsMenuItem();
            if (attempt == 0) menu.Click(); else menu.Expand();   // Expand needs no pointer, so an overlapping window cannot take it
            Thread.Sleep(350);
            item = window.FindFirstDescendant(cf => cf.ByAutomationId("ApplyDisplayPipelineMenuItem"));
        }

        Assert.True(item is not null, "The Apply Display Pipeline menu item did not appear after three attempts.");
        item!.AsMenuItem().Invoke();
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

    // ---- detached comparison viewer (#171, GUI-C-80) --------------------------------------------

    internal const string DetachedTitle = "ImageProcTest Comparison Viewer";

    internal static AutomationElement OpenDetached(Window window)
    {
        // GUI-C-82: the first click on the View menu of a freshly launched app did not always open it
        // (measured once in E-01c), so the menu is re-opened, as in ApplyDisplayPipeline.
        AutomationElement? item = null;
        for (var attempt = 0; attempt < 3 && item is null; attempt++)
        {
            window.SetForeground();
            Keyboard.Press(VirtualKeyShort.ESCAPE);
            Thread.Sleep(120);
            var menu = window.FindFirstDescendant(cf => cf.ByAutomationId("ViewMenu"))!.AsMenuItem();
            if (attempt == 0) menu.Click(); else menu.Expand();
            Thread.Sleep(300);
            item = window.FindFirstDescendant(cf => cf.ByAutomationId("DetachComparisonViewerMenuItem"));
        }

        Assert.True(item is not null, "The Detach Comparison Viewer menu item did not appear after three attempts.");
        item!.AsMenuItem().Invoke();

        for (var i = 0; i < 20; i++)
        {
            Thread.Sleep(250);
            if (FindDetached(window) is { } found) return found;
        }

        throw new InvalidOperationException($"No '{DetachedTitle}' window appeared within 5 s (#166).");
    }

    /// <summary>
    /// The detached viewer, matched on the END of its title: #175 puts <c>[MOCK] </c> in front of it
    /// whenever the backend is Mock, so an exact-name lookup misses it in every Mock run.
    /// </summary>
    internal static AutomationElement? FindDetached(Window window) =>
        window.FindAllDescendants(cf => cf.ByControlType(FlaUI.Core.Definitions.ControlType.Window))
            .FirstOrDefault(w => { try { return (w.Name ?? string.Empty).EndsWith(DetachedTitle, StringComparison.Ordinal); } catch { return false; } });

    /// <summary>
    /// The main window's Mock banner text, or null when it is not shown. The banner's Border has no
    /// automation peer (GUI-C-82 measured it absent from the tree), so the text inside it is what is read;
    /// a collapsed Border takes its text out of the tree with it.
    /// </summary>
    internal static string? MockBannerText(Window window)
    {
        var text = window.FindFirstDescendant(cf => cf.ByAutomationId("MockBackendBannerText"));
        if (text is null || text.IsOffscreen) return null;
        var name = text.Name;
        return string.IsNullOrEmpty(name) ? null : name;
    }

    /// <summary>The detached viewer's Mock banner text, or null when none is shown.</summary>
    internal static string? DetachedMockBannerText(AutomationElement detached) =>
        DetachedTexts(detached).FirstOrDefault(t => t.StartsWith("MOCK BACKEND", StringComparison.Ordinal));

    internal static void CloseDetached(Window window)
    {
        if (FindDetached(window) is not { } detached) return;

        try { detached.AsWindow().Close(); }
        catch (Exception) { /* the next assertion reports a window that will not close */ }

        Thread.Sleep(400);
    }

    /// <summary>The detached window's viewport, read through the same peer as the main one.</summary>
    internal static ViewportState DetachedViewport(AutomationElement detached)
    {
        var element = detached.FindFirstDescendant(cf => cf.ByControlType(FlaUI.Core.Definitions.ControlType.Image));
        Assert.True(element is not null, "The detached window has no viewport in the automation tree.");
        var status = element!.Properties.ItemStatus.ValueOrDefault ?? string.Empty;
        var match = StatusPattern.Match(status);
        Assert.True(match.Success, $"The detached viewport reported '{status}'.");
        return new ViewportState(
            status,
            match.Groups["s"].Value,
            int.Parse(match.Groups["sv"].Value, System.Globalization.CultureInfo.InvariantCulture),
            match.Groups["p"].Value,
            int.Parse(match.Groups["pv"].Value, System.Globalization.CultureInfo.InvariantCulture));
    }

    /// <summary>Every text the detached window shows — searched, not looked up by id, so an absent label reads as absent.</summary>
    internal static string[] DetachedTexts(AutomationElement detached) =>
        detached.FindAllDescendants(cf => cf.ByControlType(FlaUI.Core.Definitions.ControlType.Text))
            .Where(t => !t.IsOffscreen)
            .Select(t => { try { return t.Name; } catch { return string.Empty; } })
            .Where(t => !string.IsNullOrEmpty(t))
            .ToArray();

    /// <summary>The detached window's stale text, or null when none is shown.</summary>
    internal static string? DetachedStaleIndicator(AutomationElement detached) =>
        DetachedTexts(detached).FirstOrDefault(t => t.StartsWith("STALE", StringComparison.Ordinal));

    /// <summary>The centre the detached window's HUD names, or null when it shows no HUD.</summary>
    internal static string? DetachedHudCenter(AutomationElement detached)
    {
        foreach (var text in DetachedTexts(detached))
        {
            var match = Regex.Match(text, @"^C\s+(\S+)\s+·");
            if (match.Success) return match.Groups[1].Value;
        }

        return null;
    }

    /// <summary>The status-bar runtime line (<c>mode=… | common=… | display=… | src=…</c>).</summary>
    internal static string RuntimeSummary(Window window) =>
        window.FindFirstDescendant(cf => cf.ByAutomationId("RuntimeCommonVersionText"))?.Name ?? string.Empty;

    /// <summary>
    /// GUI-C-81: under Native the fault seam must wrap the NATIVE backend. The factory falls back to Mock
    /// silently, and <c>mode=</c> is only the requested setting, so the proof is <c>src=</c> — reported by
    /// the backend through the wrapper, and absent for Mock.
    /// </summary>
    internal static void AssertWrappedBackendMatches(Window window, string? requestedMode, Action<string> log, string scenario)
    {
        var summary = RuntimeSummary(window);
        log($"{scenario} runtime: requested={requestedMode} status-bar='{summary}'");
        if (string.Equals(requestedMode, "Native", StringComparison.Ordinal))
        {
            Assert.True(
                summary.Contains("src=", StringComparison.Ordinal),
                $"Native was requested but the app reports '{summary}' — no native source, so the seam wraps a fallback backend.");
        }
    }

    /// <summary>The main window's fault-injection status (<c>faultInjection=off</c> unless armed).</summary>
    internal static string FaultInjectionStatus(Window window) =>
        window.Properties.ItemStatus.ValueOrDefault ?? string.Empty;
}
