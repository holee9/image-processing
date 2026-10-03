using System.Diagnostics;

namespace ImageProcTest.IntegrationTests.Diagnostics;

/// <summary>
/// GUI-C-226d (Codex #123): how the verdict-holder tests wait. Two rules.
///
/// 1. A test hook that holds a worker thread at a chosen moment (a gate) must not let go on its own and leave the test believing it exercised that moment. <see cref="Gap"/> records a release by
///    time-out, and the test fails on it. The hook's own limit is LONGER than every outer wait in the tests (<see cref="HookLimit"/> against <see cref="OuterWait"/>), so an outer wait gives up
///    (and says so) before the hook would let go.
/// 2. When a wait fails, it says WHICH step, how long it waited and what the holder was doing, so that the next time-out can be told apart: the holder really stalled, or the runner was slow.
/// </summary>
internal static class VerdictTestWaits
{
    /// <summary>The longest any outer wait in the verdict tests lasts.</summary>
    public static readonly TimeSpan OuterWait = TimeSpan.FromSeconds(30);

    /// <summary>How long a gate holds a worker before it gives up and records that it did. Longer than <see cref="OuterWait"/> on purpose.</summary>
    public static readonly TimeSpan HookLimit = TimeSpan.FromSeconds(60);

    /// <summary>A gate for a worker thread: <see cref="Hit"/> is called by the hook on the worker, the test acts and then calls <see cref="Release"/>.</summary>
    public sealed class Gap : IDisposable
    {
        private readonly ManualResetEventSlim _reached = new();
        private readonly ManualResetEventSlim _go = new();

        /// <summary>True when the hook gave up waiting for <see cref="Release"/> (the test never released it within <see cref="HookLimit"/>).</summary>
        public volatile bool TimedOut;

        public ManualResetEventSlim Reached => _reached;

        /// <summary>Called by the hook on the worker thread; only the first call holds.</summary>
        public void Hit()
        {
            if (_reached.IsSet)
            {
                return;
            }

            _reached.Set();
            if (!_go.Wait(HookLimit))
            {
                TimedOut = true;
            }
        }

        public void Release() => _go.Set();

        /// <summary>Fails the test when the hook let go by itself: the moment the test meant to exercise was not the one it exercised.</summary>
        public void AssertHeldUntilReleased(string name) =>
            Xunit.Assert.False(TimedOut, $"the gate '{name}' released itself after {HookLimit.TotalSeconds:0} s: the test did not act in the moment it was written for");

        public void Dispose()
        {
            _go.Set();   // never leave a worker thread parked behind a test that has ended
            _reached.Dispose();
            _go.Dispose();
        }
    }

    /// <summary>Waits for <paramref name="signal"/>; on time-out fails with the step, the time waited and <paramref name="context"/> (counters and the holder's state).</summary>
    public static void Expect(ManualResetEventSlim signal, string step, Func<string> context, TimeSpan? timeout = null)
    {
        var limit = timeout ?? OuterWait;
        var watch = Stopwatch.StartNew();
        if (!signal.Wait(limit))
        {
            Xunit.Assert.Fail($"step '{step}' did not happen within {limit.TotalSeconds:0} s (waited {watch.Elapsed.TotalSeconds:0.0} s; {context()})");
        }
    }

    /// <summary>Polls <paramref name="probe"/> until it returns something; on time-out fails like <see cref="Expect"/>.</summary>
    public static async Task<T> PollAsync<T>(Func<T?> probe, string step, Func<string> context, TimeSpan? timeout = null) where T : class
    {
        var limit = timeout ?? OuterWait;
        var watch = Stopwatch.StartNew();
        while (true)
        {
            var value = probe();
            if (value is not null)
            {
                return value;
            }

            if (watch.Elapsed >= limit)
            {
                Xunit.Assert.Fail($"step '{step}' did not happen within {limit.TotalSeconds:0} s (waited {watch.Elapsed.TotalSeconds:0.0} s; {context()})");
            }

            await Task.Delay(20);
        }
    }
}
