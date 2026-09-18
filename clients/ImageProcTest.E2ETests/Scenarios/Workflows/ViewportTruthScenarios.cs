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
using static ImageProcTest.E2ETests.Scenarios.Workflows.WorkbenchObservation;

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
    /// W-28 (#173, GUI-C-111): the viewport DREW the image — not merely received it.
    ///
    /// <para>W-20 above reads what the control was handed (its dependency properties). That is what
    /// #172 broke, and it is the right check for that defect. It is not the same claim as "something is
    /// on screen": a control can hold both images and still draw nothing, and this suite has no pixel
    /// capture to notice (GUI-C-72, GUI-C-78). So this case reads the render pass instead — the mode the
    /// last frame drew, the hash of the processed pixels it composed, and their mean brightness. All
    /// three are written INSIDE OnRender, so they cannot be true while the screen is blank.</para>
    ///
    /// <para>What this deliberately does NOT check: that the binding expressions name the right
    /// properties. They named the wrong ones for four months and every string-level check agreed with
    /// them — the bindings existed, the pipeline behind the names did not. A check that reads names
    /// would have agreed too.</para>
    ///
    /// <para>The mean is asserted above zero, not merely present: an all-black frame hashes to a
    /// perfectly good value, and "drew a black rectangle" is the failure this is meant to exclude.</para>
    /// </summary>
    [SkippableFact]
    public void W28_MainViewport_DrewTheImage()
    {
        Measure("W28", window =>
        {
            var help = ViewportHelp(window);
            output.WriteLine($"W28 help='{help}'");

            var mode = Regex.Match(help, @"^rendered=(?<m>[^;]+)");
            Assert.True(mode.Success, $"The viewport peer reported '{help}', which names no rendered mode.");
            Assert.True(
                mode.Groups["m"].Value.Trim() is not ("none" or ""),
                $"The last frame drew mode '{mode.Groups["m"].Value.Trim()}' — the viewport rendered its " +
                $"empty state, so nothing of the loaded image is on screen ({help}).");

            var hash = Regex.Match(help, @"processed=(?<h>[0-9a-f]{16})");
            Assert.True(hash.Success,
                $"The frame reported no processed-pixel hash, so it composed no processed layer: '{help}'.");

            var mean = Regex.Match(help, @"processedMean=(?<v>-?[0-9.]+)");
            Assert.True(mean.Success, $"The frame reported no mean brightness: '{help}'.");
            var value = double.Parse(mean.Groups["v"].Value, System.Globalization.NumberStyles.Float,
                System.Globalization.CultureInfo.InvariantCulture);

            output.WriteLine($"W28 drawn mode={mode.Groups["m"].Value.Trim()} hash={hash.Groups["h"].Value} mean={value:0.000}");
            Assert.True(value > 0.0,
                $"The frame drew a processed layer whose mean brightness is {value:0.###} — a blank frame. " +
                "Receiving an image and drawing it are different claims (#173).");
        });
    }

    /// <summary>The viewport peer's HelpText: what the last frame DREW, as opposed to what it received.</summary>
    private static string ViewportHelp(Window window)
    {
        var element = window.FindFirstDescendant(cf => cf.ByAutomationId("WorkbenchViewport"));
        Assert.True(element is not null, "WorkbenchViewport is not in the automation tree.");
        return element!.HelpText ?? string.Empty;
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

    /// <summary>
    /// W-22 (#171 ①): editing a display input without applying it marks the image stale.
    ///
    /// <para>"Stale" is asserted on two observations together: the viewport's processed image was NOT
    /// replaced (its version did not move), and the indicator says why. The preset change and the Apply
    /// are the controls — both replace the image and neither leaves the indicator up — so a version that
    /// never moves, or an indicator that is always up, fails here rather than passing.</para>
    /// </summary>
    [SkippableFact]
    public void W22_EditingVoiWithoutApplying_MarksTheImageStale()
    {
        Measure("W22", window =>
        {
            OpenParameters(window);
            var original = BodyPart(window);
            try
            {
                // Control 1: a preset change replaces the image and leaves nothing stale.
                var beforePreset = Viewport(window).ProcessedVersion;
                SelectBodyPart(window, original == "Bone" ? "Lung" : "Bone");
                var afterPreset = WaitForProcessedVersionAbove(window, beforePreset);
                var indicatorAfterPreset = StaleIndicator(window);
                output.WriteLine($"W22 preset: v{beforePreset} -> v{afterPreset} indicator='{indicatorAfterPreset}'");
                Assert.True(afterPreset > beforePreset, "A preset change did not replace the processed image; the version cannot be trusted.");
                Assert.True(indicatorAfterPreset is null, $"After a preset change the stale indicator reads '{indicatorAfterPreset}'.");

                // The edit: nothing is rendered, so the image must be marked stale.
                TypeCenter(window, "12345");
                var afterEdit = Viewport(window).ProcessedVersion;
                var indicatorAfterEdit = StaleIndicator(window);
                output.WriteLine($"W22 edit: v{afterEdit} indicator='{indicatorAfterEdit}'");
                Assert.True(afterEdit == afterPreset, $"Typing a VOI value replaced the image (v{afterPreset} -> v{afterEdit}); this case assumes apply-to-render (#171).");
                Assert.True(
                    indicatorAfterEdit is not null && indicatorAfterEdit.Contains("parameters changed", StringComparison.Ordinal),
                    $"The image was not re-rendered after a VOI edit, and the stale indicator reads '{indicatorAfterEdit ?? "(absent)"}'. " +
                    "An out-of-date image with no sign of it is HAZ-GUI-004 (#171).");

                // Control 2: applying replaces the image and clears the indicator.
                ApplyDisplayPipeline(window);
                var afterApply = WaitForProcessedVersionAbove(window, afterEdit);
                var indicatorAfterApply = StaleIndicator(window);
                output.WriteLine($"W22 apply: v{afterApply} indicator='{indicatorAfterApply}'");
                Assert.True(afterApply > afterEdit, "Applying the display pipeline did not replace the processed image.");
                Assert.True(indicatorAfterApply is null, $"After applying, the stale indicator still reads '{indicatorAfterApply}'.");
            }
            finally
            {
                SelectBodyPart(window, original);
            }
        });
    }

    /// <summary>
    /// W-24 (#171): an ordinary launch carries no fault — checked, not assumed.
    ///
    /// <para>This app was started without <c>--automation-fault</c> and has already rendered, so if the
    /// seam were constructed it would report a call count. The armed app in W-23 reads the same property
    /// and gets <c>display-pipeline-after:2 calls=N</c>, which is what makes <c>off</c> here a reading
    /// rather than an empty default.</para>
    /// </summary>
    [SkippableFact]
    public void W24_OrdinaryLaunch_HasNoFaultInjection()
    {
        Measure("W24", window =>
        {
            var status = FaultInjectionStatus(window);
            AssertWrappedBackendMatches(window, app.BackendMode, output.WriteLine, "W24");
            output.WriteLine($"W24 title='{window.Title}' status='{status}' viewport='{Viewport(window).Status}'");
            Assert.Equal("faultInjection=off", status);
            Assert.DoesNotContain("FAULT INJECTION", window.Title, StringComparison.Ordinal);
            Assert.True(Viewport(window).ProcessedVersion > 0, "Nothing has been rendered yet, so 'off' would say nothing.");
        });
    }

    /// <summary>
    /// W-25 (#171 ①②, GUI-C-80): the detached viewer tells the same truth as the main window.
    ///
    /// <para>Before #172 the detached viewer was the only place the image was visible, so a control that
    /// lives only in the main window leaves that surface uncontrolled. The main window's indicator and HUD,
    /// read in the same run, are the control: they must move while the detached ones are compared.</para>
    /// </summary>
    [SkippableFact]
    public void W25_DetachedViewer_MarksAnUnappliedEditStale()
    {
        Measure("W25", window =>
        {
            CloseDetached(window);
            OpenParameters(window);
            var original = BodyPart(window);
            var detached = OpenDetached(window);
            try
            {
                SelectBodyPart(window, original == "Bone" ? "Lung" : "Bone");
                var rendered = CenterInput(window);
                WaitForHudCenter(window, rendered);
                output.WriteLine($"W25 preset: main hud={HudCenter(window)} detached hud={DetachedHudCenter(detached) ?? "(none)"} " +
                                 $"main='{Viewport(window).Status}' detached='{DetachedViewport(detached).Status}'");
                Assert.True(DetachedViewport(detached).ProcessedVersion > 0, "The detached viewer has no processed image to judge.");
                var detachedHudAfterPreset = DetachedHudCenter(detached);

                TypeCenter(window, "12345");
                var mainIndicator = StaleIndicator(window);
                var detachedIndicator = DetachedStaleIndicator(detached);
                output.WriteLine($"W25 edit: main indicator='{mainIndicator}' detached indicator='{detachedIndicator}' " +
                                 $"detached hud={DetachedHudCenter(detached) ?? "(none)"} detached texts=[{string.Join(" | ", DetachedTexts(detached))}]");
                Assert.True(mainIndicator is not null, "Control failed: the main window shows no stale indicator after an unapplied edit.");
                Assert.True(
                    detachedIndicator is not null && detachedIndicator.Contains("parameters changed", StringComparison.Ordinal),
                    $"The main window marks the image stale, but the detached viewer showing the same image reads " +
                    $"'{detachedIndicator ?? "(absent)"}' (#171 ①).");
                Assert.True(
                    detachedHudAfterPreset == rendered,
                    $"After the preset the detached viewer named C={detachedHudAfterPreset ?? "(no HUD)"} beside an image rendered at C={rendered} (#171 ②).");
                Assert.True(DetachedHudCenter(detached) == rendered, $"After an unapplied edit the detached HUD reads C={DetachedHudCenter(detached)}, not {rendered}.");

                ApplyDisplayPipeline(window);
                WaitForHudCenter(window, "12345");
                output.WriteLine($"W25 apply: main indicator='{StaleIndicator(window)}' detached indicator='{DetachedStaleIndicator(detached)}'");
                Assert.True(DetachedStaleIndicator(detached) is null, "After applying, the detached viewer still shows a stale indicator.");
                Assert.True(DetachedHudCenter(detached) == "12345", $"After applying, the detached HUD reads C={DetachedHudCenter(detached)}.");
            }
            finally
            {
                CloseDetached(window);
                SelectBodyPart(window, original);
            }
        });
    }

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
