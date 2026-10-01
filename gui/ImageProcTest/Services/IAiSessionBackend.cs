// #225 row 10 (GUI-C-185): the AI session, for the backends that have one. Kept apart from IXpeBackend: only the native
// backend has an AI module, and the Mock and the fault-injecting wrapper would otherwise carry two methods that cannot do anything.
namespace ImageProcTest.Services;

internal interface IAiSessionBackend
{
    /// <summary>The worker's state as the module reports it; <see cref="AiWorkerStatus.Unknown"/> when there is no session.</summary>
    AiWorkerStatus GetAiWorkerStatus();

    /// <summary>Shutdown then init under the one lock. Starts a new session with a clean failure count.</summary>
    AiRestartResult RestartAiSession(string modelDirectory);
}
