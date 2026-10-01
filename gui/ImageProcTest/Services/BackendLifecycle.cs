// #225 row 10 (GUI-C-186e, Codex #33): ending a backend waits for the AI session gate, so it is done off the UI thread.
namespace ImageProcTest.Services;

/// <summary>
/// One backend lifecycle transition at a time (today: shutdown, from the Shutdown button and from closing the window).
/// The work waits for the AI session gate — a frame that waits on a silent worker holds it for up to the module's time budget — so it
/// runs in the BACKGROUND (<c>runInBackground</c>) and the UI thread only starts it and, later, finishes it (<c>postToUi</c>). While
/// one runs, processing requests are refused (<see cref="TryAdmit"/>), and whatever has to follow it (closing the window) is queued
/// with <see cref="WhenIdle"/>. All members run on the UI thread except the background work itself, so no field needs a lock.
/// </summary>
internal sealed class BackendLifecycle(Action<Action> runInBackground, Action<Action> postToUi)
{
    private readonly List<Action> _afterDone = [];

    /// <summary>True from <see cref="Begin"/> until the transition's UI-side completion has run.</summary>
    public bool IsTransitioning { get; private set; }

    /// <summary>
    /// Starts <paramref name="backgroundWork"/> off the UI thread and returns at once. False (and nothing started) when a transition is
    /// already running. <paramref name="completedOnUi"/> receives the exception the work threw, or null; it runs on the UI thread,
    /// after which <see cref="IsTransitioning"/> is false and the queued <see cref="WhenIdle"/> actions run.
    /// </summary>
    public bool Begin(Action backgroundWork, Action<Exception?> completedOnUi)
    {
        if (IsTransitioning)
        {
            return false;
        }

        IsTransitioning = true;
        runInBackground(() =>
        {
            Exception? error = null;
            try
            {
                backgroundWork();
            }
            catch (Exception ex)
            {
                error = ex;
            }

            postToUi(() =>
            {
                IsTransitioning = false;
                completedOnUi(error);
                var queued = _afterDone.ToArray();
                _afterDone.Clear();
                foreach (var action in queued)
                {
                    action();
                }
            });
        });
        return true;
    }

    /// <summary>Runs <paramref name="action"/> on the UI thread once no transition is running: now, when none is.</summary>
    public void WhenIdle(Action action)
    {
        ArgumentNullException.ThrowIfNull(action);
        if (IsTransitioning)
        {
            _afterDone.Add(action);
            return;
        }

        action();
    }

    /// <summary>A processing request is admitted only while no transition runs; otherwise the line to show.</summary>
    public bool TryAdmit(out string? refusal)
    {
        refusal = IsTransitioning ? "The backend is shutting down; the request was not run." : null;
        return !IsTransitioning;
    }
}
