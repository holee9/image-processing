// GUI-C-229, 229b, 229c, 229d, 229e: what a failed real-keystroke scenario can say about WHERE keys went, from observations taken before the input and AFTER the judgment read. Pure code, no input and no application.
using System.Text;

namespace ImageProcTest.E2ETests.Fixtures;

/// <summary>
/// What was read from the window at one moment: the Center and Width boxes, who holds the keyboard focus (as UI Automation names it) and which process holds the system foreground. The "before the keys"
/// reading is taken before the input. The "600 ms" reading is the judgment read of Center followed at once by the rest. Nothing is read between the keys or between the last key and the judgment read: the gap
/// between the keys of the real input and the 600 ms to the judgment are the conditions under which the failure was seen, and they must not change.
/// </summary>
internal readonly record struct Observation(string Center, string Width, string FocusAutomationId, int FocusProcessId, int ForegroundProcessId)
{
    public override string ToString() =>
        $"center '{Center}', width '{Width}', keyboard focus '{FocusAutomationId}' (pid {FocusProcessId}), foreground pid {ForegroundProcessId}";
}

/// <summary>
/// An <see cref="Observation"/> that may not have been possible to take. A diagnostic read that throws is kept as a note (<c>diagnostic read failed: type: message</c>) instead of an exception, so a failed
/// lookup of Width or of the focus can never change a verdict or erase a record (GUI-C-229e).
/// </summary>
internal readonly record struct Reading(Observation? Value, string? Failure)
{
    public static implicit operator Reading(Observation value) => new(value, null);

    /// <summary>Takes the reading inside a best-effort guard: any exception becomes the note.</summary>
    public static Reading Take(Func<Observation> read)
    {
        try
        {
            return new Reading(read(), null);
        }
        catch (Exception ex)
        {
            return new Reading(null, $"diagnostic read failed: {ex.GetType().Name}: {ex.Message}");
        }
    }

    public override string ToString() => Value is { } v ? v.ToString() : Failure ?? "(not read)";
}

/// <summary>
/// "Typed 4321, the box holds 43" (CI run 37157733109, GUI-C-229): which of the explanations the observations fit. A reading aid for the failure message that decides nothing: the scenario's own assertion is
/// strict and fails whatever this says. When the observations fit more than one explanation, or none, the answer is <see cref="Cause.Unclassified"/>.
///
/// <para>What each name rests on (facts are the observations; the name is the inference):</para>
/// <list type="bullet">
/// <item><b>NotSentForegroundWasNotTheApp</b> — checked just BEFORE the keys: the application was not in front, so no key was sent and the scenario failed there.</item>
/// <item><b>ReadTooEarly</b> — the box read short at 600 ms and held the whole text 1.5 s later.</item>
/// <item><b>KeysWentToAnotherBox</b> — the Width box, which is read to the letter before the keys, ends up as its old text plus exactly the characters that are missing from Center.</item>
/// <item><b>FocusLeftTheBoxStayedInApp</b> — the application stays in front, Center stays short, Width did not take the missing characters, and the keyboard focus is no longer in Center. Where the keys went is not observed.</item>
/// <item><b>KeysNotInBoxFocusStayed</b> — the application stays in front, Center stays short and the focus is still in Center. "The application replaced the value after the keys arrived" and "the application dropped the keys" are not told apart by these observations, so they share this name.</item>
/// </list>
///
/// <para><b>The foreground is a fact, not a destination.</b> If another process holds the foreground at the 600 ms reading or at the re-read, the message says so as a fact. It does not say that keys went there:
/// the change may have come after the last key, and these readings cannot place it. In that case the answer is <see cref="Cause.Unclassified"/> whatever else fits.</para>
///
/// <para>The Width reading says something only if Width's old text is not itself the missing tail: a Width that already ended in "21" before the keys and still does after them cannot show whether the keys went there.
/// In that case the readings that rest on Width are not claimed.</para>
/// </summary>
internal static class KeyLossDiagnosis
{
    /// <summary>The wait between the last key and the judgment read, as the scenario has always had it.</summary>
    internal const int JudgmentWaitMs = 600;

    internal enum Cause
    {
        None,
        NotSentForegroundWasNotTheApp,
        ReadTooEarly,
        KeysWentToAnotherBox,
        FocusLeftTheBoxStayedInApp,
        KeysNotInBoxFocusStayed,
        Unclassified,
    }

    /// <summary>The refusal message, or null when the application is in front and the keys may be sent. Pure: the scenario calls it just before the first key and fails with the message instead of sending.</summary>
    internal static string? RefuseIfNotInFront(bool appInFront, string whatIsInFront) =>
        appInFront
            ? null
            : "The application was not the window in front just before the keys, so NO key was sent (a key goes to whichever window is in front, GUI-C-171). " + whatIsInFront;

    /// <summary>
    /// The record of the judgment read, in milliseconds after the last key: when the read began and when it completed. Written on every run, passing or failing, and nothing is decided from it: the
    /// verdict is the original one (600 ms, read Center, is it "4321"). A read that began or completed late could hide a loss (a late value is not the value at 600 ms); that limit is the original
    /// scenario's too, and a run that completed late can be told apart afterwards from this line.
    /// </summary>
    internal static string JudgmentLine(long startedAtMs, long completedAtMs) =>
        $"judgment read: started +{startedAtMs} ms, completed +{completedAtMs} ms after the last key (nominal wait {JudgmentWaitMs} ms; the verdict does not depend on these times)";

    /// <param name="typed">The text sent.</param>
    /// <param name="before">Read just before the keys.</param>
    /// <param name="at600">The judgment read of Center, 600 ms after the last key, followed by Width, focus and foreground.</param>
    /// <param name="later">Read 1.5 s after that, or null if it was not taken.</param>
    /// <param name="appProcessId">The application's process id.</param>
    /// <param name="keysSent">False only when the scenario refused to send because the application was not in front.</param>
    internal static Cause Classify(string typed, Observation before, Observation at600, Observation? later, int appProcessId, bool keysSent)
    {
        if (!keysSent)
        {
            return Cause.NotSentForegroundWasNotTheApp;
        }

        if (at600.Center == typed)
        {
            return Cause.None;
        }

        var last = later ?? at600;
        if (ForegroundWasAnotherProcess(at600, later, appProcessId))
        {
            return Cause.Unclassified;
        }

        var matches = new List<Cause>();

        if (later is { } l && l.Center == typed)
        {
            matches.Add(Cause.ReadTooEarly);
        }

        var shortBox = typed.StartsWith(at600.Center, StringComparison.Ordinal) && at600.Center.Length < typed.Length;
        var tail = shortBox ? typed[at600.Center.Length..] : null;
        var widthSays = tail is not null && !before.Width.EndsWith(tail, StringComparison.Ordinal);   // Width's old text is not itself the tail: its change is informative
        var stillShort = later is { } l2 ? l2.Center == at600.Center : true;

        if (tail is not null && widthSays && last.Width == before.Width + tail)
        {
            matches.Add(Cause.KeysWentToAnotherBox);
        }

        if (tail is not null && widthSays && stillShort && last.Width == before.Width)
        {
            matches.Add(last.FocusAutomationId == at600.FocusAutomationId && last.FocusAutomationId == before.FocusAutomationId && last.FocusProcessId == appProcessId
                ? Cause.KeysNotInBoxFocusStayed
                : Cause.FocusLeftTheBoxStayedInApp);
        }

        return matches.Count == 1 ? matches[0] : Cause.Unclassified;
    }

    private static bool ForegroundWasAnotherProcess(Observation at600, Observation? later, int appProcessId) =>
        at600.ForegroundProcessId != appProcessId || (later is { } l && l.ForegroundProcessId != appProcessId);

    internal static string Explain(Cause cause) => cause switch
    {
        Cause.NotSentForegroundWasNotTheApp => "the application was not in front just before the keys; nothing was sent.",
        Cause.ReadTooEarly => "the box held the whole text a moment later: the keys arrived and were taken in after the 600 ms read.",
        Cause.KeysWentToAnotherBox => "the missing characters are in the Width box: the keyboard focus moved to it while typing.",
        Cause.FocusLeftTheBoxStayedInApp => "the application stayed in front, Center stayed short, Width did not take the missing characters and the keyboard focus is no longer in Center: where the keys went is not observed.",
        Cause.KeysNotInBoxFocusStayed => "the application stayed in front, Center stayed short and the keyboard focus stayed in Center: the value was replaced after the keys arrived or the keys were dropped; these observations do not tell those two apart.",
        Cause.Unclassified => "the observations fit no single explanation (more than one fits, none fits, or the foreground was another process when read, which says nothing about where the keys went).",
        _ => "the box holds what was typed.",
    };

    /// <summary>The foreground facts as sentences, or an empty string when the application held the foreground at every reading after the keys. No destination is claimed.</summary>
    internal static string ForegroundFacts(Observation? at600, Observation? later, int appProcessId)
    {
        var sb = new StringBuilder();
        if (at600 is { } a && a.ForegroundProcessId != appProcessId)
        {
            sb.Append($"At the 600 ms reading the foreground was process {a.ForegroundProcessId}, not the application (pid {appProcessId}). ");
        }

        if (later is { } l && l.ForegroundProcessId != appProcessId)
        {
            sb.Append($"At the 1.5 s re-read the foreground was process {l.ForegroundProcessId}, not the application (pid {appProcessId}). ");
        }

        if (sb.Length > 0)
        {
            sb.Append("When the foreground changed is not observed, so where the keys went is not either.");
        }

        return sb.ToString();
    }

    /// <summary>The observations, in order, for the failure message and for the log of a passing run.</summary>
    internal static string Facts(Reading before, Reading at600, long readStartedAtMs, long readCompletedAtMs, Reading? later) =>
        new StringBuilder()
            .Append("before the keys: ").Append(before)
            .Append($"; 600 ms after (judgment read +{readStartedAtMs}..+{readCompletedAtMs} ms): ").Append(at600)
            .Append("; 1.5 s later: ").Append(later is { } l ? l.ToString() : "(not read)")
            .ToString();

    /// <summary>
    /// <see cref="Classify"/> for readings that may be missing: when a diagnostic read failed the answer is <see cref="Cause.Unclassified"/> (the judgment value <paramref name="center"/> is not a diagnostic and
    /// is always there). A missing re-read is passed on as "not taken".
    /// </summary>
    internal static Cause ClassifyReadings(string typed, string center, Reading before, Reading at600, Reading? later, int appProcessId, bool keysSent)
    {
        if (!keysSent)
        {
            return Cause.NotSentForegroundWasNotTheApp;
        }

        if (center == typed)
        {
            return Cause.None;
        }

        if (before.Value is not { } b || at600.Value is not { } a)
        {
            return Cause.Unclassified;
        }

        return Classify(typed, b, a, later?.Value, appProcessId, keysSent: true);
    }
}
