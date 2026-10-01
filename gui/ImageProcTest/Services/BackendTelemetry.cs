// #225 row 10 (GUI-C-186f, Codex #36 finding 4): the backend's log and alert lists are written from several threads.
using ImageProcTest.Models;

namespace ImageProcTest.Services;

/// <summary>
/// What one drain takes from a backend: the log lines and alerts that arrived after the cursors the caller holds, and the totals the
/// cursors move to. Read as ONE step (<see cref="BackendTelemetry.Since"/>), so a count and the items it counts can never disagree.
/// </summary>
public sealed record TelemetrySnapshot(IReadOnlyList<string> Logs, int LogTotal, IReadOnlyList<AlertEntry> Alerts, int AlertTotal);

/// <summary>
/// The log lines and alerts of one backend. Both backends write them from pool threads (a chain or display call, a background
/// shutdown) while the UI thread drains them, and a plain <c>List</c> read by index while another thread adds to it loses or repeats
/// items, or throws. Every write and the one read take the same lock, and the read copies what it hands out: the caller never holds a
/// reference into a list another thread may still change.
/// </summary>
public sealed class BackendTelemetry
{
    private readonly object _gate = new();
    private readonly List<string> _logs = [];
    private readonly List<AlertEntry> _alerts = [];

    public void AddLog(string line)
    {
        lock (_gate)
        {
            _logs.Add(line);
        }
    }

    public void AddAlert(AlertEntry alert)
    {
        lock (_gate)
        {
            _alerts.Add(alert);
        }
    }

    public void AddAlerts(IEnumerable<AlertEntry> alerts)
    {
        lock (_gate)
        {
            _alerts.AddRange(alerts);
        }
    }

    public void Clear()
    {
        lock (_gate)
        {
            _alerts.Clear();
            _logs.Clear();
        }
    }

    /// <summary>
    /// The items after the first <paramref name="logsSeen"/> log lines and the first <paramref name="alertsSeen"/> alerts, with the
    /// totals. A cursor beyond the total (the lists were cleared since) starts again from the beginning.
    /// </summary>
    public TelemetrySnapshot Since(int logsSeen, int alertsSeen)
    {
        lock (_gate)
        {
            var logStart = logsSeen < 0 || logsSeen > _logs.Count ? 0 : logsSeen;
            var alertStart = alertsSeen < 0 || alertsSeen > _alerts.Count ? 0 : alertsSeen;
            return new TelemetrySnapshot(
                _logs.GetRange(logStart, _logs.Count - logStart),
                _logs.Count,
                _alerts.GetRange(alertStart, _alerts.Count - alertStart),
                _alerts.Count);
        }
    }
}
