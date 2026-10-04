// GUI-C-212 (#249): the legacy diagnostic app says the same thing about preprocess on every tab.
using System.Diagnostics;
using FlaUI.Core;
using FlaUI.Core.AutomationElements;
using FlaUI.Core.Definitions;
using FlaUI.UIA3;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Legacy;

/// <summary>
/// GUI-C-211 measured on screen that <c>clients/ImageProcTest</c> told three stories about one module: the Diagnostics tab and the Calibration tab turned preprocess off because the
/// synthetic oracle failed, while the Evaluation tab said "Preprocess=ready" and enabled its switches (it asked <c>IsExportReady</c>, which ignores the oracle). They now ask one question
/// (<c>ModuleReadinessGrading.IsProcessingEnabled</c>). These scenarios read all three tabs of the REAL app and require the same answer from each, in both states:
/// the oracle passes (preprocess is enabled everywhere) and the oracle cannot run (it is blocked everywhere). The second state is produced by an environment fault, not by editing anything:
/// the app's temp path points at a file, so the oracle cannot create its calibration folder — exports are still ready, which is exactly the state that used to split the tabs.
///
/// <para>UIA patterns only (tab <c>SelectionItem.Select</c>, reading names and enabled flags): no key, no mouse. This is the DIAGNOSTIC app (the one with the readiness matrix), not the
/// user app the rest of this project drives; it is launched from <c>clients/ImageProcTest/bin/Debug/net8.0-windows</c> and needs <c>XPE_NATIVE_DIR</c> to name a folder with the native DLLs
/// (it finds the other modules' DLLs through the build tree; only xpe_common and xpe_preprocess matter here).</para>
/// </summary>
public sealed class LegacyPreprocessReadinessScenarios(ITestOutputHelper output)
{
    [SkippableFact]
    public void R01_AllThreeTabsEnablePreprocess_WhenTheOraclePasses()
    {
        using var app = LegacyApp.LaunchOrSkip(breakTemp: false);
        var view = app.ReadPreprocessOnEveryTab();
        output.WriteLine(view.Describe());

        Assert.True(view.Diagnostics, $"Diagnostics tab: {view.DiagnosticsEvidence}");
        Assert.True(view.Calibration, $"Calibration tab: {view.CalibrationEvidence}");
        Assert.True(view.Evaluation, $"Evaluation tab: {view.EvaluationEvidence}");
    }

    [SkippableFact]
    public void R02_AllThreeTabsBlockPreprocess_WhenTheOracleCannotRun_NotJustTwoOfThem()
    {
        using var app = LegacyApp.LaunchOrSkip(breakTemp: true);
        var view = app.ReadPreprocessOnEveryTab();
        output.WriteLine(view.Describe());

        // Exports are still ready in this state, so a tab that asks IsExportReady (the old Evaluation tab) says "ready" here.
        Assert.False(view.Diagnostics, $"Diagnostics tab: {view.DiagnosticsEvidence}");
        Assert.False(view.Calibration, $"Calibration tab: {view.CalibrationEvidence}");
        Assert.False(view.Evaluation, $"Evaluation tab: {view.EvaluationEvidence}");
    }

    /// <summary>
    /// GUI-C-219 (#249): while the oracle is still running in the background, the window says "checking" on every tab (neither ready nor failed), keeps preprocess blocked, and answers UIA
    /// calls promptly; when the verdict arrives it says "ready" on every tab. The oracle worker is delayed by <c>XPE_ORACLE_TEST_DELAY_MS</c> so the state is long enough to observe. The three tabs
    /// share ONE verdict: the worker log (<c>XPE_ORACLE_TEST_LOG</c>) has exactly one line after startup, and a second line only after the user's refresh.
    /// </summary>
    [SkippableFact]
    public void R03_WhileTheOracleRuns_EveryTabSaysChecking_AndTheWindowAnswers_ThenEveryTabSaysReady_FromOneRun()
    {
        var log = Path.Combine(Path.GetTempPath(), $"xpe_oracle_log_{Guid.NewGuid():N}.txt");
        var gate = Path.Combine(Path.GetTempPath(), $"xpe_oracle_gate_{Guid.NewGuid():N}.txt");
        try
        {
            // the worker waits for the gate file, so the "checking" state lasts until the test has read it (a fixed delay raced the slow UIA reads: 8 of 23 runs ended it too early)
            var env = new Dictionary<string, string> { ["XPE_ORACLE_TEST_GATE"] = gate, ["XPE_ORACLE_TEST_LOG"] = log };
            using var app = LegacyApp.LaunchOrSkip(breakTemp: false, extraEnvironment: env);

            // while the worker sleeps: "checking" everywhere, preprocess blocked, and the window is answering (this very read is the proof)
            var watch = Stopwatch.StartNew();
            var checking = app.ReadPreprocessWhileChecking();
            watch.Stop();
            output.WriteLine($"while checking ({watch.ElapsedMilliseconds} ms to read all three tabs): {checking}");
            Assert.True(checking.SmokeSaysChecking, $"Diagnostics smoke line: '{checking.Smoke}'");
            Assert.True(checking.MatrixSaysChecking, $"matrix row: '{checking.Row}'");
            Assert.False(checking.EvaluationReady, $"Evaluation: '{checking.StageModes}'; Offset switch enabled={checking.OffsetSwitchEnabled}");
            Assert.False(checking.OffsetSwitchEnabled, "the Offset switch must stay disabled while the oracle has not answered");
            Assert.Equal(0, checking.BlockingFindings);   // "checking" is not reported as a failure on the Calibration tab
            Assert.True(checking.CheckingFindings > 0, "the Calibration tab has no NATIVE-CHECKING finding: nothing says that the preprocess stages are blocked WHILE the oracle runs");   // GUI-C-219b

            File.WriteAllText(gate, "go");   // now the worker may run the oracle

            var view = app.ReadPreprocessOnEveryTab();
            output.WriteLine(view.Describe());
            Assert.True(view.Diagnostics, $"Diagnostics tab: {view.DiagnosticsEvidence}");
            Assert.True(view.Calibration, $"Calibration tab: {view.CalibrationEvidence}");
            Assert.True(view.Evaluation, $"Evaluation tab: {view.EvaluationEvidence}");

            Assert.Equal(1, File.ReadAllLines(log).Length);   // one oracle run served the startup, the diagnostics report and all three tabs
            app.ClickButtonOnTab("Diagnostics", "Refresh");   // the user's refresh asks again
            var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(60);
            while (DateTime.UtcNow < deadline && File.ReadAllLines(log).Length < 2) Thread.Sleep(300);
            Assert.Equal(2, File.ReadAllLines(log).Length);
        }
        finally
        {
            try { File.Delete(log); } catch (IOException) { }
            try { File.Delete(gate); } catch (IOException) { }
        }
    }

    /// <summary>
    /// GUI-C-219b (Codex #109): after the user presses "Refresh Modules" the three screens say the same thing. That button used to refresh only the module matrix (which read "checking") while the
    /// Diagnostics smoke line kept the previous "pass=True" and the Calibration tab turned to "not ready". Here the oracle has passed and every tab says so; the worker is then held at its gate,
    /// the button is pressed, and all three tabs must say "checking" (no stale pass, no "not ready"); when the gate opens, all three say "ready" again, from a second run.
    /// </summary>
    [SkippableFact]
    public void R04_RefreshModules_MakesAllThreeTabsSayChecking_ThenAllThreeSayReady()
    {
        var log = Path.Combine(Path.GetTempPath(), $"xpe_oracle_log_{Guid.NewGuid():N}.txt");
        var gate = Path.Combine(Path.GetTempPath(), $"xpe_oracle_gate_{Guid.NewGuid():N}.txt");
        try
        {
            File.WriteAllText(gate, "go");   // the first run is not held
            var env = new Dictionary<string, string> { ["XPE_ORACLE_TEST_GATE"] = gate, ["XPE_ORACLE_TEST_LOG"] = log };
            using var app = LegacyApp.LaunchOrSkip(breakTemp: false, extraEnvironment: env);

            var first = app.ReadPreprocessOnEveryTab();
            output.WriteLine("before the refresh: " + first.Describe());
            Assert.True(first.Diagnostics && first.Calibration && first.Evaluation, "the start state is not 'ready' on all three tabs: " + first.Describe());

            File.Delete(gate);                                       // the next worker waits at its gate
            app.ClickButtonOnTab("Diagnostics", "Refresh Modules");
            var checking = app.ReadPreprocessWhileChecking();
            output.WriteLine("after the refresh, oracle held: " + checking);
            Assert.True(checking.SmokeSaysChecking, $"Diagnostics smoke line after Refresh Modules: '{checking.Smoke}' (a stale pass shows here)");
            Assert.True(checking.MatrixSaysChecking, $"matrix row after Refresh Modules: '{checking.Row}'");
            Assert.Equal(0, checking.BlockingFindings);                       // not "not ready"
            Assert.True(checking.CheckingFindings > 0, "the Calibration tab shows no NATIVE-CHECKING finding after Refresh Modules");
            Assert.False(checking.EvaluationReady, $"Evaluation after Refresh Modules: '{checking.StageModes}'");
            Assert.False(checking.OffsetSwitchEnabled, "the Offset switch is enabled while the oracle has not answered");

            File.WriteAllText(gate, "go");
            var second = app.ReadPreprocessOnEveryTab();
            output.WriteLine("after the answer: " + second.Describe());
            Assert.True(second.Diagnostics && second.Calibration && second.Evaluation, "the three tabs do not agree on 'ready' after the second run: " + second.Describe());
            Assert.Equal(2, File.ReadAllLines(log).Length);
        }
        finally
        {
            try { File.Delete(log); } catch (IOException) { }
            try { File.Delete(gate); } catch (IOException) { }
        }
    }

    /// <summary>
    /// GUI-C-218: a launch that fails after the app has started leaves no app, no fault file and no automation object behind. The failure is made certain by giving the window a 1 ms budget
    /// (the app needs about half a second to show one); the check is on the system, not on the fixture's own bookkeeping.
    /// </summary>
    [SkippableFact]
    public void ALaunchThatFailsAfterTheAppStarted_LeavesNoAppAndNoFaultFileBehind()
    {
        var exe = FindLegacyExecutable();
        Skip.If(exe is null, "clients/ImageProcTest/bin/Debug/net8.0-windows/ImageProcTest.exe was not found: build clients/ImageProcTest first.");
        var before = AppProcessIds(exe!);
        var faultFilesBefore = Directory.GetFiles(Path.GetTempPath(), "xpe_not_a_directory_*").Length;

        Assert.ThrowsAny<Exception>(() => LegacyApp.LaunchOrSkip(breakTemp: true, windowTimeout: TimeSpan.FromMilliseconds(1)));

        var leaked = AppProcessIds(exe!).Except(before).ToList();
        foreach (var pid in leaked) { try { Process.GetProcessById(pid).Kill(entireProcessTree: true); } catch (ArgumentException) { } }   // do not let a failing run leak either
        Assert.True(leaked.Count == 0, $"a failed launch left {leaked.Count} app instance(s) running: {string.Join(", ", leaked)}");
        Assert.Equal(faultFilesBefore, Directory.GetFiles(Path.GetTempPath(), "xpe_not_a_directory_*").Length);
    }

    private static string? FindLegacyExecutable()
    {
        for (var dir = new DirectoryInfo(AppContext.BaseDirectory); dir is not null; dir = dir.Parent)
        {
            var candidate = Path.Combine(dir.FullName, "clients", "ImageProcTest", "bin", "Debug", "net8.0-windows", "ImageProcTest.exe");
            if (File.Exists(candidate)) return candidate;
        }

        return null;
    }

    private static HashSet<int> AppProcessIds(string exe) =>
        Process.GetProcessesByName(Path.GetFileNameWithoutExtension(exe))
            .Where(p => { try { return string.Equals(p.MainModule?.FileName, exe, StringComparison.OrdinalIgnoreCase); } catch (Exception) { return false; } })
            .Select(p => p.Id)
            .ToHashSet();

    private const string RefusalText = "the preprocess DLLs changed since they were checked, so nothing was run";

    /// <summary>
    /// GUI-C-226 (Codex #118, low): the real window, a real click, the real refusal. The app runs against a PRIVATE COPY of the native folder. Once the oracle has passed, a rebuild/redeploy is
    /// imitated on the copy (the loaded DLL is renamed away and a file with other content takes its name), and the "Run Selected" button of the Calibration tab is pressed through its UIA
    /// Invoke pattern (no key, no mouse). The window must say that the DLLs changed and that nothing was run, and the command's own text must stay as it was. The control is the same click
    /// with the DLL untouched: the command is reached (its own text changes) and the refusal is absent, so the refusal is about the changed file and not about the button.
    /// </summary>
    [SkippableFact]
    public void R05_APressedButton_IsRefusedOnScreen_WhenTheDllChangedAfterTheVerdict_AndNothingRuns()
    {
        var control = RunTheCommandAfter(changeDll: false);
        output.WriteLine($"control: command text '{control.CommandBefore}' -> '{control.CommandAfter}'; preview text '{control.PreviewText}'");
        Assert.NotEqual(control.CommandBefore, control.CommandAfter);                       // the click reached the command
        Assert.DoesNotContain(RefusalText, control.PreviewText, StringComparison.Ordinal);   // and was not refused

        var changed = RunTheCommandAfter(changeDll: true);
        output.WriteLine($"changed: command text '{changed.CommandBefore}' -> '{changed.CommandAfter}'; preview text '{changed.PreviewText}'");
        Assert.Contains(RefusalText, changed.PreviewText, StringComparison.Ordinal);         // the refusal is on screen
        Assert.Equal(changed.CommandBefore, changed.CommandAfter);                          // and the command did not run
    }

    private const string ReadyAgainText = "They were checked again and are ready now";
    private const string StillWaitingText = "They are checked again automatically";

    /// <summary>
    /// GUI-C-226b (Codex #122): the life of the refusal text. After a refusal (1) the notice stays on screen when other things happen to the preview text, and never comes back in its old form;
    /// (2) when the check the refusal started has succeeded the notice says so instead of telling the user to wait; (3) a command that goes ahead removes the notice AT ONCE, even when that
    /// command writes nothing else to the preview text. UIA patterns only (tab select, Invoke, Toggle, reading names): no key, no mouse.
    /// </summary>
    [SkippableFact]
    public void R06_TheRefusalText_SurvivesOtherWrites_SaysWhenTheCheckHasSucceeded_AndGoesWhenACommandRuns()
    {
        var native = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
        Skip.If(string.IsNullOrEmpty(native) || !File.Exists(Path.Combine(native, "xpe_preprocess.dll")), "XPE_NATIVE_DIR does not name a folder containing xpe_preprocess.dll.");
        var copy = Path.Combine(Path.GetTempPath(), $"xpe_c226b_native_{Guid.NewGuid():N}");
        Directory.CreateDirectory(copy);
        try
        {
            foreach (var file in Directory.GetFiles(native!)) File.Copy(file, Path.Combine(copy, Path.GetFileName(file)));
            using var app = LegacyApp.LaunchOrSkip(breakTemp: false, extraEnvironment: new Dictionary<string, string> { ["XPE_NATIVE_DIR"] = copy });
            app.WaitForPreprocessVerdict();
            var before = app.ReadText("Calibration", "AlgorithmValidationResultText");

            // the refusal, as in R05
            ReplaceLikeARedeploy(Path.Combine(copy, "xpe_preprocess.dll"));
            app.ClickButtonById("Calibration", "RunSelectedAlgorithmButton");
            var refused = app.WaitForTextOnTab("Evaluation", "NativePreviewText", t => t.Contains(RefusalText, StringComparison.Ordinal), TimeSpan.FromSeconds(15));
            output.WriteLine("refused:      " + refused);

            // (2) the check the refusal started succeeds: the notice stops telling the user to wait
            app.WaitForTextOnTab("Evaluation", "StageModesInfoText", t => t.StartsWith("Preprocess=ready", StringComparison.Ordinal), TimeSpan.FromSeconds(60));
            var ready = app.WaitForTextOnTab("Evaluation", "NativePreviewText", t => t.Contains(ReadyAgainText, StringComparison.Ordinal), TimeSpan.FromSeconds(15));
            output.WriteLine("ready again:  " + ready);
            Assert.DoesNotContain(StillWaitingText, ready, StringComparison.Ordinal);

            // (1) another write to the preview text (a stage switch) does not remove the notice, and a refresh does not bring back the old wording
            app.ToggleCheckBox("Evaluation", "OffsetEnabledCheckBox");
            var afterToggle = app.WaitForTextOnTab("Evaluation", "NativePreviewText", t => t.Contains("stage selection changed", StringComparison.Ordinal), TimeSpan.FromSeconds(15));
            output.WriteLine("after toggle: " + afterToggle);
            Assert.Contains(RefusalText, afterToggle, StringComparison.Ordinal);
            Assert.DoesNotContain(StillWaitingText, afterToggle, StringComparison.Ordinal);
            app.ClickButtonOnTab("Diagnostics", "Refresh Modules");
            app.WaitForTextOnTab("Evaluation", "StageModesInfoText", t => t.StartsWith("Preprocess=ready", StringComparison.Ordinal), TimeSpan.FromSeconds(60));
            var afterRefresh = app.ReadText("Evaluation", "NativePreviewText");
            output.WriteLine("after refresh:" + afterRefresh);
            Assert.DoesNotContain(StillWaitingText, afterRefresh, StringComparison.Ordinal);

            // (3) a command that goes ahead: the notice is gone at once (this command only writes its own text box, not the preview text)
            app.ClickButtonById("Calibration", "RunSelectedAlgorithmButton");
            app.WaitForTextOnTab("Calibration", "AlgorithmValidationResultText", t => t != before, TimeSpan.FromSeconds(15));
            var afterRun = app.ReadText("Evaluation", "NativePreviewText");
            output.WriteLine("after run:    " + afterRun);
            Assert.DoesNotContain(RefusalText, afterRun, StringComparison.Ordinal);
        }
        finally
        {
            try { Directory.Delete(copy, recursive: true); } catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { /* a DLL the app still holds */ }
        }
    }

    private const string FinishedWithoutPassingText = "FINISHED without passing";

    /// <summary>A rebuild that leaves a file that is not a DLL at all: the check of that file cannot pass.</summary>
    private static void ReplaceWithGarbage(string path)
    {
        File.Move(path, path + ".garbled" + Guid.NewGuid().ToString("N")[..6]);
        File.WriteAllBytes(path, [0x4D, 0x5A, 0x00, 0x01, 0x02, 0x03]);
    }

    /// <summary>
    /// GUI-C-226d (Codex #123): the notice follows the CURRENT state of the check, in every direction, not only towards "ready". The worker is held by a gate file (present = it runs at once, absent =
    /// it waits), so each state can be read while it lasts. Sequence: refused while the re-check runs (checking) → the re-check passes (ready now) → the DLL changes again and Refresh Modules is
    /// pressed (back to checking: "ready now" must NOT survive) → that check finishes without passing (says so). UIA patterns only.
    /// </summary>
    [SkippableFact]
    public void R07_TheRefusalNotice_FollowsTheCheck_PassThenChangedAgainThenFailed()
    {
        var gate = Path.Combine(Path.GetTempPath(), $"xpe_c226d_gate_{Guid.NewGuid():N}.txt");
        File.WriteAllText(gate, "go");
        var native = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
        Skip.If(string.IsNullOrEmpty(native) || !File.Exists(Path.Combine(native, "xpe_preprocess.dll")), "XPE_NATIVE_DIR does not name a folder containing xpe_preprocess.dll.");
        var copy = Path.Combine(Path.GetTempPath(), $"xpe_c226d_native_{Guid.NewGuid():N}");
        Directory.CreateDirectory(copy);
        try
        {
            foreach (var file in Directory.GetFiles(native!)) File.Copy(file, Path.Combine(copy, Path.GetFileName(file)));
            var env = new Dictionary<string, string> { ["XPE_NATIVE_DIR"] = copy, ["XPE_ORACLE_TEST_GATE"] = gate };
            using var app = LegacyApp.LaunchOrSkip(breakTemp: false, extraEnvironment: env);
            app.WaitForPreprocessVerdict();

            // 1. refused while the re-check is held: the notice says it is being checked
            File.Delete(gate);
            ReplaceLikeARedeploy(Path.Combine(copy, "xpe_preprocess.dll"));
            app.ClickButtonById("Calibration", "RunSelectedAlgorithmButton");
            var checking = app.WaitForTextOnTab("Evaluation", "NativePreviewText", t => t.Contains(RefusalText, StringComparison.Ordinal), TimeSpan.FromSeconds(15));
            output.WriteLine("1 refused, check held: " + checking);
            Assert.Contains(StillWaitingText, checking, StringComparison.Ordinal);
            Assert.DoesNotContain(ReadyAgainText, checking, StringComparison.Ordinal);

            // 2. the re-check passes
            File.WriteAllText(gate, "go");
            var ready = app.WaitForTextOnTab("Evaluation", "NativePreviewText", t => t.Contains(ReadyAgainText, StringComparison.Ordinal), TimeSpan.FromSeconds(60));
            output.WriteLine("2 re-check passed:        " + ready);

            // 3. the DLL is redeployed AGAIN and the user refreshes: the check runs again, and "ready now" must not stay on screen while it does
            File.Delete(gate);
            ReplaceLikeARedeploy(Path.Combine(copy, "xpe_preprocess.dll"), "second");
            app.ClickButtonOnTab("Diagnostics", "Refresh Modules");
            var again = app.WaitForTextOnTab("Evaluation", "NativePreviewText", t => !t.Contains(ReadyAgainText, StringComparison.Ordinal), TimeSpan.FromSeconds(30));
            output.WriteLine("3 redeployed again, refresh, check held: " + again);
            Assert.Contains(RefusalText, again, StringComparison.Ordinal);
            Assert.Contains(StillWaitingText, again, StringComparison.Ordinal);

            // 4. that check passes too
            File.WriteAllText(gate, "go");
            var readyTwice = app.WaitForTextOnTab("Evaluation", "NativePreviewText", t => t.Contains(ReadyAgainText, StringComparison.Ordinal), TimeSpan.FromSeconds(60));
            output.WriteLine("4 passed again:                          " + readyTwice);

            // 5. the DLL is broken after "ready now" and the user refreshes (Codex's reproduction): the block shows, and the notice must say the check FAILED, not "ready now"
            ReplaceWithGarbage(Path.Combine(copy, "xpe_preprocess.dll"));
            app.ClickButtonOnTab("Diagnostics", "Refresh Modules");
            var failed = app.WaitForTextOnTab("Evaluation", "NativePreviewText", t => t.Contains(FinishedWithoutPassingText, StringComparison.Ordinal), TimeSpan.FromSeconds(60));
            output.WriteLine("5 broken, refresh:                       " + failed);
            Assert.DoesNotContain(ReadyAgainText, failed, StringComparison.Ordinal);
            Assert.DoesNotContain(StillWaitingText, failed, StringComparison.Ordinal);
        }
        finally
        {
            try { File.Delete(gate); } catch (IOException) { }
            try { Directory.Delete(copy, recursive: true); } catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { /* a DLL the app still holds */ }
        }
    }

    /// <summary>GUI-C-226d: the first re-check after a refusal fails at once (the rebuild left a file that is not a DLL): checking, then "finished without passing", never "ready now".</summary>
    [SkippableFact]
    public void R08_TheRefusalNotice_SaysSo_WhenTheFirstRecheckFails()
    {
        var gate = Path.Combine(Path.GetTempPath(), $"xpe_c226d_gate_{Guid.NewGuid():N}.txt");
        File.WriteAllText(gate, "go");
        var native = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
        Skip.If(string.IsNullOrEmpty(native) || !File.Exists(Path.Combine(native, "xpe_preprocess.dll")), "XPE_NATIVE_DIR does not name a folder containing xpe_preprocess.dll.");
        var copy = Path.Combine(Path.GetTempPath(), $"xpe_c226d_native_{Guid.NewGuid():N}");
        Directory.CreateDirectory(copy);
        try
        {
            foreach (var file in Directory.GetFiles(native!)) File.Copy(file, Path.Combine(copy, Path.GetFileName(file)));
            var env = new Dictionary<string, string> { ["XPE_NATIVE_DIR"] = copy, ["XPE_ORACLE_TEST_GATE"] = gate };
            using var app = LegacyApp.LaunchOrSkip(breakTemp: false, extraEnvironment: env);
            app.WaitForPreprocessVerdict();

            File.Delete(gate);
            ReplaceWithGarbage(Path.Combine(copy, "xpe_preprocess.dll"));
            app.ClickButtonById("Calibration", "RunSelectedAlgorithmButton");
            var checking = app.WaitForTextOnTab("Evaluation", "NativePreviewText", t => t.Contains(RefusalText, StringComparison.Ordinal), TimeSpan.FromSeconds(15));
            output.WriteLine("1 refused, check held: " + checking);
            Assert.DoesNotContain(ReadyAgainText, checking, StringComparison.Ordinal);

            File.WriteAllText(gate, "go");
            var failed = app.WaitForTextOnTab("Evaluation", "NativePreviewText", t => t.Contains(FinishedWithoutPassingText, StringComparison.Ordinal), TimeSpan.FromSeconds(60));
            output.WriteLine("2 re-check failed:        " + failed);
            Assert.DoesNotContain(ReadyAgainText, failed, StringComparison.Ordinal);
        }
        finally
        {
            try { File.Delete(gate); } catch (IOException) { }
            try { Directory.Delete(copy, recursive: true); } catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { /* a DLL the app still holds */ }
        }
    }

    private sealed record ClickOutcome(string CommandBefore, string CommandAfter, string PreviewText);

    private static ClickOutcome RunTheCommandAfter(bool changeDll)
    {
        var native = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
        Skip.If(string.IsNullOrEmpty(native) || !File.Exists(Path.Combine(native, "xpe_preprocess.dll")), "XPE_NATIVE_DIR does not name a folder containing xpe_preprocess.dll.");
        var copy = Path.Combine(Path.GetTempPath(), $"xpe_c226_native_{Guid.NewGuid():N}");
        Directory.CreateDirectory(copy);
        try
        {
            foreach (var file in Directory.GetFiles(native!)) File.Copy(file, Path.Combine(copy, Path.GetFileName(file)));
            using var app = LegacyApp.LaunchOrSkip(breakTemp: false, extraEnvironment: new Dictionary<string, string> { ["XPE_NATIVE_DIR"] = copy });
            app.WaitForPreprocessVerdict();
            var before = app.ReadText("Calibration", "AlgorithmValidationResultText");

            if (changeDll) ReplaceLikeARedeploy(Path.Combine(copy, "xpe_preprocess.dll"));
            app.ClickButtonById("Calibration", "RunSelectedAlgorithmButton");

            // one of the two things happens within moments: the command's text changes (it ran as far as its own first check) or the refusal appears
            var deadline = DateTime.UtcNow.AddSeconds(15);
            string after = before, preview = string.Empty;
            while (DateTime.UtcNow < deadline)
            {
                after = app.ReadText("Calibration", "AlgorithmValidationResultText");
                preview = app.ReadText("Evaluation", "NativePreviewText");
                if (after != before || preview.Contains(RefusalText, StringComparison.Ordinal)) break;
                Thread.Sleep(300);
            }

            return new ClickOutcome(before, after, preview);
        }
        finally
        {
            try { Directory.Delete(copy, recursive: true); } catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { /* a DLL the app still holds: left to the temp folder's cleanup */ }
        }
    }

    /// <summary>What a rebuild or redeploy does to a DLL that is in use: the loaded file is renamed out of the way (Windows allows that) and a file with other content is put under its name.</summary>
    private static void ReplaceLikeARedeploy(string path, string generation = "first")
    {
        var moved = path + "." + generation + ".replaced";
        File.Move(path, moved);
        File.Copy(moved, path);
        using var stream = new FileStream(path, FileMode.Append, FileAccess.Write);
        stream.WriteByte(0);   // the PE image is unchanged; the file is not the one that was judged
    }

    internal sealed record CheckingView(string Smoke, string Row, int BlockingFindings, string StageModes, bool OffsetSwitchEnabled, int CheckingFindings = 0)
    {
        public bool SmokeSaysChecking => Smoke.Contains("checking", StringComparison.OrdinalIgnoreCase) && !Smoke.Contains("pass=", StringComparison.Ordinal);
        public bool MatrixSaysChecking => Row.Contains("Synthetic oracle checking", StringComparison.Ordinal);
        public bool EvaluationReady => StageModes.StartsWith("Preprocess=ready", StringComparison.Ordinal);
        public override string ToString() => $"smoke='{Smoke}' | row has checking={MatrixSaysChecking} | NATIVE-NOT-READY={BlockingFindings} | NATIVE-CHECKING={CheckingFindings} | stage modes='{StageModes.Split('.')[0]}' | Offset enabled={OffsetSwitchEnabled}";
    }

    internal sealed record TabView(bool Diagnostics, string DiagnosticsEvidence, bool Calibration, string CalibrationEvidence, bool Evaluation, string EvaluationEvidence)
    {
        public string Describe() => $"diagnostics={Diagnostics} [{DiagnosticsEvidence}] | calibration={Calibration} [{CalibrationEvidence}] | evaluation={Evaluation} [{EvaluationEvidence}]";
    }

    internal sealed class LegacyApp : IDisposable
    {
        private readonly UIA3Automation _automation;
        private readonly Application _application;
        private readonly Window _window;
        private readonly string? _faultFile;

        private LegacyApp(Application application, Window window, UIA3Automation automation, string? faultFile)
        {
            _application = application;
            _window = window;
            _automation = automation;
            _faultFile = faultFile;
        }

        /// <summary>
        /// GUI-C-218 (#249): everything from the moment the app exists is exception-safe. This used to <c>Application.Launch</c> and then <c>GetMainWindow</c> with nothing around them, so a
        /// UIA timeout in <c>GetMainWindow</c> (FlaUI's connection timeout is 2 s; the app's UI thread can be busy that long at startup) threw out of here BEFORE a <see cref="LegacyApp"/>
        /// existed. The test's <c>using var app</c> then had nothing to dispose: the app, the automation object and the fault file leaked, and the leaked app (started with
        /// <c>UseShellExecute=false</c>, so it inherits this process's standard handles) kept the test host's output pipe open, which is what made <c>dotnet test</c> look hung.
        /// </summary>
        public static LegacyApp LaunchOrSkip(bool breakTemp, TimeSpan? windowTimeout = null, IReadOnlyDictionary<string, string>? extraEnvironment = null)
        {
            var exe = FindExecutable();
            Skip.If(exe is null, "clients/ImageProcTest/bin/Debug/net8.0-windows/ImageProcTest.exe was not found: build clients/ImageProcTest first.");
            AssertFresh(exe!);

            var nativeDir = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
            Skip.If(string.IsNullOrEmpty(nativeDir) || !File.Exists(Path.Combine(nativeDir, "xpe_preprocess.dll")), "XPE_NATIVE_DIR does not name a folder containing xpe_preprocess.dll.");

            var info = new ProcessStartInfo(exe!) { WorkingDirectory = Path.GetDirectoryName(exe)!, UseShellExecute = false };
            info.Environment["XPE_NATIVE_DIR"] = nativeDir;
            foreach (var (name, value) in extraEnvironment ?? new Dictionary<string, string>()) info.Environment[name] = value;
            string? faultFile = null;
            if (breakTemp)
            {
                // A FILE where the temp directory should be: Path.GetTempPath() returns it, and nothing can be created beneath it.
                faultFile = Path.Combine(Path.GetTempPath(), $"xpe_not_a_directory_{Guid.NewGuid():N}");
                File.WriteAllText(faultFile, "not a directory");
                info.Environment["TMP"] = faultFile;
                info.Environment["TEMP"] = faultFile;
            }

            UIA3Automation? automation = null;
            Application? application = null;
            try
            {
                automation = new UIA3Automation();
                application = Application.Launch(info);
                var window = application.GetMainWindow(automation, windowTimeout ?? TimeSpan.FromSeconds(60));
                Assert.NotNull(window);
                return new LegacyApp(application, window, automation, faultFile);
            }
            catch
            {
                KillTree(application);
                automation?.Dispose();
                if (faultFile is not null) { try { File.Delete(faultFile); } catch (IOException) { } }
                throw;
            }
        }

        /// <summary>Ends the app and anything it started, and does not throw: this runs on failure paths where the original exception is the one worth reporting.</summary>
        private static void KillTree(Application? application)
        {
            if (application is null) return;
            try
            {
                using var process = Process.GetProcessById(application.ProcessId);
                process.Kill(entireProcessTree: true);
                process.WaitForExit(5000);
            }
            catch (Exception ex) when (ex is ArgumentException or InvalidOperationException or System.ComponentModel.Win32Exception)
            {
                // already gone
            }
        }

        public TabView ReadPreprocessOnEveryTab()
        {
            // Diagnostics: the smoke line, the matrix row and the summary of what is executable.
            SelectTab("Diagnostics");
            // GUI-C-219: the oracle answers in the background, so the smoke line is "checking" first; the answer is waited for FIRST and everything derived from it is read after it.
            var smoke = WaitForText("PreprocessSmokeText", t => t.Contains("pass=", StringComparison.Ordinal), TimeSpan.FromSeconds(60));
            var summary = WaitForText("ModuleReadinessSummaryText", t => t.StartsWith("Executable modules=", StringComparison.Ordinal), TimeSpan.FromSeconds(30));
            var row = DataItems().FirstOrDefault(n => n.Contains("ModuleReadinessSnapshot { ModuleName = xpe_preprocess,", StringComparison.Ordinal)) ?? "(no xpe_preprocess row)";
            var level = row.Contains("ModuleName = xpe_preprocess, Level = R3,", StringComparison.Ordinal) ? "R3" : "below R3";
            var diagnostics = smoke.Contains("pass=True", StringComparison.Ordinal) && level == "R3" && !summary.Contains("xpe_preprocess:R", StringComparison.Ordinal);
            var diagnosticsEvidence = $"{smoke}; matrix level {level}; summary '{summary}'";

            // Calibration: the dependency findings that block the preprocess stages.
            SelectTab("Calibration");
            Thread.Sleep(500);
            var blocking = DataItems().Count(n => n.Contains("RuleId = NATIVE-NOT-READY", StringComparison.Ordinal) && n.Contains("native preprocess adapter is not ready", StringComparison.Ordinal));
            var calibration = blocking == 0;
            var calibrationEvidence = $"{blocking} NATIVE-NOT-READY finding(s) for the preprocess stages";

            // Evaluation: the stage-mode line and the first preprocess switch.
            SelectTab("Evaluation");
            var info = WaitForText("StageModesInfoText", t => t.StartsWith("Preprocess=", StringComparison.Ordinal) || t.StartsWith("Native pre/post exports are not ready", StringComparison.Ordinal), TimeSpan.FromSeconds(30));
            var offsetSwitch = _window.FindFirstDescendant(cf => cf.ByAutomationId("OffsetEnabledCheckBox"));
            var switchEnabled = offsetSwitch is not null && offsetSwitch.IsEnabled;
            var evaluation = info.StartsWith("Preprocess=ready", StringComparison.Ordinal) && switchEnabled;
            var evaluationEvidence = $"'{info.Split('.')[0]}'; Offset switch enabled={switchEnabled}";

            return new TabView(diagnostics, diagnosticsEvidence, calibration, calibrationEvidence, evaluation, evaluationEvidence);
        }

        /// <summary>What the three tabs say right after launch, while the oracle worker is still sleeping. UIA patterns only.</summary>
        public CheckingView ReadPreprocessWhileChecking()
        {
            SelectTab("Diagnostics");
            var smoke = _window.FindFirstDescendant(cf => cf.ByAutomationId("PreprocessSmokeText"))?.Name ?? string.Empty;
            var row = DataItems().FirstOrDefault(n => n.Contains("ModuleReadinessSnapshot { ModuleName = xpe_preprocess,", StringComparison.Ordinal)) ?? "(no xpe_preprocess row)";
            SelectTab("Calibration");
            var blocking = DataItems().Count(n => n.Contains("RuleId = NATIVE-NOT-READY", StringComparison.Ordinal) && n.Contains("native preprocess adapter is not ready", StringComparison.Ordinal));
            var checkingFindings = DataItems().Count(n => n.Contains("RuleId = NATIVE-CHECKING", StringComparison.Ordinal));
            SelectTab("Evaluation");
            var info = _window.FindFirstDescendant(cf => cf.ByAutomationId("StageModesInfoText"))?.Name ?? string.Empty;
            var offsetSwitch = _window.FindFirstDescendant(cf => cf.ByAutomationId("OffsetEnabledCheckBox"));
            return new CheckingView(smoke, row, blocking, info, offsetSwitch is not null && offsetSwitch.IsEnabled, checkingFindings);
        }

        /// <summary>Waits for the oracle's answer on the Diagnostics tab (the same wait <see cref="ReadPreprocessOnEveryTab"/> starts with).</summary>
        public void WaitForPreprocessVerdict()
        {
            SelectTab("Diagnostics");
            WaitForText("PreprocessSmokeText", t => t.Contains("pass=", StringComparison.Ordinal), TimeSpan.FromSeconds(60));
        }

        public string WaitForTextOnTab(string tab, string automationId, Func<string, bool> done, TimeSpan timeout)
        {
            SelectTab(tab);
            return WaitForText(automationId, done, timeout);
        }

        public void ToggleCheckBox(string tab, string automationId)
        {
            SelectTab(tab);
            var box = _window.FindFirstDescendant(cf => cf.ByAutomationId(automationId))?.AsCheckBox();
            Assert.True(box is not null && box.IsEnabled, $"check box '{automationId}' was not found or is not enabled");
            box!.Toggle();   // UIA TogglePattern: no mouse
        }

        public string ReadText(string tab, string automationId)
        {
            SelectTab(tab);
            return _window.FindFirstDescendant(cf => cf.ByAutomationId(automationId))?.Name ?? string.Empty;
        }

        public void ClickButtonById(string tab, string automationId)
        {
            SelectTab(tab);
            var button = _window.FindFirstDescendant(cf => cf.ByAutomationId(automationId))?.AsButton();
            Assert.True(button is not null, $"button '{automationId}' was not found");
            button!.Invoke();   // UIA InvokePattern: no mouse
        }

        public void ClickButtonOnTab(string tab, string name)
        {
            SelectTab(tab);
            ClickButton(name);
        }

        public void ClickButton(string name)
        {
            var button = _window.FindAllDescendants(cf => cf.ByControlType(ControlType.Button)).FirstOrDefault(e => e.Name == name)?.AsButton();
            Assert.True(button is not null, $"button '{name}' was not found");
            button!.Invoke();   // UIA InvokePattern: no mouse
        }

        /// <summary>The text of a text box as UI Automation's ValuePattern reports it (a text box's Name is empty).</summary>
        public string ReadValue(string tab, string automationId)
        {
            SelectTab(tab);
            var element = _window.FindFirstDescendant(cf => cf.ByAutomationId(automationId));
            Assert.True(element is not null, $"'{automationId}' was not found");
            var pattern = element!.Patterns.Value.PatternOrDefault;
            Assert.True(pattern is not null, $"'{automationId}' has no ValuePattern");
            return pattern!.Value.ValueOrDefault ?? string.Empty;
        }

        /// <summary>Puts text in a text box through the ValuePattern (UI Automation only: never a key press, which would go to whichever window is in front).</summary>
        public void SetValue(string tab, string automationId, string text)
        {
            SelectTab(tab);
            var element = _window.FindFirstDescendant(cf => cf.ByAutomationId(automationId));
            Assert.True(element is not null, $"'{automationId}' was not found");
            var pattern = element!.Patterns.Value.PatternOrDefault;
            Assert.True(pattern is not null && !pattern.IsReadOnly.ValueOrDefault, $"'{automationId}' has no writable ValuePattern");
            pattern!.SetValue(text);
            Thread.Sleep(300);
        }

        /// <summary>HelpText, ItemStatus and the tool-tip-like properties UI Automation exposes for an element: what the app tells the user about the control, other than its value.</summary>
        public string DescribeControl(string tab, string automationId)
        {
            SelectTab(tab);
            var element = _window.FindFirstDescendant(cf => cf.ByAutomationId(automationId));
            Assert.True(element is not null, $"'{automationId}' was not found");
            return $"help='{element!.Properties.HelpText.ValueOrDefault}' status='{element.Properties.ItemStatus.ValueOrDefault}' enabled={element.IsEnabled} offscreen={element.IsOffscreen}";
        }

        /// <summary>How many top-level windows the app has: a dialog or a message box the app raised would show here.</summary>
        public int TopLevelWindowCount() => _application.GetAllTopLevelWindows(_automation).Length;

        private void SelectTab(string header)
        {
            var tab = _window.FindAllDescendants(cf => cf.ByControlType(ControlType.TabItem)).FirstOrDefault(t => t.Name == header)?.AsTabItem();
            Assert.True(tab is not null, $"tab '{header}' was not found");
            tab!.Select();
            Thread.Sleep(600);
        }

        private IEnumerable<string> DataItems() =>
            _window.FindAllDescendants(cf => cf.ByControlType(ControlType.DataItem)).Select(e => e.Name ?? string.Empty).ToList();

        private string WaitForText(string automationId, Func<string, bool> done, TimeSpan timeout)
        {
            var deadline = DateTime.UtcNow + timeout;
            var text = string.Empty;
            while (DateTime.UtcNow < deadline)
            {
                text = _window.FindFirstDescendant(cf => cf.ByAutomationId(automationId))?.Name ?? string.Empty;
                if (done(text)) return text;
                Thread.Sleep(500);
            }

            Assert.Fail($"'{automationId}' never reached the expected state within {timeout.TotalSeconds:0} s; last text: '{text}'");
            return text;
        }

        public void Dispose()
        {
            try
            {
                if (!_application.HasExited) _application.Close();
            }
            catch (Exception)
            {
                // The process may already be gone; the kill below is the guarantee.
            }

            KillTree(_application);

            _automation.Dispose();
            if (_faultFile is not null) { try { File.Delete(_faultFile); } catch (IOException) { } }
        }

        private static string? FindExecutable()
        {
            for (var dir = new DirectoryInfo(AppContext.BaseDirectory); dir is not null; dir = dir.Parent)
            {
                var candidate = Path.Combine(dir.FullName, "clients", "ImageProcTest", "bin", "Debug", "net8.0-windows", "ImageProcTest.exe");
                if (File.Exists(candidate)) return candidate;
            }

            return null;
        }

        /// <summary>A stale binary reports a green for code that never ran: the exe must be newer than every source it was built from (the user-app fixture does the same).</summary>
        private static void AssertFresh(string exe)
        {
            var projectDir = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(exe)!, "..", "..", ".."));
            // The .exe is a launcher stub that a rebuild does not rewrite; the code is in the .dll beside it.
            var built = File.GetLastWriteTimeUtc(Path.ChangeExtension(exe, ".dll"));
            var newer = Directory.EnumerateFiles(projectDir, "*.cs", SearchOption.AllDirectories)
                .Where(f => !f.Contains($"{Path.DirectorySeparatorChar}obj{Path.DirectorySeparatorChar}") && !f.Contains($"{Path.DirectorySeparatorChar}bin{Path.DirectorySeparatorChar}"))
                .FirstOrDefault(f => File.GetLastWriteTimeUtc(f) > built);
            Assert.True(newer is null, $"clients/ImageProcTest's ImageProcTest.dll is older than {newer}: rebuild it (dotnet build clients/ImageProcTest/ImageProcTest.csproj -c Debug) before this scenario.");
        }
    }
}
