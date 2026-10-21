// QA-A-279c (W15b diagnostic, Reviewer #267/#268): an observation-only record of the display-delay fault's sleeps.
#if XPE_TEST_FAULTS   // GUI-C-193: test-build only. This type is not in a shipped build; it has no WPF dependency so a plain net8.0 test project can link it.
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;

namespace ImageProcTest.Services;

/// <summary>Names shared by the recorder (in the app), the test that launches the app, and the reader.</summary>
internal static class FaultDelayLog
{
    /// <summary>The BASE path of the log. Each wrapper writes its own file <c>base + "." + wrapperId + <see cref="FileSuffix"/></c>, so two wrappers in one process never share or overwrite a file.</summary>
    public const string EnvPath = "XPE_TEST_FAULT_LOG";

    /// <summary>A run number the test chose; written in the header so a file of an earlier run is told apart (stale).</summary>
    public const string EnvRun = "XPE_TEST_FAULT_RUN";

    /// <summary>The name the test gave this app (for example <c>delay-1500</c>).</summary>
    public const string EnvApp = "XPE_TEST_FAULT_APP";

    public const string FileSuffix = ".fdl";
    public const string ChainOverload = "chain";
    public const string DisplayInputOverload = "displayInput";
    public const int DefaultCapacity = 256;
}

/// <summary>
/// Records each injected sleep in a fixed-size memory table and writes the table to a file ONCE, after the observation is over.
///
/// <para><b>What it costs while measuring.</b> Per sleep: two <see cref="Stopwatch.GetTimestamp"/> calls and a few array writes, plus one null check at the caller. That is not zero and it has not been
/// measured; nothing here claims a number. There is no thread, no lock, no allocation and no file access while the table fills (the header is written when the wrapper is created, before any measured open).</para>
///
/// <para><b>File shape.</b> One header line (<c>H</c>), one line per finished sleep (<c>E</c>), one line per slot that was reserved but not finished at export (<c>P</c>), a footer (<c>F</c>) and a last line
/// <c>END</c>. A file without <c>END</c> is incomplete by definition (see <see cref="FaultDelayLogReader"/>).</para>
///
/// <para><b>Errors.</b> Every recording or export failure is caught and counted; none reaches the caller, so the backend call and any exception it raises are never replaced.</para>
/// </summary>
internal sealed class FaultDelayRecorder
{
    private const byte ChainCode = 1;
    private const byte DisplayInputCode = 2;

    private readonly string _file;
    private readonly string _run;
    private readonly string _app;
    private readonly string _wrapper;
    private readonly int _capacity;
    private readonly Func<int> _calls;
    private readonly long[] _start;
    private readonly long[] _end;
    private readonly int[] _call;
    private readonly int[] _requested;
    private readonly int[] _thread;
    private readonly int[] _state;   // 0 reserved, 1 started, 2 finished
    private readonly byte[] _overload;
    private readonly ManualResetEventSlim _done = new(false);
    private int _next;
    private int _dropped;
    private int _errors;
    private int _late;
    private int _sealed;
    private int _exported;

    private FaultDelayRecorder(string basePath, string run, string app, int armedMs, int capacity, Func<int> calls)
    {
        _run = Token(run);
        _app = Token(app);
        _capacity = capacity;
        _calls = calls;
        _wrapper = Guid.NewGuid().ToString("N");
        _start = new long[capacity];
        _end = new long[capacity];
        _call = new int[capacity];
        _requested = new int[capacity];
        _thread = new int[capacity];
        _state = new int[capacity];
        _overload = new byte[capacity];
        _file = basePath + "." + _wrapper + FaultDelayLog.FileSuffix;

        var directory = Path.GetDirectoryName(_file);
        if (!string.IsNullOrEmpty(directory))
        {
            Directory.CreateDirectory(directory);
        }

        var before = Stopwatch.GetTimestamp();
        var utc = DateTime.UtcNow;
        var after = Stopwatch.GetTimestamp();
        var header = string.Create(CultureInfo.InvariantCulture,
            $"H v=1 run={_run} app={_app} pid={Environment.ProcessId} wrapper={_wrapper} freq={Stopwatch.Frequency} armed={armedMs} capacity={capacity} utc={utc:O} ticks0={before} ticks1={after}\n");
        using var stream = new FileStream(_file, FileMode.CreateNew, FileAccess.Write, FileShare.Read);
        var bytes = Encoding.ASCII.GetBytes(header);
        stream.Write(bytes, 0, bytes.Length);
        stream.Flush(true);
    }

    /// <summary>The file this recorder writes.</summary>
    public string FilePath => _file;

    /// <summary>
    /// A recorder when <see cref="FaultDelayLog.EnvPath"/> names a path, else null (the existing path, nothing created). A failure to create the header file also gives null: the app then starts
    /// normally and the reader finds no file (<c>INCOMPLETE: no-file</c>).
    /// </summary>
    public static FaultDelayRecorder? TryCreate(int armedMs, Func<int> calls, Func<string, string?>? environment = null, int capacity = FaultDelayLog.DefaultCapacity)
    {
        try
        {
            environment ??= Environment.GetEnvironmentVariable;
            var basePath = environment(FaultDelayLog.EnvPath);
            if (string.IsNullOrWhiteSpace(basePath))
            {
                return null;
            }

            var recorder = new FaultDelayRecorder(basePath, environment(FaultDelayLog.EnvRun) ?? "-", environment(FaultDelayLog.EnvApp) ?? "-", armedMs, capacity, calls);
            AppDomain.CurrentDomain.ProcessExit += (_, _) => recorder.Export("ProcessExit");
            return recorder;
        }
        catch (Exception)
        {
            return null;
        }
    }

    /// <summary>Reserves a slot and stamps its start. Returns the slot, or -1 when nothing was recorded (table full, sealed, or an internal error - each counted).</summary>
    public int Begin(int call, string overload, int requestedMs)
    {
        try
        {
            if (!IsOpenForBegin())
            {
                Interlocked.Increment(ref _late);
                return -1;
            }

            return Reserve(call, overload, requestedMs);
        }
        catch (Exception)
        {
            Interlocked.Increment(ref _errors);
            return -1;
        }
    }

    /// <summary>Step 1 of <see cref="Begin"/>: the table has not been sealed yet. Separate from <see cref="Reserve"/> so that a test can place an export exactly between the two (no hook in the measured path).</summary>
    internal bool IsOpenForBegin() => Volatile.Read(ref _sealed) == 0;

    /// <summary>
    /// Step 2 of <see cref="Begin"/>: takes a slot, then looks at the seal AGAIN. A seal that arrived between step 1 and the slot is counted as late and nothing is recorded; the slot, if the export had
    /// already taken its snapshot over it, is written there as pending, and if the snapshot was taken before the slot existed the call is a call without a sleep in the file (the reader calls that partial).
    /// No lock is taken.
    /// </summary>
    internal int Reserve(int call, string overload, int requestedMs)
    {
        var slot = Interlocked.Increment(ref _next) - 1;
        if (Volatile.Read(ref _sealed) != 0)
        {
            Interlocked.Increment(ref _late);
            return -1;
        }

        if (slot >= _capacity)
        {
            Interlocked.Increment(ref _dropped);
            return -1;
        }

        _call[slot] = call;
        _overload[slot] = overload == FaultDelayLog.ChainOverload ? ChainCode : DisplayInputCode;
        _requested[slot] = requestedMs;
        _thread[slot] = Environment.CurrentManagedThreadId;
        _start[slot] = Stopwatch.GetTimestamp();
        Volatile.Write(ref _state[slot], 1);
        return slot;
    }

    /// <summary>How many Begin calls found the table already sealed (in memory; the file's footer has the value at the moment it was written).</summary>
    internal int LateCount => Volatile.Read(ref _late);

    /// <summary>Stamps the end of the sleep in <paramref name="slot"/> and publishes it (finished). A negative slot is ignored.</summary>
    public void End(int slot)
    {
        if (slot < 0)
        {
            return;
        }

        try
        {
            _end[slot] = Stopwatch.GetTimestamp();
            Volatile.Write(ref _state[slot], 2);
        }
        catch (Exception)
        {
            Interlocked.Increment(ref _errors);
        }
    }

    /// <summary>
    /// Seals the table and appends it to the file with the footer and <c>END</c>. Only the first caller writes; a concurrent caller waits up to 2 s for the first to finish and returns false. A slot that
    /// is reserved or started but not finished at the moment of sealing is written as <c>P</c> and counted in <c>pending</c>: such a file is never complete. Returns true when this call wrote the whole file.
    /// </summary>
    public bool Export(string by)
    {
        if (Interlocked.CompareExchange(ref _exported, 1, 0) != 0)
        {
            _done.Wait(TimeSpan.FromSeconds(2));
            return false;
        }

        try
        {
            Volatile.Write(ref _sealed, 1);
            var count = Math.Min(Volatile.Read(ref _next), _capacity);
            var text = new StringBuilder();
            var events = 0;
            var pending = 0;
            for (var i = 0; i < count; i++)
            {
                if (Volatile.Read(ref _state[i]) != 2)
                {
                    pending++;
                    text.Append(CultureInfo.InvariantCulture, $"P order={i} state={Volatile.Read(ref _state[i])}\n");
                    continue;
                }

                events++;
                text.Append(CultureInfo.InvariantCulture,
                    $"E order={i} call={_call[i]} ov={(_overload[i] == ChainCode ? FaultDelayLog.ChainOverload : FaultDelayLog.DisplayInputOverload)} req={_requested[i]} t0={_start[i]} t1={_end[i]} thr={_thread[i]}\n");
            }

            // The call count is read AFTER the slots were looked at: a call that arrived while the export ran is then counted as a call without a sleep in the file, which the reader
            // calls partial (INCOMPLETE). Read the other way round it could be an event without a call, which is also refused (inconsistent). Either way the file is never complete by luck.
            text.Append(CultureInfo.InvariantCulture,
                $"F calls={_calls()} events={events} pending={pending} dropped={Volatile.Read(ref _dropped)} errors={Volatile.Read(ref _errors)} late={Volatile.Read(ref _late)} by={Token(by)}\nEND\n");
            var bytes = Encoding.ASCII.GetBytes(text.ToString());
            using var stream = new FileStream(_file, FileMode.Append, FileAccess.Write, FileShare.Read);
            stream.Write(bytes, 0, bytes.Length);
            stream.Flush(true);
            return true;
        }
        catch (Exception)
        {
            Interlocked.Increment(ref _errors);
            return false;
        }
        finally
        {
            _done.Set();
        }
    }

    private static string Token(string value)
    {
        var text = string.IsNullOrWhiteSpace(value) ? "-" : value.Trim();
        return new string(text.Select(c => char.IsWhiteSpace(c) || c == '=' ? '_' : c).ToArray());
    }
}

internal sealed record FaultDelayHeader(string Run, string App, int Pid, string Wrapper, long Frequency, int ArmedMs, int Capacity, string Utc, long Ticks0, long Ticks1);

internal sealed record FaultDelayEvent(int Order, int Call, string Overload, int RequestedMs, long StartTicks, long EndTicks, int Thread);

internal sealed record FaultDelayFooter(int Calls, int Events, int Pending, int Dropped, int Errors, int Late, string By);

internal enum FaultDelayStatus
{
    /// <summary>The file is whole and consistent and holds at least one sleep. This says the FILE is whole, not what the sleeps prove.</summary>
    Complete,

    /// <summary>The file is whole and consistent and holds no sleep, and that is what the settings predict (nothing armed, or no display call happened).</summary>
    CompleteEmpty,

    /// <summary>The file is whole, but its content disagrees with the settings (for example a sleep was requested and called for, and none was recorded). Not a conclusion about the cause: the injection OR the recording may be at fault.</summary>
    Anomaly,

    /// <summary>No file, no header, a file of another run, no footer, a loss, or an inconsistency: nothing can be said about the sleeps.</summary>
    Incomplete,
}

internal sealed record FaultDelayReading(FaultDelayStatus Status, string Reason, FaultDelayHeader? Header, IReadOnlyList<FaultDelayEvent> Events, FaultDelayFooter? Footer, string? File)
{
    /// <summary>One line for the test output.</summary>
    public string Describe() => Status switch
    {
        FaultDelayStatus.Complete => $"COMPLETE: {Reason}",
        FaultDelayStatus.CompleteEmpty => $"COMPLETE-EMPTY: {Reason}",
        FaultDelayStatus.Anomaly => $"ANOMALY: {Reason}",
        _ => $"INCOMPLETE: {Reason}",
    };
}

/// <summary>Reads and classifies the files a <see cref="FaultDelayRecorder"/> wrote. A reading that is not <see cref="FaultDelayStatus.Complete"/> or <see cref="FaultDelayStatus.CompleteEmpty"/> never supports a statement about the sleeps.</summary>
internal static class FaultDelayLogReader
{
    /// <summary>
    /// Reads the one file written for <paramref name="basePath"/>. More than one wrapper file is <c>INCOMPLETE: multiple-wrappers</c> (the attribution would be ambiguous), none is <c>no-file</c>.
    /// </summary>
    public static FaultDelayReading Read(string basePath, string expectedRun, string expectedApp, int expectedArmedMs, int? expectedPid)
    {
        string[] files;
        try
        {
            var directory = Path.GetDirectoryName(basePath);
            files = string.IsNullOrEmpty(directory) || !Directory.Exists(directory)
                ? []
                : Directory.GetFiles(directory, Path.GetFileName(basePath) + ".*" + FaultDelayLog.FileSuffix);
        }
        catch (Exception ex)
        {
            return Incomplete($"unreadable-directory ({ex.GetType().Name})", null, null);
        }

        if (files.Length == 0)
        {
            return Incomplete("no-file (variable not passed, recording off, or the header could not be written)", null, null);
        }

        if (files.Length > 1)
        {
            return Incomplete($"multiple-wrappers={files.Length} (one file per wrapper; the attribution is ambiguous)", null, null);
        }

        string[] lines;
        try
        {
            using var stream = new FileStream(files[0], FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
            using var reader = new StreamReader(stream, Encoding.ASCII);
            lines = reader.ReadToEnd().Split('\n', StringSplitOptions.RemoveEmptyEntries);
        }
        catch (Exception ex)
        {
            return Incomplete($"unreadable-file ({ex.GetType().Name})", null, files[0]);
        }

        return Classify(lines, files[0], expectedRun, expectedApp, expectedArmedMs, expectedPid);
    }

    /// <summary>The classification of the lines of one file, separated from the file access so a test can feed it any content.</summary>
    public static FaultDelayReading Classify(IReadOnlyList<string> lines, string? file, string expectedRun, string expectedApp, int expectedArmedMs, int? expectedPid)
    {
        FaultDelayHeader? header = null;
        FaultDelayFooter? footer = null;
        var events = new List<FaultDelayEvent>();
        var sawEnd = false;
        var pendingLines = 0;
        try
        {
            for (var i = 0; i < lines.Count; i++)
            {
                var line = lines[i].TrimEnd('\r');
                if (i == 0)
                {
                    if (!line.StartsWith("H ", StringComparison.Ordinal))
                    {
                        return Incomplete("no-header", null, file);
                    }

                    var h = Pairs(line);
                    header = new FaultDelayHeader(h["run"], h["app"], Int(h["pid"]), h["wrapper"], Long(h["freq"]), Int(h["armed"]), Int(h["capacity"]), h["utc"], Long(h["ticks0"]), Long(h["ticks1"]));
                    continue;
                }

                if (sawEnd)
                {
                    return Incomplete("unparseable: text after END", header, file);
                }

                if (line == "END")
                {
                    sawEnd = true;
                }
                else if (line.StartsWith("E ", StringComparison.Ordinal))
                {
                    var e = Pairs(line);
                    events.Add(new FaultDelayEvent(Int(e["order"]), Int(e["call"]), e["ov"], Int(e["req"]), Long(e["t0"]), Long(e["t1"]), Int(e["thr"])));
                }
                else if (line.StartsWith("P ", StringComparison.Ordinal))
                {
                    pendingLines++;
                }
                else if (line.StartsWith("F ", StringComparison.Ordinal))
                {
                    var f = Pairs(line);
                    footer = new FaultDelayFooter(Int(f["calls"]), Int(f["events"]), Int(f["pending"]), Int(f["dropped"]), Int(f["errors"]), Int(f["late"]), f["by"]);
                }
                else
                {
                    return Incomplete($"unparseable-line {i + 1}", header, file);
                }
            }
        }
        catch (Exception ex) when (ex is KeyNotFoundException or FormatException or OverflowException)
        {
            return Incomplete($"no-header or unparseable ({ex.GetType().Name})", header, file);
        }

        if (header is null)
        {
            return Incomplete("no-header", null, file);
        }

        if (header.Run != expectedRun || header.App != expectedApp || (expectedPid is { } pid && header.Pid != pid))
        {
            return Incomplete($"stale (header run={header.Run} app={header.App} pid={header.Pid}; expected run={expectedRun} app={expectedApp} pid={expectedPid?.ToString(CultureInfo.InvariantCulture) ?? "any"})", header, file);
        }

        if (header.ArmedMs != expectedArmedMs)
        {
            return Incomplete($"config-mismatch (header armed={header.ArmedMs} ms, the test requested {expectedArmedMs} ms)", header, file);
        }

        if (header.Frequency <= 0)
        {
            return Incomplete($"bad-frequency ({header.Frequency})", header, file);
        }

        if (footer is null || !sawEnd)
        {
            return Incomplete("no-footer (not exported, killed first, or the export failed part-way)", header, file);
        }

        var problems = new List<string>();
        if (footer.Dropped > 0) problems.Add($"dropped={footer.Dropped}");
        if (footer.Errors > 0) problems.Add($"errors={footer.Errors}");
        if (footer.Pending > 0 || pendingLines > 0) problems.Add($"pending={Math.Max(footer.Pending, pendingLines)} (a sleep was still under way at export)");
        if (footer.Late > 0) problems.Add($"late={footer.Late}");
        if (problems.Count > 0)
        {
            return Incomplete(string.Join(", ", problems), header, file, events, footer);
        }

        if (events.Count != footer.Events)
        {
            return Incomplete($"inconsistent (events in file {events.Count}, footer says {footer.Events})", header, file, events, footer);
        }

        if (events.Select(e => e.Call).Distinct().Count() != events.Count || events.Zip(events.Skip(1), (a, b) => b.Order <= a.Order).Any(bad => bad))
        {
            return Incomplete("inconsistent (duplicate call numbers or order not increasing)", header, file, events, footer);
        }

        if (events.Any(e => e.StartTicks <= 0 || e.EndTicks < e.StartTicks))
        {
            return Incomplete("inconsistent (an interval that is not positive in time)", header, file, events, footer);
        }

        if (footer.Events > footer.Calls)
        {
            return Incomplete($"inconsistent (events {footer.Events} exceed calls {footer.Calls})", header, file, events, footer);
        }

        if (events.Count == 0)
        {
            if (header.ArmedMs == 0)
            {
                return new FaultDelayReading(FaultDelayStatus.CompleteEmpty, $"armed=0 ms, no sleep expected; calls={footer.Calls}", header, events, footer, file);
            }

            return footer.Calls == 0
                ? new FaultDelayReading(FaultDelayStatus.CompleteEmpty, $"armed={header.ArmedMs} ms, but no display pipeline call happened (calls=0)", header, events, footer, file)
                : new FaultDelayReading(FaultDelayStatus.Anomaly,
                    $"SLEEP-RECORD-ABSENT-VS-REQUEST: armed={header.ArmedMs} ms and calls={footer.Calls}, but no sleep was recorded. The file itself is whole; the injection or the recording is at fault and this log cannot say which - compare with another source",
                    header, events, footer, file);
        }

        if (header.ArmedMs == 0)
        {
            return new FaultDelayReading(FaultDelayStatus.Anomaly, $"armed=0 ms but {events.Count} sleep(s) recorded", header, events, footer, file);
        }

        if (events.Any(e => e.Call < 1 || e.Call > footer.Calls))
        {
            return Incomplete($"inconsistent (a call number outside 1..{footer.Calls})", header, file, events, footer);
        }

        // QA-A-279d (Reviewer #269 F269-1): with a delay armed and nothing else armed, every counted display call sleeps once. Some sleeps recorded and some not is a PARTIAL record, whatever the
        // rest of the file looks like - a call that arrived while the export ran lands here, and so would a counted call that failed before its sleep (a mixed fault, which this log does not
        // describe). It is not COMPLETE, and the numbers of the calls without a sleep are named.
        if (events.Count < footer.Calls)
        {
            var missing = Enumerable.Range(1, footer.Calls).Except(events.Select(e => e.Call)).ToList();
            return Incomplete($"partial (sleeps recorded for {events.Count} of {footer.Calls} calls; no sleep for call number(s) {string.Join(",", missing.Take(10))}{(missing.Count > 10 ? ",..." : string.Empty)})", header, file, events, footer);
        }

        if (events.Any(e => e.RequestedMs != header.ArmedMs))
        {
            return new FaultDelayReading(FaultDelayStatus.Anomaly, $"requestedMs differs from the header's armed={header.ArmedMs} ms in {events.Count(e => e.RequestedMs != header.ArmedMs)} event(s)", header, events, footer, file);
        }

        return new FaultDelayReading(FaultDelayStatus.Complete, $"events={events.Count} calls={footer.Calls} (the file is whole; it does not say what the sleeps did to the proxy)", header, events, footer, file);
    }

    private static FaultDelayReading Incomplete(string reason, FaultDelayHeader? header, string? file, IReadOnlyList<FaultDelayEvent>? events = null, FaultDelayFooter? footer = null) =>
        new(FaultDelayStatus.Incomplete, reason, header, events ?? [], footer, file);

    private static Dictionary<string, string> Pairs(string line)
    {
        var result = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var token in line.Split(' ', StringSplitOptions.RemoveEmptyEntries).Skip(1))
        {
            var at = token.IndexOf('=');
            if (at <= 0)
            {
                throw new FormatException("token without '='");
            }

            result[token[..at]] = token[(at + 1)..];
        }

        return result;
    }

    private static int Int(string value) => int.Parse(value, NumberStyles.Integer, CultureInfo.InvariantCulture);

    private static long Long(string value) => long.Parse(value, NumberStyles.Integer, CultureInfo.InvariantCulture);
}

internal enum FaultDelayPlacement
{
    /// <summary>Starts and ends inside the window the test watched for one open. That window is wider than the app's proxy interval, so this does not mean "inside the proxy".</summary>
    Inside,
    StraddleStart,
    StraddleEnd,
    Spans,
    Outside,
}

/// <summary>
/// What one watched window of one open contains. <paramref name="InsideSumMs"/> is the plain sum of the events placed Inside; it is raw data and is never subtracted from the proxy
/// (QA-A-279d, Reviewer #269 F269-2: no "proxy minus sleep sum" figure exists in this code).
/// </summary>
internal sealed record FaultDelayWindow(int Inside, int StraddleStart, int StraddleEnd, int Spans, double InsideSumMs, bool Overlap);

internal static class FaultDelayAttribution
{
    public static FaultDelayPlacement Place(FaultDelayEvent e, long windowStart, long windowEnd)
    {
        var startsIn = e.StartTicks >= windowStart && e.StartTicks <= windowEnd;
        var endsIn = e.EndTicks >= windowStart && e.EndTicks <= windowEnd;
        if (startsIn && endsIn) return FaultDelayPlacement.Inside;
        if (!startsIn && endsIn) return FaultDelayPlacement.StraddleStart;
        if (startsIn) return FaultDelayPlacement.StraddleEnd;
        return e.StartTicks < windowStart && e.EndTicks > windowEnd ? FaultDelayPlacement.Spans : FaultDelayPlacement.Outside;
    }

    public static double ElapsedMs(FaultDelayEvent e, long frequency) => (e.EndTicks - e.StartTicks) * 1000.0 / frequency;

    /// <summary>Counts what the window holds. The sum covers only events placed Inside.</summary>
    public static FaultDelayWindow Summarize(FaultDelayReading reading, long windowStart, long windowEnd)
    {
        var frequency = reading.Header?.Frequency ?? 0;
        var inside = reading.Events.Where(e => Place(e, windowStart, windowEnd) == FaultDelayPlacement.Inside).OrderBy(e => e.StartTicks).ToList();
        var straddleStart = reading.Events.Count(e => Place(e, windowStart, windowEnd) == FaultDelayPlacement.StraddleStart);
        var straddleEnd = reading.Events.Count(e => Place(e, windowStart, windowEnd) == FaultDelayPlacement.StraddleEnd);
        var spans = reading.Events.Count(e => Place(e, windowStart, windowEnd) == FaultDelayPlacement.Spans);
        var overlap = inside.Zip(inside.Skip(1), (a, b) => b.StartTicks < a.EndTicks).Any(bad => bad);
        var sum = frequency > 0 ? inside.Sum(e => ElapsedMs(e, frequency)) : double.NaN;

        return new FaultDelayWindow(inside.Count, straddleStart, straddleEnd, spans, sum, overlap);
    }

    /// <summary>
    /// The difference, in ms, between how far apart the header's clock anchor and the test's anchor are on the tick clock and on the UTC clock. Near zero when both processes read the same tick clock.
    /// It is a sanity check on the clock domain, not a proof; the header's own anchor interval (ticks1 - ticks0) is reported beside it.
    /// </summary>
    public static double ClockSkewMs(FaultDelayHeader header, DateTime testUtc, long testTicks)
    {
        var headerMid = (header.Ticks0 + header.Ticks1) / 2;
        var tickDeltaMs = (headerMid - testTicks) * 1000.0 / header.Frequency;
        var utcDeltaMs = (DateTime.Parse(header.Utc, CultureInfo.InvariantCulture, DateTimeStyles.RoundtripKind) - testUtc).TotalMilliseconds;
        return tickDeltaMs - utcDeltaMs;
    }

    public static double AnchorIntervalMs(FaultDelayHeader header) => (header.Ticks1 - header.Ticks0) * 1000.0 / header.Frequency;
}
#endif
