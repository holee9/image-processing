// GUI-C-229, 229b: what a failed real-keystroke scenario can say about WHERE keys went, from observations taken only before and after the input. Pure code, no input and no application.
using System.Text;

namespace ImageProcTest.E2ETests.Fixtures;

/// <summary>
/// What was read from the window at one moment: the Center and Width boxes, who holds the keyboard focus (as UI Automation names it) and which process holds the system foreground. Reads happen before the
/// keys and after them, never between them: the gap between the keys of the real input is the condition under which the failure was seen, and it must not change.
/// </summary>
internal readonly record struct Observation(string Center, string Width, string FocusAutomationId, int FocusProcessId, int ForegroundProcessId)
{
    public override string ToString() =>
        $"center '{Center}', width '{Width}', keyboard focus '{FocusAutomationId}' (pid {FocusProcessId}), foreground pid {ForegroundProcessId}";
}

/// <summary>
/// "Typed 4321, the box holds 43" (CI run 37157733109, GUI-C-229): which of the explanations the observations fit. A reading aid for the failure message that decides nothing: the scenario's own assertion is
/// strict and fails whatever this says. When the observations fit more than one explanation, or none, the answer is <see cref="Cause.Unclassified"/>.
///
/// <para>What each name rests on (facts are the observations; the name is the inference):</para>
/// <list type="bullet">
/// <item><b>NotSentForegroundWasNotTheApp</b> — checked just BEFORE the keys: the application was not in front, so no key was sent and the scenario failed there.</item>
/// <item><b>ForegroundLeftDuringTyping</b> — a later reading has another process in front while the reading before the keys had the application. Keys sent after that point went to the other window.</item>
/// <item><b>ReadTooEarly</b> — the box read short at 600 ms and held the whole text 1.5 s later.</item>
/// <item><b>KeysWentToAnotherBox</b> — the Width box, which is read to the letter before the keys, ends up as its old text plus exactly the characters that are missing from Center.</item>
/// <item><b>FocusLeftTheBoxStayedInApp</b> — the application stays in front, Center stays short, Width did not take the missing characters, and the keyboard focus is no longer in Center. Where the keys went is not observed.</item>
/// <item><b>KeysNotInBoxFocusStayed</b> — the application stays in front, Center stays short and the focus is still in Center. "The application replaced the value after the keys arrived" and "the application dropped the keys" are not told apart by these observations, so they share this name.</item>
/// </list>
///
/// <para>The Width reading says something only if Width's old text is not itself the missing tail: a Width that already ended in "21" before the keys and still does after them cannot show whether the keys went there.
/// In that case the readings that rest on Width are not claimed.</para>
/// </summary>
internal static class KeyLossDiagnosis
{
    internal enum Cause
    {
        None,
        NotSentForegroundWasNotTheApp,
        ForegroundLeftDuringTyping,
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

    /// <param name="typed">The text sent.</param>
    /// <param name="before">Read just before the keys.</param>
    /// <param name="at600">Read 600 ms after the last key (the reading the scenario's assertion uses).</param>
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
        var matches = new List<Cause>();

        if (before.ForegroundProcessId == appProcessId && (at600.ForegroundProcessId != appProcessId || last.ForegroundProcessId != appProcessId))
        {
            matches.Add(Cause.ForegroundLeftDuringTyping);
        }

        if (later is { } l && l.Center == typed)
        {
            matches.Add(Cause.ReadTooEarly);
        }

        var shortBox = typed.StartsWith(at600.Center, StringComparison.Ordinal) && at600.Center.Length < typed.Length;
        var tail = shortBox ? typed[at600.Center.Length..] : null;
        var widthSays = tail is not null && !before.Width.EndsWith(tail, StringComparison.Ordinal);   // Width's old text is not itself the tail: its change is informative
        var stillShort = later is { } l2 ? l2.Center == at600.Center : true;
        var appInFrontThroughout = at600.ForegroundProcessId == appProcessId && last.ForegroundProcessId == appProcessId;

        if (tail is not null && widthSays && last.Width == before.Width + tail)
        {
            matches.Add(Cause.KeysWentToAnotherBox);
        }

        if (tail is not null && widthSays && stillShort && appInFrontThroughout && last.Width == before.Width)
        {
            matches.Add(last.FocusAutomationId == at600.FocusAutomationId && last.FocusAutomationId == before.FocusAutomationId && last.FocusProcessId == appProcessId
                ? Cause.KeysNotInBoxFocusStayed
                : Cause.FocusLeftTheBoxStayedInApp);
        }

        return matches.Count == 1 ? matches[0] : Cause.Unclassified;
    }

    internal static string Explain(Cause cause) => cause switch
    {
        Cause.NotSentForegroundWasNotTheApp => "the application was not in front just before the keys; nothing was sent.",
        Cause.ForegroundLeftDuringTyping => "another process held the foreground after the keys began: keys sent after that point went to that window.",
        Cause.ReadTooEarly => "the box held the whole text a moment later: the keys arrived and were taken in after the 600 ms read.",
        Cause.KeysWentToAnotherBox => "the missing characters are in the Width box: the keyboard focus moved to it while typing.",
        Cause.FocusLeftTheBoxStayedInApp => "the application stayed in front, Center stayed short, Width did not take the missing characters and the keyboard focus is no longer in Center: where the keys went is not observed.",
        Cause.KeysNotInBoxFocusStayed => "the application stayed in front, Center stayed short and the keyboard focus stayed in Center: the value was replaced after the keys arrived or the keys were dropped; these observations do not tell those two apart.",
        Cause.Unclassified => "the observations fit no single explanation (more than one fits, or none).",
        _ => "the box holds what was typed.",
    };

    /// <summary>The observations, in order, for the failure message and for the log of a passing run.</summary>
    internal static string Facts(Observation before, Observation after, Observation at600, Observation? later) =>
        new StringBuilder()
            .Append("before the keys: ").Append(before)
            .Append("; right after the keys: ").Append(after)
            .Append("; 600 ms after: ").Append(at600)
            .Append("; 1.5 s later: ").Append(later is { } l ? l.ToString() : "(not read)")
            .ToString();
}
