// #198 / #194 (GUI-C-129): the clamp alert reaches the screen when the gui runs gain correction.
using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading;
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// The last of the alerts #198 names: <c>gain_correct.cpp</c> pushes one alert per frame, carrying the
/// count, when pixels fall outside the gain polynomial's fitted dose range (#194). Measured end to end
/// here — the gui already runs gain correction, so with a polynomial calibration in place the alert
/// travels the whole way: native queue → <c>DrainNativeAlerts</c> → <c>RaiseAlert</c> → the log line
/// GUI-C-125 made visible.
///
/// <para><b>The arrangement, and why it is legitimate.</b> The generator writes the polynomial as
/// <c>gain_poly.xcal</c>, while the gui loads <c>gain.xcal</c> by name. The fixture copies the former
/// over the latter — the gui loads whatever <c>gain.xcal</c> holds, and what it holds here is a
/// polynomial. No product code is changed to make the measurement possible.</para>
///
/// <para><b>Out-of-range pixels are required.</b> A frame entirely inside the fitted range raises
/// nothing, which is a correct outcome rather than a failure. The integration-level measurement
/// (GainPolyClampAlertTests) pins the alert itself with a frame built for it; this case measures the
/// journey to the screen, and skips with a reason rather than asserting if this frame happens to stay
/// inside the range.</para>
/// </summary>
public sealed class ClampAlertOnScreenScenarios(ITestOutputHelper output)
{
    private const string ClampMarker = "fell outside the gain polynomial";

    [SkippableFact]
    public void TheClampAlert_ReachesTheLog()
    {
        using var app = new PolynomialCalibrationApplicationFixture();
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");

        // The alert comes from the native gain stage; the Mock backend has no such stage, so under Mock
        // this measures nothing. Skipping says "not measured" rather than failing on a build where the
        // path does not exist.
        Skip.If(app.BackendMode != "Native", "The clamp alert is pushed by the native gain stage.");
        Skip.If(app.PolynomialCalibrationDirectory is null, app.PolynomialNote);

        var window = app.MainWindow!;
        output.WriteLine($"calibration: {app.PolynomialNote}");

        RunPreprocessing(window);
        OpenLogs(window);

        var lines = LogLines(window);
        output.WriteLine($"log lines: {lines.Length}");
        foreach (var l in lines.Take(12)) output.WriteLine($"  {l}");

        var preprocess = lines.FirstOrDefault(t => t.Contains("Preprocess", StringComparison.Ordinal));
        output.WriteLine($"preprocess line: {preprocess ?? "(none)"}");

        var clamp = lines.FirstOrDefault(t => t.Contains(ClampMarker, StringComparison.Ordinal));
        Skip.If(clamp is null && preprocess is not null && preprocess.Contains("skipped", StringComparison.OrdinalIgnoreCase),
            $"Preprocessing did not run: {preprocess}");
        Skip.If(clamp is null && lines.Any(t => t.Contains("no pixel", StringComparison.OrdinalIgnoreCase)),
            "No pixel fell outside the fitted range on this frame, so the alert is correctly silent.");

        Assert.True(clamp is not null,
            "The gain stage ran with a polynomial calibration and no clamp alert reached the log (#198/#194). " +
            $"Lines carrying ALERT: {lines.Count(t => t.Contains("ALERT", StringComparison.Ordinal))}.");

        output.WriteLine($"clamp line: {clamp}");

        // It arrives as an alert (GUI-C-125) from the native queue (GUI-C-126), carrying the count.
        Assert.Contains("ALERT", clamp!, StringComparison.Ordinal);
        Assert.Contains("NATIVE_ALERT", clamp!, StringComparison.Ordinal);
        Assert.Matches(@"\d+ pixel\(s\) fell outside", clamp!);
    }

    private static void RunPreprocessing(Window window)
    {
        window.SetForeground();
        Thread.Sleep(200);
        var item = window.FindFirstDescendant(cf => cf.ByAutomationId("RunPreprocessingMenuItem"));
        if (item is null)
        {
            var menu = window.FindFirstDescendant(cf => cf.ByAutomationId("PipelineMenu"));
            Assert.True(menu is not null, "PipelineMenu is not in the tree.");
            menu!.Patterns.ExpandCollapse.Pattern.Expand();
            Thread.Sleep(350);
            item = window.FindFirstDescendant(cf => cf.ByAutomationId("RunPreprocessingMenuItem"));
        }

        Assert.True(item is not null, "RunPreprocessingMenuItem is not in the tree.");
        item!.AsMenuItem().Invoke();
        Thread.Sleep(3000);
    }

    private static string[] LogLines(Window window)
    {
        var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));
        return list is null ? [] : list.FindAllChildren().Select(i => i.Name ?? string.Empty).ToArray();
    }

    private static void OpenLogs(Window window)
    {
        var view = window.FindFirstDescendant(cf => cf.ByAutomationId("ViewMenu"));
        Assert.True(view is not null, "ViewMenu is not in the tree.");
        view!.Patterns.ExpandCollapse.Pattern.Expand();
        Thread.Sleep(300);

        var toggle = window.FindFirstDescendant(cf => cf.ByAutomationId("ShowLogsPanelMenuItem"));
        Assert.True(toggle is not null, "ShowLogsPanelMenuItem is not in the tree.");
        if (toggle!.Patterns.Toggle.Pattern.ToggleState != FlaUI.Core.Definitions.ToggleState.On)
        {
            toggle.Patterns.Toggle.Pattern.Toggle();
            Thread.Sleep(200);
        }

        view.Patterns.ExpandCollapse.Pattern.Collapse();
        Thread.Sleep(200);
        window.FindFirstDescendant(cf => cf.ByName("Log"))?.AsButton().Invoke();
        Thread.Sleep(600);
    }
}
