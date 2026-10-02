// #225 row 9 (GUI-C-196 M4): the whole baseline command apart from the window — two runs, verdict, DICOM, evidence file, status line — against scripted runs and an
// in-memory DICOM module; and the wiring checks that keep the command from touching settings or the image on screen.
using System.Text.Json;
using ImageProcTest.Models;
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

[Trait("Category", "Functional")]
public sealed class BaselineExecutionTests : IDisposable
{
    private readonly string _root = Path.Combine(Path.GetTempPath(), "xpe-baseline-exec-" + Guid.NewGuid().ToString("N"));

    public void Dispose()
    {
        try
        {
            Directory.Delete(_root, recursive: true);
        }
        catch (IOException)
        {
        }
    }

    private static readonly ushort[] Raw = [10, 20, 30, 40, 50, 60];
    private static readonly BaselineDicomMetadata Meta = new("CHEST", 120f, 0.15f);

    private static ChainResult Chain(StageStatus enhance = StageStatus.Applied, string preprocessReason = "Preprocess ok. uncalibrated EI (보정 안 된 EI) = 212.5, DI = 0.27 (measured, not a pass criterion).")
    {
        ushort[] pixels = [1, 2, 3, 4, 5, 6];
        return new ChainResult(Raw,
        [
            new StageOutcome(StageIds.Preprocess, StageStatus.Applied, pixels, preprocessReason, 11),
            new StageOutcome(StageIds.EnhanceBasic, enhance, enhance is StageStatus.Applied ? pixels : null, enhance is StageStatus.Applied ? "ok" : "refused by the module", 22),
        ]);
    }

    /// <summary>An in-memory DICOM module: what is written is what is read back, unless a knob says otherwise.</summary>
    private sealed class MemoryDicom : IDicomSession
    {
        public int Writes;
        public ushort[]? Written;
        public string Report = "{\"valid\":true,\"errors\":[],\"warnings\":[]}";

        public int Write(string path, ushort[] pixels, int width, int height, BaselineDicomMetadata metadata)
        {
            Writes++;
            Written = (ushort[])pixels.Clone();
            return 0;
        }

        public (int Code, string Json) Validate(string path) => (0, Report);

        public DicomReadBack ReadBack(string path) => new(0, 0, 0, 3, 2, (ushort[])Written!.Clone(), Meta, null);
    }

    private BaselineExecutionResult Run(Func<BaselineSingleRun> runOnce, IDicomSession? dicom, string? folder = null) =>
        BaselineExecution.Run((ushort[])Raw.Clone(), 3, 2, runOnce, dicom, Meta, folder ?? Path.Combine(_root, "baseline-1"));

    private static BaselineSingleRun Good() => new(Chain(), [100, 200, 300, 400, 500, 600]);

    [Fact]
    public void ABaselineThatPasses_RunsTwice_WritesTheFirstOutputOnce_AndSaysSo()
    {
        var runs = 0;
        var dicom = new MemoryDicom();
        var result = Run(() => { runs++; return Good(); }, dicom);

        Assert.True(result.Passed, result.Status);
        Assert.Equal(2, runs);
        Assert.Equal(1, dicom.Writes);
        Assert.Equal(new ushort[] { 100, 200, 300, 400, 500, 600 }, dicom.Written);
        Assert.StartsWith("Deterministic Baseline PASS: two runs bit-identical (3x2, ", result.Status, StringComparison.Ordinal);
        Assert.EndsWith("; DICOM valid)", result.Status, StringComparison.Ordinal);
        Assert.True(result.DicomValid);
        Assert.True(result.DicomRoundTripIdentical);
    }

    [Fact]
    public void TheEvidenceFile_CarriesTheVerdict_TheHashes_TheTimes_AndTheDicomOutcome()
    {
        var result = Run(Good, new MemoryDicom());

        Assert.Null(result.EvidenceWriteProblem);
        using var document = JsonDocument.Parse(File.ReadAllText(result.JsonPath));
        var root = document.RootElement;
        Assert.Equal("Pass", root.GetProperty("status").GetString());
        Assert.True(root.GetProperty("bitIdentical").GetBoolean());
        Assert.True(root.GetProperty("inputPreserved").GetBoolean());
        Assert.Equal(2, root.GetProperty("runsExecuted").GetInt32());
        Assert.Equal(64, root.GetProperty("outputSha256").GetString()!.Length);
        Assert.Equal(2, root.GetProperty("stageHashesRun1").GetArrayLength());
        Assert.Contains("preprocess=11 ms", root.GetProperty("stageTimes").GetString(), StringComparison.Ordinal);
        Assert.Equal(3000, root.GetProperty("budgetMs").GetInt32());
        Assert.Contains("not asserted", root.GetProperty("budgetNote").GetString(), StringComparison.Ordinal);
        Assert.True(root.GetProperty("dicom").GetProperty("valid").GetBoolean());
        Assert.Contains("uncalibrated EI", root.GetProperty("exposureIndex").GetString(), StringComparison.Ordinal);
        Assert.Equal(Path.Combine(_root, "baseline-1", "baseline.json"), result.JsonPath);
    }

    [Fact]
    public void TheExposureIndexIsLabelledUncalibrated_AndIsEmptyWhenTheStageDidNotMeasure()
    {
        var measured = Run(Good, new MemoryDicom());
        Assert.StartsWith("uncalibrated EI (보정 안 된 EI) = 212.5", measured.ExposureIndex, StringComparison.Ordinal);

        var unmeasured = Run(() => new BaselineSingleRun(Chain(preprocessReason: "Preprocess ok."), [1, 2, 3, 4, 5, 6]), new MemoryDicom(), Path.Combine(_root, "b2"));
        Assert.Equal(string.Empty, unmeasured.ExposureIndex);
    }

    [Fact]
    public void TwoRunsThatDiffer_Fail_NameTheFirstDifference_AndWriteNoDicom()
    {
        var calls = 0;
        var dicom = new MemoryDicom();
        var result = Run(() => new BaselineSingleRun(Chain(), calls++ == 0 ? [100, 200, 300, 400, 500, 600] : [100, 200, 300, 401, 500, 600]), dicom);

        Assert.False(result.Passed);
        Assert.Equal(0, dicom.Writes);   // D5: a baseline that did not pass leaves no DICOM output
        Assert.StartsWith("Deterministic Baseline FAIL: ", result.Status, StringComparison.Ordinal);
        Assert.Contains("first at pixel 3", result.Status, StringComparison.Ordinal);
        using var document = JsonDocument.Parse(File.ReadAllText(result.JsonPath));
        Assert.Equal("Fail", document.RootElement.GetProperty("status").GetString());
        Assert.Equal(3, document.RootElement.GetProperty("difference").GetProperty("firstIndex").GetInt32());
        Assert.Equal(JsonValueKind.Null, document.RootElement.GetProperty("dicom").ValueKind);
    }

    [Fact]
    public void ARefusedStage_EndsAfterOneRun_AsAFailure_WithNoDicom()
    {
        var runs = 0;
        var dicom = new MemoryDicom();
        var result = Run(() => { runs++; return new BaselineSingleRun(Chain(StageStatus.RequestedNotApplied), []); }, dicom);

        Assert.False(result.Passed);
        Assert.Equal(1, runs);
        Assert.Equal(0, dicom.Writes);
        Assert.Contains("enhance_basic", result.Status, StringComparison.Ordinal);
    }

    [Fact]
    public void ARunThatThrows_IsAFailureWithItsMessage()
    {
        var result = Run(() => throw new InvalidOperationException("the module is gone"), new MemoryDicom());
        Assert.False(result.Passed);
        Assert.Contains("the module is gone", result.Status, StringComparison.Ordinal);
    }

    [Fact]
    public void APassingComparisonWithAnInvalidDicomFile_IsStillAFailure()
    {
        var result = Run(Good, new MemoryDicom { Report = "{\"valid\":false,\"errors\":[{\"tag\":\"0010,0020\",\"message\":\"Missing\"}],\"warnings\":[]}" });

        Assert.False(result.Passed);
        Assert.True(result.Verdict.BitIdentical);   // the comparison itself passed: it is the file that failed
        Assert.False(result.DicomValid);
        Assert.StartsWith("Deterministic Baseline FAIL: DICOM export: ", result.Status, StringComparison.Ordinal);
    }

    [Fact]
    public void WithoutADicomSession_APassingComparisonCannotPass()
    {
        var result = Run(Good, null);
        Assert.False(result.Passed);
        Assert.Contains("no DICOM session", result.Status, StringComparison.Ordinal);
    }

    [Fact]
    public void AnEvidenceFolderThatCannotBeCreated_IsReported_AndTheBaselineDoesNotPassWithNoFile()
    {
        Directory.CreateDirectory(_root);
        var blocker = Path.Combine(_root, "a-file");
        File.WriteAllText(blocker, "x");

        var result = Run(Good, new MemoryDicom(), Path.Combine(blocker, "baseline-1"));   // a folder cannot be created under a file

        Assert.NotNull(result.EvidenceWriteProblem);
        Assert.False(result.Passed);   // the DICOM export needs the same folder: a baseline whose output could not be written does not pass
    }

    [Fact]
    public void TheBudgetIsMeasuredAgainst_NeverAsserted()
    {
        Assert.Equal(3000, BaselineExecution.BudgetMs);

        var source = Read("gui/ImageProcTest/Services/BaselineExecution.cs");
        var uses = System.Text.RegularExpressions.Regex.Matches(source, @"BudgetMs").Count;
        // the declaration and the evidence file's budgetMs entry; a comparison would be a third use
        Assert.Equal(2, uses);
    }

    // ---- wiring (source reading) ---------------------------------------------------------------------------------------------------------

    private static string Read(string relative) => File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile(relative)).Replace("\r\n", "\n");

    private static string Between(string text, string start, string end)
    {
        var from = text.IndexOf(start, StringComparison.Ordinal);
        Assert.True(from >= 0, "missing: " + start);
        var to = text.IndexOf(end, from, StringComparison.Ordinal);
        Assert.True(to > from, "missing end: " + end);
        return text[from..to];
    }

    [Fact]
    public void TheCommand_ChangesNoSetting_AndNoImageOnScreen_AndNoStageTiming()
    {
        var vm = Read("gui/ImageProcTest/ViewModels/MainWindowViewModel.cs");
        var body = Between(vm, "internal async Task RunDeterministicBaselineAsync()", "private void FinishBaselineWithoutRun(");

        // Only a snapshot of the settings is read, and nothing is assigned to them.
        var settingsUses = System.Text.RegularExpressions.Regex.Matches(body, @"Settings\.\w+").Select(m => m.Value).Distinct().ToArray();
        Assert.Equal(["Settings.Snapshot"], settingsUses);

        foreach (var forbidden in new[] { "ApplyDisplayPipelineAsync", "ActiveImageFrame =", "ProcessedImage =", "SourceImage =", "ChainStatus =", "PipelineTimings =", "LastChain =", "ReportChain(", "RenderLanesAsync" })
        {
            Assert.DoesNotContain(forbidden, body, StringComparison.Ordinal);
        }

        // A lifetime ticket, not a request number: an Apply in flight is neither superseded by the baseline nor does it supersede it.
        Assert.Contains("TakeTicket()", body, StringComparison.Ordinal);
        Assert.DoesNotContain("TakeRequestTicket()", body, StringComparison.Ordinal);
    }

    [Fact]
    public void TheBaselineDisplay_ReadsNoUserDisplaySetting()
    {
        var real = Read("gui/ImageProcTest/Services/RealXpeBackend.cs");
        var display = Between(real, "private static ushort[] RunBaselineDisplay(", "    /// <summary>\n    /// #225 row 9 (GUI-C-196 M2)");
        Assert.DoesNotContain("settings", display, StringComparison.OrdinalIgnoreCase);
        Assert.Contains("BaselineParameters.VoiWindowCenter", display, StringComparison.Ordinal);
        Assert.Contains("BaselineParameters.GsdfEnabled", display, StringComparison.Ordinal);

        var once = Between(real, "BaselineSingleRun IBaselineBackend.RunBaselineOnce(", "private static ushort[] RunBaselineDisplay(");
        Assert.Contains("BaselineParameters.ForBaseline(settings)", once, StringComparison.Ordinal);
        Assert.Contains("ProcessingChainPlan.BuildBaselineStages()", once, StringComparison.Ordinal);
    }

    [Fact]
    public void TheOrdinaryChain_NeverMeasuresTheExposureIndex()
    {
        var real = Read("gui/ImageProcTest/Services/RealXpeBackend.cs");
        Assert.Contains("RunChainCore(rawFrame, stages, settings, measureExposureIndex: false)", real, StringComparison.Ordinal);
        Assert.Equal(1, System.Text.RegularExpressions.Regex.Matches(real, @"measureExposureIndex: true").Count);

        var preprocess = Read("gui/ImageProcTest/Services/Native/GuiPreprocessRunner.cs");
        Assert.Contains("bool measureExposureIndex = false", preprocess, StringComparison.Ordinal);
        Assert.Contains("measureExposureIndex ? MeasureUncalibratedExposureIndex(", preprocess, StringComparison.Ordinal);
    }

    [Fact]
    public void TheMenuItem_IsACommandBoundToTheBackendCapability_AndLeftTheDisabledList()
    {
        var xaml = Read("gui/ImageProcTest/MainWindow.xaml");
        var item = Between(xaml, "x:Name=\"RunDeterministicBaselineMenuItem\"", "/>");
        Assert.Contains("Command=\"{Binding RunDeterministicBaselineCommand}\"", item, StringComparison.Ordinal);
        Assert.Contains("IsEnabled=\"{Binding CanRunDeterministicBaseline}\"", item, StringComparison.Ordinal);
        Assert.DoesNotContain("IsEnabled=\"False\"", item, StringComparison.Ordinal);

        var code = Read("gui/ImageProcTest/MainWindow.xaml.cs");
        var list = Between(code, "report.DisabledFutureCommandCount = new[]", ".Count(item => !item.IsEnabled)");
        Assert.DoesNotContain("RunDeterministicBaselineMenuItem,", list, StringComparison.Ordinal);

        var real = Read("gui/ImageProcTest/Services/RealXpeBackend.cs");
        Assert.Contains("IBaselineBackend", Between(real, "public sealed class RealXpeBackend", "{"), StringComparison.Ordinal);
        var mock = Read("gui/ImageProcTest/Services/MockXpeBackend.cs");
        Assert.DoesNotContain("IBaselineBackend", mock, StringComparison.Ordinal);
    }
}
