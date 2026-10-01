// #225 row 10 (GUI-C-184): AI bone suppression — what the stage makes of the module's answer, where the label comes from,
// and where the init/shutdown calls sit.
using ImageProcTest.Models;
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

[Trait("Category", "Functional")]
public sealed class AiBoneSuppressionStageTests
{
    private static StageRequest AiStage => new(StageIds.AiBoneSuppression, true);

    private static ChainResult RunWith(ushort[] raw, StageExecution answer) =>
        ProcessingChainRunner.Run(raw, [AiStage], (_, _) => answer);

    // ---- The rule this stage exists for: a failed call must not read as "ran, nothing to change" ---------------------------

    /// <summary>
    /// A failed worker call leaves the output equal to the input. If that output were taken as the stage's result, the
    /// chain runner would compare equal pixels and record AppliedNoChange. The same pixels with return code 0 ARE
    /// AppliedNoChange; with a failure code they must be RequestedNotApplied, and the display must stay on the raw frame.
    /// </summary>
    [Theory]
    [InlineData(-3)]
    [InlineData(-4)]
    [InlineData(-9)]
    public void AFailedCall_WhoseOutputEqualsTheInput_IsRequestedNotApplied_NotAppliedNoChange(int failureCode)
    {
        ushort[] raw = [100, 200, 300, 400];
        var echo = AiBoneSuppressionStage.ToFloat(raw); // what a failed call hands back: the input

        var failed = RunWith(raw, AiBoneSuppressionStage.Interpret(failureCode, echo));
        var stage = Assert.Single(failed.Stages);
        Assert.Equal(StageStatus.RequestedNotApplied, stage.Status);
        Assert.Null(stage.Pixels);
        Assert.True(failed.DisplaysRaw);
        Assert.Equal(string.Empty, AiBoneSuppressionStage.LabelFor(failed));

        // The control: the SAME output with return code 0 is the unchanged-image case the runner already knows.
        var succeededUnchanged = RunWith(raw, AiBoneSuppressionStage.Interpret(AiBoneSuppressionStage.Ok, echo));
        Assert.Equal(StageStatus.AppliedNoChange, Assert.Single(succeededUnchanged.Stages).Status);
    }

    [Fact]
    public void ASuccessfulCall_ThatChangedThePixels_IsApplied_AndOnlyThenIsLabelled()
    {
        ushort[] raw = [1000, 2000, 3000, 4000];
        var softTissue = AiBoneSuppressionStage.ToFloat(raw).Select(v => v * 0.5f).ToArray();

        var chain = RunWith(raw, AiBoneSuppressionStage.Interpret(AiBoneSuppressionStage.Ok, softTissue));

        var stage = Assert.Single(chain.Stages);
        Assert.Equal(StageStatus.Applied, stage.Status);
        Assert.Equal<ushort[]>([500, 1000, 1500, 2000], stage.Pixels!);
        Assert.Equal("AI-processed: bone suppression", AiBoneSuppressionStage.LabelFor(chain));
    }

    [Fact]
    public void TheLabel_IsNeverGiven_ToAnyStatusButApplied_OrToAnotherStagesSuccess()
    {
        ushort[] raw = [1, 2, 3, 4];
        ChainResult With(StageStatus status) => new(raw, [new StageOutcome(StageIds.AiBoneSuppression, status, null, "x")]);

        foreach (var status in new[] { StageStatus.NotRequested, StageStatus.AppliedNoChange, StageStatus.RequestedNotApplied })
        {
            Assert.Equal(string.Empty, AiBoneSuppressionStage.LabelFor(With(status)));
        }

        Assert.Equal(AiBoneSuppressionStage.ProcessedLabel, AiBoneSuppressionStage.LabelFor(With(StageStatus.Applied)));

        // Another stage being Applied is not the AI stage being Applied.
        var other = new ChainResult(raw,
        [
            new StageOutcome(StageIds.Preprocess, StageStatus.Applied, [9, 9, 9, 9], "x"),
            new StageOutcome(StageIds.AiBoneSuppression, StageStatus.RequestedNotApplied, null, "x"),
        ]);
        Assert.Equal(string.Empty, AiBoneSuppressionStage.LabelFor(other));
    }

    [Fact]
    public void TheSuccessMessage_DoesNotCarryTheLabel()
    {
        // A stage that returned 0 with an unchanged image is AppliedNoChange and shows its Reason on screen; the label
        // must not be in that text, because the label means "the image was processed".
        var answer = AiBoneSuppressionStage.Interpret(AiBoneSuppressionStage.Ok, [0.5f]);
        Assert.True(answer.Ran);
        Assert.DoesNotContain("AI-processed", answer.Message, StringComparison.Ordinal);
    }

    // ---- Reading the return code -------------------------------------------------------------------------------------------

    [Theory]
    [InlineData(0, "Succeeded")]
    [InlineData(-1, "NotAttempted")]
    [InlineData(-6, "NotAttempted")]
    [InlineData(-7, "NotAttempted")]
    [InlineData(-3, "Failed")]
    [InlineData(-4, "Failed")]
    [InlineData(-9, "Failed")]
    public void TheReturnCode_IsClassified(int code, string expected) =>
        Assert.Equal(expected, AiBoneSuppressionStage.Classify(code).ToString());

    [Fact]
    public void NotAttempted_IsSaidApartFromFailed()
    {
        var refused = AiBoneSuppressionStage.Interpret(AiBoneSuppressionStage.UnsupportedFormat, null);
        Assert.False(refused.Ran);
        Assert.Contains("not attempted (code -7", refused.Message, StringComparison.Ordinal);
        Assert.DoesNotContain("NOT applied", refused.Message, StringComparison.Ordinal);

        var failed = AiBoneSuppressionStage.Interpret(AiBoneSuppressionStage.ProcessingFailed, null);
        Assert.False(failed.Ran);
        Assert.Contains("NOT applied (code -3)", failed.Message, StringComparison.Ordinal);
        Assert.DoesNotContain("not attempted", failed.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void ACodeZero_WithOutputTheChainCannotCarry_IsNotAResult()
    {
        var withNaN = AiBoneSuppressionStage.Interpret(AiBoneSuppressionStage.Ok, [0.1f, float.NaN]);
        Assert.False(withNaN.Ran);
        Assert.Null(withNaN.Pixels);

        var withoutOutput = AiBoneSuppressionStage.Interpret(AiBoneSuppressionStage.Ok, null);
        Assert.False(withoutOutput.Ran);
    }

    [Fact]
    public void AnInitFailure_IsSaidAsNotStarted()
    {
        var answer = AiBoneSuppressionStage.InterpretInit(-9);
        Assert.False(answer.Ran);
        Assert.Contains("not started", answer.Message, StringComparison.Ordinal);
        Assert.Contains("code -9", answer.Message, StringComparison.Ordinal);
    }

    // ---- The assumed [0, 1] scale ---------------------------------------------------------------------------------------------

    [Fact]
    public void TheScaleRoundTrip_IsExactForEvery16BitValue()
    {
        var all = Enumerable.Range(0, 65536).Select(i => (ushort)i).ToArray();
        Assert.True(AiBoneSuppressionStage.TryToUInt16(AiBoneSuppressionStage.ToFloat(all), out var back));
        Assert.Equal(all, back);
    }

    [Fact]
    public void ValuesOutsideTheScale_AreClamped()
    {
        Assert.True(AiBoneSuppressionStage.TryToUInt16([-0.5f, 0f, 1f, 1.5f], out var pixels));
        Assert.Equal<ushort[]>([0, 0, 65535, 65535], pixels);
    }

    // ---- The plan and the settings ------------------------------------------------------------------------------------------

    [Fact]
    public void ThePlan_PutsTheAiStageThirdAndOffByDefault()
    {
        var stages = ProcessingChainPlan.BuildStages(new AppSettings());
        Assert.Equal([StageIds.Preprocess, StageIds.Gsvg, StageIds.AiBoneSuppression], stages.Select(s => s.StageId));
        Assert.False(stages[2].Enabled);

        var on = ProcessingChainPlan.BuildStages(new AppSettings { AiBoneSuppressionInChain = true });
        Assert.True(on[2].Enabled);
    }

    [Fact]
    public void TheAiSettings_SurviveACopy_AndAnEmptyModelDirectoryFallsBack()
    {
        var copy = new AppSettings { AiBoneSuppressionInChain = true, AiModelDirectory = @"D:\models\ai" }.Snapshot();
        Assert.True(copy.AiBoneSuppressionInChain);
        Assert.Equal(@"D:\models\ai", copy.AiModelDirectory);

        Assert.Equal("data/models", new AppSettings { AiModelDirectory = "  " }.AiModelDirectory);
    }

    // ---- Where xpe_ai_init and xpe_ai_shutdown are called ----------------------------------------------------------------

    /// <summary>
    /// xpe_ai_worker_state (next card) must not run together with init or shutdown, so the GUI calls them from one place
    /// under one lock. This reads the sources: each call must appear exactly once outside the DllImport declaration, in
    /// GuiAiRunner.cs, within reach of a <c>WithLock(</c> before it. It cannot see a call made by reflection or from
    /// another assembly; the scope is the GUI's own source tree.
    /// </summary>
    [Theory]
    [InlineData("XpeAiNative.xpe_ai_init(")]
    [InlineData("XpeAiNative.xpe_ai_shutdown(")]
    public void InitAndShutdown_AreCalledInOnePlace_UnderTheLock(string call)
    {
        // ResolveRepositoryFile finds FILES; the project file names the directory.
        var root = Path.GetDirectoryName(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/ImageProcTest.csproj"))!;
        var hits = Directory.EnumerateFiles(root, "*.cs", SearchOption.AllDirectories)
            .Where(path => !path.Contains($"{Path.DirectorySeparatorChar}obj{Path.DirectorySeparatorChar}") &&
                           !path.Contains($"{Path.DirectorySeparatorChar}bin{Path.DirectorySeparatorChar}"))
            .SelectMany(path => FindAll(File.ReadAllText(path), call).Select(index => (path, text: File.ReadAllText(path), index)))
            .ToList();

        var hit = Assert.Single(hits);
        Assert.EndsWith("GuiAiRunner.cs", hit.path, StringComparison.Ordinal);
        var before = hit.text[Math.Max(0, hit.index - 300)..hit.index];
        Assert.Contains("WithLock(", before, StringComparison.Ordinal);
    }

    private static IEnumerable<int> FindAll(string text, string needle)
    {
        for (var at = text.IndexOf(needle, StringComparison.Ordinal); at >= 0; at = text.IndexOf(needle, at + 1, StringComparison.Ordinal))
        {
            yield return at;
        }
    }
}
