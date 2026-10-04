// GUI-C-230 (#251): what the legacy diagnostic app shows a user who types a bilateral sigma_space above the module's new limit (7.5). UI Automation patterns only: no key, no mouse.
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Legacy;

/// <summary>
/// The box <c>NoiseSigmaSpaceTextBox</c> (Evaluation tab, "Noise s/r") is checked when a run is STARTED (<c>ApplyNativePreview</c>): a value outside 0.1..7.5 is refused with a message in the preview text and a
/// red status line, before anything runs (GUI-C-230b, Codex #153, user decision 2026-10-04: tell the user and do not run; the first version of GUI-C-230 cut such a value to 7.5 silently and ran).
/// While the user TYPES nothing is checked, so these scenarios pin what is visible at that time, by UI Automation, in the real app: the box keeps exactly what was typed (it is never cut or rewritten,
/// which is part of the decision), no message appears, no dialog appears, and the run button stays disabled because no raw image is loaded.
///
/// <para><b>Why L01 changed (GUI-C-230b).</b> Its assertions about the box (kept as typed, nothing said while typing) are still true and still wanted: they are what lets the refusal message name the value
/// the user typed. What changed is the meaning: before, "nothing is said" was the whole behaviour, because the value was cut silently at run time; now the refusal at run time is the behaviour and the silence
/// while typing is only the absence of a premature message. The refusal itself is NOT driven here.</para>
///
/// <para><b>What this does not show, and why (measured, GUI-C-230b).</b> The refusal (value 8, press Run Selected) and the run (value 7.5) need a loaded raw image, and the app loads one only through the "Load Raw..." common
/// file dialog. That dialog opens and its file-name edit accepts the path through UI Automation (read back equal), but its Open button does nothing when invoked by UI Automation (tried: InvokePattern, LegacyIAccessible
/// DoDefaultAction, focusing the edit first, bringing the dialog to the foreground; the dialog stayed open each time, and the app then reported "failed to exit"). Pressing Enter would work but is a key press that goes to
/// whichever window is in front, which this lane does not do on a shared desktop. So the refusal is covered where it can be: the decision function and its wiring are tested in the integration project, and the run
/// button's disabled state without a raw image is asserted here.</para>
/// </summary>
public sealed class LegacyEnhanceInputLimitScenarios(ITestOutputHelper output)
{
    private const string Tab = "Evaluation";
    private const string Box = "NoiseSigmaSpaceTextBox";

    [SkippableFact]
    public void L01_ASigmaSpaceAboveTheLimit_IsKeptInTheBox_AndTheAppSaysNothing_WhileTyping()
    {
        using var app = LegacyPreprocessReadinessScenarios.LegacyApp.LaunchOrSkip(breakTemp: false);

        // the control: the reading works and the box starts with the XAML default
        var start = app.ReadValue(Tab, Box);
        var preview = app.ReadText(Tab, "NativePreviewText");
        var modes = app.ReadText(Tab, "StageModesInfoText");
        var control = app.DescribeControl(Tab, Box);
        var windows = app.TopLevelWindowCount();
        output.WriteLine($"start: box='{start}' preview='{preview}' control: {control} windows={windows}");
        Assert.Equal("3.0", start);
        Assert.Contains("help=''", control);
        Assert.Contains("status=''", control);
        Assert.Contains("enabled=False", app.DescribeControl(Tab, "ApplyNativePreviewButton"));   // no raw image loaded: Run Selected cannot start a run at all, whatever the box says

        // below the limit, at it, above it (the old limit was 100), far above it, and not a number
        foreach (var typed in new[] { "7.4", "7.5", "7.6", "50", "100", "1e6", "abc" })
        {
            app.SetValue(Tab, Box, typed);

            var shown = app.ReadValue(Tab, Box);
            output.WriteLine($"typed '{typed}': box shows '{shown}'; preview unchanged={app.ReadText(Tab, "NativePreviewText") == preview}; windows={app.TopLevelWindowCount()}");

            Assert.Equal(typed, shown);                                                       // the box keeps what was typed: it is not cut, not rewritten, not reset
            Assert.Equal(preview, app.ReadText(Tab, "NativePreviewText"));                    // no message about the value
            Assert.Equal(modes, app.ReadText(Tab, "StageModesInfoText"));
            Assert.Equal(control, app.DescribeControl(Tab, Box));                             // the control itself says nothing about a range or an error
            Assert.Equal(windows, app.TopLevelWindowCount());                                 // no dialog, no message box
            Assert.Contains("enabled=False", app.DescribeControl(Tab, "ApplyNativePreviewButton"));   // typing a value does not enable or trigger a run
        }
    }
}
