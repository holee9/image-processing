// GUI-C-229f: a process id that is unknown never counts as "the application's". No window and no Win32 are used.
using ImageProcTest.E2ETests.Fixtures;
using Xunit;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

public sealed class ProcessIdentityTests
{
    private const long Hwnd = 0x1234;
    private const uint Thread = 77;

    [Theory]
    // the Codex #136 reproduction: a foreground handle, GetWindowThreadProcessId failed (0, 0) and the application's id is unknown (0): 0 == 0 must not count
    [InlineData(Hwnd, 0u, 0u, 0, false)]
    // both process ids 0, thread id fine
    [InlineData(Hwnd, Thread, 0u, 0, false)]
    // Win32 failed (thread id 0) although a process id was written out and matches
    [InlineData(Hwnd, 0u, 100u, 100, false)]
    // no foreground window
    [InlineData(0L, Thread, 100u, 100, false)]
    [InlineData(0L, 0u, 0u, 100, false)]
    // the application's id is unknown, the foreground's is fine
    [InlineData(Hwnd, Thread, 100u, 0, false)]
    // the foreground's id is unknown, the application's is fine
    [InlineData(Hwnd, Thread, 0u, 100, false)]
    // everything known and equal
    [InlineData(Hwnd, Thread, 100u, 100, true)]
    // everything known and different
    [InlineData(Hwnd, Thread, 200u, 100, false)]
    // a negative application id (not a real id) never matches
    [InlineData(Hwnd, Thread, 100u, -1, false)]
    // a negative application id converts to the largest unsigned id: a plain comparison with a foreground id of 0xFFFFFFFF would call that a match
    [InlineData(Hwnd, Thread, 0xFFFFFFFFu, -1, false)]
    public void TheForegroundIsTheApplication_OnlyWhenEveryValueIsKnownAndTheyAreEqual(long handle, uint threadId, uint foregroundPid, int appPid, bool expected) =>
        Assert.Equal(expected, ProcessIdentity.ForegroundIsTheApplication(handle, threadId, foregroundPid, appPid));

    [Theory]
    [InlineData(0, 0, false)]
    [InlineData(0, 100, false)]
    [InlineData(100, 0, false)]
    [InlineData(100, 200, false)]
    [InlineData(-1, -1, false)]
    [InlineData(100, 100, true)]
    public void TwoProcessIds_AreTheSameKnownProcess_OnlyWhenBothAreKnownAndEqual(int processId, int appPid, bool expected) =>
        Assert.Equal(expected, ProcessIdentity.SameKnownProcess(processId, appPid));

    [Fact]
    public void AnUnknownForegroundPid_IsNotTakenForTheApplication_InTheReadingsOfAFailedRun()
    {
        // 0 and 0: before 229f the diagnosis would have read these as "the application held the foreground".
        var facts = KeyLossDiagnosis.ForegroundFacts(new Observation("43", "1000", "VoiWindowCenterInput", 0, 0), null, appProcessId: 0);

        Assert.Contains("the foreground was process 0, not the application", facts);
    }
}
