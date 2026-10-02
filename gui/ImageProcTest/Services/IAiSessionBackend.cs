// #225 row 10 (GUI-C-185): the AI session, for the backends that have one. Kept apart from IXpeBackend: only the native
// backend has an AI module, and the Mock and the fault-injecting wrapper would otherwise carry two methods that cannot do anything.
namespace ImageProcTest.Services;

internal interface IAiSessionBackend
{
    /// <summary>
    /// Whether this backend really has an AI session (Codex #78 finding 2). Implementing the interface is not the same: the test wrapper implements it for every backend it wraps
    /// and has no session of its own when it wraps the Mock. Required of every implementer, with no default, so that a new one has to say.
    /// </summary>
    bool HasAiSession { get; }

    /// <summary>
    /// The worker's state as the module reports it; <see cref="AiWorkerStatus.Unknown"/> when there is no session. It waits for the
    /// session gate with NO time limit, so a running frame delays it: call it OFF the UI thread (GUI-C-186d; the caller is
    /// <see cref="AiStatusRefresher"/>).
    /// </summary>
    AiWorkerStatus GetAiWorkerStatus();

    /// <summary>Shutdown then init under the one lock. Starts a new session with a clean failure count.</summary>
    AiRestartResult RestartAiSession(string modelDirectory);

    /// <summary>
    /// GUI-C-192e: counts the AI sessions this backend has had (<see cref="AiSessionTracker.Epoch"/>). A status answer read under one count
    /// is dropped when the count has moved, so a session replaced by code that does not tell the refresher still cannot show its predecessor's state.
    /// </summary>
    int AiSessionEpoch => 0;
}
