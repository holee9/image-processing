// GUI-C-234c (Codex #181 finding 2): the float image behind the display input belongs to the chain run that made the pixels.
using ImageProcTest.Models;
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// The display stage windows the enhancement float image instead of its 16-bit rounding. Which float image is the question: the first version kept the LAST one in a field of the backend and accepted it
/// when the display input equalled its rounding - but rounding is many-to-one, so another run floats pass that test. The floats now ride on the stage outcome of the run itself
/// (<see cref="StageOutcome.Floats"/>, <see cref="ChainResult.DisplayFloats"/>); these tests pin that, with two runs whose rounded pixels are identical and whose floats are not.
/// </summary>
[Trait("Category", "Functional")]
public sealed class ChainFloatOwnershipTests
{
    private static readonly ushort[] Raw = [100, 201, 300];

    private static StageExecution Enhance(float[] floats) =>
        new(true, floats.Select(f => (ushort)Math.Round(f, MidpointRounding.ToEven)).ToArray(), "enhance", Floats: floats);

    /// <summary>Two runs overlap (the second starts and ends inside the first); each result keeps its own floats although the rounded pixels are the same.</summary>
    [Fact]
    public void OverlappingRuns_WithTheSameRoundedPixels_KeepTheirOwnFloats()
    {
        float[] a = [100.1f, 200.6f, 300.5f];
        float[] b = [100.4f, 200.9f, 300.4f];
        ChainResult? inner = null;

        var outer = ProcessingChainRunner.Run(Raw, [new(StageIds.EnhanceBasic, true)], (_, _) =>
        {
            inner = ProcessingChainRunner.Run(Raw, [new(StageIds.EnhanceBasic, true)], (_, _) => Enhance(b));
            return Enhance(a);
        });

        Assert.NotNull(inner);
        Assert.Equal(outer.DisplayInput, inner!.DisplayInput);          // control: the 16-bit pixels cannot tell the runs apart
        Assert.Same(a, outer.DisplayFloats);
        Assert.Same(b, inner.DisplayFloats);
    }

    /// <summary>The floats are those of the stage that produced the display input: a later stage without floats (or a refused enhancement) leaves none.</summary>
    [Fact]
    public void ALaterStageWithoutFloats_ReplacesThePixels_SoNoFloatsRemain()
    {
        float[] floats = [100.1f, 200.6f, 300.5f];
        var result = ProcessingChainRunner.Run(Raw, [new(StageIds.EnhanceBasic, true), new(StageIds.Gsvg, true)], (request, input) =>
            request.StageId == StageIds.EnhanceBasic
                ? Enhance(floats)
                : new StageExecution(true, input.Select(v => (ushort)(v + 1)).ToArray(), "gsvg"));

        Assert.Null(result.DisplayFloats);                                // the display input is gsvg output; the enhancement floats describe an earlier image

        var refused = ProcessingChainRunner.Run(Raw, [new(StageIds.EnhanceBasic, true)], (_, _) => new StageExecution(false, null, "refused", Floats: floats));
        Assert.Null(refused.DisplayFloats);                               // a stage that did not apply has no output, so no floats either
        Assert.True(refused.DisplaysRaw);
    }

    /// <summary>
    /// GUI-C-234f: a later stage that changed nothing (output equal to input, <see cref="StageStatus.AppliedNoChange"/>) leaves the enhancement float image standing; one that changed a pixel does not.
    /// </summary>
    [Fact]
    public void ALaterStageThatChangedNothing_KeepsTheFloatsOfTheImageItLeftAlone()
    {
        float[] floats = [100.1f, 200.6f, 300.5f];
        var unchanged = ProcessingChainRunner.Run(Raw, [new(StageIds.EnhanceBasic, true), new(StageIds.Gsvg, true)], (request, input) =>
            request.StageId == StageIds.EnhanceBasic ? Enhance(floats) : new StageExecution(true, (ushort[])input.Clone(), "gsvg: no change"));

        Assert.Equal(StageStatus.AppliedNoChange, unchanged.Stages[1].Status);
        Assert.Same(floats, unchanged.DisplayFloats);

        var changed = ProcessingChainRunner.Run(Raw, [new(StageIds.EnhanceBasic, true), new(StageIds.Gsvg, true)], (request, input) =>
            request.StageId == StageIds.EnhanceBasic ? Enhance(floats) : new StageExecution(true, input.Select(v => (ushort)(v + 1)).ToArray(), "gsvg"));
        Assert.Equal(StageStatus.Applied, changed.Stages[1].Status);
        Assert.Null(changed.DisplayFloats);
    }

    /// <summary>A float image whose length is not the pixels is not theirs.</summary>
    [Fact]
    public void FloatsOfTheWrongLength_AreDropped()
    {
        var result = ProcessingChainRunner.Run(Raw, [new(StageIds.EnhanceBasic, true)], (_, input) =>
            new StageExecution(true, input.Select(v => (ushort)(v + 1)).ToArray(), "enhance", Floats: [1f, 2f]));

        Assert.Null(result.DisplayFloats);
    }
}
