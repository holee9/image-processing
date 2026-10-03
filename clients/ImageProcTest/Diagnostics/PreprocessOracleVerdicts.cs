using System;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;

namespace ImageProcTest
{
    /// <summary>
    /// GUI-C-219 (#249): the ONE place the synthetic-oracle verdict for a DLL is produced and kept.
    ///
    /// 212b moved the oracle into a child process and left three callers asking for it synchronously on the UI thread at startup (the diagnostics report, the module readiness
    /// evaluation, and a second readiness refresh), so the window stopped answering for a median of 963 ms and up to 4.4 s (GUI-C-218). Here the verdict is produced on a thread-pool
    /// thread, kept per DLL file (path and write time), and shared by every caller: <see cref="TryGet"/> never waits (it starts the run the first time, and answers null until it has
    /// finished); <see cref="Wait"/> is for the headless paths that have no window to keep responsive. A UI that gets null shows "checking" and is told when the answer is in
    /// (<see cref="Completed"/>). Asking again does not run it again: only <see cref="Invalidate"/> (the user's refresh) discards a finished verdict.
    /// </summary>
    internal static class PreprocessOracleVerdicts
    {
        private sealed class Entry
        {
            public PreprocessSyntheticOracleResult? Result;
            public Task? Run;
        }

        private static readonly object Gate = new();
        private static readonly Dictionary<string, Entry> Entries = [];

        /// <summary>How a verdict is produced: the child-process oracle. A test replaces it to observe the threading and the sharing.</summary>
        internal static Func<string, PreprocessSyntheticOracleResult> Runner { get; set; } = XpePreprocessOracleProcess.Run;

        /// <summary>Raised on the thread that produced the verdict, after it is stored; the argument is the DLL path. A UI marshals to its own thread.</summary>
        internal static event Action<string>? Completed;

        /// <summary>The finished verdict for this DLL file, or null while it is still being produced (the first call starts the run). Never waits.</summary>
        public static PreprocessSyntheticOracleResult? TryGet(string dllPath)
        {
            lock (Gate)
            {
                return EntryFor(dllPath).Result;
            }
        }

        /// <summary>The verdict for this DLL file; blocks until it exists. For callers with no window to keep responsive (the headless probe and the fixture E2E services).</summary>
        public static PreprocessSyntheticOracleResult Wait(string dllPath)
        {
            Entry entry;
            lock (Gate)
            {
                entry = EntryFor(dllPath);
            }

            entry.Run!.Wait();
            lock (Gate)
            {
                return entry.Result!;
            }
        }

        /// <summary>The user's refresh: finished verdicts are discarded so the next ask runs the oracle again. A run in progress is left alone (its answer is still the answer for the DLL).</summary>
        public static void Invalidate()
        {
            lock (Gate)
            {
                foreach (var key in new List<string>(Entries.Keys))
                {
                    if (Entries[key].Result is not null)
                    {
                        Entries.Remove(key);
                    }
                }
            }
        }

        /// <summary>For tests: forget everything, including runs in progress, and restore the default runner.</summary>
        internal static void ResetForTests()
        {
            lock (Gate)
            {
                Entries.Clear();
            }

            Runner = XpePreprocessOracleProcess.Run;
            Completed = null;
        }

        private static string KeyOf(string dllPath) =>
            dllPath + "|" + (File.Exists(dllPath) ? File.GetLastWriteTimeUtc(dllPath).Ticks : 0);

        private static Entry EntryFor(string dllPath)
        {
            var key = KeyOf(dllPath);
            if (Entries.TryGetValue(key, out var existing))
            {
                return existing;
            }

            var entry = new Entry();
            Entries[key] = entry;
            entry.Run = Task.Run(() =>
            {
                PreprocessSyntheticOracleResult result;
                try
                {
                    result = Runner(dllPath);
                }
                catch (Exception ex)
                {
                    result = PreprocessSyntheticOracleResult.Failed("Synthetic oracle exception", ex.Message);
                }

                lock (Gate)
                {
                    entry.Result = result;
                }

                try
                {
                    Completed?.Invoke(dllPath);
                }
                catch (Exception)
                {
                    // a listener's failure is not the verdict's
                }
            });
            return entry;
        }
    }
}
