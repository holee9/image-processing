// #225 row 10 (GUI-C-186c A1 + A3, GUI-C-186d): the status is read in the background with no limit and applied only while current;
// an init that threw has ONE reason string; no control byte stands in for a regex backreference.
using System.Collections.Concurrent;
using ImageProcTest.Models;
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// The real <see cref="AiStatusRefresher"/>, <see cref="AiSessionGate"/>, <see cref="AiSessionTracker"/> and <see cref="ProcessingChainRunner"/>
/// with a fake UI thread (a queue the test pumps) and fake reads: no native code runs.
/// </summary>
[Trait("Category", "Functional")]
public sealed class AiStatusRefresherTests
{
    private static readonly AiWorkerStatus Active = new(AiWorkerState.Active, 0, 3);
    private static readonly AiWorkerStatus Disabled = new(AiWorkerState.Disabled, 3, 3);

    /// <summary>
    /// The refresher with a fake UI thread (a queue the test pumps) and a real background thread for the read. What the "UI thread"
    /// does is only what the test pumps, so a read that blocked it would show as a test that cannot proceed.
    /// </summary>
    private sealed class Rig
    {
        public object Current = new();
        public readonly ConcurrentQueue<Action> Ui = new();
        public readonly List<AiWorkerStatus> Applied = [];
        public int Reads;
        public Func<object?, AiWorkerStatus?> Read = _ => Active;
        public TimeSpan Now;
        public int Epoch;
        public readonly AiStatusRefresher Refresher;

        public Rig(bool backgroundReads = true)
        {
            Refresher = new AiStatusRefresher(
                () => Current,
                backend =>
                {
                    Interlocked.Increment(ref Reads);
                    return Read(backend);
                },
                Applied.Add,
                backgroundReads ? work => Task.Run(work) : work => work(),
                Ui.Enqueue,
                () => Now,
                sessionEpoch: () => Epoch);
        }

        /// <summary>Runs exactly one waiting UI action (so a test can look between two completions).</summary>
        public bool PumpOne()
        {
            if (!Ui.TryDequeue(out var action))
            {
                return false;
            }

            action();
            return true;
        }

        /// <summary>Runs what is waiting for the UI thread; how many actions ran.</summary>
        public int Pump()
        {
            var ran = 0;
            while (Ui.TryDequeue(out var action))
            {
                action();
                ran++;
            }

            return ran;
        }

        public void PumpUntil(Func<bool> done, string what)
        {
            var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(10);
            while (!done() && DateTime.UtcNow < deadline)
            {
                Pump();
                if (!done())
                {
                    Thread.Sleep(5);
                }
            }

            Assert.True(done(), what);
        }
    }

    private static readonly TimeSpan Long = TimeSpan.FromSeconds(10);

    // ---- A2 (structure, GUI-C-186d): the read is in the background, with no limit and no retry count -----------------------------

    /// <summary>
    /// Codex #31 finding 1: a frame that holds the gate longer than any retry window must not leave the screen stale. The frame's
    /// duration is an event here (the property is that nothing depends on it), and the proof is that after it ends NO further event
    /// is needed: the one read that was waiting completes and is shown.
    /// </summary>
    [Fact]
    public void AFrameThatHoldsTheGateForAsLongAsItLikes_ThenEnds_LeavesTheScreenCurrent_WithNoFurtherEvent()
    {
        var gate = new AiSessionGate();
        var frameIn = new ManualResetEventSlim();
        var frameMayEnd = new ManualResetEventSlim();
        var frame = Task.Run(() => gate.WithLock(() =>
        {
            frameIn.Set();
            Assert.True(frameMayEnd.Wait(Long));
            return 0;
        }));
        Assert.True(frameIn.Wait(Long));

        var rig = new Rig { Read = _ => gate.WithLock(() => Disabled) };
        rig.Refresher.Request();

        Assert.Empty(rig.Applied);                 // the frame still has the gate: nothing to show yet
        frameMayEnd.Set();                         // the frame ends; nobody asks again
        #pragma warning disable xUnit1031 // a bounded wait on a real thread: what is measured here
        Assert.True(frame.Wait(Long));
        #pragma warning restore xUnit1031
        rig.PumpUntil(() => rig.Applied.Count == 1, "The read that was waiting for the gate never reached the screen.");

        Assert.Equal(Disabled, rig.Applied[0]);
        Assert.Equal(1, rig.Reads);                // one read, no retries
    }

    /// <summary>Codex #31 finding 3: while the gate is held, the caller of <c>Request</c> and the UI thread both go on.</summary>
    [Fact]
    public void WhileTheGateIsHeld_RequestReturnsAtOnce_AndTheUiThreadKeepsWorking()
    {
        var gate = new AiSessionGate();
        var frameIn = new ManualResetEventSlim();
        var frameMayEnd = new ManualResetEventSlim();
        var frame = Task.Run(() => gate.WithLock(() =>
        {
            frameIn.Set();
            Assert.True(frameMayEnd.Wait(Long));
            return 0;
        }));
        Assert.True(frameIn.Wait(Long));

        var rig = new Rig { Read = _ => gate.WithLock(() => Active) };
        var watch = System.Diagnostics.Stopwatch.StartNew();
        rig.Refresher.Request();
        rig.Refresher.Request();
        var requested = watch.Elapsed;

        var uiWorkRan = false;
        rig.Ui.Enqueue(() => uiWorkRan = true);    // other UI work that arrives while the read waits
        rig.Pump();

        frameMayEnd.Set();
        #pragma warning disable xUnit1031 // a bounded wait on a real thread: what is measured here
        Assert.True(frame.Wait(Long));
        #pragma warning restore xUnit1031
        rig.PumpUntil(() => rig.Applied.Count == 1, "The read never completed after the frame ended.");

        Assert.True(requested < TimeSpan.FromSeconds(1), $"Request waited {requested.TotalMilliseconds:0} ms for a gate that was held: the UI thread would have waited the same.");
        Assert.True(uiWorkRan, "UI work queued while the read waited was not processed.");
    }

    /// <summary>Codex #31 finding 2: a status read for a backend that was replaced is dropped, and the screen shows what the new one says.</summary>
    [Fact]
    public void AfterTheBackendIsReplaced_AStaleReadIsDropped_AndTheScreenShowsTheNewBackend()
    {
        var oldBackend = new object();
        var newBackend = new object();
        var releaseOld = new ManualResetEventSlim();
        var oldReading = new ManualResetEventSlim();
        var rig = new Rig { Current = oldBackend };
        rig.Read = backend =>
        {
            if (ReferenceEquals(backend, oldBackend))
            {
                oldReading.Set();
                Assert.True(releaseOld.Wait(Long));
                return Disabled;                    // what the old (native) backend would still say
            }

            return AiWorkerStatus.Unknown;          // the new (mock) backend has no AI session
        };

        rig.Refresher.Request();
        Assert.True(oldReading.Wait(Long));        // the read for the old backend is running and blocked

        rig.Current = newBackend;                   // the view model replaces the backend ...
        rig.Refresher.Reset();                      // ... raising the generation and showing Unknown at once
        rig.Refresher.Request();                    // ... and asks again after the initialisation (one more, after the running one)
        Assert.Equal([AiWorkerStatus.Unknown], rig.Applied);

        releaseOld.Set();
        rig.PumpUntil(() => rig.Reads == 2 && rig.Ui.IsEmpty && rig.Applied.Count >= 2, "The read for the new backend never reached the screen.");
        rig.Pump();

        Assert.DoesNotContain(Disabled, rig.Applied);   // the old backend's answer was never shown
        Assert.Equal(AiWorkerStatus.Unknown, rig.Applied[^1]);
    }

    /// <summary>
    /// The backend shutdown keeps the same backend object: only the generation tells a read started before it from a read after it.
    /// </summary>
    [Fact]
    public void ARead_StartedBeforeAReset_OfTheSameBackend_IsDropped_AndTheOneAfterItIsShown()
    {
        var release = new ManualResetEventSlim();
        var reading = new ManualResetEventSlim();
        var rig = new Rig();
        var calls = 0;
        rig.Read = _ =>
        {
            if (Interlocked.Increment(ref calls) == 1)
            {
                reading.Set();
                Assert.True(release.Wait(Long));
                return Disabled;                    // the session that was ending when the read began
            }

            return Active;                          // what the module says once the shutdown and restart are over
        };

        rig.Refresher.Request();
        Assert.True(reading.Wait(Long));
        rig.Refresher.Reset();                      // the shutdown begins: same backend object, new generation
        rig.Refresher.Request();                    // and a read is wanted after it
        release.Set();
        rig.PumpUntil(() => rig.Applied.Count == 2, "The read after the reset never reached the screen.");

        Assert.Equal([AiWorkerStatus.Unknown, Active], rig.Applied);   // the old session's Disabled never was shown
    }

    /// <summary>The generation alone is not the guard: a backend swapped without a <c>Reset</c> is caught by identity.</summary>
    [Fact]
    public void ARead_ForABackendThatIsNoLongerTheCurrentOne_IsDropped_EvenWithTheSameGeneration()
    {
        var first = new object();
        var second = new object();
        var release = new ManualResetEventSlim();
        var reading = new ManualResetEventSlim();
        var rig = new Rig { Current = first };
        rig.Read = backend =>
        {
            if (ReferenceEquals(backend, first))
            {
                reading.Set();
                Assert.True(release.Wait(Long));
                return Disabled;
            }

            return Active;
        };

        rig.Refresher.Request();
        Assert.True(reading.Wait(Long));
        rig.Current = second;                       // no Reset: only the identity differs
        release.Set();
        rig.PumpUntil(() => rig.Applied.Count == 1, "The read for the current backend never reached the screen.");

        Assert.Equal(Active, rig.Applied[0]);       // the stale Disabled was dropped, the current backend was read instead
        Assert.Equal(2, rig.Reads);
    }

    /// <summary>The application is closing: a read that was running, and one requested afterwards, change nothing.</summary>
    [Fact]
    public void AfterStop_ARunningReadIsNotApplied_AndANewRequestStartsNothing()
    {
        var release = new ManualResetEventSlim();
        var reading = new ManualResetEventSlim();
        var rig = new Rig();
        rig.Read = _ =>
        {
            reading.Set();
            Assert.True(release.Wait(Long));
            return Disabled;
        };

        rig.Refresher.Request();
        Assert.True(reading.Wait(Long));
        rig.Refresher.Stop();
        release.Set();
        Thread.Sleep(200);                          // let the finished read's completion reach the queue, if it is going to
        rig.Pump();

        rig.Refresher.Request();                    // after the stop
        rig.Refresher.Reset();
        rig.Pump();
        Thread.Sleep(100);
        rig.Pump();

        Assert.Empty(rig.Applied);                  // not the running read, not Reset's Unknown
        Assert.Equal(1, rig.Reads);                 // and no read was started after the stop
    }

    [Fact]
    public void RequestsThatPileUpWhileAReadRuns_BecomeExactlyOneMoreRead()
    {
        var release = new ManualResetEventSlim();
        var reading = new ManualResetEventSlim();
        var rig = new Rig();
        rig.Read = _ =>
        {
            reading.Set();
            Assert.True(release.Wait(Long));
            return Active;
        };

        rig.Refresher.Request();
        Assert.True(reading.Wait(Long));
        for (var i = 0; i < 6; i++)
        {
            rig.Refresher.Request();                // six more while one runs
        }

        release.Set();
        rig.PumpUntil(() => rig.Applied.Count == 1, "The one extra read never completed.");
        Thread.Sleep(100);
        rig.Pump();

        Assert.Equal(2, rig.Reads);                 // the running one, and ONE more for all six requests
        Assert.Single(rig.Applied);                 // GUI-C-192: only the newest answer is shown; the running one was older than the requests
    }

    // ---- GUI-C-192: an answer older than a newer request is never shown ---------------------------------------------------------------

    /// <summary>
    /// The reproduction from GUI-C-191b as a test: the first read is held inside the module and answers from what it saw when it STARTED;
    /// a newer request is made while it is held; when it is released its answer must not reach the screen. Recorded at <c>apply</c> (where
    /// the screen's property changes), not sampled: a stale value shown for a millisecond is exactly what a poll misses.
    /// </summary>
    [Fact]
    public void AnAnswerOlderThanANewerRequest_IsNeverApplied_OnlyTheNewestIs()
    {
        var release = new ManualResetEventSlim();
        var reading = new ManualResetEventSlim();
        var rig = new Rig();
        var stale = new AiWorkerStatus(AiWorkerState.Active, 0, 3);
        var fresh = new AiWorkerStatus(AiWorkerState.Active, 2, 3);
        var call = 0;
        rig.Read = _ =>
        {
            if (Interlocked.Increment(ref call) == 1)
            {
                reading.Set();
                Assert.True(release.Wait(Long));
                return stale;                        // what the module said when this read began
            }

            return fresh;
        };

        rig.Refresher.Request();                     // Apply A's read, held inside the module
        Assert.True(reading.Wait(Long));
        rig.Refresher.Request();                     // Apply B asks again while it is held
        release.Set();
        rig.PumpUntil(() => rig.Applied.Count >= 1, "The newest read was never shown.");
        Thread.Sleep(150);
        rig.Pump();

        Assert.Equal([fresh], rig.Applied);          // exactly one change on the screen, and it is the newest
        Assert.DoesNotContain(stale, rig.Applied);
    }

    [Fact]
    public void AnAnswerWithNoNewerRequest_IsStillApplied()
    {
        var rig = new Rig();
        rig.Read = _ => Active;

        rig.Refresher.Request();
        rig.PumpUntil(() => rig.Applied.Count == 1, "A read nobody superseded was not shown.");

        Assert.Equal([Active], rig.Applied);
    }

    [Fact]
    public void AReadThatThrows_ShowsNothing_AndTheNextRequestStillWorks()
    {
        var rig = new Rig();
        var fail = true;
        rig.Read = _ => fail ? throw new InvalidOperationException("the read failed") : Active;

        rig.Refresher.Request();
        Thread.Sleep(200);                          // the failed read has finished by now; its completion is queued for the UI
        rig.Pump();
        Assert.Equal(1, rig.Reads);
        Assert.Empty(rig.Applied);

        fail = false;
        rig.Refresher.Request();                    // the failed read must not leave "a read is running" set
        rig.PumpUntil(() => rig.Applied.Count == 1, "A request after a failed read never completed.");
        Assert.Equal(Active, rig.Applied[0]);
    }

    [Fact]
    public void TheGateRead_WaitsForAFrame_WithNoLimit_AndReturnsWhenItEnds()
    {
        var gate = new AiSessionGate();
        var frameIn = new ManualResetEventSlim();
        var frameMayEnd = new ManualResetEventSlim();
        var frame = Task.Run(() => gate.WithLock(() =>
        {
            frameIn.Set();
            Assert.True(frameMayEnd.Wait(Long));
            return 0;
        }));
        Assert.True(frameIn.Wait(Long));

        var read = Task.Run(() => gate.WithLock(() => 7));
        #pragma warning disable xUnit1031 // a bounded wait on a real thread: what is measured here
        Assert.False(read.Wait(TimeSpan.FromMilliseconds(400)), "The read got through a gate that a frame was holding.");
        #pragma warning restore xUnit1031

        frameMayEnd.Set();
        #pragma warning disable xUnit1031 // a bounded wait on a real thread: what is measured here
        Assert.True(frame.Wait(Long));
        Assert.True(read.Wait(Long));
        Assert.Equal(7, read.Result);
        #pragma warning restore xUnit1031
    }

    // ---- GUI-C-192b (Codex #59): "Disabled" is never held back by newer requests, and an "Active" that nothing refreshes is withdrawn ----

    /// <summary>Reads that each have a newer request arrive while they run, for the first <paramref name="times"/> reads, then quiet.</summary>
    private static Rig InterleavedRig(AiWorkerStatus answer, int times)
    {
        Rig? rig = null;
        rig = new Rig(backgroundReads: false);
        var n = 0;
        rig.Read = _ =>
        {
            if (++n <= times)
            {
                rig.Refresher.Request();   // a newer request is made while this read runs
            }

            return answer;
        };
        return rig;
    }

    /// <summary>(a) A request on every read must not keep a "switched off" answer from the screen: it shows from the FIRST Disabled answer.</summary>
    [Fact]
    public void ADisabledAnswer_IsShownFromTheFirstAnswer_EvenWhenARequestArrivesDuringEveryRead()
    {
        var rig = InterleavedRig(Disabled, times: 4);
        rig.Refresher.Request();

        Assert.True(rig.PumpOne());
        Assert.Equal([Disabled], rig.Applied);     // after ONE completion, not after the requests stop
    }

    /// <summary>(d) 192 still holds: an "Active" answer that a newer request has overtaken is never shown.</summary>
    [Fact]
    public void AnActiveAnswer_ThatANewerRequestHasOvertaken_IsStillNeverShown()
    {
        var rig = InterleavedRig(Active, times: 1);
        rig.Refresher.Request();

        Assert.True(rig.PumpOne());
        Assert.Empty(rig.Applied);                 // the first answer was overtaken
        rig.Pump();
        Assert.Equal([Active], rig.Applied);       // only the newest read's answer reached the screen
    }

    /// <summary>(c) A "Disabled" from before a restart or a backend replacement belongs to a session that no longer exists.</summary>
    [Theory]
    [InlineData(false)]
    [InlineData(true)]
    public void ADisabledAnswer_FromBeforeARestartOrABackendSwap_IsNotShown(bool backendSwap)
    {
        var rig = new Rig();
        var release = new ManualResetEventSlim();
        var calls = 0;
        rig.Read = _ =>
        {
            if (Interlocked.Increment(ref calls) == 1)
            {
                Assert.True(release.Wait(Long));
                return Disabled;               // the old session's answer
            }

            return Active;                     // the new session
        };
        rig.Refresher.Request();
        rig.Refresher.Request();               // a newer request too, so the old answer is also superseded
        if (backendSwap)
        {
            rig.Current = new object();
        }

        rig.Refresher.Reset();                 // restart / replacement: generation up, Unknown on screen
        release.Set();
        rig.PumpUntil(() => rig.Applied.Contains(Active), "The new session's read never reached the screen.");

        Assert.DoesNotContain(Disabled, rig.Applied);
    }

    /// <summary>(b) A read blocked past the bound turns a standing "Active" into the conservative "Unknown"; a late answer then replaces it.</summary>
    [Fact]
    public void AnActive_NothingRefreshes_IsWithdrawnAtTheBound_AndNotBefore()
    {
        var rig = new Rig(backgroundReads: false);
        rig.Refresher.Request();
        rig.Pump();
        Assert.Equal([Active], rig.Applied);

        var block = new ManualResetEventSlim();
        rig.Read = _ =>
        {
            Assert.True(block.Wait(Long));
            return Active;
        };
        var blocked = Task.Run(rig.Refresher.Request);   // the read waits behind a silent worker
        rig.PumpUntil(() => Volatile.Read(ref rig.Reads) == 2, "The blocked read never started.");

        rig.Now = AiStatusRefresher.DefaultActiveFreshFor - TimeSpan.FromTicks(1);
        rig.Refresher.CheckFreshness();
        Assert.Equal([Active], rig.Applied);             // just under the bound: still shown

        rig.Now = AiStatusRefresher.DefaultActiveFreshFor;
        rig.Refresher.CheckFreshness();
        Assert.Equal(2, rig.Applied.Count);
        Assert.Equal(AiWorkerState.Unconfirmed, rig.Applied[1].State);   // 192c: a state that shows a notice, not the silent Unknown
        Assert.Equal("15", rig.Applied[1].Detail);

        rig.Refresher.CheckFreshness();
        Assert.Equal(2, rig.Applied.Count);              // withdrawn once, not repeated

        block.Set();
        #pragma warning disable xUnit1031 // a bounded wait on a real thread: what is measured here
        Assert.True(blocked.Wait(Long));
        #pragma warning restore xUnit1031
        rig.Pump();
        Assert.Equal(AiWorkerState.Active, rig.Applied[^1].State);   // the late answer is shown again
    }

    /// <summary>192c: an idle app must not age into a false alarm. With no read in flight an old "Active" is READ AGAIN, and stays Active.</summary>
    [Fact]
    public void AnIdleActive_IsReadAgain_NotWithdrawn()
    {
        var rig = new Rig(backgroundReads: false);
        rig.Refresher.Request();
        rig.Pump();
        Assert.Equal(1, rig.Reads);

        rig.Now = AiStatusRefresher.DefaultActiveFreshFor / 3 - TimeSpan.FromTicks(1);
        rig.Refresher.CheckFreshness();
        Assert.Equal(1, rig.Reads);                      // not yet old enough to ask again

        rig.Now = AiStatusRefresher.DefaultActiveFreshFor / 3;
        rig.Refresher.CheckFreshness();
        rig.Pump();
        Assert.Equal(2, rig.Reads);                      // asked again, and the answer was Active again

        for (var second = 1; second <= 120; second++)    // two minutes of an idle app, ticking every second
        {
            rig.Now += TimeSpan.FromSeconds(1);
            rig.Refresher.CheckFreshness();
            rig.Pump();
        }

        Assert.DoesNotContain(rig.Applied, status => status.State == AiWorkerState.Unconfirmed);
        Assert.True(rig.Reads > 10);                     // it kept asking
    }

    /// <summary>192c: the normal "Unknown" (before the first read, after a Reset) is never turned into a notice, however long it lasts.</summary>
    [Fact]
    public void AnUnknownThatWasNeverActive_IsNeverTurnedIntoANotice()
    {
        var rig = new Rig(backgroundReads: false) { Read = _ => AiWorkerStatus.Unknown };
        rig.Refresher.Request();
        rig.Pump();
        rig.Now = TimeSpan.FromHours(1);
        rig.Refresher.CheckFreshness();
        rig.Refresher.Reset();
        rig.Now = TimeSpan.FromHours(2);
        rig.Refresher.CheckFreshness();

        Assert.DoesNotContain(rig.Applied, status => status.State == AiWorkerState.Unconfirmed);
    }

    /// <summary>192c: the notice goes when a new answer arrives (and when the session is replaced); it is read again meanwhile.</summary>
    [Fact]
    public void TheNotice_GoesWhenANewAnswerArrives_AndIsReadAgainWhileItStands()
    {
        var rig = new Rig(backgroundReads: false);
        rig.Refresher.Request();
        rig.Pump();
        rig.Read = _ => null;                            // reads fail: nothing is confirmed
        rig.Now = AiStatusRefresher.DefaultActiveFreshFor;
        rig.Refresher.CheckFreshness();
        Assert.Equal(AiWorkerState.Unconfirmed, rig.Applied[^1].State);

        var readsBefore = rig.Reads;
        rig.Now += AiStatusRefresher.DefaultActiveFreshFor / 3;
        rig.Refresher.CheckFreshness();                  // the notice is read again too
        rig.Pump();
        Assert.Equal(readsBefore + 1, rig.Reads);
        Assert.Equal(AiWorkerState.Unconfirmed, rig.Applied[^1].State);   // a failed read confirms nothing: the notice stays

        rig.Read = _ => Active;
        rig.Now += AiStatusRefresher.DefaultActiveFreshFor / 3;
        rig.Refresher.CheckFreshness();
        rig.Pump();
        Assert.Equal(Active, rig.Applied[^1]);           // the worker answered: the notice is gone

        rig.Read = _ => null;
        rig.Now += AiStatusRefresher.DefaultActiveFreshFor;
        rig.Refresher.CheckFreshness();
        Assert.Equal(AiWorkerState.Unconfirmed, rig.Applied[^1].State);
        rig.Refresher.Reset();                           // a restart or a backend swap
        Assert.Equal(AiWorkerStatus.Unknown, rig.Applied[^1]);
    }

    // ---- GUI-C-192e (Codex #62) ------------------------------------------------------------------------------------------------------

    /// <summary>
    /// Finding 2: a first read that THROWS is not an answer. It is asked again, and when the failures go on for the bound the never-confirmed
    /// notice is raised; a read that RETURNS Unknown (AI not requested) is an answer and is never retried.
    /// </summary>
    [Fact]
    public void AFirstReadThatFails_IsRetried_AndRaisesTheNoticeWhenTheFailuresGoOn()
    {
        var rig = new Rig(backgroundReads: false) { Read = _ => throw new InvalidOperationException("the read failed") };
        rig.Refresher.Request();
        rig.Pump();
        Assert.Equal(1, rig.Reads);
        Assert.Empty(rig.Applied);

        rig.Now = AiStatusRefresher.DefaultActiveFreshFor / 3 - TimeSpan.FromTicks(1);
        rig.Refresher.CheckFreshness();
        Assert.Equal(1, rig.Reads);                       // not asked again before a third of the bound

        rig.Now = AiStatusRefresher.DefaultActiveFreshFor / 3;
        rig.Refresher.CheckFreshness();
        rig.Pump();
        Assert.Equal(2, rig.Reads);                       // asked again

        for (var second = 6; second < 15; second++)       // the failures go on, a check every second: retries are spaced, not a storm
        {
            rig.Now = TimeSpan.FromSeconds(second);
            rig.Refresher.CheckFreshness();
            rig.Pump();
        }

        Assert.Empty(rig.Applied);                        // under the bound: no notice
        Assert.InRange(rig.Reads, 3, 4);

        rig.Now = AiStatusRefresher.DefaultActiveFreshFor;
        rig.Refresher.CheckFreshness();
        Assert.Single(rig.Applied);
        Assert.True(rig.Applied[0].NeverConfirmed);       // counted from the FIRST unanswered read, not from the last retry
        Assert.Equal(AiWorkerState.Unconfirmed, rig.Applied[0].State);

        rig.Read = _ => AiWorkerStatus.Unknown;           // the read returns at last: an answer, even "Unknown", ends the notice
        rig.Now += AiStatusRefresher.DefaultActiveFreshFor / 3;
        rig.Refresher.CheckFreshness();
        rig.Pump();
        Assert.Equal(AiWorkerStatus.Unknown, rig.Applied[^1]);
        var readsAfterAnswer = rig.Reads;
        rig.Now += TimeSpan.FromHours(1);
        rig.Refresher.CheckFreshness();
        Assert.Equal(readsAfterAnswer, rig.Reads);        // an answered Unknown is never retried
    }

    /// <summary>Finding 1: a session replaced by code that does not call Reset (a frame's re-init) still cannot show its predecessor's "Disabled".</summary>
    [Fact]
    public void ADisabledAnswer_ReadUnderAnEarlierSession_IsDropped_EvenWithNoReset()
    {
        var (rig, release) = BlockedFirstRead(Disabled);
        rig.Refresher.Request();                          // a newer request exists, as in the real wiring
        rig.Epoch++;                                      // the module session was replaced; nobody called Reset
        rig.Read = _ => Active;                           // the new session
        release.Set();
        rig.PumpUntil(() => rig.Applied.Contains(Active), "The new session's answer never reached the screen.");

        Assert.DoesNotContain(Disabled, rig.Applied);
    }

    /// <summary>Finding 4: the longest gap between two checks (the UI stall) is measured.</summary>
    [Fact]
    public void TheLongestGapBetweenChecks_IsMeasured()
    {
        var rig = new Rig(backgroundReads: false);
        rig.Now = TimeSpan.FromSeconds(1);
        rig.Refresher.CheckFreshness();
        rig.Now = TimeSpan.FromSeconds(2);
        rig.Refresher.CheckFreshness();
        rig.Now = TimeSpan.FromSeconds(22);               // the UI thread did not run for 20 s
        rig.Refresher.CheckFreshness();
        rig.Now = TimeSpan.FromSeconds(23);
        rig.Refresher.CheckFreshness();

        Assert.Contains("maxUiGapMs=20000", rig.Refresher.Measurements, StringComparison.Ordinal);
    }

    /// <summary>
    /// Finding 3: the default clock multiplied the raw counter by TicksPerSecond in a long, which overflows at counter 922,337,203,685 (about
    /// 25.6 h at 10 MHz). The clock now converts a DIFFERENCE from its own start. Counter values are injected on both sides of that boundary.
    /// </summary>
    [Fact]
    public void TheMonotonicClock_DoesNotOverflow_AndDoesNotAssumeTheCounterStartsAtZero()
    {
        const long frequency = 10_000_000;
        const long overflowsAt = long.MaxValue / TimeSpan.TicksPerSecond;      // 922_337_203_685: where counter * 10^7 leaves a long
        Assert.True(unchecked(overflowsAt * 2 * TimeSpan.TicksPerSecond) < 0, "the old formula is expected to overflow here, or this test shows nothing");

        var counter = overflowsAt - 4 * frequency;                              // a start 4 s below the old formula's limit
        var clock = new MonotonicClock(() => counter, frequency);
        Assert.Equal(TimeSpan.Zero, clock.Elapsed());
        var last = TimeSpan.Zero;
        for (var step = 1; step <= 20; step++)                                  // 20 s in 1 s steps, across the boundary
        {
            counter += frequency;
            var elapsed = clock.Elapsed();
            Assert.Equal(TimeSpan.FromSeconds(step), elapsed);
            Assert.True(elapsed > last, "the clock went backwards");
            last = elapsed;
        }

        counter += frequency / 2;
        Assert.Equal(TimeSpan.FromSeconds(20.5), clock.Elapsed());

        var odd = new MonotonicClock(() => counter, 1_000_000_007);             // a frequency that does not divide a second evenly
        var oddStart = counter;
        counter = oddStart + 2_500_000_017;
        Assert.InRange(odd.Elapsed().TotalSeconds, 2.4999999, 2.5000001);
    }

    /// <summary>The 5 s and 15 s decisions are right across the boundary: the refresher on the real clock class, counter injected.</summary>
    [Fact]
    public void TheDecisions_AreRightAcrossTheCounterBoundary()
    {
        const long frequency = 10_000_000;
        var counter = long.MaxValue / TimeSpan.TicksPerSecond - 3 * frequency;
        var clock = new MonotonicClock(() => counter, frequency);
        var applied = new List<AiWorkerStatus>();
        var reads = 0;
        var ui = new Queue<Action>();
        var refresher = new AiStatusRefresher(() => this, _ => { reads++; return Active; }, applied.Add, work => work(), ui.Enqueue, clock.Elapsed);
        refresher.Request();
        while (ui.Count > 0) { ui.Dequeue()(); }
        Assert.Equal([Active], applied);

        counter += 5 * frequency;                         // 5 s later, past the old formula's limit: asked again
        refresher.CheckFreshness();
        while (ui.Count > 0) { ui.Dequeue()(); }
        Assert.Equal(2, reads);

        refresher = new AiStatusRefresher(() => this, _ => null, applied.Add, work => work(), ui.Enqueue, clock.Elapsed);
        applied.Clear();
        refresher.Request();                              // reads fail from here on
        while (ui.Count > 0) { ui.Dequeue()(); }
        counter += 14 * frequency;
        refresher.CheckFreshness();
        Assert.Empty(applied);                            // 14 s: still under the bound
        counter += 1 * frequency;
        refresher.CheckFreshness();
        Assert.Single(applied);                           // 15 s: the notice
        Assert.True(applied[0].NeverConfirmed);
    }

    // ---- GUI-C-192f (Codex #66) ------------------------------------------------------------------------------------------------------

    /// <summary>
    /// Finding 1: an answer ALREADY on screen belongs to the session it was read under. When a frame replaces the session with no Reset, the
    /// epoch moves; the shown "Active" or "Disabled" is taken back to Unknown at once, and the new read's answer, failure or delay is what follows,
    /// never the old answer standing. Each shown state x each kind of new read.
    /// </summary>
    [Theory]
    [InlineData(true, "answers")]
    [InlineData(true, "fails")]
    [InlineData(true, "blocks")]
    [InlineData(false, "answers")]
    [InlineData(false, "fails")]
    [InlineData(false, "blocks")]
    public void ADisabledOrActiveAlreadyOnScreen_IsWithdrawn_WhenTheSessionIsReplacedWithNoReset(bool disabled, string newRead)
    {
        var shownBefore = disabled ? Disabled : Active;
        var rig = new Rig();
        var block = new ManualResetEventSlim();
        var calls = 0;
        rig.Read = _ =>
        {
            if (Interlocked.Increment(ref calls) == 1)
            {
                return shownBefore;                              // the old session's answer
            }

            return newRead switch
            {
                "answers" => new AiWorkerStatus(AiWorkerState.Active, 0, 3),
                "fails" => throw new InvalidOperationException("the new session's read failed"),
                _ => WaitThenAnswer(block),
            };
        };
        rig.Refresher.Request();
        rig.PumpUntil(() => rig.Applied.Count == 1, "The first answer never reached the screen.");
        Assert.Equal(shownBefore, rig.Applied[0]);

        rig.Epoch++;                                             // a frame replaced the session; nobody called Reset
        rig.Now = TimeSpan.FromSeconds(1);
        rig.Refresher.CheckFreshness();                          // the timer tick
        Assert.Equal(AiWorkerStatus.Unknown, rig.Applied[1]);    // withdrawn AT ONCE, before the new read can say anything

        if (newRead == "answers")
        {
            rig.PumpUntil(() => rig.Applied.Count == 3, "The new session's answer never reached the screen.");
            Assert.Equal(AiWorkerState.Active, rig.Applied[2].State);
            return;
        }

        // The new read fails or hangs: the old answer must NOT come back, and the failure ends in the never-confirmed notice at the bound.
        rig.PumpUntil(() => Volatile.Read(ref calls) >= 2, "The new session's read never started.");
        for (var second = 2; second <= 17; second++)
        {
            rig.Now = TimeSpan.FromSeconds(second);
            rig.Refresher.CheckFreshness();
            rig.Pump();
        }

        Assert.DoesNotContain(rig.Applied.Skip(1), status => status.State == AiWorkerState.Disabled);
        Assert.Equal(AiWorkerState.Unconfirmed, rig.Applied[^1].State);
        Assert.True(rig.Applied[^1].NeverConfirmed);
        block.Set();
    }

    private static AiWorkerStatus WaitThenAnswer(ManualResetEventSlim release)
    {
        release.Wait(Long);
        return new AiWorkerStatus(AiWorkerState.Active, 0, 3);
    }

    /// <summary>The request path notices the moved session as well, not only the timer.</summary>
    [Fact]
    public void ARequest_WithdrawsAnAnswerOfAReplacedSession_BeforeItReads()
    {
        var rig = new Rig(backgroundReads: false) { Read = _ => Disabled };
        rig.Refresher.Request();
        rig.Pump();
        Assert.Equal([Disabled], rig.Applied);

        rig.Epoch++;
        rig.Read = _ => throw new InvalidOperationException("the new session's read failed");
        rig.Refresher.Request();
        rig.Pump();

        Assert.Equal(AiWorkerStatus.Unknown, rig.Applied[^1]);   // the old "switched off" is gone and does not return
        Assert.Contains("sessionWithdrawals=1", rig.Refresher.Measurements, StringComparison.Ordinal);
    }

    // ---- Finding 2: the epoch and the state the status call reads without the gate are ONE value --------------------------------------

    /// <summary>
    /// A reader that runs while the writer is between building the next value and publishing it sees the OLD value whole: the old state with the
    /// old epoch. (Before: the epoch was raised first and the state written after, so this reader saw the old state under the NEW epoch.)
    /// </summary>
    [Fact]
    public void TheTrackerSnapshot_IsOldWhole_OrNewWhole_NeverAMixture()
    {
        var tracker = new AiSessionTracker();
        tracker.InitSucceeded("models");                          // started, epoch 1
        AiSessionTracker.SessionState? duringStop = null;
        AiWorkerStatus? ownDuringStop = null;
        tracker.BeforePublish = () =>
        {
            duringStop = tracker.Snapshot;                        // the writer is paused here, mid-update
            ownDuringStop = tracker.OwnStatus();
        };
        tracker.Stopped();
        tracker.BeforePublish = null;

        Assert.NotNull(duringStop);
        Assert.True(duringStop!.Started && duringStop.Epoch == 1, $"a reader mid-update saw a mixture: {duringStop}");
        Assert.Null(ownDuringStop);                               // the old session's own answer: ask the module
        Assert.False(tracker.Snapshot.Started);
        Assert.Equal(2, tracker.Snapshot.Epoch);
        Assert.Equal(AiWorkerStatus.Unknown, tracker.OwnStatus());

        AiSessionTracker.SessionState? duringStart = null;
        tracker.BeforePublish = () => duringStart = tracker.Snapshot;
        tracker.InitSucceeded("models");
        tracker.BeforePublish = null;
        Assert.True(!duringStart!.Started && duringStart.Epoch == 2, $"a reader mid-update saw a mixture: {duringStart}");
        Assert.True(tracker.Snapshot.Started && tracker.Snapshot.Epoch == 3);
    }

    /// <summary>The same property under real concurrency: a started session always has an odd epoch, a stopped one an even one.</summary>
    [Fact]
    public void TheTrackerSnapshot_KeepsItsInvariant_UnderConcurrentReads()
    {
        var tracker = new AiSessionTracker();
        var stop = new ManualResetEventSlim();
        var violations = 0;
        var readers = Enumerable.Range(0, 3).Select(_ => Task.Run(() =>
        {
            while (!stop.IsSet)
            {
                var snapshot = tracker.Snapshot;
                if (snapshot.Started != (snapshot.Epoch % 2 == 1))
                {
                    Interlocked.Increment(ref violations);
                }
            }
        })).ToArray();

        for (var round = 0; round < 50_000; round++)
        {
            tracker.InitSucceeded("models");
            tracker.Stopped();
        }

        stop.Set();
        #pragma warning disable xUnit1031 // a bounded wait on real threads: what is measured here
        Assert.True(Task.WaitAll(readers, Long));
        #pragma warning restore xUnit1031
        Assert.Equal(0, violations);
    }

    // ---- GUI-C-192g (Codex #68): the completion callback is a boundary for what is already on screen ----------------------------------

    /// <summary>
    /// Neither <c>Request</c> nor the timer is called after the session moves: only an OLD read (started under the earlier session) comes back.
    /// The shown "Disabled" or "Active" must be taken back INSIDE that completion, and the new read that follows (failing or hanging) must not
    /// leave it up. Deterministic: the old read is held by an event and the only calls are the completion itself.
    /// </summary>
    [Theory]
    [InlineData(true, "fails")]
    [InlineData(true, "blocks")]
    [InlineData(false, "fails")]
    [InlineData(false, "blocks")]
    public void TheCompletionOfAnOldRead_WithdrawsTheShownAnswerOfTheReplacedSession(bool disabled, string newRead)
    {
        var shownBefore = disabled ? Disabled : Active;
        var rig = new Rig();
        var oldRead = new ManualResetEventSlim();
        var newReadMayEnd = new ManualResetEventSlim();
        var calls = 0;
        rig.Read = _ =>
        {
            switch (Interlocked.Increment(ref calls))
            {
                case 1:
                    return shownBefore;
                case 2:
                    Assert.True(oldRead.Wait(Long));
                    return shownBefore;                               // the old session's answer, late
                default:
                    if (newRead == "blocks")
                    {
                        newReadMayEnd.Wait(Long);
                        return null;
                    }

                    throw new InvalidOperationException("the new session's read failed");
            }
        };
        rig.Refresher.Request();
        rig.PumpUntil(() => rig.Applied.Count == 1, "The first answer never reached the screen.");
        Assert.Equal(shownBefore, rig.Applied[0]);

        rig.Refresher.Request();                                    // the read that will come back late (started under the OLD session)
        rig.PumpUntil(() => Volatile.Read(ref calls) == 2, "The second read never started.");
        rig.Epoch++;                                                // the session is replaced; no Reset, no Request, no timer tick from here on
        oldRead.Set();                                              // the old read comes back
        rig.PumpUntil(() => rig.Applied.Count >= 2, "The completion did not withdraw the shown answer.");

        Assert.Equal(AiWorkerStatus.Unknown, rig.Applied[1]);       // withdrawn, and by the completion itself (nothing else was called)
        rig.PumpUntil(() => Volatile.Read(ref calls) >= 3, "The new session's read never started.");
        rig.Pump();
        Assert.Equal(2, rig.Applied.Count);                         // the old answer was dropped and nothing else was applied
        Assert.DoesNotContain(rig.Applied.Skip(1), status => status.State is AiWorkerState.Disabled or AiWorkerState.Active);
        newReadMayEnd.Set();
    }

    /// <summary>
    /// The table of state transitions (192g): every path that can leave an answer of the OLD session on screen compares the session counter, or
    /// is the path that rewrites the screen itself. A source reading: it sees this tree's text only, and fails when a path loses its comparison.
    /// </summary>
    [Fact]
    public void EveryTransitionPath_ComparesTheSessionOrRewritesTheScreen()
    {
        var source = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/Services/AiBoneSuppressionStage.cs"));
        string Body(string signature)
        {
            var at = source.IndexOf(signature, StringComparison.Ordinal);
            Assert.True(at >= 0, $"{signature} was not found.");
            var next = source.IndexOf("\n    public ", at + signature.Length, StringComparison.Ordinal);
            var nextPrivate = source.IndexOf("\n    private ", at + signature.Length, StringComparison.Ordinal);
            var end = new[] { next, nextPrivate }.Where(i => i > 0).DefaultIfEmpty(source.Length).Min();
            return source[at..end];
        }

        // Request, CheckFreshness and the completion callback each compare the counter against what is shown.
        Assert.Contains("WithdrawIfTheSessionMoved();", Body("public void Request()"), StringComparison.Ordinal);
        Assert.Contains("WithdrawIfTheSessionMoved()", Body("public void CheckFreshness()"), StringComparison.Ordinal);
        Assert.Contains("WithdrawIfTheSessionMoved();", Body("private void Complete("), StringComparison.Ordinal);
        // Reset rewrites the screen itself (Unknown, stamped with the current counter); Show stamps every answer with the counter it belongs to.
        Assert.Contains("Show(AiWorkerStatus.Unknown);", Body("public void Reset()"), StringComparison.Ordinal);
        Assert.Contains("_shownEpoch = shownUnderEpoch ?? _epoch();", Body("private void Show("), StringComparison.Ordinal);
        // The withdrawal itself skips only "nothing shown" and "same counter".
        Assert.Contains("_shown == AiWorkerState.Unknown || _epoch() == _shownEpoch", Body("private bool WithdrawIfTheSessionMoved()"), StringComparison.Ordinal);
    }

    // ---- GUI-C-192d: an AI that was asked for and never answers is told too ---------------------------------------------------------

    /// <summary>A rig whose first read blocks until released, then answers <paramref name="answer"/>. The read is in flight when this returns.</summary>
    private static (Rig Rig, ManualResetEventSlim Release) BlockedFirstRead(AiWorkerStatus answer)
    {
        var rig = new Rig();
        var release = new ManualResetEventSlim();
        rig.Read = _ =>
        {
            Assert.True(release.Wait(Long));
            return answer;
        };
        rig.Refresher.Request();
        rig.PumpUntil(() => Volatile.Read(ref rig.Reads) == 1, "The first read never started.");
        return (rig, release);
    }

    /// <summary>The read of an AI session that was asked for and sits behind a silent worker: no notice before the bound, one at it, and it goes when the read returns.</summary>
    [Fact]
    public void AReadThatNeverReturnsFromTheStart_RaisesTheNeverConfirmedNotice_AtTheBound()
    {
        var (rig, release) = BlockedFirstRead(AiWorkerStatus.Unknown);

        rig.Now = AiStatusRefresher.DefaultActiveFreshFor - TimeSpan.FromTicks(1);
        rig.Refresher.CheckFreshness();
        Assert.Empty(rig.Applied);                       // just under the bound: nothing

        rig.Now = AiStatusRefresher.DefaultActiveFreshFor;
        rig.Refresher.CheckFreshness();
        Assert.Single(rig.Applied);
        Assert.Equal(AiWorkerState.Unconfirmed, rig.Applied[0].State);
        Assert.True(rig.Applied[0].NeverConfirmed);      // the notice says "since the start", not "since the last answer"
        Assert.Contains("no answer since the AI session started", AiBoneSuppressionStage.BannerFor(rig.Applied[0]), StringComparison.Ordinal);

        rig.Refresher.CheckFreshness();
        Assert.Single(rig.Applied);                      // raised once

        release.Set();                                   // the read returns "Unknown": the module has no session after all
        rig.PumpUntil(() => rig.Applied.Count == 2, "The returning read did not clear the notice.");
        Assert.Equal(AiWorkerStatus.Unknown, rig.Applied[1]);
    }

    /// <summary>A first read that answers inside the bound never raises a notice, however the answer reads.</summary>
    [Theory]
    [InlineData(false)]
    [InlineData(true)]
    public void AFirstReadThatAnswersInsideTheBound_RaisesNoNotice(bool active)
    {
        var (rig, release) = BlockedFirstRead(active ? Active : AiWorkerStatus.Unknown);

        rig.Now = AiStatusRefresher.DefaultActiveFreshFor - TimeSpan.FromTicks(1);
        rig.Refresher.CheckFreshness();
        release.Set();
        rig.PumpUntil(() => rig.Applied.Count == 1, "The first read never reached the screen.");
        rig.Now = TimeSpan.FromHours(1);
        rig.Refresher.CheckFreshness();

        Assert.DoesNotContain(rig.Applied, status => status.State == AiWorkerState.Unconfirmed && status.NeverConfirmed);
    }

    /// <summary>After a restart or a backend swap the wait for a first answer starts again: the Reset is the new start.</summary>
    [Fact]
    public void AfterAReset_TheNeverConfirmedWait_CountsFromTheReset()
    {
        var (rig, release) = BlockedFirstRead(Active);   // an old read still waiting behind the gate

        rig.Now = TimeSpan.FromSeconds(100);
        rig.Refresher.Reset();                           // the session is replaced; the old read is still blocked
        rig.Now = TimeSpan.FromSeconds(100) + AiStatusRefresher.DefaultActiveFreshFor - TimeSpan.FromTicks(1);
        rig.Refresher.CheckFreshness();
        Assert.DoesNotContain(rig.Applied, status => status.State == AiWorkerState.Unconfirmed);   // counted from the Reset, not from the old read

        rig.Now = TimeSpan.FromSeconds(100) + AiStatusRefresher.DefaultActiveFreshFor;
        rig.Refresher.CheckFreshness();
        Assert.Equal(AiWorkerState.Unconfirmed, rig.Applied[^1].State);
        Assert.True(rig.Applied[^1].NeverConfirmed);
        release.Set();
    }

    /// <summary>The measurements the native CI log carries: the slowest read and the longest wait of a shown answer, in milliseconds, and the notices.</summary>
    [Fact]
    public void TheMeasurements_NameTheSlowestRead_TheLongestAnswerGap_AndTheNotices()
    {
        var (rig, release) = BlockedFirstRead(Active);
        rig.Now = TimeSpan.FromMilliseconds(7000);
        release.Set();
        rig.PumpUntil(() => rig.Applied.Count == 1, "The read never reached the screen.");

        var line = rig.Refresher.Measurements;
        Assert.Contains("reads=1", line, StringComparison.Ordinal);
        Assert.Contains("maxReadMs=7000", line, StringComparison.Ordinal);
        Assert.Contains("maxAnswerGapMs=7000", line, StringComparison.Ordinal);
        Assert.Contains("boundMs=15000", line, StringComparison.Ordinal);
        Assert.Contains("noticesWithdrawn=0 noticesNeverConfirmed=0", line, StringComparison.Ordinal);

        rig.Read = _ => null;                            // reads now fail; the shown "Active" ages out
        rig.Now += AiStatusRefresher.DefaultActiveFreshFor;
        rig.Refresher.CheckFreshness();
        Assert.Contains("noticesWithdrawn=1", rig.Refresher.Measurements, StringComparison.Ordinal);
    }

    /// <summary>A switched-off worker stays switched off until a restart, so the bound never takes a "Disabled" back.</summary>
    [Fact]
    public void ADisabled_IsNeverWithdrawnByTheBound()
    {
        var rig = new Rig(backgroundReads: false) { Read = _ => Disabled };
        rig.Refresher.Request();
        rig.Pump();

        rig.Now = TimeSpan.FromHours(1);
        rig.Refresher.CheckFreshness();

        Assert.Equal([Disabled], rig.Applied);
    }

    [Fact]
    public void TheViewModel_RefreshesInTheBackground_ResetsAtEverySwap_AndStopsAtClose()
    {
        var source = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/ViewModels/MainWindowViewModel.cs"));

        // The refresher is built with a real background runner and the UI dispatcher, and the refresh path waits for nothing.
        Assert.Contains("new AiStatusRefresher(() => _backend, ReadAiWorkerStatus, ApplyAiWorkerStatus, work => Task.Run(work), PostToUi,", source, StringComparison.Ordinal);
        Assert.Contains("private void RefreshAiWorkerStatus() => AiStatus.Request();", source, StringComparison.Ordinal);
        Assert.Contains("_uiDispatcher.BeginInvoke(", source, StringComparison.Ordinal);

        // The swap: Reset BEFORE the new backend is built, a read requested AFTER the initialisation, in the path that is taken
        // whether the initialisation worked or not.
        var init = source.IndexOf("private void InitializeBackend()", StringComparison.Ordinal);
        var initEnd = source.IndexOf("private void RememberRecentRawFile", init, StringComparison.Ordinal);
        Assert.True(init >= 0 && initEnd > init, "InitializeBackend was not found.");
        var initBody = source[init..initEnd];
        var reset = initBody.IndexOf("AiStatus.Reset();", StringComparison.Ordinal);
        var build = initBody.IndexOf("_backend = _backendFactory(Settings);", StringComparison.Ordinal);
        var catchAt = initBody.IndexOf("catch (Exception ex)", StringComparison.Ordinal);
        var request = initBody.LastIndexOf("RefreshAiWorkerStatus();", StringComparison.Ordinal);
        Assert.True(reset >= 0 && build > reset && catchAt > build && request > catchAt,
            "InitializeBackend must reset before it builds the backend and request a read after the try/catch.");

        // The shutdown (GUI-C-186e moved it to the background): the status is reset when it starts, and read again when it is done.
        var begin = source.IndexOf("public void BeginShutdown(", StringComparison.Ordinal);
        var finish = source.IndexOf("private void FinishShutdown(", begin, StringComparison.Ordinal);
        var finishEnd = source.IndexOf("public void ShutdownBackendBlocking()", finish, StringComparison.Ordinal);
        Assert.True(begin >= 0 && finish > begin && finishEnd > finish, "BeginShutdown / FinishShutdown were not found.");
        Assert.Contains("AiStatus.Reset();", source[begin..finish], StringComparison.Ordinal);
        Assert.Contains("RefreshAiWorkerStatus();", source[finish..finishEnd], StringComparison.Ordinal);

        // The close: updates stop before the shutdown that precedes the close is started.
        var window = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/MainWindow.xaml.cs"));
        var stop = window.IndexOf("viewModel.StopAiStatusUpdates();", StringComparison.Ordinal);
        Assert.True(stop >= 0 && window.IndexOf("viewModel.BeginShutdown(", stop, StringComparison.Ordinal) > stop,
            "OnClosing must stop the status updates before it starts the shutdown.");

        // The old structure is gone: no bounded read, no timer, no retry numbers, anywhere in the app's sources.
        var banned = new[] { "TryWithLock", "StateReadWait", "ScheduleOnUiThread", "MaxRetries", "RetryDelay" };
        var root = Path.GetDirectoryName(BenchmarkRunnerServiceTests.ResolveRepositoryFile("CMakeLists.txt"))!;
        var appFiles = Directory.EnumerateFiles(Path.Combine(root, "gui", "ImageProcTest"), "*.cs", SearchOption.AllDirectories)
            .Where(path => !path.Contains($"{Path.DirectorySeparatorChar}obj{Path.DirectorySeparatorChar}") && !path.Contains($"{Path.DirectorySeparatorChar}bin{Path.DirectorySeparatorChar}"))
            .ToList();
        Assert.True(appFiles.Count > 50, "The scan found too few app sources to mean anything.");
        foreach (var path in appFiles)
        {
            var text = File.ReadAllText(path);
            foreach (var word in banned)
            {
                Assert.False(text.Contains(word, StringComparison.Ordinal), $"{Path.GetRelativePath(root, path)} still mentions '{word}'.");
            }
        }
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
