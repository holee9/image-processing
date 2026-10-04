// GUI-C-230 (#251): what the legacy diagnostic app shows a user who types a bilateral sigma_space above the module's new limit (7.5). UI Automation patterns only: no key, no mouse.
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Legacy;

/// <summary>
/// The box <c>NoiseSigmaSpaceTextBox</c> (Evaluation tab, "Noise s/r") is read when the stage RUNS, through <c>ReadFloat(…, min, max)</c>, which cuts a number to its limits and replaces a non-number with the default
/// (<c>EnhanceBasicInputLimits.ParseClamped</c>, tested in the integration project). Nothing is read, cut or reported while the user types. So, for a value above the limit, what a user SEES at input time is:
/// the box keeps exactly what was typed, the status lines do not change, no dialog appears, and the control carries no hint about its range. These scenarios pin that, by UI Automation, in the real app.
///
/// <para><b>What this does not show.</b> The run itself. Running the stage needs a target raw image, and the app loads one only through its Calibration Setup dialog (file and folder pickers), which UI Automation
/// patterns cannot drive. The value the stage then uses (the typed number cut to 7.5) is shown by the integration tests on the same parser; where it appears on screen after a run (the stage's detail line
/// <c>sigmaSpace=…</c>, from the value that was passed on) is read from the service's code, not observed here.</para>
///
/// <para><b>If a hint is added later</b> (a range in the tool-tip, the box rewritten to the cut value, a message), the assertions here that say "nothing" fail on purpose: they record today's behaviour so a change is a
/// decision, not an accident.</para>
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
        }
    }
}
