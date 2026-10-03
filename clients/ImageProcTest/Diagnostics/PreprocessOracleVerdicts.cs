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
    /// thread, kept per DLL, and shared by every caller. A UI that gets null shows "checking" and is told when the answer is in (<see cref="Completed"/>) or when the answer it had was
    /// dropped (<see cref="Changed"/>).
    ///
    /// GUI-C-219b/219c (Codex #109, #112): WHICH bytes a verdict belongs to is not decided by a path or a time stamp. Every check starts by taking a private SNAPSHOT of the preprocess DLL and the
    /// DLLs it loads (<see cref="PreprocessOracleSnapshot"/>), hashes the COPIES, and hands the copy to the worker: what was hashed is what was judged, whatever happens to the original while the
    /// oracle runs, and the verdict is stored under that identity. Because of that there is no "hash again afterwards and run again" and no retry budget: nothing can change under a run.
    /// The copy and the hashing happen on the worker thread, never on the caller's: <see cref="TryGet"/> only takes a lock for a moment.
    ///
    /// The price of keeping the UI thread free is that <see cref="TryGet"/> answers with the stored verdict at once and verifies it in the background (one snapshot, a few milliseconds): if the
    /// DLL turns out to be different, the stored verdict is dropped, <see cref="Changed"/> is raised (the UI shows "checking" again) and the new content is judged. The stale answer therefore
    /// lives for the duration of one copy-and-hash, not for a refresh interval, and it is never a verdict for content that is not on disk any more once that verification has run.
    /// </summary>
    internal static class PreprocessOracleVerdicts
    {
        private sealed class State
        {
            /// <summary>One verification or run at a time for this DLL; the background job and <see cref="Wait"/> take turns on it.</summary>
            public readonly object Work = new();

            public string? Identity;
            public PreprocessSyntheticOracleResult? Result;
            public bool JobQueued;

            /// <summary>The running job has taken its snapshot: an ask that comes after that is not covered by it (the file may have changed since) and has to be verified by one more pass.</summary>
            public bool SnapshotTaken;

            public bool Rerun;
        }

        private static readonly object Gate = new();
        private static readonly Dictionary<string, State> States = new(StringComparer.OrdinalIgnoreCase);
        private static int reclaimed;
        private static int generation;   // bumped by ResetForTests: a job that started before it must not store, announce or run anything after it

        /// <summary>How a verdict is produced from the path of the SNAPSHOT of the DLL: the child-process oracle. A test replaces it to observe the threading, the sharing and what the worker is given.</summary>
        internal static Func<string, PreprocessSyntheticOracleResult> Runner { get; set; } = XpePreprocessOracleProcess.Run;

        /// <summary>Test seam: called on the worker thread once the snapshot's files are copied and before they are hashed (a slow disk, or a file changing at the worst moment).</summary>
        internal static Action<string>? AfterSnapshotCopy { get; set; }

        /// <summary>Test seam (GUI-C-219d): called on the worker thread after a pass, before it decides whether another pass is wanted. An ask that arrives here must get a pass of its own.</summary>
        internal static Action<string>? BeforeRerunDecision { get; set; }

        /// <summary>Test seam (GUI-C-219d): called on the worker thread right after the decision that ended the job. An ask that arrives here finds no job and must start one of its own.</summary>
        internal static Action<string>? AfterRerunDecision { get; set; }

        /// <summary>Raised on the thread that produced a verdict, after it is stored; the argument is the DLL path. A UI marshals to its own thread.</summary>
        internal static event Action<string>? Completed;

        /// <summary>Raised when a background verification found that the DLL is no longer the content a stored verdict was made for: that verdict has been dropped. A UI refreshes and shows "checking".</summary>
        internal static event Action<string>? Changed;

        /// <summary>The stored verdict for this DLL, or null while there is none (the first call starts the check). Never reads the file, never waits: a verification of the stored verdict against the DLL as it is now runs in the background.</summary>
        public static PreprocessSyntheticOracleResult? TryGet(string dllPath)
        {
            State state;
            PreprocessSyntheticOracleResult? stored;
            bool start;
            lock (Gate)
            {
                state = StateOf(dllPath);
                stored = state.Result;
                start = !state.JobQueued;
                state.JobQueued = true;
                if (!start && state.SnapshotTaken)
                {
                    state.Rerun = true;   // the running job's copy predates this ask
                }
            }

            if (start)
            {
                var born = System.Threading.Volatile.Read(ref generation);
                Task.Run(() => RunJob(dllPath, state, born));
            }

            return stored;
        }

        /// <summary>The verdict for this DLL as it is on disk now; blocks until it exists. For callers with no window to keep responsive (the headless probe and the fixture E2E services): called on the UI thread it throws.</summary>
        public static PreprocessSyntheticOracleResult Wait(string dllPath)
        {
            OracleThreadGuard.AssertNotUiThread(nameof(PreprocessOracleVerdicts) + "." + nameof(Wait));
            State state;
            lock (Gate)
            {
                state = StateOf(dllPath);
            }

            lock (state.Work)
            {
                return Verify(dllPath, state, System.Threading.Volatile.Read(ref generation));
            }
        }

        /// <summary>
        /// GUI-C-219d (Codex #116, high): the ONLY thing that may enable a processing command. The stored verdict that <see cref="TryGet"/> hands out can be stale for the few milliseconds a background
        /// verification takes; this is asked just before a command runs, hashes the files as they are NOW (on a pool thread: a caller on the UI thread awaits it and stays responsive) and says whether
        /// they are the files the stored verdict was made for. When they are not, or when no verdict exists, it returns false and starts the verification, so the UI goes to "checking".
        ///
        /// Limit (decided with the card): this closes ACCIDENTAL change (a rebuild or redeploy while the app runs). Between this answer and the command opening the DLL there is still a gap of
        /// milliseconds; a deliberate replacement aimed at it is out of scope, because whoever can write the application folder can do much more than that and this application is not a security
        /// boundary against such a user.
        /// </summary>
        public static Task<bool> ConfirmContentAsync(string dllPath) => Task.Run(() => IsCurrent(dllPath));

        /// <summary>The synchronous form of <see cref="ConfirmContentAsync"/>, for callers that are not on a UI thread (the headless fixture services).</summary>
        public static bool IsCurrent(string dllPath)
        {
            OracleThreadGuard.AssertNotUiThread(nameof(PreprocessOracleVerdicts) + "." + nameof(IsCurrent));
            string? storedIdentity;
            lock (Gate)
            {
                storedIdentity = States.TryGetValue(dllPath, out var state) && state.Result is not null ? state.Identity : null;
            }

            if (storedIdentity is not null)
            {
                try
                {
                    if (string.Equals(PreprocessOracleSnapshot.IdentityOfOriginals(dllPath), storedIdentity, StringComparison.Ordinal))
                    {
                        return true;
                    }
                }
                catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
                {
                    // a file of the set cannot be read now: it is not the content that was judged
                }
            }

            TryGet(dllPath);   // not the verified content (or nothing verified yet): check again; the result reaches the UI through Completed / Changed
            return false;
        }

        /// <summary>The user's refresh: finished verdicts are discarded so the next ask runs the oracle again even on the same bytes. A check in progress is left alone.</summary>
        public static void Invalidate()
        {
            lock (Gate)
            {
                foreach (var state in States.Values)
                {
                    if (state.Result is not null)
                    {
                        state.Result = null;
                        state.Identity = null;
                    }
                }
            }
        }

        /// <summary>For tests: forget everything and restore the defaults.</summary>
        internal static void ResetForTests()
        {
            lock (Gate)
            {
                System.Threading.Interlocked.Increment(ref generation);
                States.Clear();
            }

            Runner = XpePreprocessOracleProcess.Run;
            AfterSnapshotCopy = null;
            PreprocessOracleSnapshot.BeforeCopy = null;
            BeforeRerunDecision = null;
            AfterRerunDecision = null;
            Completed = null;
            Changed = null;
        }

        /// <summary>For tests: the identity (names with SHA-256) the stored verdict for this DLL was made for, or null.</summary>
        internal static string? StoredIdentityOf(string dllPath)
        {
            lock (Gate)
            {
                return States.TryGetValue(dllPath, out var state) ? state.Identity : null;
            }
        }

        // Caller holds Gate.
        private static State StateOf(string dllPath)
        {
            if (!States.TryGetValue(dllPath, out var state))
            {
                States[dllPath] = state = new State();
            }

            return state;
        }

        private static void RunJob(string dllPath, State state, int born)
        {
            var released = false;   // true once the decision under Gate has ended the job: after that the flags belong to whichever ask starts the next one
            try
            {
                lock (state.Work)
                {
                    try
                    {
                        // one pass per distinct ask: an ask that arrives after a pass has copied the files sets Rerun and gets a pass of its own, which finds the same bytes (and says nothing) unless they changed
                        while (true)
                        {
                            lock (Gate)
                            {
                                state.Rerun = false;
                                state.SnapshotTaken = false;
                            }

                            Verify(dllPath, state, born);
                            BeforeRerunDecision?.Invoke(dllPath);

                            // GUI-C-219d (Codex #116): "is another pass wanted?" and "this job is over" are ONE decision under ONE lock. An ask that comes before it sets Rerun and is run again here;
                            // one that comes after finds JobQueued false and starts a job of its own. There is no moment in which an ask is neither.
                            bool another;
                            lock (Gate)
                            {
                                another = state.Rerun;
                                if (!another)
                                {
                                    state.JobQueued = false;
                                    state.SnapshotTaken = false;
                                    released = true;
                                }
                            }

                            if (!another)
                            {
                                AfterRerunDecision?.Invoke(dllPath);
                                break;
                            }
                        }
                    }
                    catch (Exception ex)
                    {
                        // nothing may escape a pool thread, and a failed check is a verdict: the caller sees it as one
                        StoreFailureOnce(dllPath, state, born, "error:" + ex.GetType().Name + ":" + ex.Message, PreprocessSyntheticOracleResult.Failed("Synthetic oracle exception", ex.Message));
                    }
                }
            }
            finally
            {
                if (!released)
                {
                    lock (Gate)
                    {
                        state.JobQueued = false;
                        state.SnapshotTaken = false;
                        state.Rerun = false;
                    }
                }
            }
        }

        // Caller holds state.Work. Snapshot, identity, and then either the stored verdict (same identity) or a new run on the snapshot.
        private static PreprocessSyntheticOracleResult Verify(string dllPath, State state, int born)
        {
            if (System.Threading.Interlocked.Exchange(ref reclaimed, 1) == 0)
            {
                PreprocessOracleSnapshot.ReclaimOldFolders();
            }

            PreprocessOracleSnapshot snapshot;
            try
            {
                snapshot = PreprocessOracleSnapshot.Create(dllPath, AfterSnapshotCopy);
            }
            catch (Exception ex)
            {
                // The private copy could not be made (the temp folder is unusable, the disk is full): the DLL cannot be judged, which is a verdict. It is stored under an identity of its own, and a
                // pass that finds the same failure says nothing, or every refresh the window makes in answer to it would start another pass (a refresh storm).
                return StoreFailureOnce(dllPath, state, born, "setup:" + ex.GetType().Name + ":" + ex.Message,
                    PreprocessSyntheticOracleResult.Failed("Synthetic oracle setup failed", "A private copy of the preprocess DLLs could not be made, so the DLL was not judged: " + ex.Message));
            }

            using var ownedSnapshot = snapshot;
            lock (Gate)
            {
                state.SnapshotTaken = true;
            }

            PreprocessSyntheticOracleResult? stored;
            string? storedIdentity;
            lock (Gate)
            {
                stored = state.Result;
                storedIdentity = state.Identity;
            }

            if (stored is not null && string.Equals(storedIdentity, snapshot.Identity, StringComparison.Ordinal))
            {
                return stored;   // the same bytes: the same answer, and nothing to announce
            }

            if (born != System.Threading.Volatile.Read(ref generation))
            {
                return PreprocessSyntheticOracleResult.NotRun("The verdict holder was reset while this check was starting.");   // only a test can reset it
            }

            if (stored is not null)
            {
                lock (Gate)
                {
                    state.Result = null;
                    state.Identity = null;
                }

                RaiseChanged(dllPath);
            }

            PreprocessSyntheticOracleResult result;
            try
            {
                result = Runner(snapshot.DllPath);
            }
            catch (Exception ex)
            {
                result = PreprocessSyntheticOracleResult.Failed("Synthetic oracle exception", ex.Message);
            }

            if (born == System.Threading.Volatile.Read(ref generation))
            {
                Store(dllPath, state, snapshot.Identity, result, raise: true);
            }

            return result;
        }

        // A failure verdict whose identity equals the stored one is the same answer again: no announcement.
        private static PreprocessSyntheticOracleResult StoreFailureOnce(string dllPath, State state, int born, string identity, PreprocessSyntheticOracleResult failure)
        {
            lock (Gate)
            {
                if (state.Result is not null && string.Equals(state.Identity, identity, StringComparison.Ordinal))
                {
                    return state.Result;
                }
            }

            if (born == System.Threading.Volatile.Read(ref generation))
            {
                Store(dllPath, state, identity, failure, raise: true);
            }

            return failure;
        }

        private static void Store(string dllPath, State state, string identity, PreprocessSyntheticOracleResult result, bool raise)
        {
            lock (Gate)
            {
                state.Identity = identity;
                state.Result = result;
            }

            if (raise)
            {
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

        private static void RaiseChanged(string dllPath)
        {
            try
            {
                Changed?.Invoke(dllPath);
            }
            catch (Exception)
            {
                // a listener's failure is not the verdict's
            }
        }
    }
}
