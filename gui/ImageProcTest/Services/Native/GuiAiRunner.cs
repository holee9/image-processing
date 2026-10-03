// #225 row 10 (GUI-C-184): AI bone suppression, the native side. Declared where the shared resolver sees it.
using System.IO;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace ImageProcTest.Services.Native;

/// <summary>
/// P/Invoke surface for <c>xpe_ai.dll</c> (<c>ai_api.h</c>): the three calls the GUI makes, and nothing else.
/// </summary>
internal static class XpeAiNative
{
    private const string DllName = "xpe_ai.dll";

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    internal static extern int xpe_ai_init(string modelDirPath, string? configJson);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void xpe_ai_shutdown();

    /// <summary>Read-only status of the worker path (ai_api.h). MUST NOT run with init or shutdown: see <see cref="GuiAiSession"/>.</summary>
    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_ai_worker_state(out int state, out uint consecutiveFailures, out uint ceiling);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    internal static extern int xpe_bone_suppress(
        ref XpeImageBufferNative img,
        ref XpeImageBufferNative softTissueOut,
        string? configJson);
}

/// <summary>
/// The config <c>xpe_ai_init</c> reads. Serialized from an object rather than written as a string, like the gsvg
/// config: the module ignores a key it does not know, so a misspelt <c>use_worker</c> would silently run the model in
/// the host process, which REQ-AI-003 forbids.
/// </summary>
internal sealed record AiConfig
{
    /// <summary>Route <c>xpe_bone_suppress</c> through the worker process (ai_api.h; default false there).</summary>
    [JsonPropertyName("use_worker")] public bool UseWorker { get; init; }

    private static readonly JsonSerializerOptions Options = new();

    public string ToJson() => JsonSerializer.Serialize(this, Options);
}

/// <summary>
/// The one place the GUI calls <c>xpe_ai_init</c> and <c>xpe_ai_shutdown</c>, under one lock (GUI-C-184).
///
/// <para><c>xpe_ai_worker_state</c> (QA-B-173, not in the module yet) must not run at the same time as init or
/// shutdown. The state poll and the restart button that come with it take <see cref="WithLock{T}"/> too, so the
/// rule is kept by where the calls are, not by each caller remembering it.</para>
///
/// <para>The session is NOT started and stopped per run: <c>xpe_ai_init</c> is idempotent, the worker's failure
/// count belongs to the session (3 in a row switch it off until shutdown), and a restart per run would hide that count.
/// It is stopped when the backend shuts down, and only if this process started it.</para>
/// </summary>
internal static class GuiAiSession
{
    /// <summary>The one gate: a whole AI frame, a restart, the status read and every init and shutdown go through it.</summary>
    internal static readonly AiSessionGate Gate = new();

    private static readonly AiSessionTracker Tracker = new();

    // GUI-C-189: what this process's native session has seen, for the automation tree (AiWorkerStatus.Diagnostics). Written only
    // under the gate. The first native C-09 run (main CI, 53ec370a) saw six failed calls and no "switched off" state, and could not
    // say whether the module was ever on the worker path: this records the module's own answers instead of the GUI's reading of them.
    private static volatile string _initDiagnostics = "init: none yet";   // read without the gate by the status call (GUI-C-192e): volatile
    private static int _initCount;
    private static int _readCount;

    /// <summary>Runs <paramref name="action"/> while no other frame, init, shutdown or state call can run.</summary>
    public static T WithLock<T>(Func<T> action) => Gate.WithLock(action);

    /// <summary>
    /// Starts the AI module with the worker path on. Already started with the SAME directory is not an error (the module ignores
    /// it). Already started with ANOTHER directory is a new session: the module ignores a second init whatever it is given
    /// (ai.cpp:501-504), so the old session is shut down first, under this same lock. The directory handed to the module is the
    /// absolute form (<see cref="AiBoneSuppressionStage.NormalizeDirectory"/>).
    /// </summary>
    public static int Init(string modelDirectory) => InitCore(AiBoneSuppressionStage.NormalizeDirectory(modelDirectory));

    /// <summary>
    /// The init for a directory that is already absolute and normalised: what a frame calls with the string it checked the model file
    /// at, and what <see cref="Init"/> reaches after resolving its argument ONCE.
    /// </summary>
    public static int InitCore(string directory) =>
        WithLock(() =>
        {
            if (Tracker.NeedsNewSession(directory))
            {
                Shutdown();
            }

            // A start that does not work is recorded where it happens, whoever called (the render, or Restart): a restart that
            // fails must leave an error state with its reason, not "unknown" (Codex #24 B1). The exception still goes up.
            int code;
            try
            {
                var config = new AiConfig { UseWorker = true }.ToJson();

                // Asks the module its worker state BEFORE this init, to learn whether something else had initialised it already
                // (xpe_ai_init ignores a second call's arguments, so an earlier init would leave the worker path off). Read-only, and
                // inside this lock, where no init or shutdown can run, as the header requires.
                string probe;
                try
                {
                    var probeCode = XpeAiNative.xpe_ai_worker_state(out var probeState, out var probeFailures, out var probeCeiling);
                    probe = $"before-init worker_state code={probeCode} state={probeState} failures={probeFailures} ceiling={probeCeiling}";
                }
                catch (Exception probeError)
                {
                    probe = $"before-init worker_state probe threw {probeError.GetType().Name}";
                }

                // GUI-C-201: the spelling the running session was started with, when this is the same directory (see AiSessionTracker.DirectoryToSend).
                // Taken after the shutdown above, so a different directory has already ended the session and is sent as requested.
                directory = Tracker.DirectoryToSend(directory);
                code = XpeAiNative.xpe_ai_init(directory, config);
                _initDiagnostics = $"init#{++_initCount} dir='{directory}' config={config} {probe} -> code={code}";
            }
            catch (DllNotFoundException)
            {
                Tracker.InitFailed("xpe_ai.dll was not found beside the other native modules.");
                throw;
            }
            catch (EntryPointNotFoundException ex)
            {
                Tracker.InitFailed($"xpe_ai.dll does not export a function this build needs ({ex.Message}).");
                throw;
            }
            catch (Exception ex)
            {
                // ONE reason string: the tracker records it and the exception that goes up (the chain path's stage reason, the
                // Restart message) carries the same one (GUI-C-186c).
                var reason = AiBoneSuppressionStage.InitFailureReason(ex);
                Tracker.InitFailed(reason);
                throw new AiInitException(reason, ex);
            }

            if (code == 0)
            {
                Tracker.InitSucceeded(directory);
            }
            else
            {
                Tracker.InitFailed($"xpe_ai_init refused the configuration (code {code}).");
            }

            return code;
        });

    /// <summary>
    /// The worker's status, read under the same lock as init and shutdown: the module documents that the state call must
    /// not run with either (they free or create what it reads). Not started here, or an older DLL without the export,
    /// is <see cref="AiWorkerStatus.Unknown"/>, and asking then does not load the DLL. It waits for the gate with no time
    /// limit — a frame waiting on a silent worker holds it for up to the module's time budget — so it is called OFF the UI
    /// thread (<see cref="AiStatusRefresher"/>, GUI-C-186d).
    /// </summary>
    public static AiWorkerStatus QueryWorkerState()
    {
        // GUI-C-192e (Codex #62): the GUI's own answer is given BEFORE the gate is taken. It used to be read inside the lock, so a first init or a
        // restart that held the gate delayed even "not started", which needs no module at all. Safe: the tracker's fields are written under the
        // gate and are volatile, and the answer is re-checked under the gate below (a start that finished meanwhile is not missed). What this
        // does NOT cover: once the session has started, a frame that holds the gate through its first inference (the model loads lazily on that
        // first call, ai.cpp) still delays the module's answer, so "no answer for the bound" cannot be told from a slow load by this code alone.
        var early = Tracker.OwnStatus();
        if (early is not null)
        {
            return early with { Diagnostics = $"{_initDiagnostics}; the GUI answered this itself, before the gate (the module was not asked)" };
        }

        return QueryWorkerStateUnderGate();
    }

    /// <summary>The session counter (<see cref="AiSessionTracker.Epoch"/>): up whenever a session ends or one starts from nothing.</summary>
    public static int SessionEpoch => Tracker.Epoch;

    private static AiWorkerStatus QueryWorkerStateUnderGate() =>
        WithLock(() =>
        {
            var own = Tracker.OwnStatus();
            if (own is not null)
            {
                return own with { Diagnostics = $"{_initDiagnostics}; the GUI answered this itself (the module was not asked)" };
            }

            try
            {
                var code = XpeAiNative.xpe_ai_worker_state(out var state, out var failures, out var ceiling);
                return AiBoneSuppressionStage.ReadWorkerState(code, state, failures, ceiling) with
                {
                    Diagnostics = $"{_initDiagnostics}; read#{++_readCount} worker_state code={code} state={state} failures={failures} ceiling={ceiling}",
                };
            }
            catch (DllNotFoundException)
            {
                return AiWorkerStatus.Unknown;
            }
            catch (EntryPointNotFoundException)
            {
                return AiWorkerStatus.Unknown;
            }
        });

    /// <summary>
    /// Shutdown then init as ONE step under the lock, so the status call cannot run between them. This is the recovery the
    /// header names for a worker switched off for the session.
    /// </summary>
    public static AiRestartResult Restart(string modelDirectory) =>
        WithLock(() =>
        {
            Shutdown();
            try
            {
                return AiBoneSuppressionStage.InterpretRestart(Init(modelDirectory)); // Init resolves the directory once
            }
            catch (DllNotFoundException)
            {
                return new AiRestartResult(false, "AI session could not be restarted: xpe_ai.dll was not found beside the other native modules.");
            }
            catch (EntryPointNotFoundException ex)
            {
                return new AiRestartResult(false, $"AI session could not be restarted: xpe_ai.dll does not export a function this build needs ({ex.Message}).");
            }
        });

    /// <summary>Stops the module when this process started it; otherwise does nothing, and never loads the DLL to do so.</summary>
    public static void Shutdown() =>
        WithLock(() =>
        {
            var wasStarted = Tracker.Started;
            Tracker.Stopped(); // a deliberate stop also clears a recorded start failure: nothing is running and nothing is wrong
            if (!wasStarted)
            {
                return 0;
            }

            try
            {
                XpeAiNative.xpe_ai_shutdown();
            }
            catch (DllNotFoundException)
            {
            }
            catch (EntryPointNotFoundException)
            {
            }

            return 0;
        });
}

/// <summary>
/// One AI bone suppression run over a frame. The order and the locking are <see cref="AiFrame"/>'s (tested with fakes); this class
/// supplies the real operations and the buffers around them.
/// </summary>
internal static class GuiAiRunner
{
    public static StageExecution Run(ushort[] input, int width, int height, string modelDirectory)
    {
        var count = checked(width * height);
        if (width <= 0 || height <= 0 || input.Length != count)
        {
            return new StageExecution(false, null, $"AI bone suppression not started: {input.Length} pixels do not fit {width}x{height}.");
        }

        // The directory is resolved ONCE per frame, here, and this one string goes to the file check and to the init
        // (GUI-C-186b): a working directory that moves between the two cannot make them look at different places.
        var directory = AiBoneSuppressionStage.NormalizeDirectory(modelDirectory);

        // Outside the gate: the two image buffers (xpe_common, not the AI module; buffers this frame owns). See AiFrame.
        var allocated = new List<Action>();
        try
        {
            if (!TryAlloc(width, height, out var source, allocated, out var reason) ||
                !TryAlloc(width, height, out var target, allocated, out reason))
            {
                return new StageExecution(false, null, $"AI bone suppression not started: {reason}");
            }

            Marshal.Copy(AiBoneSuppressionStage.ToFloat(input), 0, source.Data, count);
            var ops = new RealAiFrameOps(source, target, count);
            return AiFrame.Run(GuiAiSession.Gate, ops, directory);
        }
        catch (DllNotFoundException)
        {
            return new StageExecution(false, null,
                "AI bone suppression not started: xpe_ai.dll was not found beside the other native modules; the original image is shown.");
        }
        catch (EntryPointNotFoundException ex)
        {
            return new StageExecution(false, null, $"AI bone suppression not started: xpe_ai.dll does not export a function this build needs ({ex.Message}).");
        }
        finally
        {
            foreach (var free in allocated)
            {
                free();
            }
        }
    }

    /// <summary>The real operations of a frame. The only caller of <c>xpe_bone_suppress</c>; it is invoked by <see cref="AiFrame.Run"/> under the gate.</summary>
    private sealed class RealAiFrameOps(XpeImageBufferNative source, XpeImageBufferNative target, int count) : IAiFrameOps
    {
        private XpeImageBufferNative _source = source;
        private XpeImageBufferNative _target = target;

        public bool ModelFileExists(string path) => File.Exists(path);

        public int InitSession(string absoluteDirectory) => GuiAiSession.InitCore(absoluteDirectory);

        public AiSuppressResult Suppress()
        {
            var code = XpeAiNative.xpe_bone_suppress(ref _source, ref _target, null);

            // The output buffer is read ONLY for code 0. After a refusal it was not written, and after a failed worker
            // call it equals the input; neither is a result (AiBoneSuppressionStage).
            float[]? output = null;
            if (code == AiBoneSuppressionStage.Ok)
            {
                output = new float[count];
                Marshal.Copy(_target.Data, output, 0, count);
            }

            return new AiSuppressResult(code, output);
        }
    }

    private static bool TryAlloc(int width, int height, out XpeImageBufferNative buffer, List<Action> allocated, out string reason)
    {
        var code = XpeCommonNative.xpe_alloc_image((uint)width, (uint)height, XpePixelFormatNative.Float32, out buffer);
        if (code != 0)
        {
            reason = $"xpe_alloc_image(Float32) failed ({code}).";
            return false;
        }

        var local = buffer;
        allocated.Add(() => XpeCommonNative.xpe_free_image(ref local));
        reason = string.Empty;
        return true;
    }
}
