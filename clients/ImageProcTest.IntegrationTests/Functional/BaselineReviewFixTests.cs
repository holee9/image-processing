// #225 row 9 (GUI-C-196 M6): the three findings of Codex #73, each pinned through the path the app really takes.
//  1. non-finite values must reach the verdict from the stages, through the same adapters and the same chain runner the real backend uses;
//  2. the evidence file is required: a record that cannot be written is a Fail;
//  3. a DICOM file that did not pass never carries the final name.
using System.Text.Json;
using ImageProcTest.Models;
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

[Trait("Category", "Functional")]
public sealed class BaselineReviewFixTests : IDisposable
{
    private readonly string _root = Path.Combine(Path.GetTempPath(), "xpe-baseline-m6-" + Guid.NewGuid().ToString("N"));
    private readonly List<IDisposable> _held = [];

    public void Dispose()
    {
        foreach (var held in _held)
        {
            held.Dispose();
        }

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

    // ---- a module that can poison the float image, behind the real EnhanceBasicStage ------------------------------------------------------

    private sealed class PoisoningBackend(int poison) : IEnhanceBasicBackend
    {
        public IEnhanceImage Open(ushort[] input, int width, int height) => new Image(input.Select(v => (float)v).ToArray(), poison);

        private sealed class Image(float[] data, int poison) : IEnhanceImage
        {
            public int LogTransform(float normFactor) => 0;

            public int NoiseReduceBilateral(float sigmaSpace, float sigmaRange)
            {
                for (var i = 0; i < poison; i++)
                {
                    data[i] = i % 2 == 0 ? float.NaN : float.PositiveInfinity;
                }

                return 0;
            }

            public int ContrastEnhance(float clipLimit, int tileWidth, int tileHeight) => 0;

            public int EdgeEnhance(float amount, float radius, float threshold) => 0;

            public long CountNonFinite() => BaselineStageAdapters.CountNonFinite(data);

            public float[] ReadFloats() => (float[])data.Clone();

            public void Dispose()
            {
            }
        }
    }

    /// <summary>One baseline run built the way RealXpeBackend builds it: the real chain runner, the real adapters, the real enhance stage.</summary>
    private static BaselineSingleRun RunOnce(int enhancePoison, long preprocessNonFinite)
    {
        var chain = ProcessingChainRunner.Run(Raw, ProcessingChainPlan.BuildBaselineStages(), (request, input) => request.StageId switch
        {
            StageIds.Preprocess => BaselineStageAdapters.FromPreprocess(true, input.Select(v => (ushort)(v + 1)).ToArray(), "Preprocess ok.", preprocessNonFinite),
            StageIds.EnhanceBasic => BaselineStageAdapters.FromEnhance(EnhanceBasicStage.Run(input, 3, 2, new PoisoningBackend(enhancePoison))),
            _ => new StageExecution(false, null, "not available"),
        });
        var applied = chain.Stages.All(s => s.Status is StageStatus.Applied or StageStatus.AppliedNoChange);
        return new BaselineSingleRun(chain, applied ? chain.DisplayInput : []);
    }

    private sealed class FileDicom : IDicomSession
    {
        public int Writes;
        public string Report = "{\"valid\":true,\"errors\":[],\"warnings\":[]}";
        public ushort[]? Written;
        public Action<string>? AfterWrite;

        public int Write(string path, ushort[] pixels, int width, int height, BaselineDicomMetadata metadata)
        {
            Writes++;
            Written = (ushort[])pixels.Clone();
            File.WriteAllBytes(path, [0x44, 0x49, 0x43, 0x4D]);
            AfterWrite?.Invoke(path);
            return 0;
        }

        public (int Code, string Json) Validate(string path) => (0, Report);

        public DicomReadBack ReadBack(string path) => new(0, 0, 0, 3, 2, (ushort[])Written!.Clone(), Meta, null);
    }

    private BaselineExecutionResult Execute(Func<BaselineSingleRun> run, IDicomSession dicom, string? folder = null) =>
        BaselineExecution.Run((ushort[])Raw.Clone(), 3, 2, run, dicom, Meta, folder ?? Path.Combine(_root, "baseline-1"));

    private static JsonElement ReadJson(BaselineExecutionResult result) => JsonDocument.Parse(File.ReadAllText(result.JsonPath)).RootElement;

    // ---- finding 1: non-finite counts ------------------------------------------------------------------------------------------------------

    [Fact]
    public void CountNonFinite_CountsNaN_BothInfinities_AndNothingElse()
    {
        Assert.Equal(0, BaselineStageAdapters.CountNonFinite([]));
        Assert.Equal(0, BaselineStageAdapters.CountNonFinite([0f, -0f, 1f, -1f, float.MaxValue, float.MinValue, float.Epsilon, 1e-45f, 65535f]));
        Assert.Equal(1, BaselineStageAdapters.CountNonFinite([float.NaN]));
        Assert.Equal(2, BaselineStageAdapters.CountNonFinite([float.PositiveInfinity, float.NegativeInfinity, 3f]));
        Assert.Equal(3, BaselineStageAdapters.CountNonFinite([BitConverter.Int32BitsToSingle(unchecked((int)0xFFC00000)), 1f, float.NaN, float.PositiveInfinity]));
    }

    [Fact]
    public void TheChainRunner_CarriesAStagesCount_ToTheChainResult_EvenForARefusedStage()
    {
        var chain = ProcessingChainRunner.Run(Raw, ProcessingChainPlan.BuildBaselineStages(), (request, input) => request.StageId == StageIds.Preprocess
            ? new StageExecution(true, input.Select(v => (ushort)(v + 1)).ToArray(), "ok", NonFiniteCount: 4)
            : new StageExecution(false, null, "refused", NonFiniteCount: 9));

        Assert.Equal(4, chain.Stages.Single(s => s.StageId == StageIds.Preprocess).NonFiniteCount);
        var refused = chain.Stages.Single(s => s.StageId == StageIds.EnhanceBasic);
        Assert.Equal(StageStatus.RequestedNotApplied, refused.Status);
        Assert.Equal(9, refused.NonFiniteCount);
    }

    [Fact]
    public void ANonFiniteValueInTheEnhanceStage_ThroughTheRealStageAdapterAndRunner_FailsTheBaseline_WithTheCount_InTheEvidenceFile()
    {
        var dicom = new FileDicom();
        var result = Execute(() => RunOnce(enhancePoison: 3, preprocessNonFinite: 0), dicom);

        Assert.False(result.Passed);
        Assert.Equal(3, result.Verdict.NaNInfCount);                  // the count made inside EnhanceBasicStage reached the verdict
        Assert.Equal(0, dicom.Writes);
        var json = ReadJson(result);
        Assert.Equal("Fail", json.GetProperty("status").GetString());
        Assert.Equal(3, json.GetProperty("nanInfCount").GetInt64());
        Assert.Contains($"{StageIds.EnhanceBasic}=3", json.GetProperty("nonFiniteByStageRun1").EnumerateArray().Select(e => e.GetString()));
        Assert.Contains("non-finite", result.Status, StringComparison.Ordinal);
    }

    [Fact]
    public void ANonFiniteValueInThePreprocessFloatImage_FailsTheBaseline_EvenWhenTheStageApplied()
    {
        var dicom = new FileDicom();
        var result = Execute(() => RunOnce(enhancePoison: 0, preprocessNonFinite: 5), dicom);

        Assert.False(result.Passed);
        Assert.Equal(10, result.Verdict.NaNInfCount);                 // 5 per run, two runs
        Assert.Equal(0, dicom.Writes);
        Assert.Contains("10 non-finite", result.Status, StringComparison.Ordinal);
        Assert.Equal(10, ReadJson(result).GetProperty("nanInfCount").GetInt64());
    }

    [Fact]
    public void WithNoNonFiniteValue_TheSameWiringPasses()
    {
        var dicom = new FileDicom();
        var result = Execute(() => RunOnce(enhancePoison: 0, preprocessNonFinite: 0), dicom);

        Assert.True(result.Passed, result.Status);
        Assert.Equal(0, result.Verdict.NaNInfCount);
        Assert.Equal(1, dicom.Writes);
    }

    [Fact]
    public void TheRealBackendAndThePreprocessRunner_UseTheCountingHops()
    {
        var real = Read("gui/ImageProcTest/Services/RealXpeBackend.cs");
        Assert.Contains("return BaselineStageAdapters.FromPreprocess(result.Ran, result.Pixels, result.Summary, result.NonFiniteCount);", real, StringComparison.Ordinal);
        Assert.Contains("return BaselineStageAdapters.FromEnhance(result);", real, StringComparison.Ordinal);

        var preprocess = Read("gui/ImageProcTest/Services/Native/GuiPreprocessRunner.cs");
        // GUI-C-232b: the shipped path is xpe_preprocess_pipeline_out, whose gain output is internal; the count is taken on the pipeline's float output, still BEFORE it is scaled to 16 bits.
        var counted = preprocess.IndexOf("BaselineStageAdapters.CountPreprocessNonFinite(ReadOnlySpan<float>.Empty, floats)", StringComparison.Ordinal);
        var scaled = preprocess.IndexOf("ScaleToUInt16(floats),", StringComparison.Ordinal);
        Assert.True(counted >= 0 && scaled > counted, "the non-finite count must be taken on the float image BEFORE the result is scaled to 16 bits");
    }

    // ---- finding 2: the evidence file is required --------------------------------------------------------------------------------------------

    [Fact]
    public void WhenOnlyTheEvidenceFileCannotBeWritten_TheBaselineFails_AndLeavesNoDicomFile()
    {
        var folder = Path.Combine(_root, "baseline-1");
        // A DIRECTORY with the evidence file's name: DICOM can succeed, the JSON cannot. Created while the DICOM is written, i.e. AFTER the run's own clearing of old outputs
        // (M8): a directory that is there beforehand is refused up front, see APreviousDirectoryUnderAFinalName_FailsTheRunBeforeAnythingIsWritten.
        var dicom = new FileDicom { AfterWrite = _ => Directory.CreateDirectory(Path.Combine(folder, "baseline.json")) };

        var result = Execute(() => RunOnce(0, 0), dicom, folder);

        Assert.Equal(1, dicom.Writes);                                     // the export itself succeeded
        Assert.False(result.Passed);
        Assert.NotNull(result.EvidenceWriteProblem);
        Assert.StartsWith("Deterministic Baseline FAIL: the evidence file", result.Status, StringComparison.Ordinal);
        Assert.DoesNotContain("PASS", result.Status, StringComparison.Ordinal);
        Assert.False(File.Exists(Path.Combine(folder, "baseline.dcm")));
        Assert.False(File.Exists(Path.Combine(folder, "baseline.dcm" + BaselineDicomExport.PartialSuffix)));
    }

    // ---- finding 3: a failed DICOM never carries the final name ----------------------------------------------------------------------------------

    [Fact]
    public void AnInvalidDicomFile_LeavesNothingUnderTheFinalNameOrThePartialName()
    {
        var folder = Path.Combine(_root, "baseline-1");
        var dicom = new FileDicom { Report = "{\"valid\":false,\"errors\":[{\"tag\":\"0010,0020\",\"message\":\"Missing\"}],\"warnings\":[]}" };

        var result = Execute(() => RunOnce(0, 0), dicom, folder);

        Assert.Equal(1, dicom.Writes);
        Assert.False(result.Passed);
        Assert.False(File.Exists(Path.Combine(folder, "baseline.dcm")), "a file that failed validation was left under the final name");
        Assert.False(File.Exists(Path.Combine(folder, "baseline.dcm" + BaselineDicomExport.PartialSuffix)), "the partial file was not cleaned up");
        Assert.Equal("Fail", ReadJson(result).GetProperty("status").GetString());
    }

    [Fact]
    public void APassingBaseline_HasTheFinalFileOnly_AndTheEvidenceSaysItIsThere()
    {
        var folder = Path.Combine(_root, "baseline-1");
        var result = Execute(() => RunOnce(0, 0), new FileDicom(), folder);

        Assert.True(result.Passed, result.Status);
        Assert.True(File.Exists(Path.Combine(folder, "baseline.dcm")));
        Assert.False(File.Exists(Path.Combine(folder, "baseline.dcm" + BaselineDicomExport.PartialSuffix)));
        var json = ReadJson(result);
        Assert.Equal("Pass", json.GetProperty("status").GetString());
        Assert.True(json.GetProperty("dicom").GetProperty("finalFileWritten").GetBoolean());
    }

    [Fact]
    public void WhileTheFinalNameIsBlocked_TheBaselineFails_AndLeavesNoPartialFile()
    {
        var folder = Path.Combine(_root, "baseline-1");
        // The final name is taken by a directory, so the rename cannot happen. Created while the DICOM is written (after the run's own clearing, M8).
        var dicomSession = new FileDicom { AfterWrite = _ => Directory.CreateDirectory(Path.Combine(folder, "baseline.dcm")) };

        var result = Execute(() => RunOnce(0, 0), dicomSession, folder);

        Assert.False(result.Passed);
        Assert.Contains("final name", result.Status, StringComparison.Ordinal);
        Assert.False(File.Exists(Path.Combine(folder, "baseline.dcm" + BaselineDicomExport.PartialSuffix)));
        var json = ReadJson(result);
        Assert.Equal("Fail", json.GetProperty("status").GetString());
        Assert.False(json.GetProperty("dicom").GetProperty("finalFileWritten").GetBoolean());
    }

    [Fact]
    public void AFailureToRemoveThePartialFile_IsRecordedInTheResult_NotSwallowed()
    {
        var folder = Path.Combine(_root, "baseline-1");
        var locked = new FileDicom { Report = "{\"valid\":false,\"errors\":[],\"warnings\":[]}" };
        // the session keeps the file it wrote open, so removing it fails
        FileStream? stream = null;
        locked.AfterWrite = path => { stream = new FileStream(path, FileMode.Open, FileAccess.ReadWrite, FileShare.None); _held.Add(stream); };

        var export = BaselineDicomExport.Export(Path.Combine(folder, "baseline.dcm"), [1, 2, 3, 4, 5, 6], 3, 2, Meta, locked);

        Assert.NotNull(stream);
        Assert.False(export.Passed);
        Assert.NotNull(export.CleanupProblem);
        Assert.Contains("could not be removed", export.Summary, StringComparison.Ordinal);
    }

    // ---- M7: the display step's float intermediates ----------------------------------------------------------------------------------------------

    private sealed class FakeDisplayBackend(int poison, string? poisonAfter = null, int refuseCode = 0, string? refuseStep = null) : IBaselineDisplayBackend
    {
        public readonly int Poison = poison;
        public readonly string? PoisonAfter = poisonAfter;
        public readonly int RefuseCode = refuseCode;
        public readonly string? RefuseStep = refuseStep;
        public readonly List<string> Calls = [];
        public int Opens;

        public IBaselineDisplayImage Open(ushort[] input, int width, int height)
        {
            Opens++;
            return new Image(this, input.Select(v => (float)v).ToArray());
        }

        private sealed class Image(FakeDisplayBackend owner, float[] data) : IBaselineDisplayImage
        {
            private int Step(string name, string call)
            {
                owner.Calls.Add(call);
                if (owner.PoisonAfter == name)
                {
                    for (var i = 0; i < owner.Poison; i++)
                    {
                        data[i] = i % 2 == 0 ? float.NaN : float.NegativeInfinity;
                    }
                }

                return owner.RefuseStep == name ? owner.RefuseCode : 0;
            }

            public int ApplyModality(float slope, float intercept) => Step("modality", $"modality({slope},{intercept})");

            public int ApplyVoi(float center, float width) => Step("voi", $"voi({center},{width})");

            public int ApplyPresentation(bool gsdfEnabled) => Step("presentation", $"presentation({gsdfEnabled})");

            public float[] ReadFloats() => (float[])data.Clone();

            public ushort[] ReadUInt16() => data.Select(v => (ushort)Math.Clamp(v, 0f, 65535f)).ToArray();

            public void Dispose()
            {
            }
        }
    }

    /// <summary>One baseline run the way RealXpeBackend composes it: the real chain runner and adapters, then BaselineDisplayStage.ComposeRun.</summary>
    private static BaselineSingleRun RunOnceThroughTheDisplay(FakeDisplayBackend display)
    {
        var chain = ProcessingChainRunner.Run(Raw, ProcessingChainPlan.BuildBaselineStages(), (request, input) => request.StageId switch
        {
            StageIds.Preprocess => BaselineStageAdapters.FromPreprocess(true, input.Select(v => (ushort)(v + 1)).ToArray(), "Preprocess ok.", 0),
            StageIds.EnhanceBasic => BaselineStageAdapters.FromEnhance(EnhanceBasicStage.Run(input, 3, 2, new PoisoningBackend(0))),
            _ => new StageExecution(false, null, "not available"),
        });
        return BaselineDisplayStage.ComposeRun(chain, 3, 2, display);
    }

    [Fact]
    public void TheDisplayStep_RunsTheThreeLutsInOrder_WithTheFixedParameters_AndCountsNothingWhenClean()
    {
        var display = new FakeDisplayBackend(poison: 0);
        var run = RunOnceThroughTheDisplay(display);

        Assert.Equal(
            [
                $"modality({BaselineParameters.ModalityRescaleSlope},{BaselineParameters.ModalityRescaleIntercept})",
                $"voi({BaselineParameters.VoiWindowCenter},{BaselineParameters.VoiWindowWidth})",
                $"presentation({BaselineParameters.GsdfEnabled})",
            ],
            display.Calls);
        Assert.Equal(0, run.NaNInfCount);
        Assert.Equal(Raw.Length, run.Output.Length);
    }

    [Theory]
    [InlineData("modality", 2, 1)]       // a non-finite value after the modality LUT ends the step before the VOI LUT
    [InlineData("voi", 3, 2)]            // after the VOI LUT: before the presentation LUT
    public void ANonFiniteValueInTheDisplaysFloatImage_IsCounted_StopsTheStep_AndLeavesNoPixels(string after, int poison, int callsMade)
    {
        var display = new FakeDisplayBackend(poison, poisonAfter: after);
        var run = RunOnceThroughTheDisplay(display);

        Assert.Equal(poison, run.NaNInfCount);
        Assert.Empty(run.Output);
        Assert.Equal(callsMade, display.Calls.Count);   // nothing after the poisoned LUT ran on the poisoned image
    }

    [Fact]
    public void ARefusedDisplayLut_Throws_NamingTheFunctionAndTheCode()
    {
        var display = new FakeDisplayBackend(0, refuseCode: -3, refuseStep: "voi");
        var ex = Assert.Throws<InvalidOperationException>(() => RunOnceThroughTheDisplay(display));
        Assert.Contains("xpe_apply_voi_lut", ex.Message, StringComparison.Ordinal);
        Assert.Contains("-3", ex.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void WhenAChainStageWasRefused_TheDisplayIsNotRun()
    {
        var display = new FakeDisplayBackend(0);
        var chain = ProcessingChainRunner.Run(Raw, ProcessingChainPlan.BuildBaselineStages(), (request, input) => new StageExecution(false, null, "refused"));

        var run = BaselineDisplayStage.ComposeRun(chain, 3, 2, display);

        Assert.Equal(0, display.Opens);
        Assert.Empty(run.Output);
    }

    [Fact]
    public void ANonFiniteValueInTheDisplayStep_ThroughTheRealComposition_FailsTheBaseline_WithTheCount_InTheEvidenceFile()
    {
        var dicom = new FileDicom();
        var result = Execute(() => RunOnceThroughTheDisplay(new FakeDisplayBackend(poison: 2, poisonAfter: "voi")), dicom);

        Assert.False(result.Passed);
        Assert.Equal(4, result.Verdict.NaNInfCount);                  // 2 per run, two runs: the chain and the comparison were fine, only the display count fails it
        Assert.Contains("4 non-finite", result.Status, StringComparison.Ordinal);
        Assert.Equal(0, dicom.Writes);
        var json = ReadJson(result);
        Assert.Equal("Fail", json.GetProperty("status").GetString());
        Assert.Equal(4, json.GetProperty("nanInfCount").GetInt64());
        Assert.Contains("display=2", json.GetProperty("nonFiniteByStageRun1").EnumerateArray().Select(e => e.GetString()));
    }

    [Fact]
    public void WithNoNonFiniteValueInTheDisplayStep_TheSameCompositionPasses()
    {
        var dicom = new FileDicom();
        var result = Execute(() => RunOnceThroughTheDisplay(new FakeDisplayBackend(poison: 0)), dicom);

        Assert.True(result.Passed, result.Status);
        Assert.Contains("display=0", ReadJson(result).GetProperty("nonFiniteByStageRun1").EnumerateArray().Select(e => e.GetString()));
    }

    private static string Read(string relative) => File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile(relative)).Replace("\r\n", "\n");
    // ---- Codex #76 finding 1 (M8): a reused evidence folder ----------------------------------------------------------------------------------

    private string PassInto(string folder, FileDicom dicom)
    {
        var first = Execute(() => RunOnce(0, 0), dicom, folder);
        Assert.True(first.Passed, first.Status);
        Assert.True(File.Exists(Path.Combine(folder, "baseline.dcm")));
        return folder;
    }

    [Fact]
    public void APassThenAChainFailure_InTheSameFolder_LeavesNoFinalDicomFile_AndTheJsonSaysFail()
    {
        var folder = PassInto(Path.Combine(_root, "reused-chain"), new FileDicom());

        var second = Execute(() => RunOnce(enhancePoison: 1, preprocessNonFinite: 0), new FileDicom(), folder);

        Assert.False(second.Passed);
        Assert.False(File.Exists(Path.Combine(folder, "baseline.dcm")), "the previous run's passing DICOM file is still there under its proper name");
        Assert.Equal("Fail", ReadJson(second).GetProperty("status").GetString());
    }

    [Fact]
    public void APassThenADicomFailure_InTheSameFolder_LeavesNoFinalDicomFile()
    {
        var folder = PassInto(Path.Combine(_root, "reused-dicom"), new FileDicom());

        var second = Execute(() => RunOnce(0, 0), new FileDicom { Report = "{\"valid\":false,\"errors\":[\"x\"],\"warnings\":[]}" }, folder);

        Assert.False(second.Passed);
        Assert.False(File.Exists(Path.Combine(folder, "baseline.dcm")), "the previous run's passing DICOM file is still there under its proper name");
        Assert.False(File.Exists(Path.Combine(folder, "baseline.dcm" + BaselineDicomExport.PartialSuffix)));
        Assert.Equal("Fail", ReadJson(second).GetProperty("status").GetString());
    }

    [Fact]
    public void APreviousFileThatCannotBeRemoved_FailsTheNewRun_WithTheReason_AndWritesNoDicom()
    {
        var folder = PassInto(Path.Combine(_root, "reused-locked"), new FileDicom());
        var dicom = new FileDicom();

        // Held open without sharing: File.Delete throws IOException on Windows, as it would for a file another program has open.
        using (new FileStream(Path.Combine(folder, "baseline.dcm"), FileMode.Open, FileAccess.Read, FileShare.None))
        {
            var second = Execute(() => RunOnce(0, 0), dicom, folder);

            Assert.False(second.Passed);
            Assert.Contains("could not be removed", second.Status, StringComparison.Ordinal);
            Assert.Contains("baseline.dcm", second.Status, StringComparison.Ordinal);
            Assert.Equal(0, dicom.Writes);
        }
    }

    [Fact]
    public void APreviousDirectoryUnderAFinalName_FailsTheRunBeforeAnythingIsWritten()
    {
        var folder = Path.Combine(_root, "reused-directory");
        Directory.CreateDirectory(Path.Combine(folder, "baseline.dcm"));
        var dicom = new FileDicom();

        var result = Execute(() => RunOnce(0, 0), dicom, folder);

        Assert.False(result.Passed);
        Assert.Contains("could not be removed", result.Status, StringComparison.Ordinal);
        Assert.Equal(0, dicom.Writes);
    }

    [Fact]
    public void AFirstRunInAFreshFolder_IsNotAffectedByTheClearing()
    {
        var result = Execute(() => RunOnce(0, 0), new FileDicom(), Path.Combine(_root, "never-existed", "nested"));

        Assert.True(result.Passed, result.Status);
    }

    // ---- Codex #76 finding 2 (M8): the gain stage's float output ---------------------------------------------------------------------------

    [Fact]
    public void ThePreprocessCount_IncludesTheGainOutput_EvenWhenTheDefectStageFillsThePixelWithAFiniteValue()
    {
        float[] gain = [1f, float.NaN, 3f, 4f, 5f, float.PositiveInfinity];
        float[] defectFilled = [1f, 2f, 3f, 4f, 5f, 6f];

        Assert.Equal(0, BaselineStageAdapters.CountNonFinite(defectFilled));   // what was counted before M8: nothing
        Assert.Equal(2, BaselineStageAdapters.CountPreprocessNonFinite(gain, defectFilled));
        Assert.Equal(3, BaselineStageAdapters.CountPreprocessNonFinite(gain, [float.NaN, 2f, 3f, 4f, 5f, 6f]));
        Assert.Equal(0, BaselineStageAdapters.CountPreprocessNonFinite(defectFilled, defectFilled));
    }

    [Fact]
    public void ANonFiniteGainOutput_ThatTheDefectStageHid_FailsTheBaseline_WithTheCountInTheStatusAndTheJson()
    {
        float[] gain = [1f, float.NaN, 3f, 4f, 5f, 6f];
        float[] defectFilled = [1f, 2f, 3f, 4f, 5f, 6f];
        var counted = BaselineStageAdapters.CountPreprocessNonFinite(gain, defectFilled);
        var dicom = new FileDicom();

        var result = Execute(() => RunOnce(0, counted), dicom, Path.Combine(_root, "gain-nan"));

        Assert.False(result.Passed);
        Assert.Equal(0, dicom.Writes);
        Assert.Contains("non-finite", result.Status, StringComparison.Ordinal);
        Assert.Equal(2, ReadJson(result).GetProperty("nanInfCount").GetInt64());   // one per run
        Assert.Contains("preprocess=1", ReadJson(result).GetProperty("nonFiniteByStageRun1").EnumerateArray().Select(e => e.GetString()), StringComparer.Ordinal);

        // The control: the same chain with the pre-M8 count (the filled image alone) passes, so the Fail above comes from the gain count and from nothing else.
        var control = Execute(() => RunOnce(0, BaselineStageAdapters.CountNonFinite(defectFilled)), new FileDicom(), Path.Combine(_root, "gain-control"));
        Assert.True(control.Passed, control.Status);
    }

    [Fact]
    public void TheRealPreprocessRunner_CountsNonFiniteValuesOnThePipelineOutput_BeforeScaling()
    {
        // GUI-C-232b (leader decision): this used to pin "the gain output is read before the defect stage is called, and both float arrays reach the count". With xpe_preprocess_pipeline_out the gain
        // stage's intermediate image is internal to the module, so a non-finite value the gain stage makes and the defect stage repairs is no longer visible to the app; the count is of the final output only.
        var code = Read("gui/ImageProcTest/Services/Native/GuiPreprocessRunner.cs");
        Assert.DoesNotContain("xpe_defect_correct(", code, StringComparison.Ordinal);
        Assert.Contains("BaselineStageAdapters.CountPreprocessNonFinite(ReadOnlySpan<float>.Empty, floats)", code, StringComparison.Ordinal);
    }

    // ---- Codex #78 finding 1 (M9): one run at a time in a folder ---------------------------------------------------------------------------

    [Theory]
    [InlineData(true)]
    [InlineData(false)]
    public async Task TwoRunsInOneFolderAtOnce_OneRuns_TheOtherFailsAtOnce_AndTheHoldersFilesAreUntouched(bool holderPasses)
    {
        var folder = Path.Combine(_root, "concurrent-" + holderPasses);
        using var started = new ManualResetEventSlim();
        using var release = new ManualResetEventSlim();
        var holderDicom = new FileDicom();

        var holder = Task.Run(() => Execute(() =>
        {
            started.Set();
            Assert.True(release.Wait(TimeSpan.FromSeconds(30)));
            return holderPasses ? RunOnce(0, 0) : RunOnce(enhancePoison: 1, preprocessNonFinite: 0);
        }, holderDicom, folder));
        Assert.True(started.Wait(TimeSpan.FromSeconds(30)), "the first run never started");

        var rivalDicom = new FileDicom();
        var rival = Execute(() => throw new InvalidOperationException("the second run must not run: the folder is taken"), rivalDicom, folder);

        Assert.False(rival.Passed);
        Assert.Contains("being used by another run", rival.Status, StringComparison.Ordinal);
        Assert.Equal(0, rivalDicom.Writes);
        Assert.False(File.Exists(rival.JsonPath), "the second run wrote into a folder it does not own");

        release.Set();
        var first = await holder;

        Assert.Equal(holderPasses, first.Passed);
        Assert.Equal(holderPasses, File.Exists(Path.Combine(folder, "baseline.dcm")));
        Assert.Equal(holderPasses ? "Pass" : "Fail", ReadJson(first).GetProperty("status").GetString());
        Assert.False(File.Exists(Path.Combine(folder, BaselineExecution.LockFileName)), "the lock file outlived the run");

        // The lock is released with the run: the next run in the same folder is not refused.
        var third = Execute(() => RunOnce(0, 0), new FileDicom(), folder);
        Assert.True(third.Passed, third.Status);
    }

    [Fact]
    public void TheLockFile_IsNeverAnOutput_PassOrFail_EvenWhenTheRunThrows()
    {
        var passFolder = Path.Combine(_root, "lock-pass");
        Assert.True(Execute(() => RunOnce(0, 0), new FileDicom(), passFolder).Passed);
        Assert.Equal(["baseline.dcm", "baseline.json"], Directory.GetFiles(passFolder).Select(Path.GetFileName).OrderBy(n => n, StringComparer.Ordinal).ToArray());

        var failFolder = Path.Combine(_root, "lock-fail");
        Assert.False(Execute(() => RunOnce(enhancePoison: 1, preprocessNonFinite: 0), new FileDicom(), failFolder).Passed);
        Assert.Equal(["baseline.json"], Directory.GetFiles(failFolder).Select(Path.GetFileName).ToArray());

        var throwFolder = Path.Combine(_root, "lock-throw");
        Assert.False(Execute(() => throw new InvalidOperationException("boom"), new FileDicom(), throwFolder).Passed);
        Assert.False(File.Exists(Path.Combine(throwFolder, BaselineExecution.LockFileName)));
    }
}
