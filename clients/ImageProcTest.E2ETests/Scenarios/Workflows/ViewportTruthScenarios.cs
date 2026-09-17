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
