// #225 (GUI-C-190b): why the app reported no runner verdict, said by the cause.
namespace ImageProcTest.E2ETests.Scenarios.Smoke;

/// <summary>What a missing verdict means.</summary>
internal enum RunnerVerdictGap
{
    /// <summary>There is no runner executable (or no checkout to find it in). Normal where the job builds only the app.</summary>
    NoExecutable,

    /// <summary>The runner was started and was still running when the app read its verdict.</summary>
    StillRunning,

    /// <summary>The executable is there but could not be started.</summary>
    DidNotStart,

    /// <summary>None of the above: the status line says something this table does not know.</summary>
    Unexplained,
}

/// <summary>
/// The wording of a skipped or failed runner scenario (A-04, A-06). It used to be ONE sentence for every missing verdict — "this
/// configuration has no runner executable beside the app" — and a runner that had grown from 9 s to 17 s, past the 15 s the app waits
/// for it, was read as exactly that. The text now follows the status line the app left, which says which of the three it was.
/// </summary>
internal static class RunnerVerdictWording
{
    public static RunnerVerdictGap Classify(string status)
    {
        if (status.Contains("runner is not built", StringComparison.Ordinal)
            || status.Contains("needs the repository", StringComparison.Ordinal))
        {
            return RunnerVerdictGap.NoExecutable;
        }

        if (status.StartsWith("Running ", StringComparison.Ordinal))
        {
            return RunnerVerdictGap.StillRunning;
        }

        if (status.Contains(" did not run:", StringComparison.Ordinal))
        {
            return RunnerVerdictGap.DidNotStart;
        }

        return RunnerVerdictGap.Unexplained;
    }

    /// <summary>The reason shown for a scenario that has no verdict to judge. Every text starts with the cause in capitals.</summary>
    public static string NoVerdict(string label, string runnerProject, string status) => Classify(status) switch
    {
        RunnerVerdictGap.NoExecutable =>
            $"NO EXECUTABLE: the app reported no {label} verdict because there is no {runnerProject} executable to run. This configuration " +
            $"does not build it (the gui-automation CI job builds gui/ImageProcTest only), which is normal. The app's status line said: '{status}'.",
        RunnerVerdictGap.StillRunning =>
            $"NOT A MISSING EXECUTABLE - DID NOT FINISH IN TIME: the {label} runner was started and was still running when the app read " +
            $"its verdict, so it took longer than the app waits for it. That is a runner that got slower (or a wait that is too short), not a " +
            $"configuration without the runner. The app's status line said: '{status}'.",
        RunnerVerdictGap.DidNotStart =>
            $"COULD NOT START: the {label} runner's file exists but the app could not start it, so there is no verdict. The app's status " +
            $"line said: '{status}'.",
        _ =>
            $"NO VERDICT, CAUSE UNKNOWN: the app reported no {label} verdict and its status line is not one this check recognises. " +
            $"The app's status line said: '{status}'.",
    };

    /// <summary>The failure text for a runner that ran and did not pass.</summary>
    public static string Failed(string label, string status) =>
        $"The {label} runner RAN and FAILED when launched from the app: '{status}'.";
}
