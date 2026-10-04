// GUI-C-229f: "is this window the application's?" decided from process ids, with an unknown id never counting as a match. Pure code: no window, no Win32.
namespace ImageProcTest.E2ETests.Fixtures;

/// <summary>
/// A process id that could not be read comes back as 0 (UI Automation's default, or a failed <c>GetWindowThreadProcessId</c>). Two unknowns compare equal, so a plain <c>a == b</c> says "the application is in
/// front" when neither the foreground window's process nor the application's could be told: the keys would then go to a window that is not the application's (Codex #136). Every comparison that decides
/// whether keys, clicks or focus belong to the application goes through here, and only values that are known AND equal count.
/// </summary>
internal static class ProcessIdentity
{
    /// <summary>
    /// True only when the application's process id is known (> 0), a foreground window exists (handle not 0), <c>GetWindowThreadProcessId</c> answered (thread id not 0), the foreground window's process id is known
    /// (> 0), and it is the application's. Anything unknown or failed is false: the caller sends nothing.
    /// </summary>
    /// <param name="foregroundHandle">The handle <c>GetForegroundWindow</c> returned (0 = none).</param>
    /// <param name="threadId">What <c>GetWindowThreadProcessId</c> returned (0 = it failed).</param>
    /// <param name="foregroundProcessId">The process id it wrote out for that window.</param>
    /// <param name="applicationProcessId">The application's process id as UI Automation gave it (0 = could not be read).</param>
    internal static bool ForegroundIsTheApplication(long foregroundHandle, uint threadId, uint foregroundProcessId, int applicationProcessId) =>
        applicationProcessId > 0
        && foregroundHandle != 0
        && threadId != 0
        && foregroundProcessId > 0
        && foregroundProcessId == (uint)applicationProcessId;

    /// <summary>Two process ids that are both known (> 0) and equal. Used where an id comes from UI Automation (the focused element's process, a window's process) rather than from Win32.</summary>
    internal static bool SameKnownProcess(int processId, int applicationProcessId) =>
        processId > 0 && applicationProcessId > 0 && processId == applicationProcessId;
}
