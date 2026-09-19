// #173 (GUI-C-113): the evaluation workbench — two lanes over ONE original, each drawn with its own
// settings. Written BEFORE the feature exists, so these are red until it does.
using System;
using System.Globalization;
using System.Threading;
using System.Text.RegularExpressions;
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;
using static ImageProcTest.E2ETests.Scenarios.Workflows.WorkbenchObservation;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// What must be true once the workbench draws two lanes.
///
/// <para><b>Why these three.</b> The workbench evaluates SETTINGS, not images: both lanes take the same
/// original and differ only in how it was processed. So the claims are (1) the original reaches both,
/// (2) a setting reaches exactly one, and (3) only the Candidate can be stale. The middle one carries
/// the weight — two lanes that accidentally share a pipeline still pass (1) and (3), and #172 was
/// exactly a binding whose pipeline was not there.</para>
///
/// <para><b>Why the drawn pixels.</b> Every assertion below reads the hash each viewport composed inside
/// its own render pass, never a setting or a binding name. GUI-C-112 measured what happens otherwise:
/// keys compared only with themselves pass while the renderer is consistently wrong, and a
/// settings-bound read says the click landed, not that anything was drawn.</para>
///
/// <para><b>Not self-comparison.</b> Case L-02 compares two lanes against EACH OTHER under a change
/// applied to one of them — the shape GUI-C-112 arrived at with V-06.</para>
/// </summary>
[Collection(WorkflowApplicationCollection.Name)]
public sealed class TwoLaneWorkbenchScenarios(WorkflowApplicationFixture app, ITestOutputHelper output)
{
    /// <summary>
    /// L-01: both lanes draw the SAME original. With no Candidate override in force they compose
    /// identical pixels, and both report the size of the frame that was loaded.
    /// </summary>
    [SkippableFact]
    public void L01_BothLanes_DrawTheSameOriginal()
    {
        Measure("L-01", window =>
        {
            MakeLanesIdentical(window);
            ApplyDisplayPipeline(window);

            var loaded = LoadedSize(window);
            var a = Lane(window, "A");
            var b = Lane(window, "B");
            output.WriteLine($"L-01 loaded={loaded} A={a} B={b}");

            Assert.Equal(loaded, a.Size);
            Assert.Equal(loaded, b.Size);
            Assert.Equal(a.Hash, b.Hash);
            Assert.True(a.Mean > 0.0, $"Lane A drew a blank frame (mean {a.Mean:0.###}).");
        });
    }

    /// <summary>
    /// L-02 — the case this feature exists for: a setting changed on the Candidate moves the Candidate
    /// and leaves the Reference where it was.
    ///
    /// <para>Both halves are asserted. Without "B changed" the case passes when the override never
    /// arrives; without "A unchanged" it passes when the two lanes are one pipeline wearing two
    /// labels.</para>
    /// </summary>
    [SkippableFact]
    public void L02_ACandidateSetting_MovesOnlyTheCandidate()
    {
        Measure("L-02", window =>
        {
            MakeLanesIdentical(window);
            ApplyDisplayPipeline(window);
            var beforeA = Lane(window, "A");
            var beforeB = Lane(window, "B");

            SetCandidateOverride(window, "900");
            ApplyDisplayPipeline(window);
            var afterA = Lane(window, "A");
            var afterB = Lane(window, "B");

            output.WriteLine($"L-02 A: {beforeA.Hash} -> {afterA.Hash} (mean {beforeA.Mean:0.###} -> {afterA.Mean:0.###})");
            output.WriteLine($"L-02 B: {beforeB.Hash} -> {afterB.Hash} (mean {beforeB.Mean:0.###} -> {afterB.Mean:0.###})");

            Assert.True(afterB.Hash != beforeB.Hash,
                $"The Candidate override did not reach Lane B's drawn pixels: still {afterB.Hash}.");
            Assert.True(afterA.Hash == beforeA.Hash,
                $"Lane A moved with a change made to Lane B ({beforeA.Hash} -> {afterA.Hash}) — " +
                "the two lanes are one pipeline, so nothing here compares two settings.");
        });
    }

    /// <summary>
    /// L-03: an unapplied Candidate edit marks the Candidate stale and leaves the Reference alone.
    /// The Reference being stale at the same time would say the edit reached it too.
    /// </summary>
    [SkippableFact]
    public void L03_OnlyTheCandidate_GoesStale()
    {
        Measure("L-03", window =>
        {
            MakeLanesIdentical(window);
            ApplyDisplayPipeline(window);
            Assert.False(LaneIsStale(window, "B"), "Lane B is stale before anything was edited.");

            SetCandidateOverride(window, "700");   // edited, NOT applied
            output.WriteLine($"L-03 after edit: A stale={LaneIsStale(window, "A")}, B stale={LaneIsStale(window, "B")}");

            Assert.True(LaneIsStale(window, "B"), "An unapplied Candidate edit left Lane B looking current.");
            Assert.False(LaneIsStale(window, "A"), "The Reference went stale for an edit made to the Candidate.");
        });
    }

    /// <summary>
    /// L-04 (#173, GUI-C-114): choosing a different ALGORITHM for the Candidate moves the Candidate and
    /// leaves the Reference where it was.
    ///
    /// <para>Same shape as L-02 and for the same reason — both halves are asserted, because "B changed"
    /// alone passes when the two lanes are one pipeline and "A unchanged" alone passes when the choice
    /// never arrives. What differs is the size of the axis: L-02 varies one display parameter, this
    /// varies which processing runs at all, and that is what the workbench exists to compare.</para>
    ///
    /// <para>Native only. The algorithm presets differ in their chain stages, and the Mock backend
    /// refuses both stages by design ("Preprocessing requires the native backend", "Grid correction
    /// requires the native backend"), so under Mock every preset would draw the same pixels and this
    /// case would pass while measuring nothing.</para>
    /// </summary>
    [SkippableFact]
    public void L04_ACandidateAlgorithm_MovesOnlyTheCandidate()
    {
        MeasureNative("L-04", window =>
        {
            MakeLanesIdentical(window);
            ApplyDisplayPipeline(window);
            var beforeA = Lane(window, "A");
            var beforeB = Lane(window, "B");
            output.WriteLine($"L-04 same algorithm: A={beforeA.Hash} B={beforeB.Hash}");
            Assert.Equal(beforeA.Hash, beforeB.Hash);   // same algorithm, so the lanes must agree first

            SelectAlgorithm(window, "B", CandidateAlgorithm);
            ApplyDisplayPipeline(window);
            var afterA = Lane(window, "A");
            var afterB = Lane(window, "B");

            output.WriteLine($"L-04 A: {beforeA.Hash} -> {afterA.Hash}");
            output.WriteLine($"L-04 B: {beforeB.Hash} -> {afterB.Hash}");

            Assert.True(afterB.Hash != beforeB.Hash,
                $"Choosing '{CandidateAlgorithm}' for the Candidate did not reach its drawn pixels: still {afterB.Hash}.");
            Assert.True(afterA.Hash == beforeA.Hash,
                $"The Reference moved with an algorithm chosen for the Candidate ({beforeA.Hash} -> {afterA.Hash}) — " +
                "the two lanes run one pipeline, so nothing here compares two algorithms.");
        });
    }

    /// <summary>An option that must differ from the Reference's in what it actually runs.</summary>
    private const string CandidateAlgorithm = "Virtual grid";

    private static void SelectAlgorithm(Window window, string lane, string option)
    {
        var id = $"Lane{lane}AlgorithmPicker";
        var picker = window.FindFirstDescendant(cf => cf.ByAutomationId(id));
        Assert.True(picker is not null, $"{id} is not in the tree.");
        Assert.True(picker!.IsEnabled,
            $"{id} is disabled, so the algorithm cannot be chosen — the picker is still a label (#182).");
        picker.AsComboBox().Select(option);
        Thread.Sleep(150);
    }

    private void MeasureNative(string scenario, Action<Window> body)
    {
        Skip.If(app.BackendMode != "Native",
            "The algorithm presets differ in chain stages, which the Mock backend refuses by design.");
        Measure(scenario, body);
    }

    private void Measure(string scenario, Action<Window> body)
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");

        if (!string.IsNullOrEmpty(app.ReacquiredNote))
        {
            output.WriteLine($"{scenario} fixture: {app.ReacquiredNote}");
        }

        var stopwatch = System.Diagnostics.Stopwatch.StartNew();
        try
        {
            body(app.MainWindow!);
        }
        finally
        {
            output.WriteLine($"{scenario}: {stopwatch.ElapsedMilliseconds} ms");
        }
    }

    // ---- observation -----------------------------------------------------------------------------

    private sealed record LaneState(string Size, string Hash, double Mean);

    /// <summary>
    /// One lane's drawn frame, read from that lane's own viewport peer — the size it received and the
    /// hash and mean of the pixels it composed.
    /// </summary>
    private static LaneState Lane(Window window, string lane)
    {
        var id = $"Lane{lane}Viewport";
        var element = window.FindFirstDescendant(cf => cf.ByAutomationId(id));
        Assert.True(element is not null, $"{id} is not in the automation tree, so this lane draws nothing.");

        var status = element!.Properties.ItemStatus.ValueOrDefault ?? string.Empty;
        var help = element.HelpText ?? string.Empty;

        var size = Regex.Match(status, @"source=(?<v>\S+) v");
        var hash = Regex.Match(help, @"processed=(?<v>[0-9a-f]{16})");
        var mean = Regex.Match(help, @"processedMean=(?<v>-?[0-9.]+)");
        Assert.True(hash.Success && mean.Success,
            $"{id} reported no drawn pixels: status='{status}' help='{help}'.");

        return new LaneState(
            size.Success ? size.Groups["v"].Value : "(none)",
            hash.Groups["v"].Value,
            double.Parse(mean.Groups["v"].Value, NumberStyles.Float, CultureInfo.InvariantCulture));
    }

    private static bool LaneIsStale(Window window, string lane) =>
        window.FindFirstDescendant(cf => cf.ByAutomationId($"Lane{lane}StaleMark")) is not null;

    private static void SetCandidateOverride(Window window, string width)
    {
        var box = window.FindFirstDescendant(cf => cf.ByAutomationId("LaneBVoiWidthInput"));
        Assert.True(box is not null, "LaneBVoiWidthInput is not in the tree, so the Candidate has no setting of its own.");
        box!.AsTextBox().Text = width;
    }

    private static void ClearCandidateOverride(Window window) => SetCandidateOverride(window, "0");

    /// <summary>
    /// Puts both lanes on the same settings: no VOI override and one named algorithm on both sides.
    ///
    /// <para>A case establishes this rather than assuming it, because "nothing differs" now spans two
    /// axes: L-01 cleared only the VOI override and started failing the moment the algorithm axis
    /// existed. The app ships with both lanes on the SAME algorithm — differing by default would look
    /// better on opening and would run the pipeline a second time on every apply, for every user,
    /// workbench or not (measured: it put W-23 and W-26 back into the red, which count the calls).</para>
    /// </summary>
    private static void MakeLanesIdentical(Window window)
    {
        ClearCandidateOverride(window);
        SelectAlgorithm(window, "A", BaselineAlgorithm);
        SelectAlgorithm(window, "B", BaselineAlgorithm);
    }

    /// <summary>The option both lanes are put on when a case needs them to agree.</summary>
    private const string BaselineAlgorithm = "No correction";
}
