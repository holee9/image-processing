// #173 (GUI-C-117): the Candidate lane's own vg_denoise_k, measured on a frame that HAS noise.
using System;
using ImageProcTest.E2ETests.Fixtures;
using FlaUI.Core.AutomationElements;
using Xunit;
using Xunit.Abstractions;
using static ImageProcTest.E2ETests.Scenarios.Workflows.WorkbenchObservation;
using static ImageProcTest.E2ETests.Scenarios.Workflows.TwoLaneWorkbenchScenarios;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// L-05 lives here, apart from its three siblings in <c>TwoLaneWorkbenchScenarios</c>, because it is
/// the one lane case whose axis the default fixture cannot express.
///
/// <para><b>Measured, not assumed.</b> Written first against the synthetic 1024×1024 fixture, it failed
/// with both hashes unchanged at <c>4af3a42b5ae9317d</c> — the value reached the module and the module
/// did nothing with it. The reason is in the module: the de-noise is a soft threshold at
/// <c>k * MAD(finest band) / 0.6745</c> (virtual_grid.cpp:614-618), and that fixture's finest band has
/// MAD 0, so the threshold is 0 for EVERY k. On the wrist frame the same measurement gives MAD 6.</para>
///
/// <para>So the synthetic frame would not have shown a broken connection either — it cannot tell a
/// connected k from an unconnected one. That is the hazard this project has hit before with uniform
/// synthetic data, and the fix is the fixture, not a looser assertion.</para>
/// </summary>
[Collection(Wrist1024SliceApplicationCollection.Name)]
public sealed class LaneDenoiseScenarios(Wrist1024SliceApplicationFixture app, ITestOutputHelper output)
{
    /// <summary>
    /// L-05: the Candidate's own virtual-grid de-noise k moves the Candidate and leaves the Reference
    /// where it was.
    ///
    /// <para>Same both-halves shape as L-02 and L-04. What this one adds is that the axis is a
    /// PARAMETER of a stage rather than which stage runs: the value is handed to the native module as
    /// <c>vg_denoise_k</c>.</para>
    ///
    /// <para><b>Both lanes are put on the virtual grid first.</b> Outside it the runner sends
    /// <c>null</c> for this key (GuiGsvgRunner.cs:124), so the same case run on "No correction" would
    /// change nothing and pass — measuring the absence of an effect rather than its presence.</para>
    ///
    /// <para>Native only, for the reason L-04 is: the Mock backend refuses the grid stage by design, so
    /// under Mock no value of k reaches any pixel.</para>
    /// </summary>
    [SkippableFact]
    public void L05_ACandidateDenoiseK_MovesOnlyTheCandidate()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        Skip.If(app.BackendMode != "Native",
            "The virtual grid is a native stage; the Mock backend refuses it by design.");

        var window = app.MainWindow!;

        // GUI-C-233n: this scenario shares one app with the rest of the wrist-slice collection and used to leave the Candidate on the virtual grid with its own de-noise k of 8.0. P-11 (which draws the
        // main viewport) then failed in any run where this scenario came first (mean|dtile| 59.3, worst tile 712; alone, and in the other order, it passed). The state is put back in a finally.
        try
        {
            ClearCandidateOverride(window);
            SelectAlgorithm(window, "A", CandidateAlgorithm);   // both lanes on the virtual grid
            SelectAlgorithm(window, "B", CandidateAlgorithm);
            SetCandidateDenoiseK(window, MainDenoiseK);
            ApplyDisplayPipeline(window);

            var beforeA = ReadLane(window, "A");
            var beforeB = ReadLane(window, "B");
            output.WriteLine($"L-05 same k: A={beforeA.Hash} B={beforeB.Hash}");
            Assert.Equal(beforeA.Hash, beforeB.Hash);   // same k, so the lanes must agree first

            SetCandidateDenoiseK(window, OtherDenoiseK);
            ApplyDisplayPipeline(window);
            var afterA = ReadLane(window, "A");
            var afterB = ReadLane(window, "B");

            output.WriteLine($"L-05 A: {beforeA.Hash} -> {afterA.Hash}");
            output.WriteLine($"L-05 B: {beforeB.Hash} -> {afterB.Hash}");

            Assert.True(afterB.Hash != beforeB.Hash,
                $"Setting the Candidate's de-noise k to {OtherDenoiseK} did not reach its drawn pixels: still {afterB.Hash}.");
            Assert.True(afterA.Hash == beforeA.Hash,
                $"The Reference moved with a de-noise k set for the Candidate ({beforeA.Hash} -> {afterA.Hash}) — " +
                "the two lanes run one pipeline, so nothing here compares two values.");
        }
        finally
        {
            // The app starts with both lanes on "Grid suppression" (AppSettings.LaneAAlgorithm / LaneBAlgorithm) and no Candidate override; that is the state the next scenario of the collection assumes.
            ClearCandidateOverride(window);
            SelectAlgorithm(window, "A", AppDefaultLaneAlgorithm);
            SelectAlgorithm(window, "B", AppDefaultLaneAlgorithm);
            ApplyDisplayPipeline(window);
        }
    }

    /// <summary>
    /// L-06 (GUI-C-234c, Codex #181 finding 4): picking the Reference lane's algorithm changes the grid correction and nothing else - in particular it does not switch the main chain's preprocessing off
    /// (three of the four presets are defined with it off, and the first version of the default-on change let that through). P-10/P-11 switch the checkbox on again before they measure; this is the case that
    /// does not, so the state after the pick is what an operator would see.
    /// </summary>
    [SkippableFact]
    public void L06_ChoosingTheReferenceAlgorithm_LeavesThePreprocessingSwitchOn()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        var window = app.MainWindow!;

        OpenParameters(window);
        var box = window.FindFirstDescendant(cf => cf.ByAutomationId("PreprocessInChainCheckBox"));
        Assert.True(box is not null, "PreprocessInChainCheckBox is not in the Parameters tab.");
        var preprocess = box!.AsCheckBox();
        preprocess.IsChecked = true;   // the app's default; set explicitly because the collection shares one app

        try
        {
            foreach (var option in new[] { "No correction", "Grid suppression", "Virtual grid", "Preprocess + virtual grid" })
            {
                SelectAlgorithm(window, "A", option);
                OpenParameters(window);
                var after = window.FindFirstDescendant(cf => cf.ByAutomationId("PreprocessInChainCheckBox"))!.AsCheckBox().IsChecked;
                output.WriteLine($"L-06 after choosing '{option}': preprocess checked = {after}");
                Assert.True(after == true, $"Choosing '{option}' for the Reference lane switched the main preprocessing off.");
            }

            ApplyDisplayPipeline(window);
            var chain = window.FindFirstDescendant(cf => cf.ByAutomationId("ChainStatusText"))?.Name ?? string.Empty;
            output.WriteLine($"L-06 chain after the picks: {chain}");
            Assert.DoesNotContain("preprocess=NotRequested", chain, StringComparison.Ordinal);
        }
        finally
        {
            SelectAlgorithm(window, "A", AppDefaultLaneAlgorithm);
            ApplyDisplayPipeline(window);
        }
    }

    /// <summary>The value the main chain carries, so the lanes start in agreement.</summary>
    /// <summary>The lanes' algorithm in a freshly started app (<c>AppSettings._laneAAlgorithm</c> / <c>_laneBAlgorithm</c>).</summary>
    private const string AppDefaultLaneAlgorithm = "Grid suppression";

    private const string MainDenoiseK = "2.0";

    /// <summary>A k far enough from the main one that the soft threshold removes visibly more.</summary>
    private const string OtherDenoiseK = "8.0";
}
