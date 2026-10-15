// GUI-C-244 (#268): a UI Automation call into an app that is not answering does not return, and no timeout the test sets can interrupt it (the PR #266 Native job stopped for its whole time limit inside one such call, with the
// app left running). The only thing that frees the call is the target going away, so every GuiApp carries this watchdog: after the limit it reports once and kills the app, the blocked call throws, the test fails with the report
// above the exception, and the app does not outlive the test.
using Xunit;

namespace ImageProcTest.E2ETests.Scenarios.Operator;

internal sealed class AppWatchdog : IDisposable
{
    private readonly Timer _timer;
    private int _fired;

    /// <summary>True once the limit passed and the app was killed (the exception a blocked call then throws is this watchdog's doing).</summary>
    public bool Fired => Volatile.Read(ref _fired) == 1;

    public AppWatchdog(TimeSpan limit, Action kill, Action<string> report)
    {
        _timer = new Timer(_ =>
        {
            if (Interlocked.Exchange(ref _fired, 1) != 0) return;
            try { report($"WATCHDOG: the app was still running {limit.TotalSeconds:0} s after it was launched; killing it so that a blocked UI Automation call returns. The exception that follows is the consequence, not the cause."); }
            catch (Exception) { /* the output helper is gone once the test has ended */ }
            try { kill(); }
            catch (Exception) { /* already gone */ }
        }, null, limit, Timeout.InfiniteTimeSpan);
    }

    /// <summary>XPE_E2E_APP_MAX_SECONDS, else 600 s (the longest app test takes about a minute; a hang is not told from slowness by waiting longer).</summary>
    public static TimeSpan DefaultLimit(string? configured = null)
    {
        configured ??= Environment.GetEnvironmentVariable("XPE_E2E_APP_MAX_SECONDS");
        return int.TryParse(configured, out var seconds) && seconds > 0 ? TimeSpan.FromSeconds(seconds) : TimeSpan.FromSeconds(600);
    }

    public void Dispose() => _timer.Dispose();
}

public class AppWatchdogTests
{
    [Fact]
    public void AfterTheLimit_ItReportsOnce_AndKillsTheApp_Once()
    {
        var kills = 0;
        var reports = new List<string>();
        var killed = new ManualResetEventSlim();
        using var watchdog = new AppWatchdog(TimeSpan.FromMilliseconds(50), () => { Interlocked.Increment(ref kills); killed.Set(); }, text => { lock (reports) reports.Add(text); });
        Assert.True(killed.Wait(TimeSpan.FromSeconds(30)), "the watchdog did not kill the app");
        Thread.Sleep(300);   // a second firing would show here
        Assert.Equal(1, Volatile.Read(ref kills));
        Assert.True(watchdog.Fired);
        lock (reports)
        {
            Assert.Single(reports);
            Assert.Contains("WATCHDOG", reports[0], StringComparison.Ordinal);
        }
    }

    [Fact]
    public void DisposedBeforeTheLimit_ItNeverKills()
    {
        var kills = 0;
        var watchdog = new AppWatchdog(TimeSpan.FromMilliseconds(200), () => Interlocked.Increment(ref kills), _ => { });
        watchdog.Dispose();
        Thread.Sleep(600);
        Assert.Equal(0, Volatile.Read(ref kills));
        Assert.False(watchdog.Fired);
    }

    [Fact]
    public void AKillThatThrows_DoesNotEscapeTheTimerThread()
    {
        var reported = new ManualResetEventSlim();
        using var watchdog = new AppWatchdog(TimeSpan.FromMilliseconds(20), () => throw new InvalidOperationException("gone"), _ => reported.Set());
        Assert.True(reported.Wait(TimeSpan.FromSeconds(30)));
        Thread.Sleep(200);
        Assert.True(watchdog.Fired);
    }

    [Theory]
    [InlineData(null, 600)]
    [InlineData("", 600)]
    [InlineData("abc", 600)]
    [InlineData("0", 600)]
    [InlineData("-5", 600)]
    [InlineData("90", 90)]
    public void TheLimit_IsTheConfiguredSeconds_ElseTenMinutes(string? configured, int expectedSeconds)
    {
        // null falls back to the environment variable, so the "unset" case is only meaningful when it is unset
        if (configured is null && Environment.GetEnvironmentVariable("XPE_E2E_APP_MAX_SECONDS") is not null) return;
        Assert.Equal(TimeSpan.FromSeconds(expectedSeconds), AppWatchdog.DefaultLimit(configured));
    }
}
