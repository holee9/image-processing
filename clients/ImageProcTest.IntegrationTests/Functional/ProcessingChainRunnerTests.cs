// #180 / #173 (GUI-C-99): order, fallback and raw protection of the GUI pixel chain.
using ImageProcTest.Models;
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// <see cref="ProcessingChainRunner"/> is the one place Real and Mock take the chain rules from, so the
/// rules are tested here against scripted stages rather than through a native module.
/// </summary>
[Trait("Category", "Functional")]
public sealed class ProcessingChainRunnerTests
{
    private static ushort[] Raw() => [10, 20, 30, 40];

    private static StageExecution Add(ushort[] input, ushort delta) =>
        new(true, input.Select(v => (ushort)(v + delta)).ToArray(), $"+{delta}");

    /// <summary>Stages run in list order, each on the previous stage's output.</summary>
    [Fact]
    public void Stages_RunInOrder_OnThePreviousOutput()
    {
        var calls = new List<(string Id, ushort First)>();
        var result = ProcessingChainRunner.Run(Raw(), [new("a", true), new("b", true)], (request, input) =>
        {
            calls.Add((request.StageId, input[0]));
            return Add(input, request.StageId == "a" ? (ushort)1 : (ushort)100);
        });

        Assert.Equal([("a", (ushort)10), ("b", (ushort)11)], calls);
        Assert.Equal([StageStatus.Applied, StageStatus.Applied], result.Stages.Select(s => s.Status));
        Assert.Equal(new ushort[] { 111, 121, 131, 141 }, result.DisplayInput);
        Assert.False(result.DisplaysRaw);
    }

    /// <summary>
    /// Control for the order test: swapping the list swaps the calls. A runner that sorted or ignored the
    /// list would give the same call sequence for both.
    /// </summary>
    [Fact]
    public void Control_SwappedList_SwapsTheCalls()
    {
        var calls = new List<string>();
        ProcessingChainRunner.Run(Raw(), [new("b", true), new("a", true)], (request, input) =>
        {
            calls.Add(request.StageId);
            return Add(input, 1);
        });

        Assert.Equal(["b", "a"], calls);
    }

    /// <summary>The display starts from the raw frame when nothing produced pixels, and nothing ran when switched off.</summary>
    [Fact]
    public void SwitchedOffStage_DoesNotRun_AndTheDisplayUsesRaw()
    {
        var raw = Raw();
        var ran = false;
        var result = ProcessingChainRunner.Run(raw, [new(StageIds.Preprocess, false)], (_, input) =>
        {
            ran = true;
            return Add(input, 1);
        });

        Assert.False(ran);
        Assert.Equal(StageStatus.NotRequested, Assert.Single(result.Stages).Status);
        Assert.Same(raw, result.DisplayInput);
        Assert.True(result.DisplaysRaw);
    }

    /// <summary>
    /// A refused, throwing or wrong-length stage is RequestedNotApplied with a reason, and the NEXT stage
    /// still runs on the last good pixels (REQ-GSVG-024 fail-safe pass-through).
    /// </summary>
    [Theory]
    [InlineData("refuse")]
    [InlineData("throw")]
    [InlineData("short")]
    public void FailedStage_FallsBackToItsInput(string failure)
    {
        var raw = Raw();
        ushort secondSaw = 0;
        var result = ProcessingChainRunner.Run(raw, [new("bad", true), new("good", true)], (request, input) =>
        {
            if (request.StageId == "good")
            {
                secondSaw = input[0];
                return Add(input, 5);
            }

            return failure switch
            {
                "refuse" => new StageExecution(false, null, "no calibration"),
                "throw" => throw new InvalidOperationException("native failure"),
                _ => new StageExecution(true, new ushort[] { 1 }, "wrong length"),
            };
        });

        var bad = result.Stages[0];
        Assert.Equal(StageStatus.RequestedNotApplied, bad.Status);
        Assert.Null(bad.Pixels);
        Assert.False(string.IsNullOrWhiteSpace(bad.Reason));
        Assert.Equal((ushort)10, secondSaw);
        Assert.Equal(new ushort[] { 15, 25, 35, 45 }, result.DisplayInput);
    }

    /// <summary>A lone failed stage leaves the display on the raw frame.</summary>
    [Fact]
    public void OnlyStageFails_DisplayUsesRaw()
    {
        var raw = Raw();
        var result = ProcessingChainRunner.Run(raw, [new(StageIds.Preprocess, true)],
            (_, _) => new StageExecution(false, null, "Preprocessing requires the native backend."));

        Assert.Equal(StageStatus.RequestedNotApplied, Assert.Single(result.Stages).Status);
        Assert.Same(raw, result.DisplayInput);
    }

    /// <summary>
    /// The raw array is never written: a stage that scribbles on its input and returns it leaves the raw
    /// frame as loaded (REQ-GSVG-022).
    /// </summary>
    [Fact]
    public void StageWritingItsInput_DoesNotTouchRaw()
    {
        var raw = Raw();
        var result = ProcessingChainRunner.Run(raw, [new("scribble", true)], (_, input) =>
        {
            for (var i = 0; i < input.Length; i++) input[i] = 999;
            return new StageExecution(true, input, "wrote its input");
        });

        Assert.Equal(new ushort[] { 10, 20, 30, 40 }, raw);
        Assert.Equal(StageStatus.Applied, result.Stages[0].Status);
        Assert.All(result.DisplayInput, v => Assert.Equal((ushort)999, v));
    }

    /// <summary>
    /// A stage that keeps a reference to what it returned cannot change the chain's result afterwards: the
    /// runner stores its own copy.
    /// </summary>
    [Fact]
    public void StageOutput_IsCopied()
    {
        ushort[]? kept = null;
        var result = ProcessingChainRunner.Run(Raw(), [new("keep", true)], (_, input) =>
        {
            kept = input.Select(v => (ushort)(v + 1)).ToArray();
            return new StageExecution(true, kept, "kept a reference");
        });

        kept![0] = 7777;
        Assert.Equal((ushort)11, result.DisplayInput[0]);
    }

    /// <summary>Output equal to the input is AppliedNoChange — not reported as a change (C-97 risk (a)).</summary>
    [Fact]
    public void UnchangedOutput_IsAppliedNoChange()
    {
        var result = ProcessingChainRunner.Run(Raw(), [new("noop", true)],
            (_, input) => new StageExecution(true, (ushort[])input.Clone(), "no grid found"));

        Assert.Equal(StageStatus.AppliedNoChange, Assert.Single(result.Stages).Status);
    }

    /// <summary>The summary names every stage and its status, in order.</summary>
    [Fact]
    public void Summary_NamesEachStage()
    {
        var result = ProcessingChainRunner.Run(Raw(), [new("a", false), new(StageIds.Preprocess, true)],
            (_, _) => new StageExecution(false, null, "refused"));

        Assert.Equal("chain: a=NotRequested, preprocess=RequestedNotApplied", result.Summary);
    }

    /// <summary>The plan follows the settings switch.</summary>
    [Theory]
    [InlineData(true)]
    [InlineData(false)]
    public void Plan_FollowsPreprocessInChain(bool on)
    {
        var stages = ProcessingChainPlan.BuildStages(new AppSettings { PreprocessInChain = on });
        var stage = Assert.Single(stages);
        Assert.Equal(StageIds.Preprocess, stage.StageId);
        Assert.Equal(on, stage.Enabled);
    }

    /// <summary>
    /// PixelPitchMm is 0.14 (140 µm, the GUI-C-100 decision) and refuses values outside the range the
    /// preprocess module reports for <c>pixelPitch_mm</c> (0.1–0.5 mm, preprocess.cpp:25).
    /// </summary>
    [Fact]
    public void PixelPitch_DefaultsTo140Micrometres_AndRejectsOutOfRange()
    {
        var settings = new AppSettings();
        Assert.Equal(0.14f, settings.PixelPitchMm);

        settings.PixelPitchMm = 0.139f;
        Assert.Equal(0.139f, settings.PixelPitchMm);

        settings.PixelPitchMm = 0.05f;
        Assert.Equal(0.14f, settings.PixelPitchMm);

        settings.PixelPitchMm = 0.9f;
        Assert.Equal(0.14f, settings.PixelPitchMm);
    }

    /// <summary>ExposureKvp keeps 70 as the default and refuses values that cannot reach native metadata.</summary>
    [Fact]
    public void ExposureKvp_DefaultsTo70_AndRejectsNonPositive()
    {
        var settings = new AppSettings();
        Assert.Equal(70.0f, settings.ExposureKvp);

        settings.ExposureKvp = 81.5f;
        Assert.Equal(81.5f, settings.ExposureKvp);

        settings.ExposureKvp = 0.0f;
        Assert.Equal(70.0f, settings.ExposureKvp);

        settings.ExposureKvp = float.NaN;
        Assert.Equal(70.0f, settings.ExposureKvp);
    }
}
