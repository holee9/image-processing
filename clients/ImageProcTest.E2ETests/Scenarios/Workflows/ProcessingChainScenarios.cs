// #180 / #173 (GUI-C-99): the pixel chain in the running app — status, fallback and the pixels drawn.
using System.Text.RegularExpressions;
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;
using static ImageProcTest.E2ETests.Scenarios.Workflows.WorkbenchObservation;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// Switches the preprocess stage on and off and reads, each time, the chain status the app shows and the
/// hash of the processed pixels the viewport drew (<c>processed=</c> in its HelpText, written in the
/// render pass). The hash is the evidence that the chain reached the screen: a status that says
/// "Applied" while the drawn pixels are unchanged is the failure this class exists to catch.
///
/// <para>Per backend: Mock has no preprocess module, so a requested stage must show
/// <c>RequestedNotApplied</c>, the display must stay on the raw frame and the pixels must not change.
/// Native with a calibration set must show <c>Applied</c> and draw different pixels, and switching the
/// stage off must bring the first pixels back.</para>
/// </summary>
[Collection(WorkflowApplicationCollection.Name)]
public sealed class ProcessingChainScenarios(WorkflowApplicationFixture app, ITestOutputHelper output)
{
    [SkippableFact]
    public void C01_PreprocessStage_ReachesTheDrawnPixelsOrSaysWhyNot()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        var window = app.MainWindow!;
        CloseDetached(window);

        try
        {
            SetPreprocess(window, false);
            ApplyDisplayPipeline(window);
            var off = WaitForChain(window, "preprocess=NotRequested");
            var offHash = DrawnHash(window);
            output.WriteLine($"C01 off: chain='{off}' hash={offHash}");
            Assert.Contains("display input=raw", off, StringComparison.Ordinal);
            Assert.NotEqual("-", offHash);

            SetPreprocess(window, true);
            ApplyDisplayPipeline(window);

            if (app.BackendMode != "Native")
            {
                var refused = WaitForChain(window, "preprocess=RequestedNotApplied");
                var refusedHash = DrawnHash(window);
                output.WriteLine($"C01 mock on: chain='{refused}' hash={refusedHash}");
                Assert.Contains("display input=raw", refused, StringComparison.Ordinal);
                Assert.Equal(offHash, refusedHash);
                return;
            }

            Skip.If(app.CalibrationDirectory is null, app.CalibrationNote);

            var applied = WaitForChain(window, "preprocess=Applied");
            var onHash = WaitForHash(window, h => h != offHash);
            output.WriteLine($"C01 native on: chain='{applied}' hash={onHash ?? DrawnHash(window)}");
            Assert.Contains("display input=chain", applied, StringComparison.Ordinal);
            Assert.True(onHash is not null,
                $"The chain reports '{applied}', but the drawn processed pixels are unchanged ({offHash}).");

            SetPreprocess(window, false);
            ApplyDisplayPipeline(window);
            WaitForChain(window, "preprocess=NotRequested");
            var backHash = WaitForHash(window, h => h == offHash);
            output.WriteLine($"C01 native off again: hash={backHash ?? DrawnHash(window)}");
            Assert.True(backHash is not null,
                $"With the stage off again the display should start from the raw frame; drawn {DrawnHash(window)}, first {offHash}.");
        }
        finally
        {
            SetPreprocess(window, false);
            ApplyDisplayPipeline(window);
        }
    }

    /// <summary>
    /// C-02 (GUI-C-100): what changing the exposure kVp does to the drawn pixels. Measured, not assumed —
    /// the preprocess module's sources mention kVp only in the parameter-range table it answers queries
    /// from (preprocess.cpp:22); offset, gain and defect only null-check the metadata. So the pixels are
    /// expected NOT to change, and this case records that. It turning red means a stage started using the
    /// value, which is a change to report, not a failure to hide.
    /// </summary>
    [SkippableFact]
    public void C02_ChangingTheExposureKvp_DoesNotChangeTheDrawnPixelsToday()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        Skip.If(app.BackendMode != "Native", "The preprocess stage only runs on the native backend.");
        var window = app.MainWindow!;
        CloseDetached(window);
        Skip.If(app.CalibrationDirectory is null, app.CalibrationNote);

        try
        {
            SetPreprocess(window, true);
            SetNumber(window, "ExposureKvpInput", "70");
            ApplyDisplayPipeline(window);
            WaitForChain(window, "preprocess=Applied");
            var at70 = DrawnHash(window);

            SetNumber(window, "ExposureKvpInput", "120");

            // The write has to reach the view model, or the comparison below would be between two runs at
            // 70 kVp. ChainInputsDiffer lists ExposureKvp (GUI-C-99), so the image is marked stale the
            // moment the new value lands — that mark is the evidence the setting changed.
            var stale = WaitForStale(window);
            output.WriteLine($"C02 after typing 120: stale='{stale}' text='{ReadNumber(window, "ExposureKvpInput")}'");
            Assert.True(stale is not null, "Typing a new kVp did not mark the image stale, so the value never reached the chain inputs.");
            Assert.Equal("120", ReadNumber(window, "ExposureKvpInput"));

            ApplyDisplayPipeline(window);
            WaitForChain(window, "preprocess=Applied");
            var at120 = DrawnHash(window);

            output.WriteLine($"C02 kVp 70 -> {at70}, kVp 120 -> {at120}");
            Assert.Equal(16, at70.Length);
            Assert.Equal(at70, at120);
        }
        finally
        {
            SetNumber(window, "ExposureKvpInput", "70");
            SetPreprocess(window, false);
            ApplyDisplayPipeline(window);
        }
    }

    private static string ReadNumber(Window window, string automationId) =>
        window.FindFirstDescendant(cf => cf.ByAutomationId(automationId))?.AsTextBox().Text ?? "(missing)";

    private static string? WaitForStale(Window window)
    {
        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(5);
        while (DateTime.UtcNow < deadline)
        {
            var reason = StaleIndicator(window);
            if (reason is not null) return reason;
            Thread.Sleep(100);
        }

        return null;
    }

    private static void SetNumber(Window window, string automationId, string value)
    {
        OpenParameters(window);
        var box = window.FindFirstDescendant(cf => cf.ByAutomationId(automationId));
        Assert.True(box is not null, $"{automationId} is not in the Parameters tab.");
        var input = box!.AsTextBox();
        input.Focus();
        input.Text = value;
        FlaUI.Core.Input.Keyboard.Press(FlaUI.Core.WindowsAPI.VirtualKeyShort.TAB);
        Thread.Sleep(400);
    }

    private static void SetPreprocess(Window window, bool on)
    {
        OpenParameters(window);
        var box = window.FindFirstDescendant(cf => cf.ByAutomationId("PreprocessInChainCheckBox"));
        Assert.True(box is not null, "PreprocessInChainCheckBox is not in the Parameters tab.");
        box!.AsCheckBox().IsChecked = on;
        Thread.Sleep(200);
    }

    private static string ChainText(Window window) =>
        window.FindFirstDescendant(cf => cf.ByAutomationId("ChainStatusText"))?.Name ?? string.Empty;

    private static string WaitForChain(Window window, string expected)
    {
        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(15);
        while (DateTime.UtcNow < deadline)
        {
            var text = ChainText(window);
            if (text.Contains(expected, StringComparison.Ordinal)) return text;
            Thread.Sleep(200);
        }

        Assert.Fail($"The chain status never showed '{expected}'; it reads '{ChainText(window)}'.");
        return string.Empty;
    }

    private static string DrawnHash(Window window)
    {
        var help = window.FindFirstDescendant(cf => cf.ByAutomationId("WorkbenchViewport"))?.HelpText ?? string.Empty;
        var m = Regex.Match(help, @"processed=([0-9a-f]{16}|-)");
        return m.Success ? m.Groups[1].Value : "(unreadable: " + help + ")";
    }

    private static string? WaitForHash(Window window, Func<string, bool> condition)
    {
        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(15);
        while (DateTime.UtcNow < deadline)
        {
            var hash = DrawnHash(window);
            if (hash.Length == 16 && condition(hash)) return hash;
            Thread.Sleep(200);
        }

        return null;
    }
}
