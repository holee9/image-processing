// #182 (GUI-C-95): settings that reach no processing are shown disabled and marked, in the running app.
using System;
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;
using static ImageProcTest.E2ETests.Scenarios.Workflows.WorkbenchObservation;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// The source survey (<c>SettingsProcessingConnectionTests</c>) reads <c>IsEnabled="False"</c> in XAML.
/// These cases read the automation tree of the running app, which is what the user gets: the controls
/// are disabled, the "미적용" mark is on screen, and its tooltip names #182. Each case also reads a
/// connected control in the same panel, so a whole panel that stopped responding cannot pass.
/// </summary>
[Collection(WorkflowApplicationCollection.Name)]
public sealed class UnappliedSettingsScenarios(WorkflowApplicationFixture app, ITestOutputHelper output)
{
    private static readonly string[] Stages = ["Offset", "Gain", "Defect", "Ghost", "Temperature", "Nonlinearity", "Binning"];

    /// <summary>U-01: all 21 calibration-stage options are disabled, one per stage still shows the stored value.</summary>
    [SkippableFact]
    public void U01_CalibrationStages_AreDisabledAndMarked()
    {
        var window = Ready();
        OpenParameters(window);
        try
        {
            foreach (var stage in Stages)
            {
                var checkedCount = 0;
                foreach (var value in new[] { "Auto", "On", "Off" })
                {
                    var id = $"Calib{stage}Mode{value}";
                    var radio = Find(window, id).AsRadioButton();
                    output.WriteLine($"U01 {id} enabled={radio.IsEnabled} checked={radio.IsChecked}");
                    Assert.False(radio.IsEnabled, $"{id} is enabled, but no processing reads the {stage} mode (#182).");
                    if (radio.IsChecked) checkedCount++;
                }

                Assert.True(checkedCount == 1, $"{stage}: {checkedCount} options are checked; the stored value must still be shown.");
            }

            AssertMark(window, "CalibrationStagesUnappliedMark");
            AssertConnectedControlEnabled(window);
        }
        finally
        {
            OpenMetrics(window);
        }
    }

    /// <summary>
    /// U-02: the Candidate's de-noise k is usable exactly while it can reach a pixel, and marked when
    /// it cannot.
    ///
    /// <para><b>This case used to assert something else, and was right to.</b> Until GUI-C-117 there
    /// were two Lane B overrides, both permanently disabled, because nothing read either one (#182).
    /// The sharpening sigma is gone — the chain has no stage for it to mean, so a disabled input
    /// claiming one was the <c>Production v1.2</c> defect wearing a different label (#193). The
    /// de-noise k now overrides the virtual grid's <c>vg_denoise_k</c>, and L-05 measures it moving the
    /// Candidate's drawn pixels.</para>
    ///
    /// <para><b>Why both directions.</b> GuiGsvgRunner sends the key only inside the virtual grid, so
    /// "enabled" and "disabled" are both correct answers depending on the Candidate's algorithm. A case
    /// asserting only the disabled half would pass on an input that is disabled always — which is what
    /// this card removed.</para>
    /// </summary>
    [SkippableFact]
    public void U02_LaneBDenoiseK_IsUsableExactlyWhenItReachesPixels()
    {
        var window = Ready();
        OpenParameters(window);
        try
        {
            Assert.True(
                window.FindFirstDescendant(cf => cf.ByAutomationId("LaneBSharpeningSigmaInput")) is null,
                "The Lane B sharpening input is back; no chain stage reads it (#193).");

            SelectLaneBAlgorithm(window, "No correction");
            var offInput = Find(window, "LaneBGsvgDenoiseKInput");
            output.WriteLine($"U02 no-correction: enabled={offInput.IsEnabled} text='{offInput.AsTextBox().Text}'");
            Assert.False(offInput.IsEnabled,
                "The de-noise k is editable while the Candidate runs no grid correction, where the key is not sent at all (#173).");
            Assert.False(string.IsNullOrWhiteSpace(offInput.AsTextBox().Text),
                "The disabled de-noise input no longer shows the stored value.");
            AssertUnappliedMark(window);

            SelectLaneBAlgorithm(window, "Virtual grid");
            var onInput = Find(window, "LaneBGsvgDenoiseKInput");
            output.WriteLine($"U02 virtual-grid: enabled={onInput.IsEnabled}");
            Assert.True(onInput.IsEnabled,
                "The de-noise k is disabled while the Candidate runs the virtual grid, which is the one place it reaches pixels (#173).");
            Assert.True(
                window.FindFirstDescendant(cf => cf.ByAutomationId("LaneBOverridesUnappliedMark")) is null,
                "The 'unapplied' mark is shown while the value does reach the drawn pixels.");

            AssertConnectedControlEnabled(window);
        }
        finally
        {
            OpenMetrics(window);
        }
    }

    /// <summary>
    /// U-03: the Lane A / Lane B algorithm pickers are usable and carry no "unapplied" mark.
    ///
    /// <para>This case used to assert the opposite, and was right to: until GUI-C-114 the choice reached
    /// no processing, so the screen had to say so — a disabled picker and a mark. The choice now selects
    /// a chain configuration and L-04 measures it moving the drawn pixels, so the same contract ("the
    /// screen tells the truth about whether a setting is connected") now requires the reverse. The mark
    /// is gone from the app, and this case fails if it comes back while the setting still works.</para>
    /// </summary>
    [SkippableFact]
    public void U03_AlgorithmPickers_AreUsableAndUnmarked()
    {
        var window = Ready();
        foreach (var lane in new[] { "A", "B" })
        {
            var picker = Find(window, $"Lane{lane}AlgorithmPicker").AsComboBox();
            var shown = picker.SelectedItem?.Text;
            output.WriteLine($"U03 lane {lane} enabled={picker.IsEnabled} selected='{shown}'");
            Assert.True(picker.IsEnabled,
                $"The Lane {lane} algorithm picker is disabled, but the choice now selects the chain configuration (#173).");
            Assert.False(string.IsNullOrWhiteSpace(shown), $"The Lane {lane} picker no longer shows the stored choice.");
            Assert.True(
                window.FindFirstDescendant(cf => cf.ByAutomationId($"Lane{lane}AlgorithmUnappliedMark")) is null,
                $"Lane {lane} still carries an 'unapplied' mark for a setting that now reaches the drawn pixels.");
        }
    }

    /// <summary>
    /// U-04 (GUI-C-98 판정): the Focus toggle is disabled and marked. Measured in GUI-C-98: with the default
    /// LeftPanelOpen/RightPanelOpen the toggle changed nothing on screen, because the Slice 8 rails are not
    /// built. The case also reads that both side panels are on screen, so a mark on a hidden layout cannot pass.
    /// </summary>
    [SkippableFact]
    public void U04_FocusToggle_IsDisabledAndMarked()
    {
        var window = Ready();
        var toggle = Find(window, "FocusModeToggle");
        output.WriteLine($"U04 FocusModeToggle enabled={toggle.IsEnabled} help='{toggle.HelpText}'");
        Assert.False(toggle.IsEnabled, "The Focus toggle is enabled, but focus mode changes nothing on screen (#182).");
        Assert.Contains("Slice 8", toggle.HelpText, StringComparison.Ordinal);

        AssertMark(window, "FocusModeUnappliedMark");

        foreach (var panel in new[] { "AnalysisPanel", "StudyQueue" })
        {
            var e = Find(window, panel);
            Assert.False(e.IsOffscreen, $"{panel} is not on screen.");
        }
    }

    /// <summary>
    /// U-05 (GUI-C-101): the exposure kVp carries a "pending connection" mark naming the issue. It is NOT
    /// disabled — the value does reach processing — but GUI-C-100 measured that the preprocess stages
    /// ignore it, so the screen says so until the virtual grid uses it.
    /// </summary>
    [SkippableFact]
    public void U05_PendingConnectionMark_IsShown()
    {
        var window = Ready();
        OpenParameters(window);
        try
        {
            var mark = Find(window, "ExposureKvpPendingMark");
            output.WriteLine($"U05 mark='{mark.Name}' offscreen={mark.IsOffscreen} help='{mark.HelpText}'");
            Assert.Contains("#180", mark.Name, StringComparison.Ordinal);
            Assert.False(mark.IsOffscreen, "The pending-connection mark is in the tree but not on screen.");
            Assert.Contains("#180", mark.HelpText, StringComparison.Ordinal);

            var input = Find(window, "ExposureKvpInput");
            Assert.True(input.IsEnabled, "The kVp input is disabled; a pending-connection setting stays editable.");
        }
        finally
        {
            OpenMetrics(window);
        }
    }

    /// <summary>The "미적용" mark for the Candidate's de-noise k, which names #173 rather than #182:
    /// the value IS connected, and what the mark reports is that this preset does not carry it.</summary>
    private void AssertUnappliedMark(Window window)
    {
        var mark = Find(window, "LaneBOverridesUnappliedMark");
        output.WriteLine($"U02 mark name='{mark.Name}' offscreen={mark.IsOffscreen} help='{mark.HelpText}'");
        Assert.Equal("미적용", mark.Name);
        Assert.False(mark.IsOffscreen, "The mark is in the tree but not on screen.");
        Assert.Contains("#173", mark.HelpText, StringComparison.Ordinal);
    }

    private static void SelectLaneBAlgorithm(Window window, string option)
    {
        Find(window, "LaneBAlgorithmPicker").AsComboBox().Select(option);
        Thread.Sleep(200);
    }

    private void AssertMark(Window window, string id)
    {
        var mark = Find(window, id);
        output.WriteLine($"{id} name='{mark.Name}' offscreen={mark.IsOffscreen} help='{mark.HelpText}'");
        Assert.Equal("미적용", mark.Name);
        Assert.False(mark.IsOffscreen, $"{id} is in the tree but not on screen.");
        Assert.Contains("#182", mark.HelpText, StringComparison.Ordinal);
    }

    /// <summary>Control: a connected setting in the same panel stays editable.</summary>
    private void AssertConnectedControlEnabled(Window window)
    {
        var center = Find(window, "VoiWindowCenterInput");
        output.WriteLine($"control VoiWindowCenterInput enabled={center.IsEnabled}");
        Assert.True(center.IsEnabled, "VoiWindowCenterInput is disabled too — the check would pass on a dead panel.");
    }

    private static AutomationElement Find(Window window, string automationId)
    {
        var element = window.FindFirstDescendant(cf => cf.ByAutomationId(automationId));
        Assert.True(element is not null, $"{automationId} is not in the automation tree.");
        return element!;
    }

    private static void OpenMetrics(Window window)
    {
        window.FindFirstDescendant(cf => cf.ByName("Metrics"))?.AsButton().Invoke();
        Thread.Sleep(300);
    }

    private Window Ready()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        return app.MainWindow!;
    }
}
