// #136 (GUI-C-51): the re-acquire branch, actually executed.
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Smoke;

/// <summary>
/// Drives the window re-acquire path that GUI-C-50 added and could not reach.
///
/// <para>That path fires when the launched window element cannot answer <c>AutomationId</c> — roughly
/// one launch in twenty. GUI-C-50 measured the behaviour in a throwaway probe, then ran the suite 74
/// times without once entering the branch. A branch no test reaches is a branch nobody has checked,
/// and a diagnostic nobody reads is not a diagnostic.</para>
///
/// <para>So the first readability check is failed on purpose — the fixture is constructed with the
/// seam armed on that instance — and the rest is real: a real app, a real launch, a real re-location
/// by process id. <b>What is simulated is the trigger, not the repair</b>: if re-location did not
/// work, this would fail. The simulated trigger is not the measured one either, and the note says so
/// — it reports <c>framework=Wpf</c> here, where the real defect reported <c>Win32</c> (GUI-C-50).</para>
///
/// <para>This class builds its own fixture rather than sharing one, because the trigger has to be
/// armed BEFORE the launch. It runs in its own collection so the serialised suite never has two apps
/// alive at once, and it skips the leftover sweep, which would otherwise kill the app another
/// collection is driving (the race measured in GUI-C-36).</para>
/// </summary>
[Collection(WindowReacquireCollection.Name)]
[Trait("Category", "Smoke")]
public sealed class WindowReacquireTests(ITestOutputHelper output)
{
    /// <summary>
    /// A window that cannot answer AutomationId is replaced, and the replacement is reported.
    ///
    /// Two assertions, deliberately: that the window WORKS afterwards, and that the run SAYS it was
    /// replaced. The second is the point of GUI-C-51 — before it, a run that hit the defect and a run
    /// that did not left identical records.
    /// </summary>
    [SkippableFact]
    public void UnreadableWindow_IsReplaced_AndTheRunSaysSo()
    {
        // Armed on THIS fixture only (GUI-C-52). The seams were static and reset in a finally; a
        // killed host would have skipped that reset and armed the next fixture instead, hanging a
        // re-acquire note on an innocent scenario. Instance state makes that impossible rather than
        // unlikely.
        using var fixture = new ApplicationFixture(simulateUnreadableChecks: 1, skipLeftoverSweep: true);
        Skip.If(!fixture.IsAvailable, fixture.SkipReason ?? "The application is not available.");

        output.WriteLine($"reacquire note: '{fixture.ReacquiredNote}'");

        Assert.False(
            string.IsNullOrEmpty(fixture.ReacquiredNote),
            "The fixture replaced the window but left no note, so a run that hit this defect " +
            "would be indistinguishable from a healthy one — the gap GUI-C-51 exists to close.");
        Assert.Contains("re-located", fixture.ReacquiredNote, StringComparison.Ordinal);

        // The replacement is a working window, not merely a different one.
        Assert.Equal("MainWindow", fixture.MainWindow!.AutomationId);
    }

    /// <summary>
    /// The healthy path stays silent: no note when the window answers on the first read.
    ///
    /// Without this, the assertion above would also pass if the note were filled unconditionally, and
    /// every run would carry a warning about a defect it never had.
    /// </summary>
    [SkippableFact]
    public void ReadableWindow_IsKept_AndLeavesNoNote()
    {
        using var fixture = new ApplicationFixture(simulateUnreadableChecks: 0, skipLeftoverSweep: true);
        Skip.If(!fixture.IsAvailable, fixture.SkipReason ?? "The application is not available.");

        // The REAL defect can fire on this launch — GUI-C-55 caught it happening in 2 of 3 Native
        // suite runs, the first time it has ever been seen outside a simulated trigger. When it does,
        // this scenario has nothing to say: it asks what happens on a healthy acquire, and this was
        // not one. Skipped with the note rather than passed, because "not measured" must not be
        // counted as "measured and fine" (GUI-C-37).
        // The token is for the CI gate, the sentence is for the person reading the log; both stay.
        // GUI-C-74 measured this at ~7% per launch, and GUI-C-75 chose to account for it rather
        // than fail on it — prose drifts silently, so the gate keys on the token alone.
        Skip.If(
            !string.IsNullOrEmpty(fixture.ReacquiredNote),
            $"Not measured: the real re-acquire fired on this launch — {fixture.ReacquiredNote} " +
            "XPE-SKIP-ALLOWED:30011");

        Assert.Equal(string.Empty, fixture.ReacquiredNote);
        Assert.Equal("MainWindow", fixture.MainWindow!.AutomationId);
    }
}

/// <summary>Its own collection: these tests launch an extra app and must not overlap another.</summary>
[CollectionDefinition(Name)]
public sealed class WindowReacquireCollection
{
    public const string Name = "WindowReacquire";
}
