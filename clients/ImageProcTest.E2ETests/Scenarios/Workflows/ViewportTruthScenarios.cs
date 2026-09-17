// #172 / #171 (GUI-C-79): what the main viewport actually shows, and whether the labels and the stale
// indicator around it tell the truth about that.
using System.Diagnostics;
using System.Text.RegularExpressions;
using FlaUI.Core.AutomationElements;
using FlaUI.Core.Input;
using FlaUI.Core.WindowsAPI;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// The workbench viewport, read through its own automation peer.
///
/// <para><b>Why the peer.</b> From 54a3ae7 until #172 the main viewport was bound to two properties
/// nothing assigned, so it received no images, and no check noticed: W-01 read the shell's presence,
/// the automation report wrote a constant, the view-model comparison looked at the view model, the
/// Rendering suite built the control by hand, and pixel capture is blind in this harness (GUI-C-72,
/// GUI-C-78). The peer reports the dependency properties the renderer itself reads, so it is the one
/// observation point that cannot be about something else. Its status reads
/// <c>source=WxH vN; processed=WxH vM</c>, where the version counts replacements.</para>
///
/// <para>Each scenario below maps to one control from SHA-GUI-001 HAZ-GUI-004 as decided in #171, and
/// is falsified on its own: turning one control off turns only its scenario red.</para>
/// </summary>
[Collection(WorkflowApplicationCollection.Name)]
public sealed class ViewportTruthScenarios(WorkflowApplicationFixture app, ITestOutputHelper output)
{
    /// <summary>
    /// W-20 (#172): the main viewport receives the loaded image, on both sides.
    ///
    /// <para>Sizes are compared with the loaded frame's own summary rather than a constant, so the case
    /// measures "the image that was loaded reached the viewport", not "some image of this size".</para>
    /// </summary>
    [SkippableFact]
    public void W20_MainViewport_ReceivesTheLoadedImage()
    {
        Measure("W20", window =>
        {
            var loaded = LoadedSize(window);
            var received = Viewport(window);
            output.WriteLine($"W20 loaded={loaded} viewport='{received.Status}'");

            Assert.True(
                received.Source == loaded,
                $"The main viewport's source image is '{received.Source}', not the loaded {loaded}. " +
                "It was bound to a property nothing assigns once already (#172).");
            Assert.True(
                received.Processed == loaded,
                $"The main viewport's processed image is '{received.Processed}', not the loaded {loaded} (#172).");
        });
    }

    /// <summary>
    /// W-21 (#171 ②): the HUD beside the image names the window that PRODUCED it.
    ///
    /// <para>GUI-C-77 measured the old HUD reading <c>C 12345</c> beside an image rendered at
    /// <c>C 40000</c>, because it was bound to the current settings. Here a VOI edit that has not been
    /// applied must leave the HUD on the rendered value, and applying it must move the HUD. The body-part
    /// change first is the control: it re-runs the pipeline, so the HUD must follow it.</para>
    /// </summary>
    [SkippableFact]
    public void W21_Hud_NamesTheWindowThatProducedTheImage()
    {
        Measure("W21", window =>
        {
            OpenParameters(window);
            var original = BodyPart(window);
            try
            {
                // Control: a preset change re-renders, so the HUD must show the preset's centre.
                SelectBodyPart(window, original == "Bone" ? "Lung" : "Bone");
                var presetCenter = CenterInput(window);
                var afterPreset = WaitForHudCenter(window, presetCenter);
                output.WriteLine($"W21 preset: input={presetCenter} hud={afterPreset}");
                Assert.True(
                    afterPreset == presetCenter,
                    $"After a preset change the HUD reads C={afterPreset}, not the rendered {presetCenter}; " +
                    "the case cannot tell a label that follows renders from one that does not.");

                // The edit alone renders nothing, so the HUD must keep the rendered value.
                TypeCenter(window, "12345");
                var afterEdit = HudCenter(window);
                output.WriteLine($"W21 edit: input=12345 hud={afterEdit}");
                Assert.True(
                    afterEdit == presetCenter,
                    $"After typing C=12345 without applying, the HUD reads C={afterEdit}. The image was rendered " +
                    $"at C={presetCenter}, so the label beside it is naming a window it was not drawn with (#171).");

                // Applying renders, so the HUD must now move.
                ApplyDisplayPipeline(window);
                var afterApply = WaitForHudCenter(window, "12345");
                output.WriteLine($"W21 apply: hud={afterApply}");
                Assert.True(afterApply == "12345", $"After applying, the HUD reads C={afterApply}, not 12345.");
            }
            finally
            {
                SelectBodyPart(window, original);
            }
        });
    }

    // ---- Analysis panel / menu helpers ----------------------------------------------------------

    private static void OpenParameters(Window window)
    {
        window.SetForeground();
        window.FindFirstDescendant(cf => cf.ByName("Parameters"))!.AsButton().Invoke();
        Thread.Sleep(400);
    }

    private static string BodyPart(Window window) =>
        window.FindFirstDescendant(cf => cf.ByAutomationId("BodyPartSelector"))!.AsComboBox().SelectedItem?.Text ?? "Abdomen";

    private static void SelectBodyPart(Window window, string bodyPart)
    {
        var combo = window.FindFirstDescendant(cf => cf.ByAutomationId("BodyPartSelector"))!.AsComboBox();
        if (combo.SelectedItem?.Text == bodyPart)
        {
            return;
        }

        combo.Select(bodyPart);
        Thread.Sleep(1200);
    }

    private static string CenterInput(Window window) =>
        window.FindFirstDescendant(cf => cf.ByAutomationId("VoiWindowCenterInput"))!.AsTextBox().Text;

    private static void TypeCenter(Window window, string value)
    {
        var input = window.FindFirstDescendant(cf => cf.ByAutomationId("VoiWindowCenterInput"))!.AsTextBox();
        input.Focus();
        input.Text = value;
        Keyboard.Press(VirtualKeyShort.TAB);
        Thread.Sleep(1200);
    }

    private static void ApplyDisplayPipeline(Window window)
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
    private static string HudCenter(Window window)
    {
        var hud = window.FindFirstDescendant(cf => cf.ByAutomationId("HudVoiWindow"));
        Assert.True(hud is not null, "HudVoiWindow is not in the automation tree.");
        var match = Regex.Match(hud!.Name, @"^C\s+(\S+)\s+·");
        Assert.True(match.Success, $"The HUD reads '{hud.Name}', which does not name a centre.");
        return match.Groups[1].Value;
    }

    private static string WaitForHudCenter(Window window, string expected)
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

    private sealed record ViewportState(string Status, string Source, int SourceVersion, string Processed, int ProcessedVersion);

    private static readonly Regex StatusPattern = new(
        @"^source=(?<s>\S+) v(?<sv>\d+); processed=(?<p>\S+) v(?<pv>\d+)$", RegexOptions.CultureInvariant);

    private static ViewportState Viewport(Window window)
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
    private static string LoadedSize(Window window)
    {
        var summary = TextElements(window).FirstOrDefault(t => t.StartsWith("RAW ", StringComparison.Ordinal));
        Assert.True(summary is not null, "No 'RAW WxH' summary is on screen, so there is no loaded image to compare with.");
        var match = Regex.Match(summary!, @"^RAW (\d+x\d+)");
        Assert.True(match.Success, $"The summary '{summary}' does not name a size.");
        return match.Groups[1].Value;
    }

    private static IEnumerable<string> TextElements(Window window) =>
        window.FindAllDescendants(cf => cf.ByControlType(FlaUI.Core.Definitions.ControlType.Text))
            .Select(t => { try { return t.Name; } catch { return string.Empty; } });

    private void Measure(string scenario, Action<Window> body)
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");

        if (!string.IsNullOrEmpty(app.ReacquiredNote))
        {
            output.WriteLine($"{scenario} fixture: {app.ReacquiredNote}");
        }

        var stopwatch = Stopwatch.StartNew();
        try
        {
            body(app.MainWindow!);
        }
        finally
        {
            output.WriteLine($"{scenario}: {stopwatch.ElapsedMilliseconds} ms");
        }
    }
}
