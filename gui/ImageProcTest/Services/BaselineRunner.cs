// #225 row 9 (GUI-C-196 M1): decides whether the Deterministic Baseline passed. Free of WPF and of native code so the integration tests link it.
using ImageProcTest.Models;

namespace ImageProcTest.Services;

/// <summary>One execution of the baseline: the chain's result and the final 16-bit pixels after the display stage (what the DICOM file will carry).</summary>
/// <param name="Chain">The result of running <see cref="ProcessingChainPlan.BuildBaselineStages"/>.</param>
/// <param name="Output">The pixels after the display pipeline, width x height long.</param>
/// <param name="NaNInfCount">Non-finite values the run saw in any float intermediate (measured by the stage that holds the floats).</param>
public sealed record BaselineSingleRun(ChainResult Chain, ushort[] Output, long NaNInfCount = 0);

public enum BaselineStatus
{
    Pass,
    Fail,
}

/// <summary>The decision, with every number it rests on (the evidence file and the automation report carry these).</summary>
public sealed record BaselineVerdict(
    BaselineStatus Status,
    string FailureReason,
    int RunsExecuted,
    bool InputPreserved,
    bool BitIdentical,
    PixelDifference? Difference,
    long NaNInfCount,
    string InputSha256Before,
    string InputSha256After,
    string? OutputSha256,
    IReadOnlyList<string> StageHashesRun1,
    IReadOnlyList<string> StageHashesRun2);

/// <summary>
/// The Deterministic Baseline's decision logic (design D5, D6; the leader's definition: the same input twice, a bit-identical output).
/// The execution of one run is injected, so this class says what counts as a pass and the callers say how a run is made.
///
/// <list type="bullet">
/// <item>The chain of a run must be EXACTLY the baseline's stage list, in order, every stage applied (changed or unchanged). A refused stage ends the command as
/// a failure and nothing is compared: a pass-through image is not a baseline (D5).</item>
/// <item>The input frame must be the same before and after both runs (sha256), and no float intermediate may have held a non-finite value.</item>
/// <item>The two outputs must be bit-identical. When they are not, where they first differ is reported and the verdict is still Fail.</item>
/// </list>
/// </summary>
public static class BaselineRunner
{
    public static BaselineVerdict Run(ushort[] raw, Func<BaselineSingleRun> runOnce)
    {
        ArgumentNullException.ThrowIfNull(raw);
        ArgumentNullException.ThrowIfNull(runOnce);

        var before = BaselineDeterminism.Sha256Hex(raw);
        var executed = 0;

        BaselineSingleRun first;
        try
        {
            executed++;
            first = runOnce();
        }
        catch (Exception ex)
        {
            return Fail($"run 1 threw: {ex.Message}", executed, raw, before, null, null);
        }

        var firstProblem = CheckChain(first, 1);
        if (firstProblem is not null)
        {
            return Fail(firstProblem, executed, raw, before, first, null);
        }

        BaselineSingleRun second;
        try
        {
            executed++;
            second = runOnce();
        }
        catch (Exception ex)
        {
            return Fail($"run 2 threw: {ex.Message}", executed, raw, before, first, null);
        }

        var secondProblem = CheckChain(second, 2);
        if (secondProblem is not null)
        {
            return Fail(secondProblem, executed, raw, before, first, second);
        }

        var after = BaselineDeterminism.Sha256Hex(raw);
        var preserved = string.Equals(before, after, StringComparison.Ordinal);
        var nanInf = NonFiniteOf(first) + NonFiniteOf(second);
        var difference = BaselineDeterminism.Compare(first.Output, second.Output);
        var outputHash = BaselineDeterminism.Sha256Hex(first.Output);

        string? reason = null;
        if (!preserved)
        {
            reason = "the loaded raw frame changed while the baseline ran (input not preserved)";
        }
        else if (nanInf != 0)
        {
            reason = $"{nanInf} non-finite value(s) in a float intermediate";
        }
        else if (!difference.Identical)
        {
            reason = difference.LengthA != difference.LengthB
                ? $"the two runs produced outputs of different length ({difference.LengthA} and {difference.LengthB} pixels)"
                : $"the two runs differ: first at pixel {difference.FirstIndex}, {difference.DifferentCount} pixel(s) differ, largest difference {difference.MaxAbsDifference}";
        }

        return new BaselineVerdict(
            reason is null ? BaselineStatus.Pass : BaselineStatus.Fail,
            reason ?? string.Empty,
            executed,
            preserved,
            difference.Identical,
            difference,
            nanInf,
            before,
            after,
            outputHash,
            StageHashes(first),
            StageHashes(second));
    }

    /// <summary>Null when the run's chain is the baseline's chain with every stage applied; otherwise why not.</summary>
    private static string? CheckChain(BaselineSingleRun run, int number)
    {
        ArgumentNullException.ThrowIfNull(run);

        var expected = ProcessingChainPlan.BuildBaselineStages();
        var stages = run.Chain.Stages;
        if (stages.Count != expected.Count || !stages.Select(s => s.StageId).SequenceEqual(expected.Select(s => s.StageId), StringComparer.Ordinal))
        {
            return $"run {number}: the chain was [{string.Join(", ", stages.Select(s => s.StageId))}], not the baseline's [{string.Join(", ", expected.Select(s => s.StageId))}]";
        }

        foreach (var stage in stages)
        {
            if (stage.Status is not (StageStatus.Applied or StageStatus.AppliedNoChange))
            {
                return $"run {number}: stage {stage.StageId} was {stage.Status}: {stage.Reason}";
            }
        }

        return null;
    }

    /// <summary>
    /// Every non-finite value a run saw: the ones its stages counted in their float intermediates (carried on the chain result) plus the run's own count for steps
    /// outside the chain. Read from the CHAIN, not from a value the caller must remember to pass, so a stage's count cannot be lost between the stage and the verdict.
    /// </summary>
    private static long NonFiniteOf(BaselineSingleRun? run) =>
        run is null ? 0 : run.NaNInfCount + run.Chain.Stages.Sum(s => s.NonFiniteCount);

    private static List<string> StageHashes(BaselineSingleRun run) =>
        run.Chain.Stages.Select(s => s.Pixels is null ? "-" : BaselineDeterminism.Sha256Hex(s.Pixels)).ToList();

    private static BaselineVerdict Fail(string reason, int executed, ushort[] raw, string before, BaselineSingleRun? first, BaselineSingleRun? second) =>
        new(
            BaselineStatus.Fail,
            reason,
            executed,
            string.Equals(before, BaselineDeterminism.Sha256Hex(raw), StringComparison.Ordinal),
            false,
            null,
            NonFiniteOf(first) + NonFiniteOf(second),
            before,
            BaselineDeterminism.Sha256Hex(raw),
            null,
            first is null ? [] : StageHashes(first),
            second is null ? [] : StageHashes(second));
}
