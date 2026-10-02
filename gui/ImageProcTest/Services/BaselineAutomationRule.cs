// #225 row 9 (GUI-C-196 M5): what the automation report says about the Deterministic Baseline, and whether that lets the report pass. Free of WPF so the
// integration tests link it.
namespace ImageProcTest.Services;

/// <summary>
/// The leader's ruling (GUI-C-196, decision 4): a baseline that FAILED fails the automation report; a baseline that was NOT RUN does not change it.
/// The reason is the unread red light: a result that says Fail inside a report that says Passed is a red light nobody reads.
///
/// <para>What counts as <i>not run</i> is narrow: the menu was disabled (Mock) or the run did not try (no calibration set, so preprocessing could not run).
/// A baseline that WAS tried and left no result — it threw, its result was dropped, or it did not finish in time — is a <c>Fail</c>, not a quiet
/// <c>NotRun</c>: otherwise a hang or a crash would pass as "nothing happened".</para>
/// </summary>
public static class BaselineAutomationRule
{
    public const string Pass = "Pass";
    public const string Fail = "Fail";
    public const string NotRun = "NotRun";

    /// <param name="attempted">The automation run invoked the command.</param>
    /// <param name="result">The command's complete result, or null when none arrived.</param>
    public static string StatusOf(bool attempted, BaselineExecutionResult? result) =>
        result is not null ? (result.Passed ? Pass : Fail)
        : attempted ? Fail
        : NotRun;

    /// <summary>True only for <c>Pass</c> and <c>NotRun</c>. Anything else, including a value this rule does not know, does not let the report pass.</summary>
    public static bool AllowsAutomationPass(string? status) => status is Pass or NotRun;
}
