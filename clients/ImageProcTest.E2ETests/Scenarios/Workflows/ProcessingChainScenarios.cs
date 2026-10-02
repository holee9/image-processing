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

    /// <summary>
    /// C-03 (GUI-C-101): the GSVG stage. Native must draw different pixels than the same frame without it
    /// (or say why not, in the chain status); Mock has no gsvg.dll and must say RequestedNotApplied.
    /// </summary>
    [SkippableTheory]
    [InlineData("GsvgModeGridSuppression", "GridSuppression")]
    [InlineData("GsvgModeVirtualGrid", "VirtualGrid")]
    public void C03_GsvgStage_ReachesTheDrawnPixelsOrSaysWhyNot(string radioId, string mode)
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        var window = app.MainWindow!;
        CloseDetached(window);

        try
        {
            SetGsvgMode(window, "GsvgModeNone");
            ApplyDisplayPipeline(window);
            var off = WaitForChain(window, "gsvg=NotRequested");
            var offHash = DrawnHash(window);
            output.WriteLine($"C03 {mode} off: chain='{off}' hash={offHash}");

            SetGsvgMode(window, radioId);
            ApplyDisplayPipeline(window);

            if (app.BackendMode != "Native")
            {
                var refused = WaitForChain(window, "gsvg=RequestedNotApplied");
                output.WriteLine($"C03 {mode} mock: chain='{refused}' hash={DrawnHash(window)}");
                Assert.Equal(offHash, DrawnHash(window));
                return;
            }

            var status = WaitForChainStage(window, "gsvg");
            var onHash = DrawnHash(window);
            output.WriteLine($"C03 {mode} native: chain='{status}' hash={onHash}");

            // What the module did is read from the status, not assumed: Applied must change the drawn
            // pixels, and any other status must leave them alone.
            if (Regex.IsMatch(status, @"gsvg=Applied\b"))
            {
                Assert.NotEqual(offHash, onHash);
            }
            else
            {
                Assert.Equal(offHash, onHash);
                Assert.Contains("gsvg=", status, StringComparison.Ordinal);
            }
        }
        finally
        {
            SetGsvgMode(window, "GsvgModeNone");
            ApplyDisplayPipeline(window);
        }
    }

    /// <summary>C-04 (GUI-C-101): a missing virtual-grid table is refused, with the path in the reason, and the image is the one from before.</summary>
    [SkippableFact]
    public void C04_MissingVirtualGridTable_IsRefusedAndTheImageIsUnchanged()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        Skip.If(app.BackendMode != "Native", "gsvg.dll only runs on the native backend.");
        var window = app.MainWindow!;
        CloseDetached(window);

        try
        {
            SetGsvgMode(window, "GsvgModeNone");
            ApplyDisplayPipeline(window);
            var offHash = DrawnHash(window);

            SetText(window, "GsvgTablePathInput", @"D:\no\such\table.csv");
            SetGsvgMode(window, "GsvgModeVirtualGrid");
            ApplyDisplayPipeline(window);

            var status = WaitForChain(window, "gsvg=RequestedNotApplied");
            output.WriteLine($"C04 chain='{status}' hash={DrawnHash(window)}");
            Assert.Equal(offHash, DrawnHash(window));
        }
        finally
        {
            SetText(window, "GsvgTablePathInput", string.Empty);
            SetGsvgMode(window, "GsvgModeNone");
            ApplyDisplayPipeline(window);
        }
    }

    /// <summary>
    /// C-06 (GUI-C-101): the grid ratio reaches the module — two ratios from the table's [grid] section
    /// draw different pixels. Without this, "the virtual grid ran" would not say whether its settings did.
    /// </summary>
    [SkippableFact]
    public void C06_ChangingTheGridRatio_ChangesTheDrawnPixels()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        Skip.If(app.BackendMode != "Native", "gsvg.dll only runs on the native backend.");
        var window = app.MainWindow!;
        CloseDetached(window);

        try
        {
            SetNumber(window, "GsvgGridRatioInput", "10");
            SetGsvgMode(window, "GsvgModeVirtualGrid");
            ApplyDisplayPipeline(window);
            var at10Status = WaitForChainStage(window, "gsvg");
            var at10 = DrawnHash(window);
            Assert.True(Regex.IsMatch(at10Status, @"gsvg=Applied\b"), $"The virtual grid did not apply at ratio 10: {at10Status}");

            SetNumber(window, "GsvgGridRatioInput", "6");
            ApplyDisplayPipeline(window);
            var at6Status = WaitForChainStage(window, "gsvg");
            var at6 = DrawnHash(window);
            output.WriteLine($"C06 ratio 10 -> {at10}, ratio 6 -> {at6}; status='{at6Status}'");
            Assert.True(Regex.IsMatch(at6Status, @"gsvg=Applied\b"), $"The virtual grid did not apply at ratio 6: {at6Status}");
            Assert.NotEqual(at10, at6);
        }
        finally
        {
            SetNumber(window, "GsvgGridRatioInput", "10");
            SetGsvgMode(window, "GsvgModeNone");
            ApplyDisplayPipeline(window);
        }
    }

    /// <summary>
    /// C-07 (GUI-C-101): a grid ratio the table does not list is refused by the module, and the GUI says so
    /// instead of showing the image as corrected. This is the case a status that ignored the module's
    /// reason would get wrong.
    /// </summary>
    [SkippableFact]
    public void C07_AGridRatioOutsideTheTable_IsRefused()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        Skip.If(app.BackendMode != "Native", "gsvg.dll only runs on the native backend.");
        var window = app.MainWindow!;
        CloseDetached(window);

        try
        {
            SetGsvgMode(window, "GsvgModeNone");
            ApplyDisplayPipeline(window);
            var offHash = DrawnHash(window);

            // The product table lists 6, 8, 10 and 12.
            SetNumber(window, "GsvgGridRatioInput", "7");
            SetGsvgMode(window, "GsvgModeVirtualGrid");
            ApplyDisplayPipeline(window);

            var status = WaitForChain(window, "gsvg=RequestedNotApplied");
            output.WriteLine($"C07 chain='{status}' hash={DrawnHash(window)}");
            Assert.Equal(offHash, DrawnHash(window));
        }
        finally
        {
            SetNumber(window, "GsvgGridRatioInput", "10");
            SetGsvgMode(window, "GsvgModeNone");
            ApplyDisplayPipeline(window);
        }
    }

    /// <summary>C-05 (GUI-C-101): the three grid-correction choices are exclusive — the module refuses two at once.</summary>
    [SkippableFact]
    public void C05_GridCorrectionChoices_AreExclusive()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        var window = app.MainWindow!;
        CloseDetached(window);

        try
        {
            foreach (var chosen in new[] { "GsvgModeGridSuppression", "GsvgModeVirtualGrid", "GsvgModeNone" })
            {
                SetGsvgMode(window, chosen);
                var states = new[] { "GsvgModeNone", "GsvgModeGridSuppression", "GsvgModeVirtualGrid" }
                    .Select(id => (Id: id, Checked: Radio(window, id).IsChecked))
                    .ToArray();

                output.WriteLine($"C05 after {chosen}: " + string.Join(", ", states.Select(s => $"{s.Id}={s.Checked}")));
                Assert.Single(states.Where(s => s.Checked));
                Assert.True(states.Single(s => s.Checked).Id == chosen, $"'{chosen}' was chosen but '{states.Single(s => s.Checked).Id}' is checked.");
            }
        }
        finally
        {
            SetGsvgMode(window, "GsvgModeNone");
            ApplyDisplayPipeline(window);
        }
    }

    /// <summary>
    /// C-08 (#225 row 10, GUI-C-184, GUI-C-185): the AI menu entry with NO model file. The model directory is pointed at a
    /// path that does not exist, so the answer does not depend on the working directory. Mock has no AI module and says so.
    /// Native looks for the file before it asks the module (the same -9 comes back for a missing model and for a worker
    /// that could not be started, so the code could not tell them apart): the reason is "not attempted: no model at
    /// &lt;the path&gt;", no worker is started, no failure is counted. C-09 is the case where the module IS asked. In both
    /// the stage must be RequestedNotApplied, the drawn pixels must be the ones from before, and nothing may say
    /// "AI-processed". A failed call returns the input unchanged, so "the pixels did not change" alone cannot
    /// tell a failure from a success on a flat image; the STATUS is what is asserted, and the hash is asserted too
    /// (the image on screen is the original).
    ///
    /// <para>This pins the failure path. It is red on purpose if the Native job ever stages an inference build: the
    /// success path then needs a case of its own with a model, which this card does not have.</para>
    /// </summary>
    [SkippableFact]
    public void C08_AiBoneSuppression_WhereTheModuleCannotSucceed_IsRefusedAndTheImageIsTheOriginal()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        var window = app.MainWindow!;
        CloseDetached(window);

        var missingDirectory = Path.Combine(Path.GetTempPath(), $"xpe-ai-nomodel-{Environment.ProcessId}-{Guid.NewGuid():N}");
        try
        {
            SetText(window, "AiModelDirectoryInput", missingDirectory);
            SetAiStage(window, false);
            ApplyDisplayPipeline(window);
            var before = WaitForChain(window, "ai_bone_suppress=NotRequested");
            var beforeHash = DrawnHash(window);
            output.WriteLine($"C08 before: chain='{before}' hash={beforeHash}");
            Assert.NotEqual("-", beforeHash);

            RequestAiStage(window);

            var after = WaitForChain(window, "ai_bone_suppress=RequestedNotApplied");
            var afterHash = DrawnHash(window);
            output.WriteLine($"C08 after (backend {app.BackendMode}): chain='{after}' hash={afterHash}");

            Assert.DoesNotContain("AI-processed", after, StringComparison.Ordinal);
            Assert.Equal(beforeHash, afterHash);

            if (app.BackendMode == "Native")
            {
                // The file check ran before the module was asked: no code, because no module call was made.
                Assert.Contains("ai_bone_suppress: AI bone suppression not attempted: no model at", after, StringComparison.Ordinal);
                Assert.Contains(Path.Combine(missingDirectory, "bone_suppress.onnx"), after, StringComparison.Ordinal);
            }
            else
            {
                Assert.Contains("ai_bone_suppress: AI bone suppression requires the native backend", after, StringComparison.Ordinal);
                // The state line the app publishes for automation (C-09 reads it too): a backend with no AI session says Unknown.
                Assert.Equal("worker=Unknown", AiStatusSummary(window));
                // The diagnostics channel C-09 reads on Native (GUI-C-189) is empty without a native session, and reading it works
                // on the real app: this is where the helper itself is exercised on every Mock run.
                Assert.Equal(string.Empty, AiDiagnostics(window));
            }
        }
        finally
        {
            SetAiStage(window, false);
            SetText(window, "AiModelDirectoryInput", string.Empty);
            ApplyDisplayPipeline(window);
        }
    }

    /// <summary>
    /// C-09 (#225 row 10, GUI-C-185): the module IS asked (a file named bone_suppress.onnx exists, though it is not a model),
    /// the worker keeps failing, and the persistent mark appears when the module reports the worker switched off
    /// (<c>xpe_ai_worker_state</c>, not an alert); the Restart AI button then starts a new session and the mark goes.
    /// Each failed call must carry a return code that came from the module, so a job that did not stage xpe_ai.dll cannot
    /// pass. The numbers in the mark are the module's: the test requires them equal (the ceiling was reached) and does
    /// not write a 3. Native only: the Mock has no AI session. Not run on a developer machine (the native E2E job's).
    /// </summary>
    [SkippableFact]
    public void C09_AWorkerSwitchedOffByRepeatedFailures_ShowsAMark_ThatRestartRemoves()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        Skip.If(app.BackendMode != "Native", "The AI session exists only on the native backend.");
        var window = app.MainWindow!;
        CloseDetached(window);

        var directory = Path.Combine(Path.GetTempPath(), $"xpe-ai-e2e-{Environment.ProcessId}-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        File.WriteAllText(Path.Combine(directory, "bone_suppress.onnx"), "not a model");
        var originalWidth = 0;
        var bodyCompleted = false;
        try
        {
            // GUI-C-191: the mark has to be reachable at the narrowest window the app allows (MinWidth 1280). It sat in a ToolBar, and below
            // about 1400 px the ToolBar sent it to its overflow popup, which is neither on screen nor in the automation tree; this run's
            // window width is whatever the runner gives it, so the width is set here instead of left to the machine.
            originalWidth = (int)window.BoundingRectangle.Width;
            var minimum = WindowMinimumWidth.ResizeToMinimum(window);
            output.WriteLine($"C09 window width before: {originalWidth}; at the minimum: {minimum.Describe()}");
            // GUI-C-191b: "resized" is not "at the minimum". A window with no Transform pattern, or one that stopped somewhere else, would
            // run the rest of this scenario at whatever width it had and say nothing about the narrow case, so it FAILS here.
            Assert.True(minimum.Problem is null, "C-09 is not running at the window's minimum width: " + minimum.Describe());

            SetText(window, "AiModelDirectoryInput", directory);
            SetAiStage(window, false);
            ApplyDisplayPipeline(window);
            Assert.Null(AiBanner(window));

            var shown = false;
            var seen = new List<string>();   // what the app said after each attempt (GUI-C-189): the failure message carries it
            for (var attempt = 1; attempt <= 6 && !shown; attempt++)
            {
                InvokeAiMenuItem(window, requireEnabled: true);
                var status = WaitForChain(window, "ai_bone_suppress=RequestedNotApplied");
                output.WriteLine($"C09 attempt {attempt}: chain='{status}'");
                Assert.Matches(@"ai_bone_suppress: AI bone suppression (NOT applied|not attempted) \(code -?\d+", status);
                Assert.DoesNotContain("was not found", status, StringComparison.Ordinal);
                Assert.DoesNotContain("no model at", status, StringComparison.Ordinal);
                // The render that follows the click, and the state read after it. Waited for as an event (the banner appearing) inside the same
                // 2.5 s bound it always had: a banner that is there ends the wait at once, one that is not costs the full bound as before.
                // GUI-C-191b: how long after the chain text the banner takes, logged every attempt: the 2.5 s bound has to come from what the
                // native runs measure, and it is not raised here.
                var sinceChain = System.Diagnostics.Stopwatch.StartNew();
                shown = PollFor(() => AiBanner(window) is not null, TimeSpan.FromMilliseconds(2500));
                var bannerAfterChain = shown ? $"{sinceChain.ElapsedMilliseconds} ms" : $"not within {sinceChain.ElapsedMilliseconds} ms";

                // Diagnostics only: nothing here is asserted. The summary is the GUI's reading of the module's state; the diagnostics
                // are the module's own raw answers (its state before this process's init, the init call, each read), so a red run says
                // whether the module was ever on the worker path and what it reported, not only that no mark appeared.
                var summary = AiStatusSummary(window);
                string diagnostics;
                try
                {
                    diagnostics = AiDiagnostics(window);
                }
                catch (Exception ex)
                {
                    // Reading the diagnostics must never be what turns this run red for another reason: say it could not be read.
                    diagnostics = $"(unreadable: {ex.GetType().Name}: {ex.Message})";
                }

                var line = $"attempt {attempt}: banner={(shown ? "shown" : "absent")} (after the chain text: {bannerAfterChain}); summary='{summary}'; diagnostics='{diagnostics}'";
                seen.Add(line);
                output.WriteLine($"C09 {line}");
            }

            var banner = AiBanner(window);
            // GUI-C-191: the failure says what was READ, not what it guesses. The first native run said "the module never reported the
            // worker switched off" over a line that read `worker=Disabled`: the module HAD said so, and the mark was missing on screen.
            var lastSummary = AiStatusSummary(window);
            var width = (int)window.BoundingRectangle.Width;
            Assert.True(banner is not null,
                (lastSummary.StartsWith("worker=Disabled", StringComparison.Ordinal)
                    ? $"The app's own status says the worker is switched off ('{lastSummary}'), and the mark is not in the automation tree (window width {width}). "
                    : $"After six attempts the app never read the worker as switched off (last status '{lastSummary}'; window width {width}). ")
                + "What the app said after each attempt: " + string.Join(" | ", seen));
            var text = banner!.Name;
            output.WriteLine($"C09 mark: '{text}'");
            var numbers = Regex.Match(text, @"after (\d+) of (\d+) failures");
            Assert.True(numbers.Success, $"The mark does not carry the module's counts: '{text}'.");
            Assert.Equal(numbers.Groups[2].Value, numbers.Groups[1].Value);

            // The state is also readable as one line from the AI checkbox's help text (automation property; the same channel the
            // viewport's drawn-pixel hash uses): the same numbers the mark carries, from the module.
            var before = AiStatusSummary(window);
            output.WriteLine($"C09 state before restart: '{before}'");
            Assert.Matches(@"worker=Disabled; failures=(\d+); ceiling=\1$", before);

            var restart = window.FindFirstDescendant(cf => cf.ByAutomationId("AiRestartButton"));
            Assert.True(restart is not null, "The mark is shown but the Restart AI button is not.");
            restart!.AsButton().Invoke();

            var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(15);
            while (DateTime.UtcNow < deadline && AiBanner(window) is not null) Thread.Sleep(200);
            Assert.True(AiBanner(window) is null,
                $"The mark is still shown after Restart AI: '{AiBanner(window)?.Name}' (status bar: '{StatusText(window)}').");

            // "The mark is gone" is also what a FAILED restart looked like before Codex #24 B1 (the state fell to Unknown, which
            // shows nothing). So the answer and the state are read as well: the success line, then the module's own report of an
            // active worker with no failures in a row.
            var answer = StatusText(window);
            var after = AiStatusSummary(window);
            output.WriteLine($"C09 after restart: status bar='{answer}'; state='{after}'");
            // GUI-C-192d: the numbers the next native CI log is read for (nothing here is asserted). The status refresher's own measurements ride
            // in the diagnostics line: the slowest status read, the longest a shown answer waited (against boundMs: a run that gets near it is the
            // false alarm to look for), the 5 s re-reads, and how many notices were raised. Real AI frames ran in this scenario.
            string measured;
            try
            {
                var diagnostics = AiDiagnostics(window);
                var at = diagnostics.IndexOf("refresher:", StringComparison.Ordinal);
                measured = at >= 0 ? diagnostics[at..] : "(no refresher measurements in the diagnostics line: '" + diagnostics + "')";
            }
            catch (Exception ex)
            {
                measured = $"(unreadable: {ex.GetType().Name}: {ex.Message})";
            }

            output.WriteLine($"C09 status-refresher measurements: {measured}");
            Assert.Contains("AI session restarted", answer, StringComparison.Ordinal);
            Assert.Matches(@"^worker=Active; failures=0; ceiling=\d+$", after);
            bodyCompleted = true;
        }
        finally
        {
            // GUI-C-192: the shared window must go back to its width whatever else the clean-up does. The width is restored FIRST, and every
            // step is guarded on its own: a UI call that throws here (an element gone, a pattern refused) used to end the clean-up before the
            // restore, and the next scenario then ran in a window left at the minimum width. A failure of the scenario itself is never
            // replaced by a clean-up failure; a clean-up failure after a passing scenario is reported, not swallowed.
            var cleanupErrors = new List<Exception>();

            void Guard(string step, Action action)
            {
                try
                {
                    action();
                }
                catch (Exception ex)
                {
                    cleanupErrors.Add(new InvalidOperationException($"C-09 clean-up step '{step}' failed: {ex.GetType().Name}: {ex.Message}", ex));
                    output.WriteLine($"C09 clean-up step '{step}' failed: {ex.GetType().Name}: {ex.Message}");
                }
            }

            if (originalWidth > 0)
            {
                Guard("restore the window width", () => ResizeTo(window, originalWidth));
            }

            Guard("press a leftover Restart AI", () =>
            {
                if (window.FindFirstDescendant(cf => cf.ByAutomationId("AiRestartButton")) is { } leftover)
                {
                    leftover.AsButton().Invoke();
                    Thread.Sleep(600);
                }
            });
            Guard("switch the AI stage off", () => SetAiStage(window, false));
            Guard("clear the AI model directory", () => SetText(window, "AiModelDirectoryInput", string.Empty));
            Guard("apply the display pipeline", () => ApplyDisplayPipeline(window));
            Guard("delete the temporary model directory", () => Directory.Delete(directory, recursive: true));

            if (bodyCompleted && cleanupErrors.Count > 0)
            {
                throw new AggregateException("C-09 passed, but its clean-up of the shared window failed.", cleanupErrors);
            }
        }
    }

    /// <summary>Polls for <paramref name="condition"/> and returns at once when it holds; false when it did not within <paramref name="limit"/>.</summary>
    private static bool PollFor(Func<bool> condition, TimeSpan limit)
    {
        var deadline = DateTime.UtcNow + limit;
        while (true)
        {
            if (condition())
            {
                return true;
            }

            if (DateTime.UtcNow >= deadline)
            {
                return false;
            }

            Thread.Sleep(100);
        }
    }

    /// <summary>Resizes the window through UI Automation, keeping its height; the width it ended up with, or -1 when the window cannot be resized.</summary>
    private static int ResizeTo(Window window, int width)
    {
        var transform = window.Patterns.Transform.PatternOrDefault;
        if (transform is null || !transform.CanResize)
        {
            return -1;
        }

        transform.Resize(width, window.BoundingRectangle.Height);
        PollFor(() => (int)window.BoundingRectangle.Width == width, TimeSpan.FromSeconds(3));
        return (int)window.BoundingRectangle.Width;
    }

    private static FlaUI.Core.AutomationElements.AutomationElement? AiBanner(Window window) =>
        window.FindFirstDescendant(cf => cf.ByAutomationId("AiWorkerBanner"));

    /// <summary>The worker state as the app publishes it for automation: the help text of the AI checkbox (Parameters tab).</summary>
    private static string AiStatusSummary(Window window)
    {
        OpenParameters(window);
        return window.FindFirstDescendant(cf => cf.ByAutomationId("AiBoneSuppressionInChainCheckBox"))?.HelpText ?? string.Empty;
    }

    /// <summary>
    /// What the native AI session saw, as the app publishes it for automation (the item status of the AI checkbox, GUI-C-189): the
    /// module's own raw answers. Empty without a native session. For failure messages; never asserted.
    /// </summary>
    private static string AiDiagnostics(Window window)
    {
        OpenParameters(window);
        return window.FindFirstDescendant(cf => cf.ByAutomationId("AiBoneSuppressionInChainCheckBox"))?.Properties.ItemStatus.ValueOrDefault ?? string.Empty;
    }

    private static string StatusText(Window window) =>
        window.FindFirstDescendant(cf => cf.ByAutomationId("StatusBarText"))?.Name ?? string.Empty;

    /// <summary>
    /// GUI-C-198: asks for the AI stage the way a user does, through the menu entry. Where the entry is disabled (the Mock backend, or a native run without xpe_ai.dll; the
    /// rule is E-01's) the same request is made through the "AI bone suppression in chain" setting, which reaches the same stage and the same refusal.
    /// </summary>
    private static void RequestAiStage(Window window)
    {
        if (AiMenuItemIsEnabled(window))
        {
            InvokeAiMenuItem(window);
            return;
        }

        SetAiStage(window, true);
        ApplyDisplayPipeline(window);
    }

    private static bool AiMenuItemIsEnabled(Window window)
    {
        var menu = window.FindFirstDescendant(cf => cf.ByAutomationId("PipelineMenu"));
        Assert.True(menu is not null, "PipelineMenu was not found.");
        menu!.AsMenuItem().Expand();
        try
        {
            FlaUI.Core.AutomationElements.AutomationElement? item = null;
            for (var attempt = 0; attempt < 20 && item is null; attempt++)
            {
                item = window.FindFirstDescendant(cf => cf.ByAutomationId("RunFullPipelineMenuItem"));
                if (item is null) Thread.Sleep(100);
            }

            Assert.True(item is not null, "RunFullPipelineMenuItem did not appear under PipelineMenu.");
            return item!.IsEnabled;
        }
        finally
        {
            try { menu.AsMenuItem().Collapse(); } catch (Exception) { /* already closed */ }
        }
    }

    private static void InvokeAiMenuItem(Window window, bool requireEnabled = false)
    {
        var menu = window.FindFirstDescendant(cf => cf.ByAutomationId("PipelineMenu"));
        Assert.True(menu is not null, "PipelineMenu was not found.");
        menu!.AsMenuItem().Expand();

        FlaUI.Core.AutomationElements.AutomationElement? item = null;
        for (var attempt = 0; attempt < 20 && item is null; attempt++)
        {
            item = window.FindFirstDescendant(cf => cf.ByAutomationId("RunFullPipelineMenuItem"));
            if (item is null) Thread.Sleep(100);
        }

        Assert.True(item is not null, "RunFullPipelineMenuItem did not appear under PipelineMenu.");
        Assert.True(!requireEnabled || item!.IsEnabled,
            "RunFullPipelineMenuItem is disabled, so the module is never asked: this scenario needs the native backend with xpe_ai.dll in the pinned native directory (GUI-C-198).");
        item!.AsMenuItem().Invoke();
        try { menu.AsMenuItem().Collapse(); } catch (Exception) { /* already closed */ }
        Thread.Sleep(400);
    }

    private static void SetAiStage(Window window, bool on)
    {
        OpenParameters(window);
        var box = window.FindFirstDescendant(cf => cf.ByAutomationId("AiBoneSuppressionInChainCheckBox"));
        Assert.True(box is not null, "AiBoneSuppressionInChainCheckBox is not in the Parameters tab.");
        box!.AsCheckBox().IsChecked = on;
        Thread.Sleep(200);
    }

    private static FlaUI.Core.AutomationElements.RadioButton Radio(Window window, string automationId)
    {
        OpenParameters(window);
        var element = window.FindFirstDescendant(cf => cf.ByAutomationId(automationId));
        Assert.True(element is not null, $"{automationId} is not in the Parameters tab.");
        return element!.AsRadioButton();
    }

    private static void SetGsvgMode(Window window, string radioId)
    {
        Radio(window, radioId).IsChecked = true;
        Thread.Sleep(250);
    }

    private static void SetText(Window window, string automationId, string value)
    {
        OpenParameters(window);
        var box = window.FindFirstDescendant(cf => cf.ByAutomationId(automationId));
        Assert.True(box is not null, $"{automationId} is not in the Parameters tab.");
        var input = box!.AsTextBox();
        input.Focus();
        input.Text = value;
        UiaInput.CommitByMovingFocus(window, input);
        Thread.Sleep(400);
    }

    /// <summary>Waits until the chain status names <paramref name="stageId"/> with any status.</summary>
    private static string WaitForChainStage(Window window, string stageId)
    {
        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(30);
        while (DateTime.UtcNow < deadline)
        {
            var text = ChainText(window);
            if (Regex.IsMatch(text, $@"\b{Regex.Escape(stageId)}=\w+")) return text;
            Thread.Sleep(200);
        }

        Assert.Fail($"The chain status never named '{stageId}'; it reads '{ChainText(window)}'.");
        return string.Empty;
    }

    private static void SetNumber(Window window, string automationId, string value)
    {
        OpenParameters(window);
        var box = window.FindFirstDescendant(cf => cf.ByAutomationId(automationId));
        Assert.True(box is not null, $"{automationId} is not in the Parameters tab.");
        var input = box!.AsTextBox();
        input.Focus();
        input.Text = value;
        UiaInput.CommitByMovingFocus(window, input);
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
