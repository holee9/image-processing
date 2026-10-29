// #182 / #278 (GUI-C-254): editable calibration modes must reach a processing request.
using System;
using System.Text.Json;
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;
using static ImageProcTest.E2ETests.Scenarios.Workflows.WorkbenchObservation;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// These cases read the running app. Connected controls must be usable; unconnected controls must
/// remain disabled and truthfully marked. U01 also couples each editable mode to a fresh backend
/// result, so enabling a radio button without connecting its binding cannot satisfy the contract.
/// </summary>
[Collection(WorkflowApplicationCollection.Name)]
public sealed class UnappliedSettingsScenarios(WorkflowApplicationFixture app, ITestOutputHelper output)
{
    private static readonly string[] ConnectedStages = ["Offset", "Gain", "Defect"];
    private static readonly string[] UnconnectedStages = ["Ghost", "Temperature", "Nonlinearity", "Binning"];
    private static readonly string[] ModeValues = ["Auto", "On", "Off"];

    /// <summary>U-01: nine connected options change processing requests; twelve unsupported options stay disabled.</summary>
    [SkippableFact]
    public void U01_CalibrationStages_AreUsableExactlyWhenConnected()
    {
        var window = Ready();
        // UIA Invoke/SelectionItem/Toggle only; do not call OpenParameters (SetForeground).
        var parameters = window.FindFirstDescendant(cf => cf.ByName("Parameters"));
        Assert.NotNull(parameters);
        parameters!.AsButton().Invoke();
        Thread.Sleep(400);
        var stages = ConnectedStages.Concat(UnconnectedStages).ToArray();
        var original = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var stage in stages)
        {
            var selected = ModeValues.Where(value => Find(window, $"Calib{stage}Mode{value}").AsRadioButton().IsChecked).ToArray();
            Assert.Single(selected);
            original[stage] = selected[0];
            foreach (var value in ModeValues)
                Assert.Equal(ConnectedStages.Contains(stage), Find(window, $"Calib{stage}Mode{value}").IsEnabled);
        }

        var preprocess = Find(window, "PreprocessInChainCheckBox").AsCheckBox();
        var originalPreprocess = preprocess.IsChecked;
        try
        {
            var mark = Find(window, "CalibrationStagesUnappliedMark");
            Assert.Equal("기본 3단계 연결", mark.Name);
            Assert.False(mark.IsOffscreen);
            foreach (var term in new[] { "Offset/Gain/Defect", "Auto", "On", "Off" })
                Assert.Contains(term, mark.HelpText, StringComparison.Ordinal);
            AssertConnectedControlEnabled(window);

            if (originalPreprocess != true) preprocess.Patterns.Toggle.Pattern.Toggle();
            Assert.True(preprocess.IsChecked == true);
            foreach (var stage in ConnectedStages) SelectCalibrationMode(window, stage, "Auto");

            // A baseline render prevents an old exported result from satisfying a later edit.
            var expected = ConnectedStages.ToDictionary(stage => stage, _ => "Auto", StringComparer.Ordinal);
            RenderAndAssertCalibrationRequest(window, expected, original);
            foreach (var stage in ConnectedStages)
            {
                foreach (var value in new[] { "Off", "On", "Auto" })
                {
                    SelectCalibrationMode(window, stage, value);
                    expected[stage] = value;
                    RenderAndAssertCalibrationRequest(window, expected, original);
                }
            }
        }
        finally
        {
            foreach (var stage in ConnectedStages) SelectCalibrationMode(window, stage, original[stage]);
            if (preprocess.IsChecked != originalPreprocess) preprocess.Patterns.Toggle.Pattern.Toggle();
            // Restore the shared fixture's rendered state as well as its controls.
            var version = Viewport(window).ProcessedVersion;
            ApplyDisplayPipeline(window);
            Assert.True(WaitForProcessedVersionAbove(window, version) > version, "U01 cleanup did not render the restored settings.");
            OpenMetrics(window);
        }
    }

    private static void SelectCalibrationMode(Window window, string stage, string value)
    {
        Find(window, $"Calib{stage}Mode{value}").Patterns.SelectionItem.Pattern.Select();
        foreach (var option in ModeValues)
            Assert.Equal(option == value, Find(window, $"Calib{stage}Mode{option}").AsRadioButton().IsChecked);
    }

    private void RenderAndAssertCalibrationRequest(Window window, IReadOnlyDictionary<string, string> expected,
        IReadOnlyDictionary<string, string> original)
    {
        var before = Viewport(window);
        ApplyDisplayPipeline(window);
        Assert.True(WaitForProcessedVersionAbove(window, before.ProcessedVersion) > before.ProcessedVersion,
            "The mode edit produced no fresh processed frame.");
        Assert.Equal(before.SourceVersion, Viewport(window).SourceVersion);
        Assert.Null(StaleIndicator(window));

        var report = ExportFreshCalibrationReport(window);
        Assert.Equal(app.BackendMode, report.GetProperty("actualBackendMode").GetString());
        var calibration = report.GetProperty("calibrationEvaluation");
        var requested = calibration.GetProperty("requested");
        var display = report.GetProperty("displayPipeline");
        Assert.True(display.GetProperty("applied").GetBoolean());
        foreach (var stage in ConnectedStages)
        {
            var key = stage.ToLowerInvariant();
            Assert.Equal(expected[stage], requested.GetProperty(key).GetString());
            Assert.Equal(expected[stage], calibration.GetProperty(key).GetString());
        }
        foreach (var stage in UnconnectedStages)
        {
            var key = stage.ToLowerInvariant();
            Assert.Equal(original[stage], requested.GetProperty(key).GetString());
            Assert.Equal(original[stage], calibration.GetProperty(key).GetString());
            foreach (var value in ModeValues)
                Assert.False(Find(window, $"Calib{stage}Mode{value}").IsEnabled);
        }

        // The settings snapshot alone is insufficient: inspect what the backend consumed too.
        if (app.BackendMode == "Mock")
        {
            var summary = display.GetProperty("summary").GetString()!;
            foreach (var stage in ConnectedStages)
            {
                var disposition = expected[stage] == "Off" ? "bypassed" : "applied";
                // Auto Defect depends on whether a directory was configured (the Mock's contract).
                if (stage == "Defect" && expected[stage] == "Auto")
                    disposition = string.IsNullOrWhiteSpace(report.GetProperty("settings").GetProperty("calibDefectDir").GetString())
                        ? "bypassed" : "applied";
                Assert.Contains($"{stage}={expected[stage]}/{disposition}", summary, StringComparison.Ordinal);
            }
            output.WriteLine($"U01 backend-consumed: {summary}");
        }
        else
        {
            Assert.Equal("Native", app.BackendMode);
            var executed = calibration.GetProperty("executed");
            Assert.Contains(executed.GetProperty("preprocess").GetString(), new[] { "Applied", "AppliedNoChange" });
            var reason = executed.GetProperty("reason").GetString()!;
            var modes = string.Join(",", ConnectedStages.Select(stage => $"{stage.ToLowerInvariant()}:{expected[stage]}"));
            Assert.Contains($"requested=[{modes}]", reason, StringComparison.Ordinal);
            Assert.Contains("unsupported ghost/temp/nonlinearity/binning bypassed", reason, StringComparison.Ordinal);
            AssertNativeStagesExecutedPerMode(report, expected, reason, executed.GetProperty("hud").GetString());
            output.WriteLine($"U01 backend-consumed: {reason}");
        }
    }

    // Native stage bits of the metadata flags (GuiPreprocessRunner.RunPipelineOut): offset 0x2000, gain 0x0008, defect 0x0004. Other bits (e.g. 0x4000 bypassed) are not asserted.
    private static readonly (string Stage, string File, string DirKey, int Bit)[] NativeStages =
    [
        ("Offset", "offset.xcal", "calibOffsetDir", 0x2000),
        ("Gain", "gain.xcal", "calibGainDir", 0x0008),
        ("Defect", "defect.xcal", "calibDefectDir", 0x0004),
    ];

    /// <summary>
    /// F264-1 (Reviewer #264): GUI-PRE-GATE-009 / CalibrationStageSelection - Off never runs a stage, On requires its map, Auto runs it only when the map exists.
    /// The expected set comes from the requested mode and the map files on disk, never from the reason's own <c>requested=[...]</c> echo; it is compared with the stages the
    /// backend reports it executed, with the real stage bits of the native flags, and with the HUD text of the same render.
    /// </summary>
    private static void AssertNativeStagesExecutedPerMode(JsonElement report, IReadOnlyDictionary<string, string> expected, string reason, string? hud)
    {
        var settings = report.GetProperty("settings");
        var wanted = new List<string>();
        foreach (var (stage, file, dirKey, _) in NativeStages)
        {
            var directory = settings.GetProperty(dirKey).GetString();
            var exists = !string.IsNullOrWhiteSpace(directory) && File.Exists(System.IO.Path.Combine(directory, file));
            // The fixture must hold all three maps, otherwise On/Auto/Off would not be told apart by the outcome.
            Assert.True(exists, $"U01 fixture has no {file} in '{directory}'.");
            if (expected[stage] != "Off") wanted.Add(stage.ToLowerInvariant());
        }

        var executedMatch = System.Text.RegularExpressions.Regex.Match(reason, @"executed=\[([^\]]*)\]");
        Assert.True(executedMatch.Success, $"The reason carries no executed=[...] list: {reason}");
        var actual = executedMatch.Groups[1].Value.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        Assert.Equal(wanted.OrderBy(x => x, StringComparer.Ordinal), actual.OrderBy(x => x, StringComparer.Ordinal));

        var flagsMatch = System.Text.RegularExpressions.Regex.Match(reason, @"nativeFlags=0x([0-9a-fA-F]+)");
        Assert.True(flagsMatch.Success, $"The reason carries no nativeFlags=0x...: {reason}");
        var flags = Convert.ToInt32(flagsMatch.Groups[1].Value, 16);
        foreach (var (stage, _, _, bit) in NativeStages)
            Assert.True(((flags & bit) != 0) == (expected[stage] != "Off"),
                $"{stage}={expected[stage]}: native flag bit 0x{bit:x} is {((flags & bit) != 0 ? "set" : "clear")} (flags 0x{flags:x}); executed=[{string.Join(",", actual)}].");

        Assert.Equal($"{wanted.Count} native stages ({string.Join(" · ", wanted)})", hud);
    }

    private JsonElement ExportFreshCalibrationReport(Window window)
    {
        var priorStatus = Find(window, "StatusBarText").Name;
        UiaMenu.Open(window, "FileMenu");
        Find(window, "ExportAutomationReportMenuItem").AsMenuItem().Invoke();
        const string prefix = "Automation report exported: ";
        for (var attempt = 0; attempt < 20; attempt++)
        {
            var status = Find(window, "StatusBarText").Name;
            if (status != priorStatus && status.StartsWith(prefix, StringComparison.Ordinal))
            {
                // Read the newly named immutable snapshot, never delete or trust an old fixed alias.
                var end = status.IndexOf(" (fixed copy:", StringComparison.Ordinal);
                if (end < 0) end = status.IndexOf(". The fixed copy ", StringComparison.Ordinal);
                Assert.True(end > prefix.Length, $"The export did not name its snapshot: {status}");
                var path = status[prefix.Length..end];
                Assert.True(File.Exists(path), $"The exported snapshot is missing: {path}");
                using var document = JsonDocument.Parse(File.ReadAllText(path));
                var root = document.RootElement;
                Assert.Equal(Path.GetFileName(path), root.GetProperty("evidence").GetProperty("snapshotFile").GetString());
                output.WriteLine($"U01 fresh evidence: {path}");
                return root.Clone();
            }
            Thread.Sleep(250);
        }
        throw new InvalidOperationException("U01 did not receive a newly exported evidence snapshot.");
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

            SelectLaneBAlgorithm(window, "No grid correction");
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
