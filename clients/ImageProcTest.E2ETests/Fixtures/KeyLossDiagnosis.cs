// GUI-C-229: what a failed real-keystroke scenario must be able to say about WHERE a key went. Pure code, no input and no application.
using System.Text;

namespace ImageProcTest.E2ETests.Fixtures;

/// <summary>One key of a typed text: when it was sent, and whether the application was in front just before it. A key that was not sent is one the harness refused to send because the application was not in front.</summary>
internal readonly record struct KeyStep(int Index, char Key, long AtMs, bool AppInFrontBefore, bool Sent);

/// <summary>
/// The three explanations for "typed 4321, the box holds 43" that the first red run (CI run 37157733109, GUI-C-229) left open, told apart from what a failed scenario can observe afterwards:
///
/// <list type="bullet">
/// <item><b>ForegroundLeft</b> — the application was not in front before some key. The harness stops there and sends nothing to whatever was in front, so the keys are not typed into another window.</item>
/// <item><b>ReadTooEarly</b> — the box shows the whole text a moment later: the keys arrived, the read came before the application had taken them in.</item>
/// <item><b>KeysWentToAnotherBox</b> — the missing characters are in another text box of the application: the keyboard focus moved inside the application.</item>
/// <item><b>FocusLeftTheBox</b> / <b>BoxOverwritten</b> — the box never showed the rest and does not change afterwards; whether the focus was still in the box separates "the keys went elsewhere" from "the application replaced the value".</item>
/// </list>
///
/// The class is a reading aid for the failure message. It decides nothing: the scenario's assertion is the strict one and fails whatever this says.
/// </summary>
internal static class KeyLossDiagnosis
{
    internal enum Cause
    {
        None,
        ForegroundLeft,
        ReadTooEarly,
        KeysWentToAnotherBox,
        FocusLeftTheBox,
        BoxOverwritten,
        Unclassified,
    }

    internal static Cause Classify(IReadOnlyList<KeyStep> steps, string typed, string seen, string seenLater, string? otherBoxText, bool focusInTheBoxAfterwards)
    {
        if (seen == typed)
        {
            return Cause.None;
        }

        if (steps.Any(s => !s.Sent))
        {
            return Cause.ForegroundLeft;
        }

        if (seenLater == typed)
        {
            return Cause.ReadTooEarly;
        }

        if (typed.StartsWith(seen, StringComparison.Ordinal) && seen.Length < typed.Length
            && otherBoxText is not null && otherBoxText.EndsWith(typed[seen.Length..], StringComparison.Ordinal))
        {
            return Cause.KeysWentToAnotherBox;
        }

        if (seenLater == seen)
        {
            return focusInTheBoxAfterwards ? Cause.BoxOverwritten : Cause.FocusLeftTheBox;
        }

        return Cause.Unclassified;
    }

    internal static string Explain(Cause cause) => cause switch
    {
        Cause.ForegroundLeft => "(a) the application was not in front before a key: the focus left it while typing. The harness stopped sending, so no key went to another window.",
        Cause.ReadTooEarly => "(c) the box held the whole text a moment later: the keys arrived and were taken in after the read. The scenario read too early for this machine's load.",
        Cause.KeysWentToAnotherBox => "(a) the missing characters are in another text box of the application: the keyboard focus moved inside the application while typing.",
        Cause.FocusLeftTheBox => "(a) the box did not change afterwards and no longer holds the keyboard focus: the focus moved away while typing.",
        Cause.BoxOverwritten => "(b) the box did not change afterwards and still holds the keyboard focus: the keys were sent while it had the focus and the value was replaced or the keys were dropped inside the application.",
        Cause.Unclassified => "none of the explanations fits the readings (see the key trace).",
        _ => "the box holds what was typed.",
    };

    /// <summary>The per-key record, for the failure message and for the log of a passing run.</summary>
    internal static string Trace(IReadOnlyList<KeyStep> steps)
    {
        var sb = new StringBuilder();
        foreach (var s in steps)
        {
            sb.Append($"key {s.Index} '{s.Key}' at +{s.AtMs} ms, app in front before it: {(s.AppInFrontBefore ? "yes" : "NO")}, {(s.Sent ? "sent" : "NOT sent")}");
            sb.Append("; ");
        }

        return sb.ToString().TrimEnd(' ', ';');
    }
}
