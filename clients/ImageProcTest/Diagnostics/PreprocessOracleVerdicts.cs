using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Threading.Tasks;

namespace ImageProcTest
{
    /// <summary>
    /// GUI-C-219 (#249): the ONE place the synthetic-oracle verdict for a DLL is produced and kept.
    ///
    /// 212b moved the oracle into a child process and left three callers asking for it synchronously on the UI thread at startup (the diagnostics report, the module readiness
    /// evaluation, and a second readiness refresh), so the window stopped answering for a median of 963 ms and up to 4.4 s (GUI-C-218). Here the verdict is produced on a thread-pool
    /// thread, kept per DLL, and shared by every caller: <see cref="TryGet"/> never waits on the oracle (it starts the run the first time, and answers null until it has
    /// finished); <see cref="Wait"/> is for the headless paths that have no window to keep responsive. A UI that gets null shows "checking" and is told when the answer is in
    /// (<see cref="Completed"/>). Asking again does not run it again: only <see cref="Invalidate"/> (the user's refresh) discards a finished verdict.
    ///
    /// GUI-C-219b (Codex #109): WHICH DLL a verdict belongs to is decided by the SHA-256 of its CONTENT, read on every ask. It used to be the path plus the last-write time, so a different DLL
    /// copied to the same path with its time preserved was handed the old "passed" verdict and became "ready" without ever being checked (the sixth stamp-instead-of-content shortcut in this
    /// repository: a fast path that answers differently from the slow one). A file's size and time are not consulted at all: hashing the file is what the slow path IS, and it costs about a
    /// millisecond for this DLL (measured in the GUI-C-219b report), so there is no cheaper path to keep honest. A file that changes WHILE the oracle runs is checked again: the
    /// result of that run belongs to neither the old content nor the new, and is stored under neither.
    /// </summary>
    internal static class PreprocessOracleVerdicts
    {
        /// <summary>How many times a verdict is produced again because the file changed under the run, before the check gives up and says so.</summary>
        internal const int MaxRunsPerAsk = 3;

        private sealed class Entry
        {
            public PreprocessSyntheticOracleResult? Result;
            public Task? Run;
            public bool Superseded;
        }

        private static readonly object Gate = new();
        private static readonly Dictionary<string, Entry> Entries = [];

        /// <summary>How a verdict is produced: the child-process oracle. A test replaces it to observe the threading and the sharing.</summary>
        internal static Func<string, PreprocessSyntheticOracleResult> Runner { get; set; } = XpePreprocessOracleProcess.Run;

        /// <summary>Raised on the thread that produced the verdict, after it is stored; the argument is the DLL path. A UI marshals to its own thread.</summary>
        internal static event Action<string>? Completed;

        /// <summary>The finished verdict for this DLL's current content, or null while it is still being produced (the first call starts the run). Never waits on the oracle.</summary>
        public static PreprocessSyntheticOracleResult? TryGet(string dllPath)
        {
            var key = KeyOf(dllPath);
            lock (Gate)
            {
                return EntryFor(dllPath, key).Result;
            }
        }

        /// <summary>The verdict for this DLL's current content; blocks until it exists. For callers with no window to keep responsive (the headless probe and the fixture E2E services).</summary>
        public static PreprocessSyntheticOracleResult Wait(string dllPath)
        {
            for (var round = 0; round < MaxRunsPerAsk; round++)
            {
                var key = KeyOf(dllPath);
                Entry entry;
                lock (Gate)
                {
                    entry = EntryFor(dllPath, key);
                }

                entry.Run!.Wait();
                lock (Gate)
                {
                    if (!entry.Superseded)
                    {
                        return entry.Result!;
                    }
                }
            }

            return PreprocessSyntheticOracleResult.Failed("Synthetic oracle unstable", "The preprocess DLL changed while the oracle ran, every time it was asked.");
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

        /// <summary>The SHA-256 of the file's content, or a marker that names why there is none (so that "missing" and "unreadable" are subjects of their own, never a hash of nothing).</summary>
        internal static string ContentIdentity(string dllPath)
        {
            try
            {
                using var stream = new FileStream(dllPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
                return Convert.ToHexString(SHA256.HashData(stream));
            }
            catch (FileNotFoundException)
            {
                return "missing";
            }
            catch (DirectoryNotFoundException)
            {
                return "missing";
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
            {
                return "unreadable:" + ex.GetType().Name;
            }
        }

        private static string KeyOf(string dllPath) => dllPath + "|" + ContentIdentity(dllPath);

        // Caller holds Gate.
        private static Entry EntryFor(string dllPath, string key)
        {
            if (Entries.TryGetValue(key, out var existing))
            {
                return existing;
            }

            var entry = new Entry();
            Entries[key] = entry;
            entry.Run = Task.Run(() => Produce(dllPath, key, entry));
            return entry;
        }

        private static void Produce(string dllPath, string key, Entry entry)
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

            // The file the verdict was asked for is the file the oracle ran on only if its content is the same after the run. Otherwise this result belongs to neither content: it is stored
            // under nothing, and the NEW content is asked for at once (so a window that is waiting for the answer is still told when one exists).
            var after = KeyOf(dllPath);
            if (!string.Equals(after, key, StringComparison.Ordinal))
            {
                bool alreadyKnown;
                lock (Gate)
                {
                    entry.Superseded = true;
                    if (Entries.TryGetValue(key, out var mapped) && ReferenceEquals(mapped, entry))
                    {
                        Entries.Remove(key);
                    }

                    // starts the run for what is on disk now (its completion raises Completed); the content may also be one that was already judged (a file put back), which raises nothing by itself
                    alreadyKnown = EntryFor(dllPath, after).Result is not null;
                }

                if (alreadyKnown)
                {
                    try { Completed?.Invoke(dllPath); } catch (Exception) { /* a listener's failure is not the verdict's */ }
                }

                return;
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
        }
    }
}
