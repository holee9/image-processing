// #110 / SRS-ALERT-007: pull the native alert queue into the app's alert list after a processing call.
using ImageProcTest.Models;

namespace ImageProcTest.Services;

/// <summary>
/// Turns the native alert queue into <see cref="AlertEntry"/> values.
///
/// The drain is expressed over a delegate rather than calling P/Invoke directly so the decision
/// logic — buffer growth, severity mapping, the never-drop rule — can be exercised without a
/// native DLL. RealXpeBackend supplies the real reader; tests supply a fake queue.
///
/// Severity is never lowered (§5.17): an unknown native value and an unreadable entry both render
/// at Error, and an overflow alert renders at Error whatever the queue reported, because the
/// contract fixes it there. Raising is safe; hiding a dropped-alert notice is not.
/// </summary>
public static class NativeAlertDrain
{
    /// <summary>Stand-in text for an entry the reader could not produce. Never dropped silently.</summary>
    public const string UnreadableMessage = "[unreadable alert]";

    /// <summary>Code carried by every entry that came from the native queue.</summary>
    public const string NativeAlertCode = "NATIVE_ALERT";

    /// <summary>Code for the queue-overflow notice, so the UI can tell it apart from a payload alert.</summary>
    public const string OverflowCode = "ALERT_QUEUE_OVERFLOW";

    /// <summary>First attempt buffer size, in characters.</summary>
    public const int InitialBufferLength = 256;

    /// <summary>Single retry buffer size after BUFFER_TOO_SMALL. One retry, not a loop.</summary>
    public const int RetryBufferLength = 8192;

    /// <summary>Native BUFFER_TOO_SMALL (xpe_error_code.h). Spelled out: gui has no shared enum.</summary>
    public const int BufferTooSmall = -8;

    /// <summary>Native XPE_OK.</summary>
    public const int Ok = 0;

    /// <summary>
    /// Reads one queue entry into a buffer of <paramref name="bufferLength"/> characters.
    /// Returns the native error code; <see cref="BufferTooSmall"/> asks for a bigger buffer.
    /// </summary>
    public delegate int ReadAlert(int index, int bufferLength, out string? message, out int severity);

    /// <summary>
    /// Drains <paramref name="pendingCount"/> entries in queue order. Every index produces exactly
    /// one entry — a read that fails twice yields <see cref="UnreadableMessage"/> rather than a gap,
    /// because a missing entry erases the fact that an alert existed.
    /// </summary>
    public static IReadOnlyList<AlertEntry> Drain(int pendingCount, ReadAlert read, DateTimeOffset timestamp)
    {
        ArgumentNullException.ThrowIfNull(read);

        var entries = new List<AlertEntry>(Math.Max(0, pendingCount));
        for (var i = 0; i < pendingCount; i++)
        {
            entries.Add(ReadOne(i, read, timestamp));
        }

        return entries;
    }

    private static AlertEntry ReadOne(int index, ReadAlert read, DateTimeOffset timestamp)
    {
        var code = read(index, InitialBufferLength, out var message, out var severity);
        if (code == BufferTooSmall)
        {
            code = read(index, RetryBufferLength, out message, out severity);
        }

        if (code != Ok || message is null)
        {
            return new AlertEntry
            {
                Severity = "ERROR",
                Code = NativeAlertCode,
                Message = UnreadableMessage,
                Timestamp = timestamp
            };
        }

        var isOverflow = AlertDisplayFormatter.IsOverflowAlert(message);
        return new AlertEntry
        {
            Severity = isOverflow ? "ERROR" : MapSeverity(severity),
            Code = isOverflow ? OverflowCode : NativeAlertCode,
            Message = AlertDisplayFormatter.FormatMessage(message),
            Timestamp = timestamp
        };
    }

    /// <summary>
    /// Runs a native call and drains the alert queue afterwards, exactly once, on every path.
    ///
    /// #134: the drain used to sit at one call site (the display pipeline), which left every other
    /// native entry point unable to surface an alert. Making it a common post-step means a new call
    /// site cannot forget it. The drain runs in a finally, so a native call that throws still
    /// surfaces whatever it queued before failing — that is usually when alerts matter most.
    ///
    /// A drain that throws is not allowed to replace the call's own exception; the caller supplies a
    /// drain that swallows its own faults (RealXpeBackend.DrainNativeAlerts does).
    /// </summary>
    public static T InvokeWithDrain<T>(Func<T> call, Action drain)
    {
        ArgumentNullException.ThrowIfNull(call);
        ArgumentNullException.ThrowIfNull(drain);

        try
        {
            return call();
        }
        finally
        {
            drain();
        }
    }

    /// <summary>Void-returning counterpart of <see cref="InvokeWithDrain{T}"/>.</summary>
    public static void InvokeWithDrain(Action call, Action drain)
    {
        ArgumentNullException.ThrowIfNull(call);

        InvokeWithDrain<object?>(() => { call(); return null; }, drain);
    }

    /// <summary>XpeAlertSeverity → the app's severity strings. Unknown values do not get lowered.</summary>
    public static string MapSeverity(int nativeSeverity) => nativeSeverity switch
    {
        0 => "INFO",
        1 => "WARN",
        2 => "ERROR",
        _ => "ERROR",
    };
}
