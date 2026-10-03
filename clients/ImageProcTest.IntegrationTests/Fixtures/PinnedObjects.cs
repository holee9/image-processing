namespace ImageProcTest.IntegrationTests.Fixtures;

/// <summary>
/// GUI-C-208 (D2): how many objects the runtime found pinned in a full, blocking, compacting collection (<see cref="GCMemoryInfo.PinnedObjectsCount"/>). A <c>GCHandle.Alloc(.., Pinned)</c>
/// that was never freed is one of them. The runtime pins a few objects of its own, so the number is compared with a baseline, never with zero.
/// </summary>
public static class PinnedObjects
{
    public static long AfterFullCollection()
    {
        GC.Collect(2, GCCollectionMode.Forced, blocking: true, compacting: true);
        GC.WaitForPendingFinalizers();
        GC.Collect(2, GCCollectionMode.Forced, blocking: true, compacting: true);
        return GC.GetGCMemoryInfo(GCKind.FullBlocking).PinnedObjectsCount;
    }
}
