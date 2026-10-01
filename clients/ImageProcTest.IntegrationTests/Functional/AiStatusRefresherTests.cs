// #225 row 10 (GUI-C-186c, Codex #28 A2 + A3): a status read that gave up is asked again; an init that threw has ONE reason string.
using ImageProcTest.Models;
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// The real <see cref="AiStatusRefresher"/>, <see cref="AiSessionTracker"/> and <see cref="ProcessingChainRunner"/> with fakes for the
/// read and for the clock: no native code runs and nothing waits.
/// </summary>
[Trait("Category", "Functional")]
public sealed class AiStatusRefresherTests
{
    private static readonly AiWorkerStatus Active = new(AiWorkerState.Active, 0, 3);
    private static readonly AiWorkerStatus Disabled = new(AiWorkerState.Disabled, 3, 3);

    /// <summary>A clock that does nothing until the test runs what was scheduled.</summary>
    private sealed class FakeClock
    {
        public readonly List<(TimeSpan Delay, Action Run)> Pending = [];

        public void Schedule(TimeSpan delay, Action action) => Pending.Add((delay, action));

        /// <summary>Runs the oldest scheduled action; false when nothing is scheduled.</summary>
        public bool RunNext()
        {
            if (Pending.Count == 0)
            {
                return false;
            }

            var next = Pending[0];
            Pending.RemoveAt(0);
            next.Run();
            return true;
        }
    }

    // ---- A2: the read gave up, the screen is brought up to date later ---------------------------------------------------------------

    [Fact]
    public void AReadThatGivesUp_IsAskedAgainLater_AndTheLaterStatusReachesTheScreen()
    {
        var reads = new Queue<AiWorkerStatus?>([null, Active]); // a frame held the gate on the first read
        var shown = new List<AiWorkerStatus>();
        var clock = new FakeClock();
        var refresher = new AiStatusRefresher(() => reads.Dequeue(), shown.Add, clock.Schedule);

        refresher.Refresh();

        Assert.Empty(shown);                  // nothing to show yet: the first read gave up
        Assert.Single(clock.Pending);         // and a second read is booked, not dropped
        Assert.Equal(AiStatusRefresher.RetryDelay, clock.Pending[0].Delay);

        clock.RunNext();

        Assert.Equal([Active], shown);        // the restart's latest state does reach the screen
        Assert.Empty(clock.Pending);          // and the asking stopped there
    }

    [Fact]
    public void AReadThatWorks_BooksNothingElse()
    {
        var shown = new List<AiWorkerStatus>();
        var clock = new FakeClock();
        var refresher = new AiStatusRefresher(() => Active, shown.Add, clock.Schedule);

        refresher.Refresh();

        Assert.Equal([Active], shown);
        Assert.Empty(clock.Pending);
    }

    [Fact]
    public void AReadThatNeverWorks_IsAskedAFixedNumberOfTimes_ThenLeftAlone()
    {
        var reads = 0;
        var shown = new List<AiWorkerStatus>();
        var clock = new FakeClock();
        var refresher = new AiStatusRefresher(() => { reads++; return null; }, shown.Add, clock.Schedule);

        refresher.Refresh();
        var guard = 0;
        while (clock.RunNext() && guard++ < 100)
        {
        }

        Assert.Equal(1 + AiStatusRefresher.MaxRetries, reads); // the first read plus the retries, and no more
        Assert.Empty(shown);                                    // what was on screen stays as it was
        Assert.Empty(clock.Pending);
    }

    [Fact]
    public void ANewerRefresh_CancelsTheRetriesOfAnOlderOne_SoTheLastEventWins()
    {
        var current = (AiWorkerStatus?)null;     // what the module would answer, when the gate is free
        var gateBusy = true;
        var shown = new List<AiWorkerStatus>();
        var clock = new FakeClock();
        var refresher = new AiStatusRefresher(() => gateBusy ? null : current, shown.Add, clock.Schedule);

        refresher.Refresh();                      // an older event: the gate is busy, a retry is booked
        Assert.Single(clock.Pending);

        gateBusy = false;
        current = Disabled;                       // the newer event finds the state, and shows it
        refresher.Refresh();
        Assert.Equal([Disabled], shown);

        current = Active;                         // the state moves on; the OLD booked retry must not read and show it as if it were
        clock.RunNext();                          // the old retry fires: it was superseded, so it does nothing

        Assert.Equal([Disabled], shown);
    }

    [Fact]
    public void TheViewModel_RefreshesThroughTheRefresher_AndSchedulesWithoutBlockingTheUiThread()
    {
        var source = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/ViewModels/MainWindowViewModel.cs"));
        var start = source.IndexOf("private void RefreshAiWorkerStatus()", StringComparison.Ordinal);
        Assert.True(start >= 0, "RefreshAiWorkerStatus was not found.");
        var end = source.IndexOf("private async void RestartAiSession()", start, StringComparison.Ordinal);
        Assert.True(end > start, "The end of the refresh block was not found.");
        var block = source[start..end];

        Assert.Contains("new AiStatusRefresher(ReadAiWorkerStatus, ApplyAiWorkerStatus, ScheduleOnUiThread)", block, StringComparison.Ordinal);
        Assert.Contains("DispatcherTimer", block, StringComparison.Ordinal);
        Assert.DoesNotContain("Thread.Sleep", block, StringComparison.Ordinal);
        Assert.DoesNotContain("Task.Delay", block, StringComparison.Ordinal);
    }

    // ---- A3: one reason string for an init that threw ----------------------------------------------------------------------------

    [Fact]
    public void TheInitFailureReason_NamesTheExceptionTypeAndItsMessage_EndingWithAFullStop()
    {
        Assert.Equal("xpe_ai_init threw InvalidOperationException: boom.",
            AiBoneSuppressionStage.InitFailureReason(new InvalidOperationException("boom")));
        Assert.Equal("xpe_ai_init threw InvalidOperationException: boom.",
            AiBoneSuppressionStage.InitFailureReason(new InvalidOperationException("boom.")));
    }

    [Fact]
    public void TheTrackersDetail_AndTheChainsStageReason_ForTheSameThrownInit_AreTheSameString()
    {
        var thrown = new InvalidOperationException("the runtime refused");
        var reason = AiBoneSuppressionStage.InitFailureReason(thrown);

        // The tracker side: what the status line shows (AiSessionTracker.OwnStatus carries the recorded detail).
        var tracker = new AiSessionTracker();
        tracker.InitFailed(reason);
        var trackerDetail = tracker.OwnStatus()!.Detail;

        // The chain side: the exception that goes up from the init is an AiInitException with that same reason as its message.
        var chain = ProcessingChainRunner.Run([1, 2, 3], [new(StageIds.AiBoneSuppression, true)],
            (_, _) => throw new AiInitException(reason, thrown));
        var chainReason = chain.Stages[0].Reason;

        Assert.Equal(reason, trackerDetail);
        Assert.Equal($"{StageIds.AiBoneSuppression} threw: {reason}", chainReason);
        Assert.Contains(trackerDetail!, chainReason, StringComparison.Ordinal);
    }

    // ---- A1: the C-09 assertion's regex, and the control byte that once stood in for its backreference ------------------------------

    private const string C09StatePattern = @"worker=Disabled; failures=(\d+); ceiling=\1$";

    [Fact]
    public void TheC09Pattern_MatchesOnlyAStateWhoseFailuresEqualItsCeiling()
    {
        Assert.Matches(C09StatePattern, "worker=Disabled; failures=3; ceiling=3");
        Assert.DoesNotMatch(C09StatePattern, "worker=Disabled; failures=2; ceiling=3");
        Assert.DoesNotMatch(C09StatePattern, "worker=Active; failures=3; ceiling=3");
    }

    [Fact]
    public void TheE2EAssertion_CarriesThatPattern_AsTheTwoCharactersBackslashOne()
    {
        var source = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("clients/ImageProcTest.E2ETests/Scenarios/Workflows/ProcessingChainScenarios.cs"));

        Assert.Contains("Assert.Matches(@\"" + C09StatePattern + "\", before);", source, StringComparison.Ordinal);
    }

    [Fact]
    public void NoSourceFileOfTheTwoClients_CarriesAControlByte()
    {
        var root = Path.GetDirectoryName(BenchmarkRunnerServiceTests.ResolveRepositoryFile("CMakeLists.txt"))!;
        var files = new[] { "clients", "gui" }
            .SelectMany(top => Directory.EnumerateFiles(Path.Combine(root, top), "*.*", SearchOption.AllDirectories))
            .Where(path => path.EndsWith(".cs", StringComparison.OrdinalIgnoreCase) || path.EndsWith(".xaml", StringComparison.OrdinalIgnoreCase))
            .Where(path => !path.Contains($"{Path.DirectorySeparatorChar}obj{Path.DirectorySeparatorChar}") && !path.Contains($"{Path.DirectorySeparatorChar}bin{Path.DirectorySeparatorChar}"))
            .ToList();
        Assert.True(files.Count > 100, $"Only {files.Count} source files were found: the scan is not looking at the sources."); // the control: it reads files

        var offenders = new List<string>();
        foreach (var path in files)
        {
            var bytes = File.ReadAllBytes(path);
            var at = Array.FindIndex(bytes, b => b < 0x20 && b != (byte)'\t' && b != (byte)'\n' && b != (byte)'\r');
            if (at >= 0)
            {
                offenders.Add($"{Path.GetRelativePath(root, path)} (byte 0x{bytes[at]:x2}, line {bytes.Take(at).Count(b => b == (byte)'\n') + 1})");
            }
        }

        Assert.True(offenders.Count == 0, "A control byte in a source file: " + string.Join("; ", offenders));
    }

    [Fact]
    public void TheNativeInit_RecordsTheReasonItThrows_FromOneVariable()
    {
        var source = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/Services/Native/GuiAiRunner.cs"));
        var start = source.IndexOf("catch (Exception ex)", StringComparison.Ordinal);
        Assert.True(start >= 0, "The generic catch of the init was not found.");
        var block = source[start..Math.Min(source.Length, start + 700)];

        var make = block.IndexOf("var reason = AiBoneSuppressionStage.InitFailureReason(ex);", StringComparison.Ordinal);
        var record = block.IndexOf("Tracker.InitFailed(reason);", StringComparison.Ordinal);
        var rethrow = block.IndexOf("throw new AiInitException(reason, ex);", StringComparison.Ordinal);

        Assert.True(make >= 0 && record > make && rethrow > record,
            "The init must build the reason once, record it, and throw an exception carrying that same string.");
    }
}
