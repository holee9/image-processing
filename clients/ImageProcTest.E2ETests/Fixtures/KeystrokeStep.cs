// GUI-C-229e: the order of the real-keystroke step of R02, with its diagnostics kept out of the verdict. The step takes its reads and its key sending as delegates, so the order and the isolation can be shown with no input and no application.
using System.Diagnostics;

namespace ImageProcTest.E2ETests.Fixtures;

/// <summary>
/// The step R02 takes between "the box holds the keyboard focus" and "the box must hold 4321": check the foreground, send the keys, wait, read Center (the judgment), and gather what is needed to explain a failure.
///
/// <para><b>Diagnostics do not take part in the verdict.</b> The record of the judgment read (started, completed) is written and the value read is kept BEFORE anything diagnostic is read. Every diagnostic
/// read (Width, keyboard focus, foreground, the 1.5 s re-read, the description of what is in front) is taken inside its own best-effort guard: if it throws, the message says
/// "diagnostic read failed: type: message" in its place and the verdict and the record are as they were. The reads before the keys are isolated the same way and never stop the keys.</para>
///
/// <para><b>The foreground check is not diagnostic.</b> It is the safety that keeps the keys from going to another window (GUI-C-171). If it says the application is not in front, or cannot say (it throws), NO key is sent
/// and the step fails. It is the one read here that is not isolated.</para>
/// </summary>
internal static class KeystrokeStep
{
    /// <summary>What the step needs from the world. In the scenario these are the real window and the real keys; in a test they are stand-ins.</summary>
    internal sealed record Seams(
        Func<bool> AppInFront,
        Func<string> DescribeFront,
        Func<Observation> ObserveBefore,
        Action SendKeys,
        Func<Stopwatch, (string Center, long StartedAtMs, long CompletedAtMs)> TakeJudgment,
        Func<string, Observation> ObserveAfterJudgment,
        Func<Observation> ObserveLater,
        int AppProcessId);

    /// <summary>The outcome. <see cref="Failure"/> is null when the verdict is a pass (Center held the text); a refusal to send and a lost-keys message are both failures.</summary>
    internal sealed record Result(bool KeysSent, string? Failure, string Center, long StartedAtMs, long CompletedAtMs, Reading Before, Reading At600);

    internal static Result Run(string typed, Seams seams, Action<string> log)
    {
        var before = Reading.Take(seams.ObserveBefore);

        // The safety: not isolated. Not in front, or unable to tell: nothing is sent.
        bool inFront;
        try
        {
            inFront = seams.AppInFront();
        }
        catch (Exception ex)
        {
            return Refused($"The foreground could not be checked ({ex.GetType().Name}: {ex.Message}), so NO key was sent (a key goes to whichever window is in front, GUI-C-171).", before);
        }

        if (!inFront)
        {
            return Refused(KeyLossDiagnosis.RefuseIfNotInFront(false, Safely(seams.DescribeFront)) ?? string.Empty, before);
        }

        seams.SendKeys();
        var sinceLastKey = Stopwatch.StartNew();

        // The judgment value and its record first, before anything diagnostic is read.
        var (center, startedAt, completedAt) = seams.TakeJudgment(sinceLastKey);
        log("R02 " + KeyLossDiagnosis.JudgmentLine(startedAt, completedAt));

        var at600 = Reading.Take(() => seams.ObserveAfterJudgment(center));
        log("R02 observations: " + KeyLossDiagnosis.Facts(before, at600, startedAt, completedAt, null));

        string? failure = null;
        if (center != typed)
        {
            var later = Reading.Take(seams.ObserveLater);
            var cause = KeyLossDiagnosis.ClassifyReadings(typed, center, before, at600, later, seams.AppProcessId, keysSent: true);
            failure =
                $"Real key presses typed '{typed}' but the center box held '{center}' 600 ms after the last key (application pid {seams.AppProcessId}). {KeyLossDiagnosis.JudgmentLine(startedAt, completedAt)}. " +
                $"FACTS (observed): {KeyLossDiagnosis.Facts(before, at600, startedAt, completedAt, later)}. {KeyLossDiagnosis.ForegroundFacts(at600.Value, later.Value, seams.AppProcessId)} {Safely(seams.DescribeFront)} " +
                $"READING (an inference from the facts above, not an observation): {cause} - {KeyLossDiagnosis.Explain(cause)}";
        }

        return new Result(true, failure, center, startedAt, completedAt, before, at600);
    }

    private static Result Refused(string message, Reading before) =>
        new(false, message + " Read before the keys: " + before, string.Empty, 0, 0, before, new Reading(null, "(no key was sent)"));

    private static string Safely(Func<string> read)
    {
        try
        {
            return read();
        }
        catch (Exception ex)
        {
            return $"(diagnostic read failed: {ex.GetType().Name}: {ex.Message})";
        }
    }
}
