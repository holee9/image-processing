// #225 row 10 (GUI-C-186f, Codex #36 finding 4): the backend's log and alert lists are written from several threads.
using ImageProcTest.Models;
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// The real <see cref="BackendTelemetry"/> written by several threads while another drains it. A plain <c>List</c> read by index while
/// another thread adds to it loses items, repeats them, or throws; the barrier makes the writers and the drainer start together so
/// the overlap is the case, not luck.
/// </summary>
[Trait("Category", "Functional")]
public sealed class BackendTelemetryTests
{
    private static AlertEntry Alert(int n) => new()
    {
        Severity = "INFO",
        Code = $"T{n}",
        Message = $"alert {n}",
        Timestamp = DateTimeOffset.UnixEpoch,
    };

    [Fact]
    public void Since_GivesTheItemsAfterTheCursors_AndTheTotalsTheCursorsMoveTo()
    {
        var telemetry = new BackendTelemetry();
        telemetry.AddLog("a");
        telemetry.AddLog("b");
        telemetry.AddLog("c");
        telemetry.AddAlert(Alert(1));
        telemetry.AddAlerts([Alert(2), Alert(3)]);

        var all = telemetry.Since(0, 0);
        Assert.Equal(["a", "b", "c"], all.Logs);
        Assert.Equal(3, all.LogTotal);
        Assert.Equal(["T1", "T2", "T3"], all.Alerts.Select(alert => alert.Code));
        Assert.Equal(3, all.AlertTotal);

        var rest = telemetry.Since(2, 1);
        Assert.Equal(["c"], rest.Logs);
        Assert.Equal(["T2", "T3"], rest.Alerts.Select(alert => alert.Code));

        var none = telemetry.Since(3, 3);
        Assert.Empty(none.Logs);
        Assert.Empty(none.Alerts);
    }

    [Fact]
    public void ACursorBeyondTheTotal_AfterAClear_StartsAgainFromTheBeginning()
    {
        var telemetry = new BackendTelemetry();
        telemetry.AddLog("old 1");
        telemetry.AddLog("old 2");
        telemetry.Clear();
        telemetry.AddLog("new");

        var snapshot = telemetry.Since(2, 0);   // the reader still holds the cursor from before the clear

        Assert.Equal(["new"], snapshot.Logs);
        Assert.Equal(1, snapshot.LogTotal);
    }

    [Fact]
    public void WritersAndADrainer_OverlappingOnABarrier_LoseNothingRepeatNothingAndNeverDisagreeWithTheirOwnCount()
    {
        const int rounds = 60;
        const int writers = 4;
        const int logsPerWriter = 400;
        const int alertsPerWriter = 100;

        for (var round = 0; round < rounds; round++)
        {
            var telemetry = new BackendTelemetry();
            var barrier = new Barrier(writers + 1);
            var errors = new List<string>();
            var seenLogs = new List<string>();
            var seenAlerts = new List<string>();
            var done = false;

            var writing = Enumerable.Range(0, writers).Select(writer => Task.Run(() =>
            {
                barrier.SignalAndWait();
                for (var i = 0; i < logsPerWriter; i++)
                {
                    telemetry.AddLog($"w{writer}-{i}");
                    if (i < alertsPerWriter)
                    {
                        telemetry.AddAlert(Alert(writer * 1000 + i));
                    }
                }
            })).ToArray();

            var drain = Task.Run(() =>
            {
                int logCursor = 0, alertCursor = 0;
                barrier.SignalAndWait();
                try
                {
                    while (true)
                    {
                        var finished = Volatile.Read(ref done);
                        var snapshot = telemetry.Since(logCursor, alertCursor);

                        // The count and the items it counts are one reading: they can never disagree.
                        if (snapshot.LogTotal != logCursor + snapshot.Logs.Count)
                        {
                            errors.Add($"log total {snapshot.LogTotal} != cursor {logCursor} + {snapshot.Logs.Count} items");
                        }

                        if (snapshot.AlertTotal != alertCursor + snapshot.Alerts.Count)
                        {
                            errors.Add($"alert total {snapshot.AlertTotal} != cursor {alertCursor} + {snapshot.Alerts.Count} items");
                        }

                        seenLogs.AddRange(snapshot.Logs);
                        seenAlerts.AddRange(snapshot.Alerts.Select(alert => alert.Code));
                        logCursor = snapshot.LogTotal;
                        alertCursor = snapshot.AlertTotal;
                        if (finished)
                        {
                            break;
                        }
                    }
                }
                catch (Exception ex)
                {
                    errors.Add($"the drain threw {ex.GetType().Name}: {ex.Message}");
                }
            });

            #pragma warning disable xUnit1031 // a bounded wait on a real thread: what is measured here
            Assert.True(Task.WaitAll(writing, TimeSpan.FromSeconds(30)), "the writers did not finish");
            #pragma warning restore xUnit1031
            Volatile.Write(ref done, true);
            #pragma warning disable xUnit1031 // a bounded wait on a real thread: what is measured here
            Assert.True(drain.Wait(TimeSpan.FromSeconds(30)), "the drain did not finish");
            #pragma warning restore xUnit1031

            Assert.True(errors.Count == 0, $"round {round}: {string.Join("; ", errors.Take(3))}");
            Assert.Equal(writers * logsPerWriter, seenLogs.Count);
            Assert.Equal(writers * logsPerWriter, seenLogs.Distinct().Count());   // none repeated, none lost
            Assert.DoesNotContain(null, seenLogs);
            Assert.Equal(writers * alertsPerWriter, seenAlerts.Count);
            Assert.Equal(writers * alertsPerWriter, seenAlerts.Distinct().Count());
        }
    }

    [Fact]
    public void BothBackends_KeepTheirListsInTheStore_AndTheViewModelDrainsThroughOneSnapshotCall()
    {
        foreach (var file in new[] { "RealXpeBackend.cs", "MockXpeBackend.cs" })
        {
            var text = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile($"gui/ImageProcTest/Services/{file}"));
            Assert.Contains("private readonly BackendTelemetry _telemetry = new();", text, StringComparison.Ordinal);
            Assert.DoesNotContain("List<string> _logs", text, StringComparison.Ordinal);
            Assert.DoesNotContain("List<AlertEntry> _alerts", text, StringComparison.Ordinal);
            Assert.Contains("GetTelemetrySince(int logsSeen, int alertsSeen) => _telemetry.Since(", text, StringComparison.Ordinal);
        }

        var wrapper = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/Services/FaultInjectingBackend.cs"));
        Assert.Contains("GetTelemetrySince(int logsSeen, int alertsSeen) => _inner.GetTelemetrySince(", wrapper, StringComparison.Ordinal);

        var viewModel = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/ViewModels/MainWindowViewModel.cs"));
        Assert.Equal(1, CountOf(viewModel, "_backend.GetTelemetrySince("));
        foreach (var old in new[] { "GetLogCount(", "GetAlertCount(", ".GetLog(", ".GetAlert(" })
        {
            Assert.DoesNotContain(old, viewModel, StringComparison.Ordinal);
        }
    }

    private static int CountOf(string text, string needle)
    {
        var count = 0;
        for (var at = text.IndexOf(needle, StringComparison.Ordinal); at >= 0; at = text.IndexOf(needle, at + needle.Length, StringComparison.Ordinal))
        {
            count++;
        }

        return count;
    }
}
