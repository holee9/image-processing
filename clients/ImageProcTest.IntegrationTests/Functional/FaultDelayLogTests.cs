// QA-A-279c (Reviewer #267/#268): the display-delay log's recorder, reader and classifier, with no GUI and no app.
#if XPE_TEST_FAULTS
using System.Diagnostics;
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// What this class proves and what it does not.
///
/// <para><b>Proves</b>, with real clocks and real sleeps: the recorder writes what happened (call number, overload, order, requested value, start and end on the monotonic clock);
/// the reader refuses a file that is not whole (no file, no header, another run, another setting, no footer, loss, pending sleep, duplicates, a second wrapper); an empty record that the settings
/// do not predict is called an anomaly and not a verdict on the injection; one export wins when two arrive together; the window attribution counts what it says.</para>
///
/// <para><b>Does not prove</b> the wiring of <c>FaultInjectingBackend.DelayIfArmed</c> to the recorder: that type needs WPF types and cannot be linked into this net8.0 project (leader decision,
/// 2026-10-22). The helper <see cref="Sleep"/> stands in for it - it does what the wrapper does, in the same order. The wiring stays LOGGING-UNVERIFIED (wiring) until the W15b run in CI prints a log.</para>
///
/// <para><b>Tolerance, fixed BEFORE any run and not to be adjusted to a result:</b> <see cref="SlackMs"/> = 20 ms - one default Windows timer period (15.6 ms) rounded up. A recorded sleep must be at
/// least (the sleep the test asked of the OS - 20 ms), must lie inside the interval the test measured around the call with its own clock, and the test's own measurement must satisfy the same lower
/// bound (otherwise the machine, not the recorder, is what the test would be reporting). There is no upper bound: scheduling can only lengthen a sleep. This slack is unrelated to the W15b limit of 1200 ms.</para>
/// </summary>
[Trait("Category", "Functional")]
public sealed class FaultDelayLogTests : IDisposable
{
    private const int SlackMs = 20;
    private readonly string _directory = Path.Combine(Path.GetTempPath(), "xpe_fdl_" + Guid.NewGuid().ToString("N"));

    public void Dispose()
    {
        try { Directory.Delete(_directory, true); } catch (Exception) { /* temp folder */ }
    }

    private string Base(string name = "log") => Path.Combine(_directory, name);

    private FaultDelayRecorder Create(int armedMs, Func<int> calls, string name = "log", int capacity = FaultDelayLog.DefaultCapacity, string run = "run1", string app = "app1")
    {
        var path = Base(name);
        var recorder = FaultDelayRecorder.TryCreate(armedMs, calls, key => key switch
        {
            FaultDelayLog.EnvPath => path,
            FaultDelayLog.EnvRun => run,
            FaultDelayLog.EnvApp => app,
            _ => null,
        }, capacity);
        Assert.NotNull(recorder);
        return recorder!;
    }

    private FaultDelayReading Read(int armedMs, string name = "log", string run = "run1", string app = "app1", int? pid = null) =>
        FaultDelayLogReader.Read(Base(name), run, app, armedMs, pid ?? Environment.ProcessId);

    /// <summary>The stand-in for <c>DelayIfArmed</c>: begin, really sleep <paramref name="actualMs"/>, end, in the wrapper's order. Returns the interval the test measured around it with its own clock.</summary>
    private static (long Start, long End) Sleep(FaultDelayRecorder recorder, int call, string overload, int requestedMs, int actualMs)
    {
        var start = Stopwatch.GetTimestamp();
        var slot = recorder.Begin(call, overload, requestedMs);
        try
        {
            Thread.Sleep(actualMs);
        }
        finally
        {
            recorder.End(slot);
        }

        return (start, Stopwatch.GetTimestamp());
    }

    private static double Ms(long ticks) => ticks * 1000.0 / Stopwatch.Frequency;

    [Fact]
    public void T1_TwoOverloads_AreRecordedWithTheirCallNumberOrderAndRequest()
    {
        var calls = 0;
        var recorder = Create(60, () => calls);
        calls = 1; Sleep(recorder, 1, FaultDelayLog.ChainOverload, 60, 60);
        calls = 2; Sleep(recorder, 2, FaultDelayLog.DisplayInputOverload, 60, 60);
        Assert.True(recorder.Export("test"));

        var reading = Read(60);
        Assert.Equal(FaultDelayStatus.Complete, reading.Status);
        Assert.Equal(2, reading.Events.Count);
        Assert.Equal(new[] { 1, 2 }, reading.Events.Select(e => e.Call));
        Assert.Equal(new[] { FaultDelayLog.ChainOverload, FaultDelayLog.DisplayInputOverload }, reading.Events.Select(e => e.Overload));
        Assert.Equal(new[] { 0, 1 }, reading.Events.Select(e => e.Order));
        Assert.All(reading.Events, e => Assert.Equal(60, e.RequestedMs));
        Assert.Equal(2, reading.Footer!.Calls);
    }

    [Fact]
    public void T2_RecordedIntervals_FollowTheRealSleep_AgainstTheTestsOwnClock()
    {
        var recorder = Create(60, () => 2);
        var first = Sleep(recorder, 1, FaultDelayLog.ChainOverload, 60, 60);
        var second = Sleep(recorder, 2, FaultDelayLog.DisplayInputOverload, 60, 100);   // a different real sleep, so a reused or zeroed value cannot pass
        Assert.True(recorder.Export("test"));

        var reading = Read(60);
        Assert.Equal(FaultDelayStatus.Complete, reading.Status);
        var frequency = reading.Header!.Frequency;
        Assert.Equal(Stopwatch.Frequency, frequency);
        foreach (var (e, outer, actual) in new[] { (reading.Events[0], first, 60), (reading.Events[1], second, 100) })
        {
            Assert.True(e.StartTicks >= outer.Start && e.EndTicks <= outer.End, "the recorded interval must lie inside the interval the test measured around the call");
            Assert.True(Ms(outer.End - outer.Start) >= actual - SlackMs, $"premise: the test's own measurement of the sleep ({Ms(outer.End - outer.Start):0.0} ms) must reach {actual - SlackMs} ms");
            Assert.True(FaultDelayAttribution.ElapsedMs(e, frequency) >= actual - SlackMs, $"call {e.Call}: recorded {FaultDelayAttribution.ElapsedMs(e, frequency):0.0} ms for a real sleep of {actual} ms");
        }
    }

    [Fact]
    public void T3_ArmedZero_IsACompleteEmptyRecord_NotAMissingOne()
    {
        var recorder = Create(0, () => 2);
        Assert.True(recorder.Export("test"));
        var reading = Read(0);
        Assert.Equal(FaultDelayStatus.CompleteEmpty, reading.Status);
        Assert.Empty(reading.Events);
        Assert.Equal(2, reading.Footer!.Calls);
    }

    [Fact]
    public void T4_WithoutThePathVariable_NoRecorderAndNoFileExist()
    {
        Assert.Null(FaultDelayRecorder.TryCreate(60, () => 0, _ => null));
        Assert.Null(FaultDelayRecorder.TryCreate(60, () => 0, _ => "   "));
        Assert.False(Directory.Exists(_directory));
        Assert.Equal(FaultDelayStatus.Incomplete, Read(60).Status);
        Assert.StartsWith("no-file", Read(60).Reason, StringComparison.Ordinal);
    }

    [Fact]
    public void T5_ANotExportedFile_IsIncomplete_AndATruncatedOneIsNeverComplete()
    {
        var recorder = Create(60, () => 1);
        Sleep(recorder, 1, FaultDelayLog.ChainOverload, 60, 60);
        var notExported = Read(60);
        Assert.Equal(FaultDelayStatus.Incomplete, notExported.Status);
        Assert.StartsWith("no-footer", notExported.Reason, StringComparison.Ordinal);

        Assert.True(recorder.Export("test"));
        var whole = File.ReadAllLines(recorder.FilePath);
        Assert.Equal(FaultDelayStatus.Complete, FaultDelayLogReader.Classify(whole, null, "run1", "app1", 60, Environment.ProcessId).Status);
        Assert.Equal("END", whole[^1]);
        Assert.Equal(FaultDelayStatus.Incomplete, FaultDelayLogReader.Classify(whole[..^1], null, "run1", "app1", 60, Environment.ProcessId).Status);             // the END line lost
        Assert.Equal(FaultDelayStatus.Incomplete, FaultDelayLogReader.Classify([.. whole[..^2], whole[^2][..10]], null, "run1", "app1", 60, Environment.ProcessId).Status);   // a footer cut in the middle
    }

    [Fact]
    public void T6_AFileOfAnotherRunAppProcessOrSetting_IsRefused()
    {
        var recorder = Create(60, () => 0, run: "runA", app: "appA");
        Assert.True(recorder.Export("test"));
        Assert.StartsWith("stale", Read(60, run: "runB", app: "appA").Reason, StringComparison.Ordinal);
        Assert.StartsWith("stale", Read(60, run: "runA", app: "appB").Reason, StringComparison.Ordinal);
        Assert.StartsWith("stale", Read(60, run: "runA", app: "appA", pid: Environment.ProcessId + 1).Reason, StringComparison.Ordinal);
        Assert.StartsWith("config-mismatch", Read(1500, run: "runA", app: "appA").Reason, StringComparison.Ordinal);
        Assert.Equal(FaultDelayStatus.CompleteEmpty, Read(60, run: "runA", app: "appA").Status);
    }

    [Fact]
    public void T7_MoreSleepsThanSlots_AreCountedAsDropped_AndTheFileIsIncomplete()
    {
        var recorder = Create(20, () => 3, capacity: 2);
        for (var call = 1; call <= 3; call++) Sleep(recorder, call, FaultDelayLog.ChainOverload, 20, 20);
        Assert.True(recorder.Export("test"));
        var reading = Read(20);
        Assert.Equal(FaultDelayStatus.Incomplete, reading.Status);
        Assert.Contains("dropped=1", reading.Reason, StringComparison.Ordinal);
        Assert.Equal(2, reading.Events.Count);
    }

    [Fact]
    public void T8_TwoWrappersOnOnePath_WriteTwoFiles_AndTheReaderCallsThatAmbiguous()
    {
        var a = Create(60, () => 1, name: "shared");
        var b = Create(80, () => 1, name: "shared");
        Sleep(a, 1, FaultDelayLog.ChainOverload, 60, 60);
        Sleep(b, 1, FaultDelayLog.ChainOverload, 80, 80);
        Assert.True(a.Export("test"));
        Assert.True(b.Export("test"));
        Assert.NotEqual(a.FilePath, b.FilePath);

        var reading = Read(60, name: "shared");
        Assert.Equal(FaultDelayStatus.Incomplete, reading.Status);
        Assert.StartsWith("multiple-wrappers=2", reading.Reason, StringComparison.Ordinal);

        // each file on its own is whole and carries its own request: nothing was overwritten or mixed
        var onlyA = FaultDelayLogReader.Classify(File.ReadAllLines(a.FilePath), a.FilePath, "run1", "app1", 60, Environment.ProcessId);
        var onlyB = FaultDelayLogReader.Classify(File.ReadAllLines(b.FilePath), b.FilePath, "run1", "app1", 80, Environment.ProcessId);
        Assert.Equal(FaultDelayStatus.Complete, onlyA.Status);
        Assert.Equal(FaultDelayStatus.Complete, onlyB.Status);
        Assert.Equal(60, onlyA.Events.Single().RequestedMs);
        Assert.Equal(80, onlyB.Events.Single().RequestedMs);
    }

    [Fact]
    public void T9_RequestedAndCalledButNothingRecorded_IsAnAnomaly_NotAVerdictOnTheInjection()
    {
        var recorder = Create(60, () => 2);   // two display calls happened, the recorder was never told of a sleep
        Assert.True(recorder.Export("test"));
        var reading = Read(60);
        Assert.Equal(FaultDelayStatus.Anomaly, reading.Status);
        Assert.Contains("SLEEP-RECORD-ABSENT-VS-REQUEST", reading.Reason, StringComparison.Ordinal);
        Assert.Contains("injection or the recording", reading.Reason, StringComparison.Ordinal);
        Assert.NotEqual(FaultDelayStatus.CompleteEmpty, reading.Status);
    }

    [Fact]
    public void T10_ASleepStillUnderWayAtExport_MakesTheFileIncomplete()
    {
        var recorder = Create(60, () => 1);
        var slot = recorder.Begin(1, FaultDelayLog.ChainOverload, 60);   // begun, never ended
        Assert.True(slot >= 0);
        Assert.True(recorder.Export("test"));
        var reading = Read(60);
        Assert.Equal(FaultDelayStatus.Incomplete, reading.Status);
        Assert.Contains("pending=1", reading.Reason, StringComparison.Ordinal);

        recorder.End(slot);   // too late to change the file; and a Begin after the seal is counted, not recorded
        Assert.Equal(-1, recorder.Begin(2, FaultDelayLog.ChainOverload, 60));
    }

    [Fact]
    public void T11_TwoExportsAtOnce_WriteOneCompleteFile()
    {
        for (var round = 0; round < 25; round++)
        {
            var recorder = Create(20, () => 1, name: "race" + round);
            Sleep(recorder, 1, FaultDelayLog.ChainOverload, 20, 20);
            using var barrier = new Barrier(4);
            var wrote = new bool[4];
            var threads = Enumerable.Range(0, 4).Select(i => new Thread(() => { barrier.SignalAndWait(); wrote[i] = recorder.Export(i % 2 == 0 ? "Shutdown" : "ProcessExit"); })).ToList();
            threads.ForEach(t => t.Start());
            threads.ForEach(t => t.Join());

            Assert.Equal(1, wrote.Count(w => w));
            var lines = File.ReadAllLines(recorder.FilePath);
            Assert.Equal(1, lines.Count(l => l.StartsWith("F ", StringComparison.Ordinal)));
            Assert.Equal(1, lines.Count(l => l == "END"));
            Assert.Equal(FaultDelayStatus.Complete, Read(20, name: "race" + round).Status);
        }
    }

    [Fact]
    public void T12_Placement_CountsWhatItSays_AndTheArithmeticFigureNeedsEveryPrecondition()
    {
        FaultDelayEvent At(int order, long t0, long t1) => new(order, order + 1, FaultDelayLog.ChainOverload, 60, t0, t1, 1);
        var header = new FaultDelayHeader("r", "a", 1, "w", 1000, 60, 256, "2026-10-22T00:00:00.0000000Z", 100, 101);
        FaultDelayReading Reading(FaultDelayStatus status, params FaultDelayEvent[] events) => new(status, "x", header, events, new FaultDelayFooter(events.Length, events.Length, 0, 0, 0, 0, "t"), null);

        // window [1000, 2000] ticks at 1000 Hz = 1 s
        Assert.Equal(FaultDelayPlacement.Inside, FaultDelayAttribution.Place(At(0, 1100, 1200), 1000, 2000));
        Assert.Equal(FaultDelayPlacement.StraddleStart, FaultDelayAttribution.Place(At(0, 900, 1200), 1000, 2000));
        Assert.Equal(FaultDelayPlacement.StraddleEnd, FaultDelayAttribution.Place(At(0, 1900, 2100), 1000, 2000));
        Assert.Equal(FaultDelayPlacement.Spans, FaultDelayAttribution.Place(At(0, 900, 2100), 1000, 2000));
        Assert.Equal(FaultDelayPlacement.Outside, FaultDelayAttribution.Place(At(0, 2200, 2300), 1000, 2000));

        var one = FaultDelayAttribution.Summarize(Reading(FaultDelayStatus.Complete, At(0, 1100, 1200)), 1000, 2000);
        Assert.Equal(1, one.Inside);
        Assert.Equal(100.0, one.InsideSumMs, 3);
        Assert.True(one.ArithmeticEligible);

        Assert.False(FaultDelayAttribution.Summarize(Reading(FaultDelayStatus.Incomplete, At(0, 1100, 1200)), 1000, 2000).ArithmeticEligible);                           // not a whole file
        Assert.False(FaultDelayAttribution.Summarize(Reading(FaultDelayStatus.Complete), 1000, 2000).ArithmeticEligible);                                                   // nothing inside
        Assert.False(FaultDelayAttribution.Summarize(Reading(FaultDelayStatus.Complete, At(0, 1100, 1200), At(1, 1900, 2100)), 1000, 2000).ArithmeticEligible);            // one crosses the edge
        var overlapping = FaultDelayAttribution.Summarize(Reading(FaultDelayStatus.Complete, At(0, 1100, 1500), At(1, 1400, 1600)), 1000, 2000);
        Assert.True(overlapping.Overlap);
        Assert.False(overlapping.ArithmeticEligible);
    }
}
#endif
