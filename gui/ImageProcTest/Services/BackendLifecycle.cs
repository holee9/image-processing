// #225 row 10 (GUI-C-186e, Codex #33): ending a backend waits for the AI session gate, so it is done off the UI thread.
namespace ImageProcTest.Services;

/// <summary>
/// A backend, the lifetime generation, and (for an Apply) the request number at the moment a piece of work took it. A request number of
/// 0 means "not an Apply": only the lifetime is asked. See <see cref="BackendLifecycle.IsCurrent"/>.
/// </summary>
internal readonly record struct BackendTicket(object? Backend, int Generation, int Request = 0);

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
    /// The lifetime generation (GUI-C-186f, Codex #36): raised the moment a shutdown or a replacement STARTS, never lowered. Work that
    /// uses the backend takes a <see cref="BackendTicket"/> when it starts and asks <see cref="IsCurrent"/> after every await and
    /// before it schedules anything further; a ticket from before the bump is never current again, even if the same backend object
    /// is still in place (a shutdown keeps the object).
    /// </summary>
    public int Generation { get; private set; }

    /// <summary>The backend is being replaced: everything started for the old one is now stale.</summary>
    public void Bump() => Generation++;

    /// <summary>
    /// The Apply request number (GUI-C-190, Codex #42): raised by every new Apply, whatever the backend. The lifetime generation only
    /// separates "this backend" from "a shutdown or a replacement"; two Applies on the SAME backend share it, so the later-finishing
    /// one would overwrite the screen the later-STARTED one drew. A native call cannot be cut short, so an older request is not
    /// stopped: its result is discarded when it arrives (<see cref="IsCurrent"/>).
    /// </summary>
    public int Request { get; private set; }

    /// <summary>What a piece of backend work captures when it starts.</summary>
    public BackendTicket Take(object? backend) => new(backend, Generation);

    /// <summary>What an Apply captures when it starts: the lifetime AND a new request number, which makes every earlier Apply stale.</summary>
    public BackendTicket TakeRequest(object? backend) => new(backend, Generation, ++Request);

    /// <summary>
    /// True while the ticket's work may still change the screen or start more work: no shutdown or replacement began since it was
    /// taken, the backend in place is the one it was taken for, no transition is running, and (for an Apply) no newer Apply started.
    /// </summary>
    public bool IsCurrent(BackendTicket ticket, object? currentBackend) =>
        !IsTransitioning && ticket.Generation == Generation && ReferenceEquals(ticket.Backend, currentBackend)
        && (ticket.Request == 0 || ticket.Request == Request);

    /// <summary>True when a NEWER Apply started after this ticket's (the one reason, besides the lifetime, a result is dropped).</summary>
    public bool IsSuperseded(BackendTicket ticket) => ticket.Request != 0 && ticket.Request != Request;

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

        Generation++; // work started for the backend that is about to end is stale from this moment, not from its completion
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
