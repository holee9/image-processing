// #170 (GUI-C-93): opt-in timing record of one app launch. Off unless XPE_C93_TIMING=1; it only
// observes, and the fixture behaves the same with it on or off.
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace ImageProcTest.E2ETests.Fixtures;

/// <summary>
/// Records, for one launch: when the process started (OS start time), when <c>GetMainWindow</c>
/// returned, when the probe first read <c>AutomationId</c> and what it got, and when a window of the
/// process first became the foreground window.
///
/// <para>The foreground is taken from a <c>SetWinEventHook(EVENT_SYSTEM_FOREGROUND)</c> hook on a
/// background thread with its own message loop, stamped with <see cref="DateTime.UtcNow"/> in the
/// callback. Polling was measured first (GUI-C-93 smoke): <c>Thread.Sleep(1)</c> gave a 15.5 ms median
/// interval, the default timer tick, and raising the timer resolution would change timing for every
/// process, the app included. The hook changes nothing for the app and fires on the change itself.</para>
/// </summary>
internal sealed class LaunchTimingRecorder : IDisposable
{
    public const string Variable = "XPE_C93_TIMING";

    private const uint EventSystemForeground = 0x0003;
    private const uint WinEventOutOfContext = 0x0000;
    private const uint WmQuit = 0x0012;

    private readonly Thread _thread;
    private readonly List<(DateTime Utc, uint Pid, uint EventMs)> _events = [];
    private readonly ManualResetEventSlim _ready = new();
    private WinEventProc? _callback;   // held so the delegate outlives the hook
    private uint _threadId;

    private LaunchTimingRecorder()
    {
        _thread = new Thread(Run) { IsBackground = true, Name = "C93 foreground hook" };
        _thread.Start();
        _ready.Wait(2000);
    }

    /// <summary>Null unless <see cref="Variable"/> is <c>1</c>.</summary>
    public static LaunchTimingRecorder? StartIfEnabled() =>
        Environment.GetEnvironmentVariable(Variable) == "1" ? new LaunchTimingRecorder() : null;

    public int ProcessId { get; set; }

    public DateTime MainWindowReturnedUtc { get; set; }

    public DateTime QueryUtc { get; set; }

    public string QueryResult { get; set; } = string.Empty;

    public string Framework { get; set; } = string.Empty;

    private void Run()
    {
        _threadId = GetCurrentThreadId();
        _callback = (hook, evt, hwnd, idObject, idChild, thread, time) =>
        {
            var now = DateTime.UtcNow;
            GetWindowThreadProcessId(hwnd, out var pid);
            lock (_events) _events.Add((now, pid, time));
        };
        var hook = SetWinEventHook(EventSystemForeground, EventSystemForeground, IntPtr.Zero, _callback, 0, 0, WinEventOutOfContext);
        _ready.Set();
        while (GetMessage(out var msg, IntPtr.Zero, 0, 0) > 0)
        {
            TranslateMessage(ref msg);
            DispatchMessage(ref msg);
        }

        if (hook != IntPtr.Zero) UnhookWinEvent(hook);
    }

    /// <summary>Stops the hook and prints one XPE-C93-TIMING line.</summary>
    public void Dispose()
    {
        PostThreadMessage(_threadId, WmQuit, IntPtr.Zero, IntPtr.Zero);
        _thread.Join(1000);

        string start;
        try { start = Process.GetProcessById(ProcessId).StartTime.ToUniversalTime().ToString("o"); }
        catch (Exception ex) { start = $"<{ex.GetType().Name}>"; }

        string foreground;
        int eventCount;
        lock (_events)
        {
            eventCount = _events.Count;
            var hit = _events.FirstOrDefault(e => e.Pid == (uint)ProcessId);
            foreground = hit.Utc == default ? "never" : hit.Utc.ToString("o");
        }

        Console.WriteLine(
            $"XPE-C93-TIMING pid={ProcessId} start={start} mainWindow={MainWindowReturnedUtc:o} " +
            $"query={QueryUtc:o} result={QueryResult} framework={Framework} " +
            $"foreground={foreground} fgEvents={eventCount} source=winevent");
    }

    private delegate void WinEventProc(IntPtr hook, uint evt, IntPtr hwnd, int idObject, int idChild, uint thread, uint time);

    [StructLayout(LayoutKind.Sequential)]
    private struct Msg
    {
        public IntPtr Hwnd;
        public uint Message;
        public IntPtr WParam;
        public IntPtr LParam;
        public uint Time;
        public int X;
        public int Y;
    }

    [DllImport("user32.dll")]
    private static extern IntPtr SetWinEventHook(uint min, uint max, IntPtr module, WinEventProc proc, uint pid, uint thread, uint flags);

    [DllImport("user32.dll")]
    private static extern bool UnhookWinEvent(IntPtr hook);

    [DllImport("user32.dll")]
    private static extern int GetMessage(out Msg msg, IntPtr hwnd, uint min, uint max);

    [DllImport("user32.dll")]
    private static extern bool TranslateMessage(ref Msg msg);

    [DllImport("user32.dll")]
    private static extern IntPtr DispatchMessage(ref Msg msg);

    [DllImport("user32.dll")]
    private static extern bool PostThreadMessage(uint thread, uint msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

    [DllImport("kernel32.dll")]
    private static extern uint GetCurrentThreadId();
}
