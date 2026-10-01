// #175 (GUI-C-83): the self-driving automation run must not pass on a backend it was not asked to use.
using System.Diagnostics;
using System.Text.Json;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Smoke;

/// <summary>
/// Launches the app the way the <c>gui-automation</c> CI job does (<c>--automation-report</c>, hidden)
/// and reads the report it writes.
///
/// <para>GUI-C-82 measured <c>Passed=True</c> with <c>BackendMode=Native ActualBackendMode=Mock</c>: the
/// CI gate reads <c>Passed</c>, so a Native automation run that fell back would have been green. A-01 is
/// that case; A-02 is the control — an intentional Mock run still passes, so a rule that fails every
/// Mock run cannot pass here.</para>
/// </summary>
[Collection(AutomationReportCollection.Name)]
public sealed class AutomationReportBackendTests(ITestOutputHelper output)
{
    private const string RawRelativePath = @"fixtures\gui-s0\raw\synthetic_1024x1024.raw";

    [SkippableFact]
    public void A01_NativeRequested_FallsBackToMock_IsNotAPass()
    {
        var empty = Directory.CreateDirectory(Path.Combine(Path.GetTempPath(), $"xpe-e2e-empty-native-a01-{Environment.ProcessId}"));
        foreach (var file in empty.GetFiles()) file.Delete();

        var (report, exitCode) = Run("A01", "Native", empty.FullName);
        Assert.Equal("Native", report.GetProperty("BackendMode").GetString());
        Assert.Equal("Mock", report.GetProperty("ActualBackendMode").GetString());
        Assert.False(report.GetProperty("BackendMatchesRequest").GetBoolean(), "The report does not flag the substituted backend.");
        Assert.False(
            report.GetProperty("Passed").GetBoolean(),
            "Native was requested, Mock ran, and the automation report still says Passed=true (#175).");
        Assert.True(exitCode == 1, $"A failed automation run exited {exitCode}; a caller reading only the exit code would take it for a pass (GUI-C-84).");
    }

    [SkippableFact]
    public void A02_MockRequested_IsStillAPass()
    {
        var (report, exitCode) = Run("A02", "Mock", nativeDirectory: null);
        Assert.Equal("Mock", report.GetProperty("ActualBackendMode").GetString());
        Assert.True(report.GetProperty("BackendMatchesRequest").GetBoolean());
        Assert.True(report.GetProperty("Passed").GetBoolean(), $"An intentional Mock automation run failed: Error='{report.GetProperty("Error")}'.");
        Assert.True(exitCode == 0, $"A passing automation run exited {exitCode}.");
    }

    /// <summary>
    /// A-03 (#225, GUI-C-155): the three commands GUI-C-154 wired report what they did, and the smoke
    /// answers from the real libraries.
    ///
    /// <para><b>Why here rather than through the menus.</b> The stop case has to catch a render that is
    /// still running, and the render is tens of milliseconds — two attempts at a menu-driven version
    /// lost that race (GUI-C-155). The automation run drives the same commands in-process, where the
    /// cancellation is issued before the render's first await, so the observation is deterministic.</para>
    /// </summary>
    [SkippableFact]
    public void A03_TheWiredCommands_ReportWhatTheyDid()
    {
        var (report, _) = Run("A03", "Native", NativeDirectoryOrSkip());

        // Row 6: answered, and from the real library — the mock display reports v0.0.0-mock-display, so
        // asserting only "passed" would not separate a fallback from a native run.
        Assert.True(report.GetProperty("PInvokeSmokeTestPassed").GetBoolean(),
            $"The smoke failed on a run with the native DLLs staged: {report.GetProperty("PInvokeSmokeTestDetail")}");
        var detail = report.GetProperty("PInvokeSmokeTestDetail").GetString() ?? string.Empty;
        Assert.Contains("xpe_display_version", detail, StringComparison.Ordinal);
        Assert.DoesNotContain("mock-display", detail, StringComparison.OrdinalIgnoreCase);

        // Row 11, in flight: the render was discarded, and the app counted it.
        var stopped = report.GetProperty("StopInFlightStatus").GetString() ?? string.Empty;
        Assert.StartsWith("Render stopped", stopped, StringComparison.Ordinal);
        Assert.True(report.GetProperty("StoppedRenderCount").GetInt32() >= 1,
            $"Stop was pressed during a render and the count stayed at {report.GetProperty("StoppedRenderCount")}.");

        // Row 11, empty: the case GUI-C-154's first implementation got wrong — it answered "the render
        // in flight will be discarded" on a render that had finished. Without this line it comes back.
        Assert.Equal("Stop: no render is in flight.", report.GetProperty("StopWithNothingRunningStatus").GetString());

        // Row 12: what it shows is the render's own chain line, not a second measurement.
        var timing = report.GetProperty("StageTimingReport").GetString() ?? string.Empty;
        Assert.Contains("chain:", timing, StringComparison.Ordinal);
        Assert.DoesNotContain("no render has run yet", timing, StringComparison.Ordinal);
    }

    /// <summary>
    /// A-04 (#225, GUI-C-159): row 15 — the self-check actually RUNS and the app reports the verdict.
    ///
    /// <para>GUI-C-158 wired the command and put <c>SelfCheckPassed</c> in the report, but nothing
    /// asserted it — the shape GUI-C-155 was corrected for. Asserted on both backends: the runner drives
    /// the mock display on purpose (its own assertion is <c>v0.0.0-mock-display</c>), so its verdict does
    /// not depend on whether native libraries are staged, and there is no precondition to skip on.</para>
    /// </summary>
    [SkippableFact]
    public void A04_SelfCheck_ActuallyRuns_AndThePassIsReported()
    {
        var (report, _) = Run("A04", "Mock", nativeDirectory: null);

        var status = report.GetProperty("SelfCheckStatus").GetString() ?? string.Empty;
        var verdict = VerdictOrSkip(report, "SelfCheckPassed", "self-check", "ImageProcTest.SelfCheck", status);
        Assert.True(verdict,
            $"The self-check did not pass: '{status}'.");
        Assert.Contains("passed", status, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>
    /// A-05 (#225, GUI-C-159): row 15 — and a FAILING self-check is reported as a failure, WITH the
    /// reason.
    ///
    /// <para>A-04 alone cannot separate "reports the verdict" from "reports everything as a pass", which
    /// is the shape #205, #207 and #212 each turned out to be. The failure is produced structurally
    /// rather than by breaking the runner's source: a copy of the runner staged outside the repository
    /// cannot locate the repository root, which GUI-C-157 measured it dying on. The app is pointed at
    /// that copy through <c>--automation-selfcheck-exe</c>.</para>
    /// </summary>
    [SkippableFact]
    public void A05_AFailingSelfCheck_IsReportedAsAFailure_WithTheReason()
    {
        var staged = StageSelfCheckRunnerOutsideTheRepository("A05");

        var (report, _) = Run("A05", "Mock", nativeDirectory: null,
            extraArgs: ["--automation-selfcheck-exe", staged]);

        var status = report.GetProperty("SelfCheckStatus").GetString() ?? string.Empty;
        Assert.False(report.GetProperty("SelfCheckPassed").GetBoolean(),
            $"The self-check was staged where it cannot run, and the app still reported a pass: '{status}'.");
        // The reason, not just the verdict: GUI-C-158 first reported the LAST stderr line ("at
        // Program...line 62"), which is true and tells the operator nothing.
        Assert.Contains("Repository root", status, StringComparison.OrdinalIgnoreCase);

        // It RAN and failed, so it says FAILED — not "did not run", which A-15 reserves for a start that failed.
        Assert.Contains("FAILED", status, StringComparison.Ordinal);
        Assert.DoesNotContain("did not run", status, StringComparison.Ordinal);
    }

    /// <summary>
    /// A-15 (#225, GUI-C-177): row 15 — a self-check that cannot be STARTED is reported as "did not run", with no
    /// verdict, and never as a failure.
    ///
    /// <para>The counterpart of A-05, which stages a runner that starts and dies. Here the app is pointed (through
    /// <c>--automation-selfcheck-exe</c>) at a file that exists but is text, not a program, so the start itself fails.
    /// The two situations were worded alike ("could not be started" with a false verdict) before GUI-C-177; the
    /// wording lives in the one runner executor that rows 16 and 17 share, so this one end-to-end case stands for
    /// the three commands, and <c>RunnerProcessTests</c> asserts the wording for each of their labels.</para>
    /// </summary>
    [SkippableFact]
    public void A15_ASelfCheckThatCannotBeStarted_IsDidNotRun_NotAFailure()
    {
        var notAProgram = Path.Combine(Path.GetTempPath(), $"xpe-notarunner-A15-{Environment.ProcessId}.exe");
        File.WriteAllText(notAProgram, "this is text, not an executable");
        try
        {
            var (report, _) = Run("A15", "Mock", nativeDirectory: null,
                extraArgs: ["--automation-selfcheck-exe", notAProgram]);

            var status = report.GetProperty("SelfCheckStatus").GetString() ?? string.Empty;
            var verdictKind = report.TryGetProperty("SelfCheckPassed", out var verdict) ? verdict.ValueKind : JsonValueKind.Null;
            Assert.True(verdictKind == JsonValueKind.Null,
                $"A self-check that never started reported a verdict ({verdictKind}): '{status}'.");
            Assert.Contains("did not run", status, StringComparison.Ordinal);
            Assert.DoesNotContain("FAILED", status, StringComparison.Ordinal);
        }
        finally
        {
            File.Delete(notAProgram);
        }
    }

    /// <summary>
    /// A-06 (#225, GUI-C-159): row 16 — the GUI E2E runner actually runs from the Tools menu and the
    /// app reports its verdict.
    ///
    /// <para>This is also the concurrency observation the card asked for, standing rather than one-off:
    /// the runner is launched by an app that is already up, so a regression where two windows interfere
    /// shows up here as a failed or never-finishing run rather than only in a report someone read once.
    /// Backend-independent for the same reason A-04 is — the runner drives the mock display.</para>
    /// </summary>
    [SkippableFact]
    public void A06_GuiE2E_RunsFromTheApp_AndTheVerdictIsReported()
    {
        var (report, _) = Run("A06", "Mock", nativeDirectory: null);

        var status = report.GetProperty("GuiE2EStatus").GetString() ?? string.Empty;
        var verdict = VerdictOrSkip(report, "GuiE2EPassed", "GUI E2E", "ImageProcTest.E2E", status);
        Assert.True(verdict,
            $"The GUI E2E runner did not pass when launched from the app: '{status}'.");
        Assert.Contains("passed", status, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>
    /// A-07 (#225, GUI-C-160): row 5 — the runtime log leaves the process as a file.
    ///
    /// <para>The line count comes from reading the written file back, not from the status line: "the
    /// app says it wrote 44 lines" and "a file with 44 lines exists" are different claims, and only the
    /// second one is the feature row 5 was missing. The list on screen was already live; the way out
    /// was not.</para>
    /// </summary>
    [SkippableFact]
    public void A07_RuntimeLogs_AreWrittenToAFile()
    {
        var (report, _) = Run("A07", "Mock", nativeDirectory: null);

        var status = report.GetProperty("RuntimeLogExportStatus").GetString() ?? string.Empty;
        Assert.Contains("exported", status, StringComparison.OrdinalIgnoreCase);
        Assert.False(string.IsNullOrWhiteSpace(report.GetProperty("RuntimeLogExportPath").GetString()),
            $"The app reported no export path: '{status}'.");
        Assert.True(report.GetProperty("RuntimeLogExportLineCount").GetInt32() > 0,
            $"The exported file held no lines, so nothing actually left the process: '{status}'.");
    }

    /// <summary>
    /// A-08 (#225, GUI-C-160): row 1 — the recent-file history SURVIVES THE PROCESS.
    ///
    /// <para>Two runs share one settings file. The first starts with an empty history and ends with
    /// one entry; the second must START with that entry, before it has loaded anything. Asserting only
    /// "the list has an entry after loading" would pass on a purely in-memory list, which is what row 1
    /// already had — <c>LastRawDirectory</c> kept a directory, not a history.</para>
    /// </summary>
    [SkippableFact]
    public void A08_RecentFileHistory_SurvivesTheProcess()
    {
        var settingsPath = Path.Combine(
            Path.GetTempPath(), $"xpe-recent-a08-{Environment.ProcessId}.json");
        File.Delete(settingsPath);

        var (first, _) = Run("A08a", "Mock", nativeDirectory: null,
            extraArgs: ["--automation-settings", settingsPath]);
        Assert.Equal(0, first.GetProperty("RecentRawFileCountAtStartup").GetInt32());
        Assert.True(first.GetProperty("RecentRawFileCount").GetInt32() >= 1,
            "The first run loaded a raw file and recorded no history entry.");
        // Attribution (#201): the scenario presses Save, so survival alone cannot say WHO persisted the
        // entry. This reading is taken between the load and that Save — measured: without it, removing
        // the app's own save left this scenario green.
        Assert.True(first.GetProperty("RecentHistoryPersistedBeforeSave").GetBoolean(),
            "The history reached disk only because the run pressed Save; the app did not persist it itself.");

        var (second, _) = Run("A08b", "Mock", nativeDirectory: null,
            extraArgs: ["--automation-settings", settingsPath]);
        Assert.True(second.GetProperty("RecentRawFileCountAtStartup").GetInt32() >= 1,
            "The second process started with an empty history, so nothing was persisted (#225 row 1).");
        Assert.False(string.IsNullOrWhiteSpace(second.GetProperty("MostRecentRawFile").GetString()));
    }

    /// <summary>
    /// Copies the built self-check runner into a temporary directory OUTSIDE the checkout and returns
    /// the copied executable. Nothing in the repository is modified.
    /// </summary>
    private static string StageSelfCheckRunnerOutsideTheRepository(string scenario)
    {
        var relative = Path.Combine(
            "gui", "ImageProcTest.SelfCheck", "bin", "Debug", "net8.0-windows");

        string? source = null;
        for (var dir = new DirectoryInfo(AppContext.BaseDirectory); dir is not null; dir = dir.Parent)
        {
            var candidate = Path.Combine(dir.FullName, relative);
            if (File.Exists(Path.Combine(candidate, "ImageProcTest.SelfCheck.exe")))
            {
                source = candidate;
                break;
            }
        }

        Skip.If(source is null, "ImageProcTest.SelfCheck.exe was not built, so its failure cannot be observed.");

        var target = Path.Combine(Path.GetTempPath(), $"xpe-selfcheck-{scenario}-{Environment.ProcessId}");
        if (Directory.Exists(target)) Directory.Delete(target, recursive: true);
        Directory.CreateDirectory(target);
        foreach (var file in Directory.GetFiles(source!))
        {
            File.Copy(file, Path.Combine(target, Path.GetFileName(file)), overwrite: true);
        }

        return Path.Combine(target, "ImageProcTest.SelfCheck.exe");
    }

    /// <summary>
    /// A-09 (#225, GUI-C-163): rows 3 and 14 — the bundle export and the evidence folder answer, and the
    /// menu shares the workbench button's command object.
    ///
    /// <para>Row 3 was never a missing capability: the menu item arrived as a placeholder in f8f9bd5 and
    /// the zip landed three weeks later in 54a3ae7 bound to two buttons. So what is asserted is the
    /// IDENTITY of the command, not that a zip appears — a re-implementation behind the menu would
    /// produce a zip too, and then drift from the buttons with nothing noticing.</para>
    ///
    /// <para>Row 14's launch is NOT asserted and cannot be: the app suppresses the file-browser launch
    /// under automation rather than leaving windows on a CI machine. The suppression is asserted instead,
    /// so the gap is visible in the data rather than hidden in prose.</para>
    /// </summary>
    [SkippableFact]
    public void A09_EvidenceBundleAndFolder_Answer()
    {
        var (report, _) = Run("A09", "Mock", nativeDirectory: null);

        Assert.True(report.GetProperty("EvidenceBundleMenuSharesButtonCommand").GetBoolean(),
            "The Export Evidence Bundle menu item does not carry the view model's command object, so it " +
            "can drift from the workbench buttons that do.");
        var bundle = report.GetProperty("EvidenceBundleStatus").GetString() ?? string.Empty;
        Assert.Contains("Evidence bundle exported", bundle, StringComparison.Ordinal);

        var folder = report.GetProperty("EvidenceFolderStatus").GetString() ?? string.Empty;
        Assert.False(string.IsNullOrWhiteSpace(report.GetProperty("EvidenceFolderPath").GetString()),
            $"The evidence folder command resolved no directory: '{folder}'.");
        Assert.True(report.GetProperty("EvidenceFolderLaunchSuppressed").GetBoolean(),
            "An automation run launched the file browser; that would leave windows behind on CI.");
    }

    /// <summary>
    /// A-10 (#225, GUI-C-168): row 13 — the diagnostics panel keeps "no measurement" apart from "0 ms".
    ///
    /// <para>The three states the card requires kept apart, each asserted from what the PANEL rendered
    /// rather than from the model: a run that has not rendered yet, a stage that was switched off, and a
    /// value that predates the operator's last edit. Reading the model would confirm the data is right
    /// while leaving the one thing this row can get wrong — a switched-off stage printed as 0 ms —
    /// unobserved, because <c>StageOutcome.ElapsedMs</c> really is 0.0 there by construction.</para>
    /// </summary>
    [SkippableFact]
    public void A10_PipelineDiagnostics_KeepsNoMeasurementApartFromZero()
    {
        var (report, _) = Run("A10", "Mock", nativeDirectory: null);

        Assert.True(report.GetProperty("PipelineDiagnosticsVisible").GetBoolean(),
            "The Tools menu command did not show the diagnostics panel.");

        // State 1: before this run rendered anything there was no measurement at all.
        Assert.False(report.GetProperty("PipelineDiagnosticsHadMeasurementAtStartup").GetBoolean(),
            "A freshly started app reported a pipeline measurement, so 'never rendered' is not distinguishable.");
        Assert.True(report.GetProperty("PipelineDiagnosticsHasMeasurement").GetBoolean(),
            "After a render the panel still reports no measurement.");

        // State 2: a switched-off stage shows its status and NO number.
        var lines = report.GetProperty("PipelineDiagnosticsStageLines").EnumerateArray()
            .Select(line => line.GetString() ?? string.Empty).ToArray();
        Assert.NotEmpty(lines);
        foreach (var line in lines.Where(l => l.Contains("NotRequested", StringComparison.Ordinal)))
        {
            Assert.Contains("—", line, StringComparison.Ordinal);
            Assert.DoesNotContain("0 ms", line, StringComparison.Ordinal);
        }

        // State 3: an edit with no re-render is visible AS an edit, and the mark comes off again.
        // The before/after/restored triple is what makes it attributable — a non-null "after" alone can
        // be left over from something else, which GUI-C-168's first attempt measured (the row-11 stop
        // test had already set a different stale reason).
        Assert.Null(report.GetProperty("StaleReasonBeforeParameterEdit").GetString());
        Assert.Contains("display parameters changed",
            report.GetProperty("StaleReasonAfterParameterEdit").GetString() ?? string.Empty,
            StringComparison.OrdinalIgnoreCase);
        Assert.Null(report.GetProperty("StaleReasonAfterParameterRestored").GetString());
    }

    /// <summary>
    /// A-11 (#225, GUI-C-169): row 20 — the API Reference menu's CLAIM matches the disk, and the count of
    /// unimplemented menu items agrees across three independent derivations.
    ///
    /// <para><b>Claim versus disk.</b> The expected entry page is derived here from the Doxyfile's own
    /// <c>OUTPUT_DIRECTORY</c> and <c>HTML_OUTPUT</c>, not from the app's constant, and existence is read
    /// from the filesystem by the test. Whatever state the checkout happens to be in, the app must describe
    /// THAT state: a path and a suppressed launch when the page exists, no path and the how-to-generate line
    /// when it does not. Both branches assert something, so neither configuration is a silent pass; the
    /// deterministic per-state coverage lives in <c>ApiReferenceServiceTests</c>, against temporary
    /// directories.</para>
    ///
    /// <para><b>Three counts.</b> The app's hand-written list of names, its structural walk of the menu tree
    /// and this test's parse of <c>MainWindow.xaml</c> are independent routes to one number, and they must
    /// agree. This replaced a floor (<c>&gt;= 10</c>) inside the app's own verdict that failed the moment
    /// row 20 took the count to 9; equality survives the count reaching 0.</para>
    /// </summary>
    [SkippableFact]
    public void A11_ApiReference_ClaimMatchesTheDisk_AndTheCountsAgree()
    {
        var repoRoot = RepositoryRootOrSkip();
        var (report, _) = Run("A11", "Mock", nativeDirectory: null);

        // -- the claim, against an independent reading of the disk ------------------------------------
        var expectedIndex = ExpectedDoxygenIndex(repoRoot);
        var status = report.GetProperty("ApiReferenceStatus").GetString() ?? string.Empty;
        var reportedPath = report.GetProperty("ApiReferencePath").GetString();
        var suppressed = report.GetProperty("ApiReferenceLaunchSuppressed").GetBoolean();

        if (File.Exists(expectedIndex))
        {
            Assert.Equal(expectedIndex, reportedPath);
            Assert.True(suppressed, "The page exists but the automation run did not record suppressing the launch.");
            Assert.Contains("generated", status, StringComparison.OrdinalIgnoreCase);
        }
        else
        {
            Assert.Null(reportedPath);
            Assert.False(suppressed, "Nothing was opened, yet the run recorded a suppressed launch.");
            Assert.Contains("doxygen Doxyfile", status, StringComparison.Ordinal);
            Assert.Contains("doxygen-awesome", status, StringComparison.Ordinal);
        }

        // -- the count, three ways --------------------------------------------------------------------
        var listed = report.GetProperty("DisabledFutureCommandCount").GetInt32();
        var walked = report.GetProperty("UnimplementedMenuLeafCount").GetInt32();
        var declared = CountMenuItemsDisabledInXaml(repoRoot);
        Assert.True(listed == walked && walked == declared,
            $"Unimplemented menu items disagree: the app's name list says {listed}, its menu-tree walk says " +
            $"{walked}, MainWindow.xaml declares {declared} MenuItem(s) with IsEnabled=\"False\". A placeholder " +
            "is uncounted, or something counted is not one.");
    }

    /// <summary>
    /// A-14 (#225, GUI-C-176): row 17 — the Benchmark Runner menu describes the build tree it finds, and an
    /// automation run never reports a verdict it did not get.
    ///
    /// <para>Whether <c>build/ci-post</c> has CTest files is read from the disk by this test, independently of
    /// the app, and each state asserts something: no tree means the "not built" answer with the two commands
    /// that build it and no launch; a tree means the launch was suppressed (a CI machine must not start a
    /// multi-minute native benchmark because a check clicked a menu). In both states
    /// <c>BenchmarkPassed</c> stays null — "did not run", which is not "passed" and not "failed". The ctest
    /// run itself is the shared runner, whose pass and fail paths A-04 and A-05 observe; the coupling to it is
    /// asserted in <c>BenchmarkRunnerServiceTests</c>.</para>
    /// </summary>
    [SkippableFact]
    public void A14_BenchmarkRunner_DescribesTheBuildTree_AndReportsNoVerdictItDidNotGet()
    {
        var repoRoot = RepositoryRootOrSkip();
        var (report, _) = Run("A14", "Mock", nativeDirectory: null);

        var status = report.GetProperty("BenchmarkStatus").GetString() ?? string.Empty;
        var suppressed = report.GetProperty("BenchmarkLaunchSuppressed").GetBoolean();
        var verdictKind = report.TryGetProperty("BenchmarkPassed", out var verdict) ? verdict.ValueKind : JsonValueKind.Null;
        Assert.True(verdictKind == JsonValueKind.Null,
            $"An automation run reported a benchmark verdict ({verdictKind}) although it must not launch ctest. Status: '{status}'.");

        var treeExists = File.Exists(Path.Combine(repoRoot, "build", "ci-post", "CTestTestfile.cmake"));
        if (treeExists)
        {
            Assert.True(suppressed, $"A build tree exists but the run did not record suppressing the launch. Status: '{status}'.");
            Assert.Contains("launch suppressed", status, StringComparison.Ordinal);
            Assert.DoesNotContain("not built", status, StringComparison.Ordinal);
        }
        else
        {
            Assert.False(suppressed, "There is no build tree, yet the run recorded a suppressed launch.");
            Assert.Contains("not built", status, StringComparison.Ordinal);
            Assert.Contains("cmake --preset ci-post", status, StringComparison.Ordinal);
            Assert.Contains("cmake --build --preset ci-post --parallel", status, StringComparison.Ordinal);
        }
    }

    /// <summary>
    /// A-16 (#225, GUI-C-181): row 21, the state with NO generated page — the menu says so and says how, and nothing is
    /// opened, started or left blank.
    ///
    /// <para>Whether the page exists is read from the disk by this test, independently of the app. Without a page the
    /// answer depends on whether DocFX's output folder exists (never generated, or generated part-way); both are asserted
    /// against what the disk shows. CI has no generated DocFX output, so this is the state observed there. A checkout that
    /// already holds a generated page cannot produce this state, and says so (A-17 covers that one).</para>
    /// </summary>
    [SkippableFact]
    public void A16_TroubleshootingPage_NotGenerated_SaysSoAndHow_AndOpensNothing()
    {
        var repoRoot = RepositoryRootOrSkip();
        var page = TroubleshootingPageOnDisk(repoRoot);
        Skip.If(File.Exists(page),
            "This checkout already holds a generated Troubleshooting page, so the not-generated state cannot be produced here; " +
            "the generated state is A-17's, and a CI run has no generated DocFX output.");

        var outputFolderExists = Directory.Exists(Path.GetDirectoryName(Path.GetDirectoryName(page)!));
        var (report, _) = Run("A16", "Mock", nativeDirectory: null);

        var status = report.GetProperty("TroubleshootingStatus").GetString() ?? string.Empty;
        Assert.Null(report.GetProperty("TroubleshootingPagePath").GetString());
        Assert.False(report.GetProperty("TroubleshootingLaunchSuppressed").GetBoolean(),
            "Nothing existed to open, yet the run recorded a suppressed launch.");
        Assert.Contains(outputFolderExists ? "is incomplete" : "has not been generated", status, StringComparison.Ordinal);
        Assert.Contains("dotnet tool install -g docfx", status, StringComparison.Ordinal);
        Assert.Contains("docfx docs/help/docfx/docfx.json", status, StringComparison.Ordinal);
    }

    /// <summary>
    /// A-17 (#225, GUI-C-181): row 21, the state WITH a generated page — it is resolved, its path is reported, and the
    /// browser launch is suppressed because this is an automation run.
    ///
    /// <para>The page is a stub staged in the checkout's gitignored <c>docs/help/generated</c> folder when the checkout has
    /// none, and removed again (with the folders this test created, nothing else). A page that is already there is used
    /// as it is and left alone. The assertions read the disk, not the app.</para>
    /// </summary>
    [SkippableFact]
    public void A17_TroubleshootingPage_Generated_IsResolved_AndTheLaunchIsSuppressedUnderAutomation()
    {
        var repoRoot = RepositoryRootOrSkip();
        var page = TroubleshootingPageOnDisk(repoRoot);
        var createdFile = false;
        var createdDirectories = new List<string>();

        if (!File.Exists(page))
        {
            for (var dir = Path.GetDirectoryName(page)!; !Directory.Exists(dir); dir = Path.GetDirectoryName(dir)!)
            {
                createdDirectories.Add(dir);
            }

            Directory.CreateDirectory(Path.GetDirectoryName(page)!);
            File.WriteAllText(page, "<html><body>A-17 stub</body></html>");
            createdFile = true;
        }

        try
        {
            var (report, _) = Run("A17", "Mock", nativeDirectory: null);

            var status = report.GetProperty("TroubleshootingStatus").GetString() ?? string.Empty;
            Assert.Equal(page, report.GetProperty("TroubleshootingPagePath").GetString());
            Assert.True(report.GetProperty("TroubleshootingLaunchSuppressed").GetBoolean(),
                $"A page exists, yet the run did not record suppressing the launch. Status: '{status}'.");
            Assert.Contains("Troubleshooting page opened", status, StringComparison.Ordinal);
            Assert.Contains("launch suppressed", status, StringComparison.Ordinal);
            Assert.DoesNotContain("not been generated", status, StringComparison.Ordinal);
        }
        finally
        {
            if (createdFile) File.Delete(page);
            foreach (var dir in createdDirectories)
            {
                // Deepest first (the list was built from the leaf upwards); only folders this test created, and only when empty.
                if (Directory.Exists(dir) && !Directory.EnumerateFileSystemEntries(dir).Any()) Directory.Delete(dir);
            }
        }
    }

    private static string TroubleshootingPageOnDisk(string repoRoot) =>
        Path.Combine(repoRoot, "docs", "help", "generated", "docfx", "content", "troubleshooting.html");

    /// <summary>
    /// A-12 (#225, GUI-C-170): rows 7 and 8 — what the two panels RENDER, read from the panels.
    ///
    /// <para>Two runs, because the calibration panel has two states that must not look alike: with a
    /// directory it prints the path, without one it prints "(not set)". The path is compared against the one
    /// this test PASSED IN, so it is independent of anything the app computes. The display panel's preset
    /// line is compared against the report's separately derived preset numbers, and neither of its "rendered"
    /// cells may still say "not applied" after a render ran.</para>
    ///
    /// <para>The Browse buttons are not pressed: they open a modal folder dialog, which an unattended run
    /// cannot answer. That gap is stated here rather than hidden; the runner asserts the buttons' commands
    /// are bound.</para>
    /// </summary>
    [SkippableFact]
    public void A12_Panels_RenderWhatTheSettingsHold()
    {
        var calibDir = Path.Combine(Path.GetTempPath(), $"xpe-a12-calib-{Environment.ProcessId}");
        Directory.CreateDirectory(calibDir);
        try
        {
            var (withDir, _) = Run("A12a", "Mock", nativeDirectory: null, extraArgs: ["--automation-calib", calibDir]);
            Assert.True(withDir.GetProperty("CalibrationPanelRendered").GetBoolean(), "The calibration panel was not on screen after its toggle was switched on.");
            var withTexts = Texts(withDir, "CalibrationPanelTexts");
            Assert.Equal(3, withTexts.Count(t => string.Equals(t, calibDir, StringComparison.OrdinalIgnoreCase)));
            Assert.DoesNotContain("(not set)", withTexts);

            // The default run: an automation run starts from the code defaults (MainWindow.CreateSettings),
            // which are three RELATIVE directories — measured in GUI-C-170; a first version of this case
            // expected "(not set)" here and was wrong. The expected values are literals on purpose. A first
            // fix read them from bin/appsettings.json, which failed in the full suite: a launch without
            // --automation-report loads and re-saves that file, so another case had already replaced its
            // directories with a temp path. A file the suite itself mutates is not an independent reference.
            var (defaults, _) = Run("A12b", "Mock", nativeDirectory: null);
            var defaultTexts = Texts(defaults, "CalibrationPanelTexts");
            foreach (var expected in new[] { "data/calibration/offset", "data/calibration/gain", "data/calibration/defect" })
            {
                Assert.Contains(expected, defaultTexts);
            }

            Assert.DoesNotContain("(not set)", defaultTexts);

            // The genuinely empty state: a settings file that clears the three directories.
            var emptyPath = Path.Combine(Path.GetTempPath(), $"xpe-a12-empty-{Environment.ProcessId}.json");
            File.WriteAllText(emptyPath, "{ \"calibOffsetDir\": \"\", \"calibGainDir\": \"\", \"calibDefectDir\": \"\" }");
            try
            {
                var (empty, _) = Run("A12c", "Mock", nativeDirectory: null, extraArgs: ["--automation-settings", emptyPath]);
                var emptyTexts = Texts(empty, "CalibrationPanelTexts");
                Assert.Equal(3, emptyTexts.Count(t => t == "(not set)"));
                Assert.DoesNotContain(emptyTexts, t => t.Contains(calibDir, StringComparison.OrdinalIgnoreCase));
            }
            finally
            {
                File.Delete(emptyPath);
            }

            Assert.True(withDir.GetProperty("DisplayPanelRendered").GetBoolean(), "The display settings panel was not on screen after its toggle was switched on.");
            var display = Texts(withDir, "DisplayPanelTexts");
            Assert.Contains("Requested", display);
            Assert.Contains("Rendered", display);
            Assert.DoesNotContain("not applied", display);

            // The preset line, against the report's own separately derived numbers.
            var applied = withDir.GetProperty("VoiPresetApplied").GetBoolean();
            var expectedCenter = $"C={withDir.GetProperty("VoiPresetCenter").GetDouble():0.###}";
            if (applied)
            {
                Assert.Contains(display, t => t.Contains(expectedCenter, StringComparison.Ordinal));
            }
            else
            {
                Assert.Contains("none applied yet", display);
            }
        }
        finally
        {
            Directory.Delete(calibDir, recursive: true);
        }
    }

    /// <summary>
    /// A-13 (#225, GUI-C-170): "on, close the app, open it again" — both panels, through two real launches.
    ///
    /// <para>Three claims, and the first is the control for the other two: the FIRST launch starts with both
    /// panels off and not on screen (the default), so a second launch that starts with them on cannot be
    /// reading a default. The run switches them on, saves, then switches them off again BEFORE its verdict,
    /// and must still pass: the verdict does not depend on the panels' state.</para>
    /// </summary>
    [SkippableFact]
    public void A13_PanelFlags_SurviveTheProcess_AndTheVerdictDoesNotDependOnThem()
    {
        var settingsPath = Path.Combine(Path.GetTempPath(), $"xpe-panels-a13-{Environment.ProcessId}.json");
        File.Delete(settingsPath);

        var (first, _) = Run("A13a", "Mock", nativeDirectory: null, extraArgs: ["--automation-settings", settingsPath]);
        Assert.False(first.GetProperty("CalibrationPanelVisible").GetBoolean());
        Assert.False(first.GetProperty("DisplayPanelVisible").GetBoolean());
        Assert.False(first.GetProperty("CalibrationPanelRenderedAtStart").GetBoolean());
        Assert.False(first.GetProperty("DisplayPanelRenderedAtStart").GetBoolean());
        Assert.True(first.GetProperty("Passed").GetBoolean(),
            "The run switches both panels off before its verdict; a panel's state must not be part of that verdict.");

        // Read the stored file itself, so that STORAGE and REPORTING are told apart. Without this, a report
        // that never fills the field fails below with the same message as a flag that was never stored
        // (measured in GUI-C-170: arms P1 and R1 both printed "The calibration flag was not persisted.").
        var stored = JsonDocument.Parse(File.ReadAllText(settingsPath)).RootElement;
        Assert.True(stored.TryGetProperty("showCalibrationPanel", out var storedCalibration) && storedCalibration.GetBoolean(),
            "The settings file does not hold showCalibrationPanel=true after the run saved it: the flag is not STORED.");
        Assert.True(stored.TryGetProperty("showDisplaySettingsPanel", out var storedDisplay) && storedDisplay.GetBoolean(),
            "The settings file does not hold showDisplaySettingsPanel=true after the run saved it: the flag is not STORED.");

        var (second, _) = Run("A13b", "Mock", nativeDirectory: null, extraArgs: ["--automation-settings", settingsPath]);
        Assert.True(second.GetProperty("CalibrationPanelVisible").GetBoolean(),
            "The flag is stored, but the second launch's report does not carry it: it is not REPORTED (or not loaded).");
        Assert.True(second.GetProperty("DisplayPanelVisible").GetBoolean(),
            "The flag is stored, but the second launch's report does not carry it: it is not REPORTED (or not loaded).");
        Assert.True(second.GetProperty("CalibrationPanelRenderedAtStart").GetBoolean(),
            "The flag came back true but the calibration panel was not on screen at start.");
        Assert.True(second.GetProperty("DisplayPanelRenderedAtStart").GetBoolean(),
            "The flag came back true but the display panel was not on screen at start.");
    }

    private static List<string> Texts(JsonElement report, string property) =>
        report.GetProperty(property).EnumerateArray().Select(e => e.GetString() ?? string.Empty).ToList();

    private static string RepositoryRootOrSkip()
    {
        for (var dir = new DirectoryInfo(AppContext.BaseDirectory); dir is not null; dir = dir.Parent)
        {
            if (File.Exists(Path.Combine(dir.FullName, "docs", "project", "sprint-plan.md"))) return dir.FullName;
        }

        Skip.If(true, "The repository root (docs/project/sprint-plan.md) is not above the test binaries, so the " +
                      "sources this scenario compares the app against are not available.");
        return string.Empty;
    }

    private static string ExpectedDoxygenIndex(string repoRoot)
    {
        var doxyDir = Path.Combine(repoRoot, "docs", "help", "doxygen");
        var text = File.ReadAllText(Path.Combine(doxyDir, "Doxyfile"));
        string Value(string key)
        {
            var m = System.Text.RegularExpressions.Regex.Match(
                text, $@"^{key}\s*=\s*(\S+)\s*$", System.Text.RegularExpressions.RegexOptions.Multiline);
            Assert.True(m.Success, $"{key} not found in the Doxyfile.");
            return m.Groups[1].Value;
        }

        return Path.GetFullPath(Path.Combine(doxyDir, Value("OUTPUT_DIRECTORY"), Value("HTML_OUTPUT"), "index.html"));
    }

    private static int CountMenuItemsDisabledInXaml(string repoRoot)
    {
        var xaml = System.Xml.Linq.XDocument.Load(Path.Combine(repoRoot, "gui", "ImageProcTest", "MainWindow.xaml"));
        return xaml.Descendants()
            .Count(e => e.Name.LocalName == "MenuItem" && (string?)e.Attribute("IsEnabled") == "False");
    }

    /// <summary>
    /// Reads a runner verdict that the app reports as THREE states, and keeps them three (#225,
    /// GUI-C-161).
    ///
    /// <para><c>null</c> is "the command did not run", not "it failed": the app leaves the field null
    /// when the runner executable is absent, exactly as <c>RunConsoleRunnerAsync</c> returns null rather
    /// than false for a runner it could not reach. Reading it with <c>GetBoolean()</c> collapsed that
    /// third state into an exception and turned main red — the test had erased a distinction the product
    /// code makes. The same defect class as #219 and #221.</para>
    ///
    /// <para>The absent-runner configuration is NORMAL, not broken: the <c>gui-automation</c> CI job
    /// builds <c>gui/ImageProcTest</c> only, so no runner exe exists beside the app there. That
    /// configuration skips with the reason stated; a configuration that HAS the runner asserts. Passing
    /// it loosely instead would make "the runner is absent" and "the runner failed" the same green
    /// (#214, GUI-C-155).</para>
    /// </summary>
    private static bool VerdictOrSkip(
        JsonElement report, string property, string label, string runnerProject, string status)
    {
        var present = report.TryGetProperty(property, out var value) &&
                      value.ValueKind is JsonValueKind.True or JsonValueKind.False;

        Skip.IfNot(present,
            $"The app reported no {label} verdict ({property} is null), which means the command did not " +
            $"run: this configuration has no {runnerProject} executable beside the app — the " +
            "gui-automation CI job builds gui/ImageProcTest only, which is normal. The app's status " +
            $"line said: '{status}'.");

        return value.GetBoolean();
    }

    /// <summary>
    /// The staged native directory, or a skip. Row 6 asserts the libraries ANSWER, so a run without
    /// them cannot observe it — and passing it anyway would make "the DLLs are absent" and "the DLLs
    /// are broken" the same green (#214). ci.yml:454-455 says the Mock job has no native DLLs.
    /// </summary>
    private static string NativeDirectoryOrSkip()
    {
        var dir = Environment.GetEnvironmentVariable(ApplicationFixture.NativeDirVariable);
        Skip.If(string.IsNullOrWhiteSpace(dir) || !Directory.Exists(dir),
            $"{ApplicationFixture.NativeDirVariable} is not set to an existing directory, so no native " +
            "libraries are staged for this run and the smoke cannot be observed answering.");
        return dir!;
    }

    private (JsonElement Report, int ExitCode) Run(
        string scenario, string backend, string? nativeDirectory, string[]? extraArgs = null)
    {
        var exe = ApplicationFixture.ResolveApplicationExecutable();
        Skip.If(exe is null, "ImageProcTest.exe was not built.");

        // #225 (GUI-C-167): these scenarios launch the app THEMSELVES rather than through
        // ApplicationFixture, so they were the one launch path with no build-freshness check — found in
        // GUI-C-166 when a falsification arm passed with a deliberately stale app and the right reading
        // was "this scenario does not pass the guard", not "the guard broke".
        //
        // Here and not inside ResolveApplicationExecutable: the census (GUI-C-167 §1) found three call
        // sites, and one of them — EvidenceBundleScenarios.EvidenceRoot — wants only the DIRECTORY and
        // launches nothing. Guarding the resolver would throw there for a staleness that cannot affect
        // it. Guarding every launch is what matters, and this helper is every launch in this class.
        ApplicationFixture.EnsureLaunchTargetIsFresh(exe!);

        var workDir = Path.GetDirectoryName(exe)!;
        Skip.If(!File.Exists(Path.Combine(workDir, RawRelativePath)), "The fixture image is not staged next to the app.");

        var reportPath = Path.Combine(Path.GetTempPath(), $"xpe-automation-{scenario}-{Environment.ProcessId}.json");
        File.Delete(reportPath);

        var start = new ProcessStartInfo(exe!) { WorkingDirectory = workDir, UseShellExecute = false, WindowStyle = ProcessWindowStyle.Hidden };
        foreach (var arg in new[]
                 {
                     "--automation-raw", RawRelativePath, "--automation-report", reportPath,
                     "--automation-backend", backend, "--automation-width", "1024", "--automation-height", "1024",
                 })
        {
            start.ArgumentList.Add(arg);
        }

        foreach (var arg in extraArgs ?? [])
        {
            start.ArgumentList.Add(arg);
        }

        if (nativeDirectory is not null)
        {
            start.Environment[ApplicationFixture.NativeDirVariable] = nativeDirectory;
            start.Environment["XPE_NATIVE_DIR_EXCLUSIVE"] = "1";
        }

        using var process = Process.Start(start)!;
        if (!process.WaitForExit(120_000))
        {
            process.Kill(entireProcessTree: true);
            Assert.Fail($"{scenario}: the automation run did not finish within 120 s.");
        }

        Assert.True(File.Exists(reportPath), $"{scenario}: no report was written (exit {process.ExitCode}).");
        using var doc = JsonDocument.Parse(File.ReadAllText(reportPath));
        var root = doc.RootElement.Clone();
        output.WriteLine($"{scenario} exit={process.ExitCode} BackendMode={root.GetProperty("BackendMode")} " +
                         $"ActualBackendMode={root.GetProperty("ActualBackendMode")} " +
                         $"BackendMatchesRequest={root.GetProperty("BackendMatchesRequest")} Passed={root.GetProperty("Passed")}");
        return (root, process.ExitCode);
    }
}

/// <summary>Its own collection: these tests launch their own app processes.</summary>
[CollectionDefinition(Name)]
public sealed class AutomationReportCollection
{
    public const string Name = "AutomationReport";
}
