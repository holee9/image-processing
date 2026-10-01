// #225 row 10 (GUI-C-189): the first native C-09 run failed without saying why; the app now publishes what the module itself answered.
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// The diagnostic surface added for the first native C-09 observation (main CI, 53ec370a: six failed calls, code -3 each, and no
/// "worker switched off" state). It has to decide between "the module was never on the worker path" and "the module counted and the
/// screen did not show it", and a native run cannot be made here, so these pin WHAT is recorded and WHERE; the values are read in CI.
/// Source readings: they see this tree's text only.
/// </summary>
[Trait("Category", "Functional")]
public sealed class AiDiagnosticsTests
{
    private static string Source(string relative) =>
        File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile(relative));

    [Fact]
    public void TheNativeSession_AsksTheModuleItsStateBeforeItsOwnInit_AndRecordsTheInitCall()
    {
        var runner = Source("gui/ImageProcTest/Services/Native/GuiAiRunner.cs");
        var init = runner.IndexOf("public static int InitCore(string directory) =>", StringComparison.Ordinal);
        Assert.True(init >= 0, "InitCore was not found.");
        var body = runner[init..Math.Min(runner.Length, init + 2500)];

        var probe = body.IndexOf("var probeCode = XpeAiNative.xpe_ai_worker_state(", StringComparison.Ordinal);
        var call = body.IndexOf("code = XpeAiNative.xpe_ai_init(directory, config);", StringComparison.Ordinal);
        var record = body.IndexOf("_initDiagnostics = $\"init#{++_initCount} dir='{directory}' config={config} {probe} -> code={code}\";", StringComparison.Ordinal);
        Assert.True(probe >= 0 && call > probe && record > call,
            "The probe must run BEFORE the init call (an earlier init is what the probe is for), and the call is recorded after it.");

        // The probe is INSIDE InitCore's lock lambda (the call-site invariant of AiBoneSuppressionStageTests allows exactly this one
        // extra site for the state call).
        Assert.True(body.IndexOf("WithLock(", StringComparison.Ordinal) is var lockAt && lockAt >= 0 && lockAt < probe, "The probe is not under InitCore's lock.");
    }

    [Fact]
    public void TheStatusRead_CarriesTheRawAnswersOfTheModule_NotOnlyTheGuisReadingOfThem()
    {
        var runner = Source("gui/ImageProcTest/Services/Native/GuiAiRunner.cs");
        var query = runner.IndexOf("public static AiWorkerStatus QueryWorkerState() =>", StringComparison.Ordinal);
        Assert.True(query >= 0, "QueryWorkerState was not found.");
        var body = runner[query..Math.Min(runner.Length, query + 1500)];

        Assert.Contains("read#{++_readCount} worker_state code={code} state={state} failures={failures} ceiling={ceiling}", body, StringComparison.Ordinal);
        Assert.Contains("the GUI answered this itself (the module was not asked)", body, StringComparison.Ordinal);
    }

    [Fact]
    public void TheDiagnostics_ReachTheAutomationTreeAsTheCheckboxItemStatus_AndAreNotShownToTheOperator()
    {
        var xaml = Source("gui/ImageProcTest/Views/AnalysisPanel.xaml");
        Assert.Contains("AutomationProperties.ItemStatus=\"{Binding AiWorkerDiagnostics}\"", xaml, StringComparison.Ordinal);
        Assert.DoesNotContain("Text=\"{Binding AiWorkerDiagnostics}\"", xaml, StringComparison.Ordinal);   // not a visible element

        var viewModel = Source("gui/ImageProcTest/ViewModels/MainWindowViewModel.cs");
        Assert.Contains("public string AiWorkerDiagnostics => _aiWorkerStatus.Diagnostics ?? string.Empty;", viewModel, StringComparison.Ordinal);
        Assert.Contains("OnPropertyChanged(nameof(AiWorkerDiagnostics));", viewModel, StringComparison.Ordinal);
    }

    /// <summary>
    /// The diagnostics change nothing the E2E asserts: the one-line summary (a regex target) and the mark come from the state and the
    /// counts alone.
    /// </summary>
    [Fact]
    public void ADiagnosticsString_ChangesNeitherTheSummaryNorTheMark()
    {
        var plain = new AiWorkerStatus(AiWorkerState.Disabled, 3, 3);
        var annotated = plain with { Diagnostics = "init#1 dir='x' config={\"use_worker\":true} before-init worker_state code=-6 -> code=0" };

        Assert.NotEqual(plain, annotated);
        Assert.Equal(AiBoneSuppressionStage.DescribeStatus(plain), AiBoneSuppressionStage.DescribeStatus(annotated));
        Assert.Equal(AiBoneSuppressionStage.ShowsMark(plain), AiBoneSuppressionStage.ShowsMark(annotated));
        Assert.Equal(AiBoneSuppressionStage.BannerFor(plain), AiBoneSuppressionStage.BannerFor(annotated));
    }

    /// <summary>
    /// GUI-C-189 / leader: the assertion is NOT loosened to make the run green. C-09 still requires the mark, still requires the numbers
    /// the module reported to be equal, and still requires the restart to bring back an active worker; the diagnostics are recorded
    /// and put in the failure message, and no assertion reads them.
    /// </summary>
    [Fact]
    public void C09_StillAssertsTheMarkTheCountsAndTheRestart_AndNoAssertionReadsTheDiagnostics()
    {
        var source = Source("clients/ImageProcTest.E2ETests/Scenarios/Workflows/ProcessingChainScenarios.cs");
        var start = source.IndexOf("public void C09_AWorkerSwitchedOffByRepeatedFailures_ShowsAMark_ThatRestartRemoves()", StringComparison.Ordinal);
        var end = source.IndexOf("private static FlaUI.Core.AutomationElements.AutomationElement? AiBanner(", start, StringComparison.Ordinal);
        Assert.True(start >= 0 && end > start, "C-09 was not found.");
        var c09 = source[start..end];

        Assert.Contains("Assert.True(banner is not null,", c09, StringComparison.Ordinal);
        Assert.Contains("Assert.Equal(numbers.Groups[2].Value, numbers.Groups[1].Value);", c09, StringComparison.Ordinal);
        Assert.Contains("Assert.Matches(@\"worker=Disabled; failures=(\\d+); ceiling=\\1$\", before);", c09, StringComparison.Ordinal);
        Assert.Contains("Assert.Matches(@\"^worker=Active; failures=0; ceiling=\\d+$\", after);", c09, StringComparison.Ordinal);
        Assert.Contains("Skip.If(app.BackendMode != \"Native\"", c09, StringComparison.Ordinal);   // the one skip is the Mock's, as before

        // The diagnostics are read into variables and printed; no Assert line mentions them.
        var assertLines = c09.Split('\n').Where(line => line.Contains("Assert.", StringComparison.Ordinal)).ToList();
        Assert.DoesNotContain(assertLines, line => line.Contains("diagnostics", StringComparison.OrdinalIgnoreCase)
            && !line.Contains("What the app said after each attempt", StringComparison.Ordinal));
        Assert.Contains("AiDiagnostics(window)", c09, StringComparison.Ordinal);
    }
}
