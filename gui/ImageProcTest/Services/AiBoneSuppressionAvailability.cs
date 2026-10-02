// #225 (GUI-C-198): when "Run AI Bone Suppression" can be used (MENU-001 section 8: the owner DLL is present and the backend is ready). Free of WPF and of native code.
namespace ImageProcTest.Services;

/// <summary>Whether the AI bone suppression entry is usable now, and when it is not, the reason in one sentence (shown in the entry's tooltip).</summary>
public sealed record AiBoneSuppressionAvailability(bool CanRun, string Reason)
{
    /// <summary>
    /// The rule. Every input is a fact the caller read: a backend with an AI session at all (the Mock has none), that backend initialized (a shut-down or not yet started
    /// backend answers no frame), no start or stop of it in flight, and <c>xpe_ai.dll</c> found by the same search the loader uses. The loaded frame is NOT an input: with no
    /// frame the command answers on the status line, which says what to do; a disabled entry would say nothing.
    /// </summary>
    public static AiBoneSuppressionAvailability Evaluate(bool backendHasAiSession, bool backendInitialized, bool backendTransitioning, bool moduleDllPresent)
    {
        if (backendTransitioning)
        {
            return new(false, "The backend is starting or shutting down.");
        }

        if (!backendHasAiSession)
        {
            return new(false, "Needs the native backend; this session runs the Mock backend, which has no AI module.");
        }

        if (!backendInitialized)
        {
            return new(false, "The backend is not initialized (Backend menu, Initialize).");
        }

        if (!moduleDllPresent)
        {
            return new(false, "xpe_ai.dll was not found beside the other native modules.");
        }

        return new(true, string.Empty);
    }
}
