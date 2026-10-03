using System;

namespace ImageProcTest
{
    /// <summary>
    /// GUI-C-219c (Codex #112, leader decision): the rule "the synthetic oracle is never run on, or waited for by, the UI thread" is enforced where the blocking calls ARE, not by a list of the
    /// files that must not contain them. The window installs <see cref="OnUiThread"/> (is the caller on the dispatcher thread?); the blocking entry points call
    /// <see cref="AssertNotUiThread"/>, which throws when they are reached from there. A caller added in any file, by any route, is caught the first time it runs.
    /// Unset (headless modes, tests, the oracle's own child process), the guard does nothing.
    /// </summary>
    internal static class OracleThreadGuard
    {
        /// <summary>True when the calling thread is the UI (dispatcher) thread. Installed by the window; null when there is no UI thread to protect.</summary>
        internal static Func<bool>? OnUiThread { get; set; }

        internal static void AssertNotUiThread(string what)
        {
            if (OnUiThread?.Invoke() == true)
            {
                throw new InvalidOperationException(
                    $"{what} was called on the UI thread. The synthetic oracle runs a child process (hundreds of milliseconds) and the UI must not wait for it: ask PreprocessOracleVerdicts.TryGet and show \"checking\" until Completed.");
            }
        }
    }
}
