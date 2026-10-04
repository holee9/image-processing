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
/// while typing is only the absence of a premature message. The refusal itself is driven in L02 below.</para>
///
/// <para><b>L02 (GUI-C-230c)</b> observes the refusal and the run in the real window. The raw image is opened through the "Load Raw..." common file dialog by UI Automation only: the button is invoked, the
/// dialog's file-name edit gets the path through its ValuePattern, and the dialog's Open button receives <c>BM_CLICK</c> addressed to its own window handle. UIA's Invoke on that button did nothing (GUI-C-230b measured it
/// five ways), but a message to one named window works and cannot reach whichever window is in front. No product code was changed to make this possible.</para>
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

    // Everything the user can read on the Evaluation tab EXCEPT the two lines a refusal is allowed to change (the preview text and the status line).
    private static readonly string[] OtherReadouts =
    [
        "ActiveContextSummaryText", "ActiveContextDetailsText", "WorkflowBeforeAfterText", "StageModesInfoText", "RawPreviewInfoText", "RawPreviewTitleText",
    ];

    [SkippableFact]
    public void L02_WithARawLoaded_AValueOutsideTheLimitsIsRefused_NothingChanges_AndThenTheLimitItselfRuns()
    {
        using var app = LegacyPreprocessReadinessScenarios.LegacyApp.LaunchOrSkip(breakTemp: false);
        var dir = Path.Combine(Path.GetTempPath(), "xpe_c230c_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        var rawPath = Path.Combine(dir, "sample.raw");
        WriteSyntheticRaw(rawPath);
        try
        {
            // The raw image is opened the way a user opens it ("Load Raw..."), by UI Automation plus one BM_CLICK addressed to the dialog's Open button: no key, no mouse.
            app.LoadRawThroughDialog(rawPath);
            Assert.Equal("Raw preview: sample.raw", app.WaitForTextOnTab(Tab, "RawPreviewTitleText", t => t.Contains("sample.raw", StringComparison.Ordinal), TimeSpan.FromSeconds(30)));
            app.ToggleCheckBox(Tab, "NoiseEnabledCheckBox");
            Assert.True(app.IsChecked(Tab, "NoiseEnabledCheckBox"));

            var before = OtherReadouts.ToDictionary(id => id, id => app.ReadText(Tab, id));
            var previewBefore = app.ReadText(Tab, "NativePreviewText");
            output.WriteLine("before: preview='" + previewBefore + "'");

            // (a) outside the limits: refused with a message and a red status; the image and every other readout stay as they were
            foreach (var typed in new[] { "8", "7.6", "100", "0.05", "0" })
            {
                app.SetValue(Tab, Box, typed);
                app.ClickButtonByIdWhenEnabled(Tab, "ApplyNativePreviewButton", TimeSpan.FromSeconds(10));
                var preview = app.WaitForTextOnTab(Tab, "NativePreviewText", t => t.Contains("sigma_space must be", StringComparison.Ordinal), TimeSpan.FromSeconds(20));
                var status = app.ReadText(Tab, "StatusText");
                output.WriteLine($"typed '{typed}': preview='{preview}' status='{status}'");

                Assert.Equal($"Native preview: Noise sigma_space must be between 0.1 and 7.5 (got {typed}). Nothing was run; change the value and run again.", preview);
                Assert.Equal("Status: Input out of range", status);
                Assert.DoesNotContain("basic-noise", preview);
                Assert.DoesNotContain("metrics=", preview);
                Assert.Equal(typed, app.ReadValue(Tab, Box));                                   // the box keeps what the user typed
                foreach (var (id, text) in before)
                {
                    Assert.Equal(text, app.ReadText(Tab, id));                                   // no result, no viewer text, no state line changed
                }

                Assert.Contains("enabled=True", app.DescribeControl(Tab, "ApplyNativePreviewButton"));
            }

            // (b) the limit itself is a valid value: it runs, and the readouts change
            app.SetValue(Tab, Box, "7.5");
            app.ClickButtonByIdWhenEnabled(Tab, "ApplyNativePreviewButton", TimeSpan.FromSeconds(10));
            var ran = app.WaitForTextOnTab(Tab, "NativePreviewText", t => t.Contains("basic-noise=", StringComparison.Ordinal), TimeSpan.FromSeconds(90));
            var after = app.ReadText(Tab, "StatusText");
            output.WriteLine($"typed '7.5': preview='{ran}' status='{after}'");

            Assert.Contains("basic-noise=OK", ran);
            Assert.Contains("metrics=", ran);
            Assert.DoesNotContain("sigma_space must be", ran);
            Assert.Equal("Status: native pre/post preview complete", after);
            Assert.NotEqual(previewBefore, ran);
        }
        finally
        {
            try { Directory.Delete(dir, true); } catch (IOException) { /* the app may still hold the file for a moment; it is a temp folder */ }
        }
    }

    private static void WriteSyntheticRaw(string path)
    {
        // 1024 x 1024 uint16 (one of the sizes the app infers from the file length): a ramp plus noise, so the bilateral filter has something to change.
        var bytes = new byte[1024 * 1024 * 2];
        var rng = new Random(7);
        for (var i = 0; i < bytes.Length; i += 2)
        {
            var v = (ushort)(2000 + (i / 2 % 1024) * 2 + rng.Next(0, 200));
            bytes[i] = (byte)(v & 0xFF);
            bytes[i + 1] = (byte)(v >> 8);
        }

        File.WriteAllBytes(path, bytes);
    }
}
