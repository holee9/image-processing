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
/// <para>So the first readability check is failed on purpose
/// (<see cref="ApplicationFixture.SimulateUnreadableChecks"/>) and the rest is real: a real app, a
/// real launch, a real re-location by process id. What is simulated is the trigger, not the repair —
/// if re-location did not work, this would fail.</para>
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
        ApplicationFixture.SimulateUnreadableChecks = 1;
        ApplicationFixture.SkipLeftoverSweep = true;

        try
        {
            using var fixture = new ApplicationFixture();
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
        finally
        {
            // Reset even on failure: a leaked counter would fail the NEXT fixture in the run, and the
            // failure would point at an innocent scenario.
            ApplicationFixture.SimulateUnreadableChecks = 0;
            ApplicationFixture.SkipLeftoverSweep = false;
        }
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
        ApplicationFixture.SkipLeftoverSweep = true;

        try
        {
            using var fixture = new ApplicationFixture();
            Skip.If(!fixture.IsAvailable, fixture.SkipReason ?? "The application is not available.");

            Assert.Equal(string.Empty, fixture.ReacquiredNote);
            Assert.Equal("MainWindow", fixture.MainWindow!.AutomationId);
        }
        finally
        {
            ApplicationFixture.SkipLeftoverSweep = false;
        }
    }
}

/// <summary>Its own collection: these tests launch an extra app and must not overlap another.</summary>
[CollectionDefinition(Name)]
public sealed class WindowReacquireCollection
{
    public const string Name = "WindowReacquire";
}
