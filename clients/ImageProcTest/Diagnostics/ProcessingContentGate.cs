using System.Threading.Tasks;

namespace ImageProcTest
{
    /// <summary>
    /// GUI-C-219d (Codex #116, high): the question every processing command asks just before it runs: "are the preprocess DLLs on disk the ones the oracle verdict was made for?".
    /// The answer is computed on a pool thread (the window awaits it and keeps answering its user); a "no" means the command does not run and the verification has been started again.
    ///
    /// This is the ONLY basis on which processing is allowed to start. What the screen shows (a stored verdict, "ready") is information for the user and is allowed to be a few milliseconds
    /// stale; it never decides. The commands that ask are listed in the GUI-C-219d report (the three click handlers of the window, and the two headless fixture services).
    ///
    /// Limit: this closes accidental change while the application runs. A deliberate replacement aimed at the milliseconds between this answer and the command opening the DLL is not covered:
    /// whoever can replace the DLLs in the application folder can already do anything this application can, so the application is not a security boundary against that user.
    /// </summary>
    internal static class ProcessingContentGate
    {
        /// <summary>True when the command may run. No DLL path (nothing native is in play) is "nothing to confirm": the readiness guards of the caller decide what is left.</summary>
        public static Task<bool> ConfirmAsync(string? preprocessDllPath) =>
            string.IsNullOrWhiteSpace(preprocessDllPath) ? Task.FromResult(true) : PreprocessOracleVerdicts.ConfirmContentAsync(preprocessDllPath);
    }
}
