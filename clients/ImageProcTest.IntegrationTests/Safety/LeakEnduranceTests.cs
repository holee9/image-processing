// AC-7: 1000-cycle init/shutdown no leak.
using ImageProcTest.IntegrationTests.Fixtures;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.Safety;

/// <summary>
/// Endurance test: 1000 consecutive xpe_init / xpe_shutdown cycles without
/// observable managed memory leak.
/// Covers REQ-GUI-IT-010, REQ-GUI-IT-051, AC-7.
/// </summary>
[Trait("Category", "Safety")]
[Collection(NativeLibraryCollection.Name)]
public sealed class LeakEnduranceTests
{
    private readonly NativeLibraryFixture _fixture;

    // Configurable threshold via env var, default 5 MiB for GC + 20 MiB for WorkingSet.
    private static readonly long GcLimitBytes = long.TryParse(
        Environment.GetEnvironmentVariable("XPE_GUI_IT_LEAK_LIMIT_MIB"), out var mib)
        ? mib * 1024 * 1024
        : 5L * 1024 * 1024; // 5 MiB

    private static readonly long WsLimitBytes = 20L * 1024 * 1024; // 20 MiB

    public LeakEnduranceTests(NativeLibraryFixture fixture)
    {
        _fixture = fixture;
    }

    /// <summary>
    /// REQ-GUI-IT-051: 1000 init/shutdown cycles — GC memory delta &lt; 5 MiB,
    /// WorkingSet delta &lt; 20 MiB. Must complete within 90 seconds.
    /// </summary>
    [SkippableFact]
    public void InitShutdown_1000Cycles_NoLeak()
    {
        SkipHelper.SkipIf(!_fixture.IsAvailable, _fixture.SkipReason);

        // Warm-up: one cycle before measurement.
        XpeCommonNative.xpe_init(null);
        XpeCommonNative.xpe_shutdown();

        GC.Collect();
        GC.WaitForPendingFinalizers();
        GC.Collect();

        var pinnedBefore = PinnedObjects.AfterFullCollection();
        var gcBefore = GC.GetTotalMemory(forceFullCollection: true);
        var wsBefore = System.Diagnostics.Process.GetCurrentProcess().WorkingSet64;

        for (var i = 0; i < 1000; i++)
        {
            var initResult = XpeCommonNative.xpe_init(null);
            // Allow OK or NOT_INITIALIZED (state machine may differ across cycles).
            Assert.True(
                initResult == XpeCommonNative.XpeErrorCode.OK ||
                initResult == XpeCommonNative.XpeErrorCode.NOT_INITIALIZED,
                $"Unexpected init result on cycle {i}: {initResult}");

            XpeCommonNative.xpe_shutdown();
        }

        GC.Collect();
        GC.WaitForPendingFinalizers();
        GC.Collect();

        var gcAfter = GC.GetTotalMemory(forceFullCollection: true);
        var wsAfter = System.Diagnostics.Process.GetCurrentProcess().WorkingSet64;

        var pinnedAfter = PinnedObjects.AfterFullCollection();
        Assert.True(pinnedAfter <= pinnedBefore, $"the 1000 init/shutdown cycles left pinned objects behind: {pinnedBefore} before, {pinnedAfter} after a full collection");

        var gcDelta = gcAfter - gcBefore;
        var wsDelta = wsAfter - wsBefore;

        Assert.True(gcDelta < GcLimitBytes,
            $"GC memory delta {gcDelta / 1024 / 1024.0:F1} MiB exceeds limit {GcLimitBytes / 1024 / 1024} MiB");
        Assert.True(wsDelta < WsLimitBytes,
            $"WorkingSet delta {wsDelta / 1024 / 1024.0:F1} MiB exceeds limit {WsLimitBytes / 1024 / 1024} MiB");
    }

    /// <summary>
    /// REQ-GUI-IT-010: no pinned handle is left outstanding. GUI-C-208 (D2): this used to be named for that and asserted only that the managed heap was under 200 MiB, which no
    /// pinned handle changes. It now reads the runtime's own pinned-object count after a full blocking collection and compares it with the count taken when the fixture was created.
    /// It runs at whatever point xUnit schedules it, so it sees what EARLIER tests left; the 1000-cycle test above checks its own loop directly. The instrument is held by
    /// <see cref="TheInstrument_CountsAPinnedHandle_AndStopsCountingItOnceFreed"/>.
    /// </summary>
    [SkippableFact]
    public void PinnedObjects_AtThisPoint_AreNoMoreThanWhenTheFixtureWasCreated()
    {
        SkipHelper.SkipIf(!_fixture.IsAvailable, _fixture.SkipReason);

        var now = PinnedObjects.AfterFullCollection();
        Assert.True(now <= _fixture.PinnedObjectsAtStart,
            $"{now} objects are pinned after a full collection, {_fixture.PinnedObjectsAtStart} were when the fixture was created: an earlier test left a GCHandle.Alloc(Pinned) outstanding");
    }

    /// <summary>The control for the test above: a pinned handle raises the count and freeing it brings it back, so "no more than at the start" can fail.</summary>
    [Fact]
    public void TheInstrument_CountsAPinnedHandle_AndStopsCountingItOnceFreed()
    {
        var baseline = PinnedObjects.AfterFullCollection();
        var handle = System.Runtime.InteropServices.GCHandle.Alloc(new byte[64], System.Runtime.InteropServices.GCHandleType.Pinned);
        long whilePinned;
        try
        {
            whilePinned = PinnedObjects.AfterFullCollection();
        }
        finally
        {
            handle.Free();
        }

        var afterFree = PinnedObjects.AfterFullCollection();
        Assert.True(whilePinned > baseline, $"a pinned GCHandle did not raise the count: {baseline} -> {whilePinned}");
        Assert.True(afterFree <= baseline, $"freeing the handle did not bring the count back: {baseline} -> {afterFree}");
    }
}
