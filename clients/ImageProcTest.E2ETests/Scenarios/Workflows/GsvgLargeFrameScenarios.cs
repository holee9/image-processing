// #180 (GUI-C-102): what the GSVG stage costs on a 3072x3072 frame, and what the defaults do to the image.
using System.Diagnostics;
using System.IO;
using System.Text.Json;
using System.Globalization;
using System.Text.RegularExpressions;
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;
using static ImageProcTest.E2ETests.Scenarios.Workflows.WorkbenchObservation;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// Measurements on the 3072×3072 frame, all of them read from the running app:
/// <list type="bullet">
/// <item>how long a render takes with the GSVG stage on, and how much of that the stage itself took
/// (the chain status carries <c>times: …</c> from a Stopwatch around the backend call);</item>
/// <item>whether a second render recomputes the stage (there is no cache);</item>
/// <item>what the two defaults the lead assumed — grid line density and air signal — do to the drawn
/// pixels and to their mean brightness.</item>
/// </list>
/// Timings are wall-clock on a developer machine and are reported as measurements, not as a gate:
/// GUI-C-90 measured what a loaded machine does to numbers like these.
/// </summary>
[Collection(LargeFrameApplicationCollection.Name)]
public sealed class GsvgLargeFrameScenarios(LargeFrameApplicationFixture app, ITestOutputHelper output)
{
    /// <summary>P-01: the cost of one render with each GSVG mode, split into the stage and the rest.</summary>
    [SkippableTheory]
    [InlineData("GsvgModeNone")]
    [InlineData("GsvgModeGridSuppression")]
    [InlineData("GsvgModeVirtualGrid")]
    public void P01_RenderCost_OnA3072Frame(string radioId)
    {
        var window = Ready();
        try
        {
            SetMode(window, radioId);

            // Two renders: the first pays whatever the module loads on its first call (the virtual-grid
            // table, for one), the second is the steady state. Both are reported.
            var first = MeasureRender(window);
            var second = MeasureRender(window);

            output.WriteLine($"P01 {radioId} first: total={first.TotalMs:0} ms, stage={first.StageMs:0} ms, status='{first.Status}'");
            output.WriteLine($"P01 {radioId} second: total={second.TotalMs:0} ms, stage={second.StageMs:0} ms");
            output.WriteLine($"P01 {radioId} GUI share (second render): {second.TotalMs - second.StageMs:0} ms");

            Assert.True(second.TotalMs >= second.StageMs,
                $"The stage cannot take longer than the render that contains it: {second.StageMs:0} > {second.TotalMs:0}.");
        }
        finally
        {
            SetMode(window, "GsvgModeNone");
            Apply(window);
        }
    }

    /// <summary>
    /// P-02: a second render with unchanged settings runs the stage again — there is no result cache
    /// (GUI-C-97 §6 left it for later). Measured rather than assumed.
    /// </summary>
    [SkippableFact]
    public void P02_RepeatedRender_RunsTheStageAgain()
    {
        var window = Ready();
        try
        {
            SetMode(window, "GsvgModeVirtualGrid");
            var first = MeasureRender(window);
            var second = MeasureRender(window);
            var third = MeasureRender(window);

            output.WriteLine($"P02 stage ms: {first.StageMs:0}, {second.StageMs:0}, {third.StageMs:0}");
            Assert.True(second.StageMs > 1.0 && third.StageMs > 1.0,
                $"A repeat render reported {second.StageMs:0} / {third.StageMs:0} ms for the stage; a cache would have to be reported as one.");
        }
        finally
        {
            SetMode(window, "GsvgModeNone");
            Apply(window);
        }
    }

    /// <summary>
    /// P-03: what the lead's two assumed defaults do. Grid line density 60 vs 40 (both are designs in the
    /// product table) and air signal 60000 vs 45000, measured as the drawn pixels and their mean.
    /// </summary>
    [SkippableFact]
    public void P03_AssumedDefaults_ChangeTheImageByThisMuch()
    {
        var window = Ready();
        try
        {
            SetMode(window, "GsvgModeVirtualGrid");
            SetNumber(window, "GsvgGridFrequencyInput", "60");
            SetNumber(window, "GsvgAirSignalInput", "60000");
            var baseline = MeasureRender(window);
            var baseHash = Field(window, "processed");
            var baseMean = Mean(window);
            Assert.True(Regex.IsMatch(baseline.Status, @"gsvg=Applied\b"), $"The baseline render did not apply: {baseline.Status}");

            SetNumber(window, "GsvgGridFrequencyInput", "40");
            var atFreq40 = MeasureRender(window);
            var freqHash = Field(window, "processed");
            var freqMean = Mean(window);

            SetNumber(window, "GsvgGridFrequencyInput", "60");
            SetNumber(window, "GsvgAirSignalInput", "45000");
            var atAir45k = MeasureRender(window);
            var airHash = Field(window, "processed");
            var airMean = Mean(window);

            output.WriteLine($"P03 baseline(60 /cm, 60000): hash={baseHash} mean={baseMean:0.###} status='{baseline.Status}'");
            output.WriteLine($"P03 lines 40 /cm: hash={freqHash} mean={freqMean:0.###} status='{atFreq40.Status}' Δmean={freqMean - baseMean:0.###}");
            output.WriteLine($"P03 air 45000: hash={airHash} mean={airMean:0.###} status='{atAir45k.Status}' Δmean={airMean - baseMean:0.###}");

            // No threshold is asserted — the point is the measurement. What IS asserted is that the two
            // settings reach the module at all: a value the chain ignored would leave the pixels identical.
            Assert.True(freqHash != baseHash || airHash != baseHash,
                "Neither the line density nor the air signal changed the drawn pixels; they may not reach the module.");
        }
        finally
        {
            SetNumber(window, "GsvgGridFrequencyInput", "60");
            SetNumber(window, "GsvgAirSignalInput", "60000");
            SetMode(window, "GsvgModeNone");
            Apply(window);
        }
    }

    /// <summary>
    /// P-04: the HUD the viewport actually drew names the chain. The HUD is drawn text, so the peer
    /// reports the exact string the render pass produced (<c>hud=</c>) — a display path, not a second
    /// copy of the view model's property.
    /// </summary>
    [SkippableFact]
    public void P04_TheDrawnHud_NamesTheChain()
    {
        var window = Ready();
        try
        {
            SetMode(window, "GsvgModeVirtualGrid");
            var render = MeasureRender(window);

            // The HUD is redrawn when the chain status changes, which is a frame after the processed image
            // the render waited on — measured: reading immediately returned the previous frame's HUD.
            var hud = WaitForHud(window, "gsvg=Applied");
            output.WriteLine($"P04 hud='{hud}'");

            Assert.Contains("chain:", hud, StringComparison.Ordinal);
            Assert.Contains("gsvg=", hud, StringComparison.Ordinal);
            Assert.Contains("times:", hud, StringComparison.Ordinal);
            Assert.Contains(render.Status.Split(';')[0], hud, StringComparison.Ordinal);
        }
        finally
        {
            SetMode(window, "GsvgModeNone");
            Apply(window);
        }
    }

    /// <summary>
    /// P-05 (GUI-C-102): the exported automation report carries the chain, including what the module said
    /// about the vignette step. The GUI passes no gain map and never enables vignette_correction, so
    /// vignette=0 is the expected reading — and this is where it is visible.
    ///
    /// <para>This assertion has NO control (GUI-C-103). Enabling the step needs BOTH config
    /// <c>"vignette_correction": true</c> AND a non-NULL gain map of width*height float32 (gsvg_api.h:
    /// "NULL disables the vignette step regardless of config"). The GUI has no source for a gain map,
    /// so there is no way from here to make this read anything but 0 — the case cannot distinguish
    /// "the step is off" from "the field is never written". Read it as the weaker claim it is.</para>
    /// </summary>
    [SkippableFact]
    public void P05_TheExportedReport_CarriesTheChainAndTheVignetteFlag()
    {
        var window = Ready();
        try
        {
            SetMode(window, "GsvgModeVirtualGrid");
            MeasureRender(window);

            var path = Path.Combine(Path.GetDirectoryName(app.ExecutablePath)!, "menu-command-report.json");
            if (File.Exists(path)) File.Delete(path);

            InvokeFileMenuItem(window, "ExportAutomationReportMenuItem");
            var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(10);
            while (DateTime.UtcNow < deadline && !File.Exists(path)) Thread.Sleep(200);
            Assert.True(File.Exists(path), $"The report was not written to {path}.");

            var json = File.ReadAllText(path);
            using var document = JsonDocument.Parse(json);
            var chain = document.RootElement.GetProperty("processingChain");
            var gsvg = chain.GetProperty("stages").EnumerateArray().Single(s => s.GetProperty("id").GetString() == "gsvg");

            output.WriteLine($"P05 gsvgMode={chain.GetProperty("gsvgMode").GetString()}, status={gsvg.GetProperty("status").GetString()}, " +
                             $"elapsedMs={gsvg.GetProperty("elapsedMs").GetDouble():0}, reason={gsvg.GetProperty("reason").GetString()}");

            Assert.Equal("VirtualGrid", chain.GetProperty("gsvgMode").GetString());
            Assert.Equal("Applied", gsvg.GetProperty("status").GetString());
            Assert.Contains("vignette=0", gsvg.GetProperty("reason").GetString()!, StringComparison.Ordinal);
            Assert.True(gsvg.GetProperty("elapsedMs").GetDouble() > 0.0, "The report records no time for a stage that ran.");
        }
        finally
        {
            SetMode(window, "GsvgModeNone");
            Apply(window);
        }
    }

    private static void InvokeFileMenuItem(Window window, string automationId)
    {
        AutomationElement? item = null;
        for (var attempt = 0; attempt < 3 && item is null; attempt++)
        {
            window.SetForeground();
            FlaUI.Core.Input.Keyboard.Press(FlaUI.Core.WindowsAPI.VirtualKeyShort.ESCAPE);
            Thread.Sleep(120);
            var menu = window.FindFirstDescendant(cf => cf.ByAutomationId("FileMenu"))!.AsMenuItem();
            if (attempt == 0) menu.Click(); else menu.Expand();
            Thread.Sleep(350);
            item = window.FindFirstDescendant(cf => cf.ByAutomationId(automationId));
        }

        Assert.True(item is not null, $"{automationId} did not appear after three attempts.");
        item!.AsMenuItem().Invoke();
        Thread.Sleep(600);
    }

    /// <summary>
    /// P-06 (GUI-C-103): what the module actually DID on this frame, per mode, read from the exported
    /// report's reason string. The card asks why the GUI's stage time (27-116 ms) is far below the post
    /// lane's module measurement (493 / 488 ms); whether the module took its full path or an early exit
    /// on this image is the first thing that has to be known, and it is only knowable from the reason.
    /// </summary>
    [SkippableTheory]
    [InlineData("GsvgModeGridSuppression")]
    [InlineData("GsvgModeVirtualGrid")]
    public void P06_WhatTheModuleDid_PerMode(string radioId)
    {
        var window = Ready();
        try
        {
            SetMode(window, radioId);
            var render = MeasureRender(window);
            var gsvg = ExportAndReadGsvgStage(window);

            output.WriteLine($"P06 {radioId} stage={render.StageMs:0} ms status={gsvg.GetProperty("status").GetString()}");
            output.WriteLine($"P06 {radioId} reason={gsvg.GetProperty("reason").GetString()}");

            Assert.False(string.IsNullOrWhiteSpace(gsvg.GetProperty("reason").GetString()),
                "The module reported no reason, so what it did on this frame cannot be read.");
        }
        finally
        {
            SetMode(window, "GsvgModeNone");
            Apply(window);
        }
    }

    /// <summary>Exports the automation report and returns the gsvg stage element from it.</summary>
    private JsonElement ExportAndReadGsvgStage(Window window)
    {
        var path = Path.Combine(Path.GetDirectoryName(app.ExecutablePath)!, "menu-command-report.json");
        if (File.Exists(path)) File.Delete(path);

        InvokeFileMenuItem(window, "ExportAutomationReportMenuItem");
        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(10);
        while (DateTime.UtcNow < deadline && !File.Exists(path)) Thread.Sleep(200);
        Assert.True(File.Exists(path), $"The report was not written to {path}.");

        // Parsed into a detached element: the JsonDocument is disposed on return, and a live
        // JsonElement over a disposed document throws when read.
        using var document = JsonDocument.Parse(File.ReadAllText(path));
        return document.RootElement.GetProperty("processingChain").GetProperty("stages")
            .EnumerateArray().Single(s => s.GetProperty("id").GetString() == "gsvg").Clone();
    }

    /// <summary>
    /// P-07 (GUI-C-103): the ~2 s to a drawn frame, split. The view model records the background work,
    /// its own share, and the display pipeline's four phases; the render share is what is left over from
    /// the outside measurement, because nothing inside the app can observe when the frame reached glass.
    ///
    /// <para>Measure-only, like P-01: it asserts the split is reported and adds up, not how fast it is.
    /// One machine and few repeats cannot calibrate a budget (GUI-C-102 §5).</para>
    /// </summary>
    [SkippableFact]
    public void P07_TheTimeToADrawnFrame_SplitsIntoPhases()
    {
        var window = Ready();
        SetMode(window, "GsvgModeNone");

        MeasureRender(window);              // warm the path; the first apply also pays one-off costs
        var render = MeasureRender(window);

        var path = Path.Combine(Path.GetDirectoryName(app.ExecutablePath)!, "menu-command-report.json");
        if (File.Exists(path)) File.Delete(path);
        InvokeFileMenuItem(window, "ExportAutomationReportMenuItem");
        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(10);
        while (DateTime.UtcNow < deadline && !File.Exists(path)) Thread.Sleep(200);
        Assert.True(File.Exists(path), $"The report was not written to {path}.");

        using var document = JsonDocument.Parse(File.ReadAllText(path));
        var timings = document.RootElement.GetProperty("processingChain").GetProperty("pipelineTimings").GetString() ?? string.Empty;

        output.WriteLine($"P07 outside total={render.TotalMs:0} ms");
        output.WriteLine($"P07 inside {timings}");
        output.WriteLine($"P07 render+dispatch share = {render.TotalMs - Part(timings, "work") - Part(timings, "vm"):0} ms");

        // How much of that share is this harness, not the app: one poll cycle is two UI-automation
        // reads plus a 20 ms sleep, and UI automation is not free. Without this the leftover would be
        // attributed to the render by default — the measurement would be measuring itself.
        var probe = Stopwatch.StartNew();
        for (var i = 0; i < 10; i++)
        {
            _ = ProcessedVersion(window);
            _ = ChainText(window);
            Thread.Sleep(20);
        }

        probe.Stop();
        output.WriteLine($"P07 apply invoke (UI automation, before the app does anything) = {LastApplyMs:0} ms");
        output.WriteLine($"P07 in-app render = {Field(window, "renderMs")} ms (viewport handed a new image -> frame drawn)");
        output.WriteLine($"P07 poll cycle cost = {probe.Elapsed.TotalMilliseconds / 10.0:0.0} ms " +
                         $"(of which 20 ms is the deliberate sleep)");

        Assert.Contains("work=", timings, StringComparison.Ordinal);
        Assert.Contains("preview=", timings, StringComparison.Ordinal);

        var phases = Part(timings, "marshal-in") + Part(timings, "native") + Part(timings, "marshal-out") + Part(timings, "preview");
        Assert.True(phases <= Part(timings, "work") + 1.0,
            $"The display phases ({phases:0} ms) cannot exceed the background work that contains them ({Part(timings, "work"):0} ms).");
    }

    /// <summary>One <c>name=N ms</c> figure out of a timings string; 0 when the name is absent.</summary>
    private static double Part(string timings, string name)
    {
        var m = Regex.Match(timings, $@"{Regex.Escape(name)}=(-?[0-9.]+) ms");
        return m.Success && double.TryParse(m.Groups[1].Value, System.Globalization.NumberStyles.Float,
            System.Globalization.CultureInfo.InvariantCulture, out var v) ? v : 0.0;
    }

    /// <summary>
    /// P-08 (GUI-C-104): a performance regression gate built from what the APP measured, not from what
    /// the harness observed. GUI-C-103 established why: invoking the button through UI automation costs
    /// ~2 s against an app that works in ~20 ms, so an outside figure cannot see the app move at all.
    ///
    /// <para>Both figures read here come from inside the process — the chain's per-stage stopwatch and
    /// the view model's pipeline split — and reach the test through the exported report. No part of the
    /// automation cost is in either number.</para>
    ///
    /// <para>The gate is stated as a multiple of a measured baseline, and the run prints median / p95 /
    /// max so the next person can re-derive it rather than trust it. It guards Native only, and CI runs
    /// the GUI suite on Mock — so this catches a regression HERE, not on CI (stated, not assumed).</para>
    /// </summary>
    [SkippableFact]
    public void P08_TheAppsOwnStageTime_StaysWithinItsMeasuredSpread()
    {
        var window = Ready();
        try
        {
            SetMode(window, "GsvgModeVirtualGrid");
            MeasureRender(window);          // warm: the first apply also pays one-off costs

            var stageMs = new List<double>();
            for (var i = 0; i < Repeats; i++) stageMs.Add(MeasureRender(window).StageMs);

            stageMs.Sort();
            var median = stageMs[stageMs.Count / 2];
            var p95 = stageMs[(int)Math.Floor((stageMs.Count - 1) * 0.95)];
            var max = stageMs[^1];
            output.WriteLine($"P08 gsvg stage over {Repeats} runs: median={median:0.0} p95={p95:0.0} max={max:0.0} ms " +
                             $"[{string.Join(", ", stageMs.Select(v => v.ToString("0")))}]");
            output.WriteLine($"P08 spread max/median = {max / Math.Max(1.0, median):0.00}x; gate = {GateMs} ms");

            Assert.True(median < GateMs,
                $"The GSVG stage's median rose to {median:0} ms against a {GateMs} ms gate " +
                $"(baseline {BaselineMs} ms, measured spread {max / Math.Max(1.0, median):0.00}x). " +
                "Either the stage got slower or the baseline needs re-measuring — do not raise the gate without re-measuring.");
        }
        finally
        {
            SetMode(window, "GsvgModeNone");
            Apply(window);
        }
    }

    /// <summary>How many applies P-08 times. Odd, so the median is an observed value rather than a mean.</summary>
    private const int Repeats = 7;

    /// <summary>
    /// Median GSVG stage time on this frame at the app's DEFAULT settings. Re-measure before changing
    /// it; a number moved to make a run pass is not a baseline.
    ///
    /// <para>It has moved twice, and each move is recorded rather than smoothed over, because a gate
    /// whose history is invisible cannot be told apart from one that was quietly relaxed:</para>
    ///
    /// <list type="number">
    /// <item>27 ms — the GUI sent no <c>vg_pyramid_levels</c> key, so the module left its pyramid off.</item>
    /// <item>50 ms — defaulting levels to 4 put the Laplacian pyramid back into every virtual-grid run.
    /// This gate went red at 51 ms against a 45 ms limit, on a change that was intended.</item>
    /// <item>62 ms — the lead's decision matched the app's defaults to the module's (levels 4, gain 1.3,
    /// de-noise k 2), which adds the soft-threshold pass the earlier two baselines never ran.</item>
    /// <item>65 ms — same settings, re-measured against a newer gsvg.dll (CI run 35294573612, head
    /// 3f520f8) after the stale local staging was replaced. The derived gate lands on 105 ms either
    /// way, so this step changed the baseline without moving the limit.</item>
    /// </list>
    ///
    /// <para>This baseline is a DEV-MACHINE number. The same test on the CI runner measured 84 ms where
    /// this machine measured 50 ms at the same code — 1.68x — so a gate derived here is not known to
    /// hold there. The runner is 4 logical cores at 19.4 GB/s against this machine's 25.5 GB/s, and the
    /// stage is memory-bound (post lane, QA-B-105). Splitting the gate per machine is a structure the
    /// lead decides; until then the number below is honest about where it was measured.</para>
    ///
    /// Every step re-measured the baseline under the new workload. None of them widened the gate to fit
    /// a red run — that is a different act, and the distinction is the whole value of this number.
    /// </summary>
    private const double BaselineMs = 65.0;

    /// <summary>
    /// The gate, derived from measurement rather than padded:
    ///
    /// <list type="bullet">
    /// <item>3 runs x 7 applies = 21 samples at the current defaults against gsvg.dll from CI run
    /// 35294573612: medians 65 / 66 / 65 ms, worst single sample 77 ms.</item>
    /// <item>Observed spread is therefore 77/65 = 1.18x of the median.</item>
    /// <item>The gate allows that spread again on top, for machine load these quiet runs did not see:
    /// 65 x 1.18 x 1.36 = 104 ms, rounded to 105 — which is 1.36x the worst sample actually observed.</item>
    /// </list>
    ///
    /// The 1.36 factor is carried over from the first derivation of this gate so the two are comparable;
    /// it is slack for load, not a safety margin against the code. Raising this is only honest after
    /// re-running P-08 and recording the new spread — a number moved to make a red run green is not a gate.
    /// </summary>
    private const double GateMs = 105.0;

    /// <summary>
    /// P-09 (GUI-C-104): the pyramid-levels setting reaches the drawn pixels. GUI-C-103 measured that
    /// omitting <c>vg_pyramid_levels</c> left the module's whole Laplacian-pyramid and de-noise step
    /// unrun — the setting existing is not evidence it is connected, so this asserts on what was drawn.
    ///
    /// <para>Levels ALONE cannot change the pixels, and this case says so rather than hiding it: with
    /// gain 1.0 the pyramid subtracts each detail band and adds it straight back, so it rebuilds the
    /// image exactly (virtual_grid.cpp PyramidContrast; the field comment reads "1 = unchanged").
    /// Measured: levels 0 and levels 4 give a byte-identical drawn hash, at ~10 ms extra cost. The
    /// connection is therefore asserted with the gain moved off 1.0, which is the only way the setting
    /// reaches a pixel. The stage cost is reported but not asserted — one machine cannot gate it.</para>
    /// </summary>
    [SkippableFact]
    public void P09_PyramidLevels_ChangeTheDrawnPixels()
    {
        var window = Ready();
        try
        {
            SetMode(window, "GsvgModeVirtualGrid");
            SetNumber(window, "GsvgDenoiseKInput", "0");
            SetNumber(window, "GsvgPyramidGainInput", "1.0");
            SetNumber(window, "GsvgPyramidLevelsInput", "0");
            var off = MeasureRender(window);
            var offHash = Field(window, "processed");
            var offMean = Mean(window);
            Assert.True(Regex.IsMatch(off.Status, @"gsvg=Applied\b"), $"The pyramid-off render did not apply: {off.Status}");

            SetNumber(window, "GsvgPyramidLevelsInput", "4");
            var unity = MeasureRender(window);
            var unityHash = Field(window, "processed");
            var unityMean = Mean(window);
            Assert.True(Regex.IsMatch(unity.Status, @"gsvg=Applied\b"), $"The pyramid-on render did not apply: {unity.Status}");

            SetNumber(window, "GsvgPyramidGainInput", "1.3");
            var gained = MeasureRender(window);
            var gainedHash = Field(window, "processed");
            var gainedMean = Mean(window);
            Assert.True(Regex.IsMatch(gained.Status, @"gsvg=Applied\b"), $"The gain render did not apply: {gained.Status}");

            output.WriteLine($"P09 levels=0          : hash={offHash} mean={offMean:0.000} stage={off.StageMs:0} ms");
            output.WriteLine($"P09 levels=4 gain=1.0 : hash={unityHash} mean={unityMean:0.000} stage={unity.StageMs:0} ms");
            output.WriteLine($"P09 levels=4 gain=1.3 : hash={gainedHash} mean={gainedMean:0.000} stage={gained.StageMs:0} ms " +
                             $"(dMean vs off={Math.Abs(gainedMean - offMean):0.000})");

            Assert.Equal(offHash, unityHash);      // measured: gain 1.0 rebuilds the image exactly
            Assert.NotEqual(offHash, gainedHash);  // the settings do reach the drawn pixels
        }
        finally
        {
            // Back to the app's defaults, which match the module's (lead's decision, GUI-C-104).
            SetNumber(window, "GsvgDenoiseKInput", "2");
            SetNumber(window, "GsvgPyramidGainInput", "1.3");
            SetNumber(window, "GsvgPyramidLevelsInput", "4");
            SetMode(window, "GsvgModeNone");
            Apply(window);
        }
    }

    /// <summary>
    /// P-10 (GUI-C-104): the preview-bitmap change kept the pixels. GUI-C-103 measured the preview as the
    /// largest phase of the app's own work (~10 ms of ~16 ms), and the change removed the per-pixel
    /// interface dispatch in its two loops. Only the dispatch went — the arithmetic is the same — so the
    /// drawn hash must be the one recorded before the change.
    ///
    /// <para>The hash is pinned rather than compared against a second run of the same build: comparing a
    /// build with itself cannot tell a preserved output from a consistently wrong one.</para>
    /// </summary>
    [SkippableFact]
    public void P10_ThePreviewChange_KeptTheDrawnPixels()
    {
        var window = Ready();
        try
        {
            SetMode(window, "GsvgModeVirtualGrid");
            SetNumber(window, "GsvgDenoiseKInput", "0");
            SetNumber(window, "GsvgPyramidGainInput", "1.0");
            SetNumber(window, "GsvgPyramidLevelsInput", "0");
            SetNumber(window, "GsvgGridFrequencyInput", "60");
            SetNumber(window, "GsvgAirSignalInput", "60000");
            var render = MeasureRender(window);

            output.WriteLine($"P10 hash={Field(window, "processed")} mean={Mean(window):0.000}");
            Assert.Equal(PreviewBaselineHash, Field(window, "processed"));
        }
        finally
        {
            SetNumber(window, "GsvgDenoiseKInput", "2");
            SetNumber(window, "GsvgPyramidGainInput", "1.3");
            SetNumber(window, "GsvgPyramidLevelsInput", "4");
            SetMode(window, "GsvgModeNone");
            Apply(window);
        }
    }

    /// <summary>
    /// The drawn hash of the virtual-grid frame at the documented settings, recorded BEFORE the preview
    /// change (GUI-C-102 P-03 baseline, and again in GUI-C-103). It is the control for P-10.
    /// </summary>
    private const string PreviewBaselineHash = "36fc547e253b07f1";

    // ---- helpers -------------------------------------------------------------------------------

    private sealed record Render(double TotalMs, double StageMs, string Status);

    /// <summary>
    /// What the last <c>Apply</c> cost before the app was even asked to do anything: finding the control
    /// through UI automation and invoking it. Recorded because the outside total is otherwise credited
    /// to the app (GUI-C-103 — the app's own share measured ~20 ms against an outside total of ~2.4 s).
    /// </summary>
    private double LastApplyMs { get; set; }

    /// <summary>
    /// Applies the display pipeline and waits until the viewport has been handed a NEW processed image,
    /// so the time includes the render — "until the screen is updated", which is what the card asks for.
    ///
    /// <para>The version is the completion signal rather than the pixel hash: a repeat render with the same
    /// settings produces the same pixels, and P-02 exists precisely to measure that case (measured — the
    /// first version of this helper waited 62 s for a hash that could not change).</para>
    /// </summary>
    private Render MeasureRender(Window window)
    {
        var before = ProcessedVersion(window);
        var stopwatch = Stopwatch.StartNew();
        Apply(window);
        LastApplyMs = stopwatch.Elapsed.TotalMilliseconds;

        // Only the version is polled. Reading the chain text too doubled the UI-automation cost of a
        // cycle, and a cycle is already ~152 ms against a render of ~3 ms (measured, GUI-C-103) — the
        // wait loop was the thing being timed. The status is read once, after the version moves; the
        // view model sets it before it hands the image over, so it is current by then.
        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(60);
        while (DateTime.UtcNow < deadline)
        {
            if (ProcessedVersion(window) > before)
            {
                stopwatch.Stop();
                var status = ChainText(window);
                return new Render(stopwatch.Elapsed.TotalMilliseconds, StageMs(status, "gsvg"), status);
            }

            Thread.Sleep(5);
        }

        stopwatch.Stop();
        var last = ChainText(window);
        Assert.Fail($"The viewport was not handed a new processed image within {stopwatch.Elapsed.TotalSeconds:0} s; status '{last}'.");
        return new Render(0, 0, last);
    }

    /// <summary>The processed layer's version from the viewport's ItemStatus — it counts images, not pixels.</summary>
    private static int ProcessedVersion(Window window) => Viewport(window).ProcessedVersion;

    /// <summary>The drawn HUD once it names <paramref name="expected"/>, or the last one seen.</summary>
    private static string WaitForHud(Window window, string expected)
    {
        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(5);
        var hud = Field(window, "hud");
        while (DateTime.UtcNow < deadline && !hud.Contains(expected, StringComparison.Ordinal))
        {
            Thread.Sleep(100);
            hud = Field(window, "hud");
        }

        return hud;
    }

    /// <summary>The stage's own milliseconds from <c>times: preprocess=0 ms, gsvg=512 ms</c>.</summary>
    private static double StageMs(string status, string stageId)
    {
        var m = Regex.Match(status, $@"{Regex.Escape(stageId)}=(\d+(?:\.\d+)?) ms");
        return m.Success ? double.Parse(m.Groups[1].Value, CultureInfo.InvariantCulture) : 0.0;
    }

    private static string ChainText(Window window) =>
        window.FindFirstDescendant(cf => cf.ByAutomationId("ChainStatusText"))?.Name ?? string.Empty;

    /// <summary>
    /// One field of the viewport peer's HelpText (<c>processed</c>, <c>processedMean</c>, <c>hud</c>).
    /// <c>hud</c> is read to the end of the string: the HUD text contains ';' of its own, and cutting at the
    /// first one is how the first version of P-04 read "no times: in the HUD" off a HUD that had it.
    /// </summary>
    private static string Field(Window window, string name)
    {
        var help = window.FindFirstDescendant(cf => cf.ByAutomationId("WorkbenchViewport"))?.HelpText ?? string.Empty;
        var pattern = name == "hud" ? @"hud=(.*)$" : $@"{Regex.Escape(name)}=([^;]*)";
        var m = Regex.Match(help, pattern, RegexOptions.Singleline);
        return m.Success ? m.Groups[1].Value.Trim() : "(missing)";
    }

    private static double Mean(Window window) =>
        double.TryParse(Field(window, "processedMean"), NumberStyles.Float, CultureInfo.InvariantCulture, out var mean) ? mean : -1.0;

    private static void Apply(Window window) => ApplyDisplayPipeline(window);

    private static void SetMode(Window window, string radioId)
    {
        OpenParameters(window);
        var radio = window.FindFirstDescendant(cf => cf.ByAutomationId(radioId));
        Assert.True(radio is not null, $"{radioId} is not in the Parameters tab.");
        radio!.AsRadioButton().IsChecked = true;
        Thread.Sleep(250);
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
        Thread.Sleep(300);
    }

    private Window Ready()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        Skip.If(app.BackendMode != "Native", "gsvg.dll only runs on the native backend.");
        var window = app.MainWindow!;
        CloseDetached(window);
        return window;
    }
}
