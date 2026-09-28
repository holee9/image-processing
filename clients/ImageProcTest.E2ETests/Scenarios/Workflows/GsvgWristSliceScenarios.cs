// #180 (GUI-C-102): what the GSVG stage costs, and what the defaults do to the image.
// #208 (GUI-C-148): the frame is the wrist slice the fixture loads — 1024x1024, NOT 3072x3072.
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
/// Measurements on the frame <see cref="Wrist1024SliceApplicationFixture"/> loads, all of them read
/// from the running app.
///
/// <para><b>Which frame that is (#208, GUI-C-148).</b> The wrist file on disk is 3072×3072, but the app
/// loads its FIRST 1024×1024 — so these are measurements on a 1024² slice. This summary used to say
/// "Measurements on the 3072×3072 frame", which was never true of what ran here.</para>
///
/// <para><b>Renamed (#208, GUI-C-149).</b> This class was <c>GsvgLargeFrameScenarios</c>. "LargeFrame"
/// promised a large frame; what runs here is the 1024² slice. The only code that cited the old name
/// was this file and one prose line in <c>P10PreviewTileSignature.txt</c> — the #200 signature is
/// addressed by that FILENAME and its values, neither of which carries a class name, so renaming
/// costs it nothing. Past reports and local logs keep the old name: they are records, not addresses.</para>
///
/// <para><b>What the slice is for, positively.</b> Not size — pixel CONTENT. At the same 1024² the wrist
/// slice is a different image from the default synthetic frame, and that is what lets these scenarios see
/// the stage doing something: GUI-C-117 found an assertion that passed on the synthetic frame even with
/// the stage disconnected, and moved it onto this fixture.</para>
///
/// <para><b>REQ-GSVG-019 is not measured here.</b> That 1.0 s requirement is on the MODULE at a real
/// 3072², met by the post lane (QA-B-102/103, 713–757 ms). Nothing below compares against it, and the
/// end-to-end time of a 3072² frame through the GUI has never been measured.</para>
///
/// What is read from the running app:
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
[Collection(Wrist1024SliceApplicationCollection.Name)]
public sealed class GsvgWristSliceScenarios(Wrist1024SliceApplicationFixture app, ITestOutputHelper output)
{
    /// <summary>
    /// P-01: the cost of one render with each GSVG mode, split into the stage and the rest — REPORTED,
    /// not gated.
    ///
    /// <para><b>Renamed (#208, GUI-C-148), for two reasons.</b> It was
    /// <c>P01_RenderCost_OnA3072Frame</c>. The frame is the 1024² wrist slice, not 3072² — and the name
    /// promised a cost assertion this test does not make. The only thing asserted is the containment
    /// invariant below: the stage is part of the render, so it cannot exceed it. The millisecond figures
    /// are written to the output as measurements — GUI-C-90 measured what a loaded machine does to
    /// numbers like these, so they are not a gate and no threshold is compared against.</para>
    /// </summary>
    [SkippableTheory]
    [InlineData("GsvgModeNone")]
    [InlineData("GsvgModeGridSuppression")]
    [InlineData("GsvgModeVirtualGrid")]
    public void P01_TheStageFitsInsideTheRender_OnTheWristSlice(string radioId)
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
    ///
    /// <para><b>Renamed (#208, GUI-C-149).</b> It was <c>P03_AssumedDefaults_ChangeTheImageByThisMuch</c>
    /// — "by this much" promises a magnitude, and no magnitude is asserted. The Δmean figures are written
    /// to the output as measurements; what is ASSERTED is only that the two settings reach the drawn
    /// pixels at all, which is what the new name says. (The body already said so; the name did not.)</para>
    /// </summary>
    [SkippableFact]
    public void P03_AssumedDefaults_ReachTheDrawnPixels()
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
    /// max so the next person can re-derive it rather than trust it.</para>
    ///
    /// <para>It DOES run on CI: the gui-e2e-native job invokes the whole project with no --filter. An
    /// earlier version of this comment said the opposite; that was read off the Mock job alone.</para>
    ///
    /// <para><b>What it can and cannot catch, measured by injection</b> (raising the virtual grid's
    /// iterations, which is real work rather than an artificial sleep):</para>
    ///
    /// <list type="bullet">
    /// <item>iterations 3 (default) — median 65-68 ms, green.</item>
    /// <item>iterations 9 — median 90 ms, a 1.38x regression, and this gate stays GREEN.</item>
    /// <item>iterations 15 — median 117 ms, red.</item>
    /// <item>pyramid levels 8 instead of 4 — median 66 ms, NOT a slowdown at all: each level is a
    /// quarter of the one above, so depth barely moves the cost. Injecting it proves nothing, and a
    /// green run under it would have been mistaken for a working gate.</item>
    /// </list>
    ///
    /// So the detection floor is roughly gate/baseline = 105/65 = 1.6x. A regression smaller than that
    /// passes here. That is the price of slack sized for machine load, and it is written down rather
    /// than discovered later by someone trusting the gate with more than it can carry.
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
            var profile = MachineProfile.Detect();
            output.WriteLine($"P08 machine: {profile}");
            output.WriteLine($"P08 spread max/median = {max / Math.Max(1.0, median):0.00}x; gate = {profile.GateMs} ms ({profile.Name})");

            Assert.True(median < profile.GateMs,
                $"The GSVG stage's median rose to {median:0} ms against a {profile.GateMs} ms gate " +
                $"for the {profile.Name} profile (baseline {profile.BaselineMs} ms, measured spread " +
                $"{max / Math.Max(1.0, median):0.00}x). Machine: {profile}. " +
                "Either the stage got slower or that profile's baseline needs re-measuring — do not raise a gate without re-measuring.");
        }
        finally
        {
            SetMode(window, "GsvgModeNone");
            Apply(window);
        }
    }

    /// <summary>
    /// How many applies P-08 times. Odd, so the median is an observed value rather than a mean.
    ///
    /// <para>It was 7, and raising it to 21 is what let the gate tighten (GUI-C-107). The gate judges a
    /// MEDIAN, so the slack it needs is the spread of that median across runs — not the spread of raw
    /// samples, which a single outlier moves. At 7 applies the sample spread was 1.18x and the median
    /// still wandered; at 21 the medians of five consecutive runs were 61 / 61 / 62 / 62 / 62 ms, a
    /// spread of 1.016x. The noise came out of the statistic rather than out of the limit.</para>
    ///
    /// <para>Cost: about 50 s a run instead of 17 s. That is paid on every Native run, here and on CI.</para>
    /// </summary>
    private const int Repeats = 21;

    /// <summary>
    /// Which machine this is running on, and the gate that was measured FOR that machine.
    ///
    /// <para>Two profiles, because one number cannot serve both: at identical code this dev machine
    /// measured a 50 ms median where the CI runner measured 84 ms — 1.68x. A single gate is either
    /// red on every CI run or blind to a 1.68x regression here.</para>
    ///
    /// <para>The profile is chosen from what can be OBSERVED about the machine, and the looser CI
    /// profile requires positive evidence on BOTH axes: the hosted-runner marker AND a core count that
    /// matches the runner. A declaration alone does not loosen the gate — if the marker were wrong or
    /// inherited, a bare env-var check would silently widen the limit and nobody would see it. This way
    /// a misread errs toward the STRICTER profile: the failure mode is a visible red, not a silent pass.
    /// The trade is real — if the runner ever grows past 8 cores this goes red until re-measured — and
    /// that is the direction worth failing in.</para>
    ///
    /// <para>Both numbers are printed on every run, so a wrong classification is readable rather than
    /// inferred.</para>
    /// </summary>
    private sealed record MachineProfile(string Name, double BaselineMs, double GateMs, int Cores, bool HostedRunner)
    {
        /// <summary>
        /// Dev machine (i7-12700 class, 20 logical cores, 25.5 GB/s), gsvg.dll from CI run 35294573612:
        /// 5 runs x 21 applies = 105 samples, medians 61 / 61 / 62 / 62 / 62 ms, worst single sample 69 ms.
        ///
        /// <para>The slack is sized by the spread of the MEDIAN, because the median is what the gate
        /// judges: 62/61 = 1.016x. Gate = 62 x 1.016 x 1.36 = 86 ms. Detection floor 86/62 = 1.39x.</para>
        ///
        /// <para>The earlier form of this number was 105 ms, derived at 7 applies a run from the spread
        /// of raw samples (1.18x). Nothing was shaved off the 1.36 load allowance to get from there to
        /// here — that factor is unchanged. What changed is that the measurement got quieter, so the
        /// noise term fell from 1.18 to 1.016 and carried the gate down with it. Measured consequence:
        /// a 1.38x regression (iterations 9, median 90 ms) passed the 105 ms gate and fails this one.</para>
        ///
        /// The baseline has moved three times before this and each move is recorded in git rather than
        /// smoothed over: 27 ms (the GUI sent no pyramid keys, so the module left the pyramid off),
        /// 50 ms (levels defaulted to 4, putting the Laplacian pyramid back into every run), 62 ms
        /// (defaults matched to the module's, adding the soft-threshold pass), 65 ms (same settings,
        /// newer gsvg.dll). Every step re-measured under the new workload; none widened a gate to fit
        /// a red run. That distinction is the whole value of these numbers.
        /// </summary>
        private static readonly MachineProfile Dev = new("dev", 62.0, 86.0, 0, false);

        /// <summary>
        /// CI runner (Xeon 6973P-C, 4 logical cores, 19.4 GB/s): 1 run x 7 applies = 7 samples, median
        /// 84 ms, worst 92 ms, spread 92/84 = 1.10x — measured in CI run 35294573612's gui-e2e-native
        /// job. Same derivation as the dev profile: 84 x 1.10 x 1.36 = 126 ms.
        ///
        /// <para>PROVISIONAL, and now known to be loose. Where 84 came from: ONE CI run (35294573612),
        /// 7 applies, at the GUI-C-104 code — before the de-noise pass joined the defaults and before the
        /// repeat count went to 21. It is recorded here because a gate whose provenance is missing cannot
        /// be re-derived, only replaced.</para>
        ///
        /// <para>Collected since, at THIS code and 21 applies:</para>
        ///
        /// <list type="bullet">
        /// <item>run 35399591335 — median 64 ms, worst 80, spread 1.25x; profile picked correctly
        /// (cores=4, hostedRunner=True).</item>
        /// </list>
        ///
        /// <para>That is 1 of the 5 runs needed to size slack by the spread of the MEDIAN, the way the dev
        /// profile is sized. It already contradicts the number above: 64 ms here against 62 ms on the dev
        /// machine is 1.03x, where the earlier pair (84 vs 50) read as 1.68x. So this gate's detection
        /// floor is currently 126/64 = 1.97x — far looser than the dev profile's 1.39x.</para>
        ///
        /// <para>It is NOT adjusted yet, on the lead's decision and for the obvious reason: one sample
        /// cannot show how far a median wanders between runs, and changing a gate from a statistic that
        /// was never measured is the mistake this whole line of work exists to avoid. Frozen until 5.</para>
        /// </summary>
        private static readonly MachineProfile Ci = new("ci", 84.0, 126.0, 0, true);

        /// <summary>Core count above which the CI profile is refused even when the marker is present.</summary>
        private const int RunnerCoreCeiling = 8;

        public static MachineProfile Detect()
        {
            var cores = Environment.ProcessorCount;
            var hosted = string.Equals(Environment.GetEnvironmentVariable("GITHUB_ACTIONS"), "true",
                StringComparison.OrdinalIgnoreCase);
            var profile = hosted && cores <= RunnerCoreCeiling ? Ci : Dev;
            return profile with { Cores = cores, HostedRunner = hosted };
        }

        public override string ToString() =>
            $"cores={Cores}, hostedRunner={HostedRunner}, profile={Name}, baseline={BaselineMs:0} ms, gate={GateMs:0} ms";
    }

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
    /// <para><b>Renamed (#208, GUI-C-149).</b> It was <c>P09_PyramidLevels_ChangeTheDrawnPixels</c>,
    /// which this test's own assertion contradicts: <c>Assert.Equal(offHash, unityHash)</c> says levels
    /// alone leave the pixels byte-identical at gain 1.0. The pixels move when the GAIN moves. The
    /// paragraph above always said this; the name said the opposite.</para>
    /// </summary>
    [SkippableFact]
    public void P09_PyramidGain_ChangesTheDrawnPixels_LevelsAloneDoNot()
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
            SetNumber(window, "GsvgGridFrequencyInput", "60");;;
            SetNumber(window, "GsvgAirSignalInput", "60000");
            var render = MeasureRender(window);

            var drawnHash = Field(window, "processed");
            output.WriteLine($"P10 hash={drawnHash} mean={Mean(window):0.000}");

            // The message says WHAT THIS TEST KNOWS and no more (#200, GUI-C-138): a hash separates
            // "identical" from "not identical" and nothing else — it cannot say how far off the render
            // is, so it must not be read as evidence of a breakage. P-11 is the test that answers the
            // magnitude question, and the two are deliberately worded to be told apart when both are
            // red at once.
            Assert.True(
                string.Equals(PreviewBaselineHash, drawnHash, StringComparison.Ordinal),
                $"SOMETHING CHANGED the drawn pixels — this test does not say whether that is a defect. "
              + $"Drawn hash {drawnHash}, recorded {PreviewBaselineHash}. A hash is bit-exact, so a "
              + "one-count rounding shift reads the same here as a broken render (measured: the #156 VOI "
              + "placement correction moved every LINEAR pixel by at most 0.0039 of a count and changed "
              + "this hash completely). WHAT TO DO: find the change that moved the pixels, then read "
              + "P11_ThePreviewRender_StillCarriesTheRecordedTileSignature in the same run — if P-11 is "
              + "GREEN the change is within the tolerance derived from a legitimate correction, and this "
              + "hash is the one to re-record after citing that change (see PreviewBaselineHash). If "
              + "P-11 is also RED the change exceeded that tolerance and is likely a breakage; do not "
              + "re-record either one until the cause is identified.");
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
    ///
    /// <para><b>Updated in GUI-C-134 (#156): <c>36fc547e253b07f1</c> → <c>f2a5640e9bd1a7fc</c>.</b> The
    /// old value is not a regression that was lost — it was printed by a WRONG formula. <c>efd14c1</c>
    /// changed <c>XPE_VOI_LINEAR</c> to the DICOM PS3.3 C.11.2.1.2.1 window placement
    /// (<c>center-0.5</c> / <c>width-1</c>), so every LINEAR output moved by design. This is a
    /// correction, not a regression.</para>
    ///
    /// <para><b>Measured both directions before touching this number</b> (GUI-C-134 §3), because
    /// updating a golden erases the evidence that would have identified the real cause:</para>
    /// <code>
    /// pre-efd14c1  xpe_display.dll : 36fc547e253b07f1  mean=85.205   (the golden returns)
    /// post-efd14c1 xpe_display.dll : f2a5640e9bd1a7fc  mean=85.206
    /// </code>
    ///
    /// <para><b>The size of the change is 0.001 counts of 255.</b> This scenario runs the default window
    /// (<c>c=32768, w=65535</c>), where the formula difference is at most <c>0.5/(w-1)</c> ≈ 0.0039 of a
    /// count — invisible on screen, yet the hash changes completely. Worth knowing about this assertion:
    /// a hash answers "are the pixels bit-identical", NOT the question its name asks
    /// ("KeptTheDrawnPixels"). It cannot distinguish a one-count rounding shift from a broken render.</para>
    ///
    /// <para><b>If this constant ever needs updating again</b>, the display pipeline's formula changed
    /// again — find and cite that change first. A hash that moved with no such change is a regression.</para>
    /// </summary>
    private const string PreviewBaselineHash = "f2a5640e9bd1a7fc";

    // ---- P-11: the tile signature that answers what the hash cannot (#200, GUI-C-138) -----------

    /// <summary>32×32 tiles over the 1024×1024 render — 1024 numbers, 10,842 bytes as stored.</summary>
    private const int SignatureTilesPerSide = 32;

    /// <summary>
    /// The largest mean absolute per-tile difference P-11 accepts, and the largest single-tile one.
    ///
    /// <para><b>Where these came from (GUI-C-138 §1).</b> Both are derived from ONE observation: the
    /// legitimate <c>#156</c> VOI-placement correction, rendered twice on P-10's own frame and settings
    /// with the pre- and post-<c>efd14c1</c> <c>xpe_display.dll</c>. That correction moves the pixels by
    /// design, so it is the largest change this assertion must stay quiet for:</para>
    /// <code>
    /// legit #156, this frame, this moment : mean|dtile| 0.001321   max|dtile| 0.043945  (119/1024 tiles moved)
    /// nearest breakage (one row zeroed)   : mean|dtile| 0.039853   max|dtile| 3.999023
    /// </code>
    /// <para>The thresholds sit between them: 7.6× the legit mean, 6.8× the legit max.</para>
    ///
    /// <para><b>The measurement moment is part of the provenance.</b> The render dump is
    /// last-render-wins, and every scenario here restores the defaults in a <c>finally</c> — which
    /// renders again. A first attempt recorded the signature from a dump copied after the test process
    /// exited, i.e. from the teardown render, and it disagreed with the measured render by
    /// <c>mean|dtile| 13.06</c>. The recorded file is read at the assertion moment, from the same call
    /// this test compares.</para>
    ///
    /// <para><b>This assertion deliberately stays quiet for a <c>#156</c>-scale correction</b>, which is
    /// the opposite of what P-10's pinned hash does: the hash goes red on a one-count rounding shift
    /// (measured — <c>36fc547e253b07f1</c> with the pre-<c>efd14c1</c> DLL against
    /// <c>f2a5640e9bd1a7fc</c> with the post one). P-11 answers the question P-10's NAME asks, not the
    /// question its hash asks. The two are not interchangeable, and removing the hash is a decision
    /// about which question the suite should ask — not a consequence of this test existing.</para>
    ///
    /// <para><b>The legitimate side is n=1.</b> Breakages can be synthesised without limit, so the upper
    /// boundary is well characterised; the lower one rests on a single real correction, on a single
    /// frame. Raising the frame count would not help — it would apply the SAME correction to more
    /// pixels. What fills this axis is the NEXT genuine display-formula correction, and that correction
    /// is the real test of these two numbers.</para>
    ///
    /// <para><b>A python model of the formula was measured first and was wrong by 108×</b>
    /// (<c>mean|dtile| 0.000019</c>, which would have put the threshold at <c>0.001</c> — a value the
    /// real correction already exceeds). The thresholds above are from rendered pixels only. Why the
    /// model under-predicted is not established; treat its numbers as an approximation that does not
    /// reproduce the render path.</para>
    ///
    /// <para><b>What this signature cannot see</b> (GUI-C-138 §2, measured on this frame): a change that
    /// preserves each tile's mean. Alternating <c>+k/-k</c> within one tile is missed up to
    /// <c>k = 2</c> (<c>k = 2</c> reaches <c>max|dtile| 0.279297</c>, just inside the 0.300 wall) and a
    /// half-tile <c>+k</c> / half-tile <c>-k</c> split is missed up to <c>k = 4</c>. A per-tile standard
    /// deviation was measured as a second statistic and rejected: it is not monotone in <c>k</c>
    /// (half-block <c>k = 2</c> gives a SMALLER signal than <c>k = 1</c>), so no threshold can be set on
    /// it.</para>
    ///
    /// <para><b>If this assertion fails, do NOT re-record the signature to make it pass.</b> Re-recording
    /// is what turns this test into a rubber stamp, and a rubber stamp is worse than no test. Instead:
    /// find the change that moved the pixels and decide whether it is a correction or a regression. If
    /// it is a correction, re-derive BOTH thresholds by rendering that correction's before and after —
    /// as this comment records for <c>#156</c> — and write the new numbers here with their provenance.
    /// A signature re-recorded without that measurement carries no information.</para>
    /// </summary>
    private const double PreviewTileSignatureTolerance = 0.010;

    /// <summary>The per-tile ceiling. Provenance and update procedure: <see cref="PreviewTileSignatureTolerance"/>.</summary>
    private const double PreviewTileSignatureMaxTolerance = 0.300;

    /// <summary>
    /// P-11 (#200, GUI-C-138): the drawn pixels still carry the recorded per-tile brightness pattern.
    ///
    /// <para>This exists because a hash answers a different question than P-10's name asks. A hash says
    /// "bit-identical or not" — it cannot separate a one-count rounding shift from a broken render, and
    /// it cannot say how far off a failing render is. Whole-frame summary statistics fail the other way:
    /// a vertical flip is a PERMUTATION, permutations preserve the value multiset, and mean, standard
    /// deviation, every percentile and the non-zero fraction are functions of that multiset alone — so
    /// those statistics are provably identical across a flip. That is an identity, not a measurement.
    /// A tile mean is bound to a position, which is what makes it able to see the flip (measured:
    /// <c>mean|dtile| 32.895002</c>).</para>
    /// </summary>
    [SkippableFact]
    public void P11_ThePreviewRender_StillCarriesTheRecordedTileSignature()
    {
        var window = Ready();
        try
        {
            SetMode(window, "GsvgModeVirtualGrid");
            SetNumber(window, "GsvgDenoiseKInput", "0");
            SetNumber(window, "GsvgPyramidGainInput", "1.0");
            SetNumber(window, "GsvgPyramidLevelsInput", "0");
            SetNumber(window, "GsvgGridFrequencyInput", "60");;;
            SetNumber(window, "GsvgAirSignalInput", "60000");
            MeasureRender(window);

            var drawn = ReadTileSignatureFromRenderDump();
            // Writes the drawn signature where asked, for the ONE case that needs it: re-deriving the
            // thresholds from a legitimate correction's before and after (the procedure in
            // PreviewTileSignatureTolerance). It exists because the signature can only be read at this
            // moment — a dump copied after the process exits holds the teardown render, which is how the
            // first recording came out wrong by mean|dtile| 13.06. Writing a new file is NOT the fix for
            // a failing assertion: without a fresh threshold derivation the new file asserts nothing.
            if (Environment.GetEnvironmentVariable("XPE_GUI_C138_RECORD") is { Length: > 0 } recordPath)
            {
                File.WriteAllLines(recordPath, drawn.Select(v => v.ToString("0.000000", CultureInfo.InvariantCulture)));
            }

            var recorded = ReadRecordedTileSignature();
            Assert.Equal(recorded.Length, drawn.Length);

            var deltas = new double[recorded.Length];
            for (var i = 0; i < recorded.Length; i++)
            {
                deltas[i] = Math.Abs(drawn[i] - recorded[i]);
            }

            var mean = deltas.Average();
            var max = deltas.Max();
            var worstTile = Array.IndexOf(deltas, max);
            output.WriteLine(
                $"P11 tiles={deltas.Length} mean|dtile|={mean:0.000000} max|dtile|={max:0.000000} " +
                $"worst tile={worstTile} (row {worstTile / SignatureTilesPerSide}, " +
                $"col {worstTile % SignatureTilesPerSide})");

            // The message names the measurement and the procedure, NOT "update the file": see
            // PreviewTileSignatureTolerance for why re-recording without a fresh derivation is refused.
            Assert.True(
                mean <= PreviewTileSignatureTolerance && max <= PreviewTileSignatureMaxTolerance,
                $"THE PIXELS MOVED BEYOND THE TOLERANCE — likely a breakage, not a rounding shift. "
              + $"mean|dtile|={mean:0.000000} (<= {PreviewTileSignatureTolerance:0.000}), "
              + $"max|dtile|={max:0.000000} (<= {PreviewTileSignatureMaxTolerance:0.000}), "
              + $"worst tile {worstTile}. This is the magnitude question that "
              + "P10_ThePreviewChange_KeptTheDrawnPixels cannot answer: its hash goes red on any change "
              + "at all, while this threshold was derived from a legitimate correction, so exceeding it "
              + "means the change is larger than one of those. "
              + "Find the change that moved the pixels before touching this test. If it is a legitimate "
              + "correction, render its before and after, re-derive BOTH thresholds from that "
              + "measurement, and record the numbers with their provenance in "
              + "PreviewTileSignatureTolerance — the thresholds currently rest on ONE observation "
              + "(#156, mean 0.001321 / max 0.043945). Re-recording the signature to make this pass "
              + "removes the only evidence that would identify the cause.");
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
    /// The 1024 tile means of the render the app last drew, read from the dump the fixture asked for.
    ///
    /// <para>Skips rather than fails when the dump is absent: a missing dump means the app was launched
    /// without the switch or the render never reached the bitmap branch, which is a harness condition,
    /// not a statement about the pixels. Reporting it as a failure would read as a defect in the app.</para>
    /// </summary>
    private static double[] ReadTileSignatureFromRenderDump()
    {
        var path = Wrist1024SliceApplicationFixture.RenderDumpPath;
        Skip.IfNot(File.Exists(path), $"No render dump at {path}; the app was launched without --automation-export-render.");

        using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
        var header = new List<byte>();
        int read;
        while ((read = stream.ReadByte()) is not -1 and not (byte)'\n')
        {
            header.Add((byte)read);
        }

        var fields = System.Text.Encoding.ASCII.GetString(header.ToArray()).Split(' ');
        Assert.Equal("XPEBGRA", fields[0]);
        var width = int.Parse(fields[1], CultureInfo.InvariantCulture);
        var height = int.Parse(fields[2], CultureInfo.InvariantCulture);

        var pixels = new byte[checked(width * height * 4)];
        var filled = 0;
        while (filled < pixels.Length)
        {
            var got = stream.Read(pixels, filled, pixels.Length - filled);
            Assert.True(got > 0, $"The dump ended after {filled} of {pixels.Length} pixel bytes.");
            filled += got;
        }

        // One channel is enough and this was checked rather than assumed: across all 1,048,576 pixels
        // of both measured renders B==G==R held and A was 255 everywhere (GUI-C-138 §0).
        var tileHeight = height / SignatureTilesPerSide;
        var tileWidth = width / SignatureTilesPerSide;
        var signature = new double[SignatureTilesPerSide * SignatureTilesPerSide];
        for (var ty = 0; ty < SignatureTilesPerSide; ty++)
        {
            for (var tx = 0; tx < SignatureTilesPerSide; tx++)
            {
                long total = 0;
                for (var y = ty * tileHeight; y < (ty + 1) * tileHeight; y++)
                {
                    var row = (y * width + tx * tileWidth) * 4;
                    for (var x = 0; x < tileWidth; x++)
                    {
                        total += pixels[row + (x * 4)];
                    }
                }

                signature[(ty * SignatureTilesPerSide) + tx] = (double)total / (tileHeight * tileWidth);
            }
        }

        return signature;
    }

    /// <summary>The recorded signature, embedded in this assembly so a skipped copy step cannot stale it.</summary>
    private static double[] ReadRecordedTileSignature()
    {
        const string resource = "ImageProcTest.E2ETests.Fixtures.P10PreviewTileSignature.txt";
        using var stream = typeof(GsvgWristSliceScenarios).Assembly.GetManifestResourceStream(resource)
            ?? throw new InvalidOperationException($"Embedded resource {resource} is missing.");
        using var reader = new StreamReader(stream);

        var values = new List<double>();
        while (reader.ReadLine() is { } line)
        {
            if (line.Length is 0 || line[0] is '#')
            {
                continue;
            }

            values.Add(double.Parse(line, CultureInfo.InvariantCulture));
        }

        Assert.Equal(SignatureTilesPerSide * SignatureTilesPerSide, values.Count);
        return [.. values];
    }

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
