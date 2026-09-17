// #180 / #173 (GUI-C-99): order, fallback and raw protection for the pixel chain, in one place.
using ImageProcTest.Models;

namespace ImageProcTest.Services;

/// <summary>
/// What a backend does to one enabled stage: given the stage's input (a private copy), return the stage's
/// output and a summary, or a reason it did not run. Throwing is treated like a refusal.
/// </summary>
public delegate StageExecution StageExecutor(StageRequest request, ushort[] input);

/// <summary>A stage's own answer before the runner classifies it.</summary>
/// <param name="Ran">False when the stage refused or could not run.</param>
/// <param name="Pixels">The stage's output when it ran.</param>
/// <param name="Message">The stage's summary, or why it did not run.</param>
public sealed record StageExecution(bool Ran, ushort[]? Pixels, string Message);

/// <summary>
/// Runs an ordered stage list over a raw frame (GUI-C-99, contract B of GUI-C-97).
///
/// <para>Rules, all enforced here so Real and Mock cannot differ on them:</para>
/// <list type="bullet">
/// <item>Stages run in list order; each gets the previous produced pixels, or the raw frame.</item>
/// <item>Each stage receives a COPY. The raw array and earlier outputs are never handed out for writing
/// (REQ-GSVG-022 original protection).</item>
/// <item>A stage that refuses, throws, or returns a buffer of the wrong length is
/// <see cref="StageStatus.RequestedNotApplied"/> with a reason, and the chain continues from its input
/// (REQ-GSVG-024 fail-safe pass-through).</item>
/// <item>Output equal to the input is <see cref="StageStatus.AppliedNoChange"/>, not Applied.</item>
/// </list>
/// Free of WPF so the integration tests can link it (GUI-C-99).
/// </summary>
public static class ProcessingChainRunner
{
    public static ChainResult Run(ushort[] raw, IReadOnlyList<StageRequest> stages, StageExecutor execute)
    {
        ArgumentNullException.ThrowIfNull(raw);
        ArgumentNullException.ThrowIfNull(stages);
        ArgumentNullException.ThrowIfNull(execute);

        var outcomes = new List<StageOutcome>(stages.Count);
        var current = raw;

        foreach (var request in stages)
        {
            if (!request.Enabled)
            {
                outcomes.Add(new StageOutcome(request.StageId, StageStatus.NotRequested, null, "switched off"));
                continue;
            }

            var input = (ushort[])current.Clone();
            StageExecution execution;
            try
            {
                execution = execute(request, input);
            }
            catch (Exception ex)
            {
                execution = new StageExecution(false, null, $"{request.StageId} threw: {ex.Message}");
            }

            if (!execution.Ran || execution.Pixels is null)
            {
                outcomes.Add(new StageOutcome(request.StageId, StageStatus.RequestedNotApplied, null, execution.Message));
                continue;
            }

            if (execution.Pixels.Length != current.Length)
            {
                outcomes.Add(new StageOutcome(request.StageId, StageStatus.RequestedNotApplied, null,
                    $"{request.StageId} returned {execution.Pixels.Length} pixels for an input of {current.Length}."));
                continue;
            }

            // A stage may hand back the very copy it was given; keep a buffer nobody else holds.
            var output = ReferenceEquals(execution.Pixels, input) ? input : (ushort[])execution.Pixels.Clone();
            var status = output.AsSpan().SequenceEqual(current) ? StageStatus.AppliedNoChange : StageStatus.Applied;
            outcomes.Add(new StageOutcome(request.StageId, status, output, execution.Message));
            current = output;
        }

        return new ChainResult(raw, outcomes);
    }
}
