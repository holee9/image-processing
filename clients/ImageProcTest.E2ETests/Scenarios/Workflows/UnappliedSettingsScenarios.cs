// #182 (GUI-C-95): settings that reach no processing are shown disabled and marked, in the running app.
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

    /// <summary>U-02: the Lane B override inputs and their reset button are disabled and marked.</summary>
    [SkippableFact]
    public void U02_LaneBOverrides_AreDisabledAndMarked()
    {
        var window = Ready();
        OpenParameters(window);
        try
        {
            foreach (var id in new[] { "LaneBSharpeningSigmaInput", "LaneBDenoiseStrengthInput", "LaneBResetDefaultsButton" })
            {
                var element = Find(window, id);
                output.WriteLine($"U02 {id} enabled={element.IsEnabled}");
                Assert.False(element.IsEnabled, $"{id} is enabled, but no processing reads the Lane B overrides (#182).");
            }

            var sigma = Find(window, "LaneBSharpeningSigmaInput").AsTextBox().Text;
            output.WriteLine($"U02 sigma text='{sigma}'");
            Assert.False(string.IsNullOrWhiteSpace(sigma), "The disabled sigma input no longer shows the stored value.");

            AssertMark(window, "LaneBOverridesUnappliedMark");
            AssertConnectedControlEnabled(window);
        }
        finally
        {
            OpenMetrics(window);
        }
    }

    /// <summary>U-03: the Lane A / Lane B algorithm pickers are disabled and marked; the chosen name is still shown.</summary>
    [SkippableFact]
    public void U03_AlgorithmPickers_AreDisabledAndMarked()
    {
        var window = Ready();
        foreach (var lane in new[] { "A", "B" })
        {
            var picker = Find(window, $"Lane{lane}AlgorithmPicker").AsComboBox();
            var shown = picker.SelectedItem?.Text;
            output.WriteLine($"U03 lane {lane} enabled={picker.IsEnabled} selected='{shown}'");
            Assert.False(picker.IsEnabled, $"The Lane {lane} algorithm picker is enabled, but no processing reads the choice (#182).");
            Assert.False(string.IsNullOrWhiteSpace(shown), $"The Lane {lane} picker no longer shows the stored choice.");
            AssertMark(window, $"Lane{lane}AlgorithmUnappliedMark");
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
