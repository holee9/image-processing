// #225 row 10 (GUI-C-184): what the AI bone suppression stage makes of the module's answer. No native call here.
using System;
using System.Linq;
using ImageProcTest.Models;

namespace ImageProcTest.Services;

/// <summary>What <c>xpe_ai_worker_state</c> reports (ai_api.h). <see cref="Unknown"/> is this GUI's: not started, or no answer.</summary>
internal enum AiWorkerState
{
    Unknown = -1,
    NotUsed = 0,
    Active = 1,
    Disabled = 2,

    /// <summary>
    /// This GUI's, not the module's: the session could not be (re)started, so there is nothing to ask. Without it a failed
    /// restart reads as <see cref="Unknown"/>, which shows nothing, and the mark and the retry button vanish exactly when
    /// recovery has just failed (Codex #24 B1).
    /// </summary>
    InitFailed = 3,
}

/// <summary>
/// The worker's state with the module's own counts. The ceiling is the module's, never a constant here. <see cref="Diagnostics"/> is
/// for the automation tree only (GUI-C-189): what the native session saw, in one line, so a failed native E2E says WHY.
/// </summary>
internal sealed record AiWorkerStatus(AiWorkerState State, uint ConsecutiveFailures, uint Ceiling, string? Detail = null, string? Diagnostics = null)
{
    public static readonly AiWorkerStatus Unknown = new(AiWorkerState.Unknown, 0, 0);
}

/// <summary>
/// The one lock around the AI session (GUI-C-186b). A re-entrant monitor: a frame that holds it can call the session's own init
/// from inside, and a restart can call shutdown then init under the one acquisition. Every caller waits for it with no limit; the
/// status read does so on a background thread (<see cref="AiStatusRefresher"/>), so a frame that is waiting on a silent worker
/// never freezes the UI thread (GUI-C-186d).
/// </summary>
internal sealed class AiSessionGate
{
    private readonly object _gate = new();

    public T WithLock<T>(Func<T> action)
    {
        ArgumentNullException.ThrowIfNull(action);
        lock (_gate)
        {
            return action();
        }
    }
}

/// <summary>The answer of one <c>xpe_bone_suppress</c> call: the return code, and the output ONLY for code 0.</summary>
internal sealed record AiSuppressResult(int Code, float[]? Output);

/// <summary>
/// What one AI frame needs from the outside world, so the frame's order and locking can be tested with fakes (no DLL).
/// The real implementation prepares its buffers BEFORE the frame starts and frees them after, which are the parts that do not
/// touch the AI module's state.
/// </summary>
internal interface IAiFrameOps
{
    bool ModelFileExists(string path);

    /// <summary>Makes the session ready for <paramref name="absoluteDirectory"/>, starting a new one when it was started for another. Returns the init code.</summary>
    int InitSession(string absoluteDirectory);

    /// <summary>Calls <c>xpe_bone_suppress</c> on the prepared buffers.</summary>
    AiSuppressResult Suppress();
}

/// <summary>
/// One AI frame, in order, under ONE hold of the session gate: the model file check, the init, the call. (Codex #25 finding 1:
/// the lock used to end with the init, so another frame's directory change or a Restart could replace the session between the
/// init and the call, and shutdown could free the state a call was about to use.) The status read takes the same gate.
///
/// <para><b>Inside the gate:</b> the file check, <c>InitSession</c>, <c>Suppress</c> — everything that uses or decides the
/// session. <b>Outside it</b> (the caller does these, and why that is safe): allocating and filling the two image buffers and
/// freeing them. They go through <c>xpe_common</c>, not through the AI module, and touch only buffers this frame owns, so no
/// session replacement can change what they do; keeping them out keeps the time the gate is held to the part that needs it.</para>
///
/// <para>The directory arrives already absolute and is used as that one string by the check and by the init.</para>
/// </summary>
internal static class AiFrame
{
    public static StageExecution Run(AiSessionGate gate, IAiFrameOps ops, string absoluteDirectory)
    {
        ArgumentNullException.ThrowIfNull(gate);
        ArgumentNullException.ThrowIfNull(ops);
        if (!System.IO.Path.IsPathRooted(absoluteDirectory))
        {
            // The caller resolves the directory once per render. A relative one here would be resolved again, by the check and by the
            // module, possibly against a different working directory.
            throw new ArgumentException("The frame takes a directory that is already absolute.", nameof(absoluteDirectory));
        }

        return gate.WithLock(() =>
        {
            var missing = AiBoneSuppressionStage.CheckModelFileAt(absoluteDirectory, ops.ModelFileExists);
            if (missing is not null)
            {
                return missing;
            }

            var initCode = ops.InitSession(absoluteDirectory);
            if (initCode != AiBoneSuppressionStage.Ok)
            {
                return AiBoneSuppressionStage.InterpretInit(initCode);
            }

            var suppressed = ops.Suppress();
            return AiBoneSuppressionStage.Interpret(suppressed.Code, suppressed.Output);
        });
    }
}

/// <summary>
/// What the GUI knows about its own AI session, apart from the module: whether it started one, with which directory, and why the
/// last start failed. Pure, so the sequences (fail, retry, succeed, shut down) are tested. The native session wraps one of these.
/// </summary>
internal sealed class AiSessionTracker
{
    private bool _started;
    private string? _startedDirectory;
    private string? _initFailure;

    public bool Started => _started;

    public bool NeedsNewSession(string directory) => _started && AiBoneSuppressionStage.NeedsNewSession(_startedDirectory, directory);

    public void InitSucceeded(string directory)
    {
        _started = true;
        _startedDirectory = directory;
        _initFailure = null;
    }

    /// <summary>The last start did not work. Stays until a start works or the session is deliberately stopped.</summary>
    public void InitFailed(string detail) => _initFailure = detail;

    /// <summary>A deliberate stop: nothing is running, and nothing is wrong.</summary>
    public void Stopped()
    {
        _started = false;
        _startedDirectory = null;
        _initFailure = null;
    }

    /// <summary>
    /// The status the GUI answers itself, or null when the module should be asked. A failed start wins over "not started":
    /// that is the persistent error state, with its detail.
    /// </summary>
    public AiWorkerStatus? OwnStatus() =>
        _initFailure is not null ? new AiWorkerStatus(AiWorkerState.InitFailed, 0, 0, _initFailure)
        : !_started ? AiWorkerStatus.Unknown
        : null;
}

/// <summary>Whether a restart worked, and the line to show.</summary>
internal sealed record AiRestartResult(bool Ok, string Message);

/// <summary>
/// An AI session start threw something other than a missing DLL or export. Its message IS the reason the tracker recorded
/// (<see cref="AiBoneSuppressionStage.InitFailureReason"/>), so the exception that goes up the chain path and the status the screen shows
/// are one string, not two spellings of the same failure (GUI-C-186c, Codex #28 A3).
/// </summary>
internal sealed class AiInitException(string reason, Exception inner) : Exception(reason, inner);

/// <summary>
/// Keeps the screen's copy of the worker status current without ever making the UI thread wait (GUI-C-186d, Codex #31).
/// The read waits for the session gate with no time limit, so it runs in the BACKGROUND (<c>runInBackground</c>); a frame that waits
/// on a silent worker only delays the read, and when the frame ends the read completes, with no event and no retry count needed.
/// The result comes back through <c>postToUi</c> and is applied only if no newer <see cref="Reset"/> or <see cref="Stop"/> happened
/// meanwhile (generation) and the backend is still the one the read was made for (identity). At most one read is in flight; requests
/// that arrive meanwhile collapse into ONE more read afterwards.
/// All members except <c>read</c> run on the UI thread, so no field needs a lock.
/// </summary>
internal sealed class AiStatusRefresher(
    Func<object?> currentBackend,
    Func<object?, AiWorkerStatus?> read,
    Action<AiWorkerStatus> apply,
    Action<Action> runInBackground,
    Action<Action> postToUi)
{
    private int _generation;
    private int _requests;
    private bool _inFlight;
    private bool _stopped;

    /// <summary>
    /// Asks for a fresh read. Returns at once; the status reaches <c>apply</c> later, on the UI thread.
    /// GUI-C-192: every request takes a number. A read remembers the number it started under, and its answer is shown only while no newer
    /// request exists: an answer older than a request that has already been made is dropped and the newest read is the one shown. (Same
    /// rule as the Apply request number of <see cref="BackendLifecycle"/> — a later start makes an earlier result stale — kept here
    /// because reads are asked for from more than Apply too: a restart, a reset. A number that only Apply issues would not order those.)
    /// </summary>
    public void Request()
    {
        if (_stopped)
        {
            return;
        }

        _requests++;
        if (_inFlight)
        {
            return; // not a second parallel read: one more after the running one, which Complete starts
        }

        Start();
    }

    /// <summary>The backend is being replaced or shut down: whatever is being read is stale, and the screen shows Unknown until a new read says otherwise.</summary>
    public void Reset()
    {
        if (_stopped)
        {
            return;
        }

        _generation++;
        apply(AiWorkerStatus.Unknown);
    }

    /// <summary>The application is closing: nothing started or running now may touch the screen again.</summary>
    public void Stop()
    {
        _stopped = true;
        _generation++;
    }

    private void Start()
    {
        _inFlight = true;
        var generation = _generation;
        var requestNumber = _requests;
        var backend = currentBackend();
        runInBackground(() =>
        {
            AiWorkerStatus? status;
            try
            {
                status = read(backend);
            }
            catch (Exception)
            {
                status = null; // nothing was read: what is shown stays
            }

            postToUi(() => Complete(generation, requestNumber, backend, status));
        });
    }

    private void Complete(int generation, int requestNumber, object? backend, AiWorkerStatus? status)
    {
        _inFlight = false;
        if (_stopped)
        {
            return;
        }

        if (generation != _generation || !ReferenceEquals(backend, currentBackend()))
        {
            Start(); // the answer is for something that no longer exists: drop it and read what exists now
        }
        else if (requestNumber != _requests)
        {
            // GUI-C-192: a newer request was made while this read ran, so this answer is older than what the screen is about to show. It used
            // to be applied first and replaced a moment later; that moment is a stale "Active" (or a late "off") on a clinical screen.
            Start();
        }
        else if (status is not null)
        {
            apply(status);
        }
    }
}

/// <summary>How the module's answer to one <c>xpe_bone_suppress</c> call is read.</summary>
internal enum AiCallClass
{
    /// <summary>Return code 0: the module says it produced a soft-tissue image.</summary>
    Succeeded,

    /// <summary>
    /// The module refused the input before it tried (invalid input, not initialised, unsupported format). The output
    /// buffer was not written, so it holds nothing to read, and the worker's failure count is not touched.
    /// </summary>
    NotAttempted,

    /// <summary>
    /// The call was made and did not succeed (return code -3 and the like). On the worker path the module then copies the
    /// INPUT into the output and returns non-zero (ai_api.h, xpe_bone_suppress).
    /// </summary>
    Failed,
}

/// <summary>
/// Interprets the module's return code and converts pixels either way (GUI-C-184, #225 row 10).
///
/// <para><b>The rule this class exists for.</b> A failed call leaves the output equal to the input, byte for byte
/// (ai_api.h). The pixel chain decides Applied versus AppliedNoChange by comparing pixels
/// (<see cref="ProcessingChainRunner"/>), so a failure whose output were taken as a result would read as "ran, nothing to
/// change". The success of this stage is therefore decided by the RETURN CODE alone: a non-zero code is returned as
/// <c>Ran = false</c> with no pixels, and the runner never compares anything (it records RequestedNotApplied).</para>
///
/// <para><b>Assumption, not a measurement: the input scale.</b> The header requires float32 and says nothing about the
/// range; the SDD says "normalize input to [0, 1]" without saying who does it. This stage divides 16-bit pixels by 65535
/// before the call and multiplies by 65535 after it, clamping to [0, 1]. Whether the module expects that scale is
/// asked of the module's owner (QA-B-173) and is not confirmed. In a stub build no inference runs, so nothing here has
/// been observed on a real model.</para>
///
/// Free of WPF and of native calls so the integration tests link it.
/// </summary>
internal static class AiBoneSuppressionStage
{
    public const int Ok = 0;
    public const int InvalidInput = -1;
    public const int ProcessingFailed = -3;
    public const int NotInitialized = -6;
    public const int UnsupportedFormat = -7;

    /// <summary>Full scale of the 16-bit pixels, the divisor and multiplier of the assumed [0, 1] contract.</summary>
    public const float FullScale = 65535f;

    /// <summary>The label for a frame the module really processed. Shown only for a stage whose status is Applied.</summary>
    public const string ProcessedLabel = "AI-processed: bone suppression";

    /// <summary>The model directory when the setting is blank (AppSettings falls back to the same value).</summary>
    public const string DefaultModelDirectory = "data/models";

    private static string? _baseDirectory;

    /// <summary>
    /// Fixes the directory relative model directories are resolved against, once, at application start (<c>App.OnStartup</c>).
    /// The first call wins; later calls do nothing. The convention is unchanged (the process's working directory at start);
    /// what changed is that it no longer follows the working directory afterwards. Installing from the application's own
    /// location instead is a deployment decision and not made here.
    /// </summary>
    public static void CaptureBaseDirectory(string? directory = null) =>
        _baseDirectory ??= System.IO.Path.GetFullPath(directory ?? Environment.CurrentDirectory);

    /// <summary>The captured base; when nothing captured it yet (a test, a harness) the working directory at this first use.</summary>
    public static string BaseDirectory => _baseDirectory ??= System.IO.Path.GetFullPath(Environment.CurrentDirectory);

    /// <summary>For the tests only: forget the captured base.</summary>
    internal static void ResetBaseDirectoryForTests() => _baseDirectory = null;

    /// <summary>
    /// The model directory as an absolute path with no trailing separator (GUI-C-186). The module resolves a relative one against
    /// the process's working directory at each use, and its worker against the working directory it was started in, so a relative
    /// directory can mean two places once the working directory has moved; the GUI therefore decides once and gives the module the
    /// absolute form, which is also what a restart compares against.
    /// </summary>
    public static string NormalizeDirectory(string? directory, string? baseDirectory = null)
    {
        // A relative directory is resolved against the base fixed at startup (GUI-C-186b, leader's decision), NOT against whatever
        // the working directory is at this moment: a file dialog can move the working directory, and the same setting must keep
        // meaning the same place. An absolute directory is not touched by the base.
        var full = System.IO.Path.GetFullPath(
            string.IsNullOrWhiteSpace(directory) ? DefaultModelDirectory : directory.Trim(),
            baseDirectory ?? BaseDirectory);
        var root = System.IO.Path.GetPathRoot(full);
        return full.Length > (root?.Length ?? 0) ? full.TrimEnd('\\', '/') : full;
    }

    /// <summary>
    /// True when a session already started with <paramref name="startedDirectory"/> must be ended and started again to use
    /// <paramref name="requestedDirectory"/>. <c>xpe_ai_init</c> while initialised returns OK and IGNORES its arguments
    /// (ai.cpp:501-504), so without this a changed directory would look accepted and change nothing. Null means no session.
    /// </summary>
    /// <remarks>
    /// Compared as resolved strings, ignoring case; NOT as the same directory on disk. Switching between a path and an alias of
    /// it (an 8.3 short name, a junction or symbolic link, a UNC form) is therefore a new session with the failure count back
    /// at 0: expected, and cheaper than the file-identity comparison it would take to avoid. A case-sensitive directory
    /// (Windows' per-folder setting) is outside what this supports: two names that differ only in case are treated as one place.
    /// (Codex #25 finding 3, decided by the lead.)
    /// </remarks>
    public static bool NeedsNewSession(string? startedDirectory, string? requestedDirectory) =>
        startedDirectory is not null &&
        !string.Equals(NormalizeDirectory(startedDirectory), NormalizeDirectory(requestedDirectory), StringComparison.OrdinalIgnoreCase);

    /// <summary>The file xpe_bone_suppress reads inside the model directory (ai_api.h: <c>&lt;modelDir&gt;/bone_suppress.onnx</c>).</summary>
    public const string ModelFileName = "bone_suppress.onnx";

    /// <summary>
    /// Looks for the model file BEFORE the module is asked, and answers "not attempted" when it is not there (GUI-C-185).
    /// The module cannot say this itself: with the worker path on, a missing model comes back as the same code (-9) as a
    /// worker executable that was not found or could not be started, so the code alone does not tell them apart. Not asking
    /// also means no worker is started and no failure is counted toward the 3 that switch the worker off.
    /// Returns null when the file is there and the call should be made. <paramref name="exists"/> is for the tests.
    /// </summary>
    public static StageExecution? CheckModelFile(string modelDirectory, Func<string, bool>? exists = null) =>
        CheckModelFileAt(NormalizeDirectory(modelDirectory), exists);

    /// <summary>
    /// The same check for a directory that is ALREADY absolute and normalised: it does not normalise again. The frame resolves a
    /// render's directory once and gives that one string to this check and to the init (GUI-C-186b), so a working directory that
    /// moves between the two cannot make them look at different places.
    /// </summary>
    public static StageExecution? CheckModelFileAt(string absoluteDirectory, Func<string, bool>? exists = null)
    {
        // The path as the module will use it, and as the message prints it: absolute, so "no model at data\models\..." can no
        // longer leave the reader guessing which directory a relative path was taken from (GUI-C-186).
        var path = System.IO.Path.Combine(absoluteDirectory, ModelFileName);
        if ((exists ?? System.IO.File.Exists)(path))
        {
            return null;
        }

        return new StageExecution(false, null,
            $"AI bone suppression not attempted: no model at {path}. The AI worker was not started and no failure was counted; the original image is shown.");
    }

    /// <summary>
    /// The label for a chain: <see cref="ProcessedLabel"/> when the AI stage's status is Applied, otherwise empty.
    /// Applied means the module returned 0 AND the pixels changed (the runner's rule), so a refused or failed stage, and
    /// a stage that returned 0 with an unchanged image, never carry it. The label is read from the status, not from an
    /// alert text or a message.
    /// </summary>
    public static string LabelFor(ChainResult chain) =>
        chain.Stages.Any(stage => stage.StageId == StageIds.AiBoneSuppression && stage.Status == StageStatus.Applied)
            ? ProcessedLabel
            : string.Empty;

    public static AiCallClass Classify(int code) =>
        code == Ok ? AiCallClass.Succeeded
        : code is InvalidInput or NotInitialized or UnsupportedFormat ? AiCallClass.NotAttempted
        : AiCallClass.Failed;

    public static float[] ToFloat(ushort[] pixels)
    {
        var result = new float[pixels.Length];
        for (var i = 0; i < pixels.Length; i++)
        {
            result[i] = pixels[i] / FullScale;
        }

        return result;
    }

    /// <summary>False when any value is not finite: a number the 16-bit chain cannot carry is not a result.</summary>
    public static bool TryToUInt16(float[] values, out ushort[] pixels)
    {
        pixels = new ushort[values.Length];
        for (var i = 0; i < values.Length; i++)
        {
            if (!float.IsFinite(values[i]))
            {
                pixels = [];
                return false;
            }

            pixels[i] = (ushort)Math.Round(Math.Clamp(values[i], 0f, 1f) * FullScale);
        }

        return true;
    }

    /// <summary>The stage's answer for one <c>xpe_bone_suppress</c> call. <paramref name="output"/> is read only for code 0.</summary>
    public static StageExecution Interpret(int code, float[]? output)
    {
        switch (Classify(code))
        {
            case AiCallClass.Succeeded:
                if (output is null || !TryToUInt16(output, out var pixels))
                {
                    return new StageExecution(false, null,
                        "AI bone suppression NOT applied: xpe_bone_suppress returned 0 but its output holds values this chain cannot use; the original image is shown.");
                }

                return new StageExecution(true, pixels, "xpe_bone_suppress returned 0 (worker path).");

            case AiCallClass.NotAttempted:
                return new StageExecution(false, null,
                    $"AI bone suppression not attempted (code {code}): {RefusalMeaning(code)} The module refused the input before trying; the original image is shown.");

            default:
                return new StageExecution(false, null,
                    $"AI bone suppression NOT applied (code {code}): the original image is shown. A failed call returns the input unchanged; " +
                    "consecutive failures switch the AI worker off for this session (the AI worker mark shows when that has happened). " +
                    "The code is the module's and is not read further here: the same code can have more than one cause.");
        }
    }

    /// <summary>The stage's answer when <c>xpe_ai_init</c> did not return 0.</summary>
    public static StageExecution InterpretInit(int code) =>
        new(false, null, $"AI bone suppression not started: xpe_ai_init refused the configuration (code {code}); the original image is shown.");

    /// <summary>
    /// The one reason string for an <c>xpe_ai_init</c> that threw something other than a missing DLL or export. The tracker records it
    /// and <see cref="AiInitException"/> carries it, so the status line, the Restart message and the chain's stage reason say the same
    /// thing (GUI-C-186c, Codex #28 A3). It ends with a full stop so the sentence the banner appends after it reads on.
    /// </summary>
    public static string InitFailureReason(Exception ex)
    {
        var message = ex.Message.TrimEnd();
        return $"xpe_ai_init threw {ex.GetType().Name}: {message}{(message.EndsWith('.') ? string.Empty : ".")}";
    }

    /// <summary>The reading of an <c>xpe_ai_worker_state</c> answer. Anything unexpected is <see cref="AiWorkerState.Unknown"/>, which shows nothing.</summary>
    public static AiWorkerStatus ReadWorkerState(int code, int state, uint consecutiveFailures, uint ceiling) =>
        code == Ok && state is >= 0 and <= 2
            ? new AiWorkerStatus((AiWorkerState)state, consecutiveFailures, ceiling)
            : AiWorkerStatus.Unknown;

    /// <summary>
    /// The persistent text for a status: non-empty ONLY when the module reports the worker switched off. The numbers are
    /// the module's (<c>consecutiveFailures</c>, <c>ceiling</c>): the ceiling is not written here, so a module that
    /// changes it changes the text with it.
    /// </summary>
    public static string BannerFor(AiWorkerStatus status) => status.State switch
    {
        AiWorkerState.Disabled =>
            $"AI worker switched off for this session after {status.ConsecutiveFailures} of {status.Ceiling} failures in a row: images are returned unchanged until it is restarted.",
        AiWorkerState.InitFailed =>
            $"AI session is not running: {status.Detail} Images are returned unchanged. Use Restart AI to try again.",
        _ => string.Empty,
    };

    /// <summary>True when the persistent mark and the Restart AI button show: the worker is off, or the session could not be started.</summary>
    public static bool ShowsMark(AiWorkerStatus status) => status.State is AiWorkerState.Disabled or AiWorkerState.InitFailed;

    /// <summary>
    /// One line a program can read from the screen (an automation property): the state with the module's own numbers.
    /// The E2E reads it to tell "restarted, active, no failures" from "the mark is merely gone".
    /// </summary>
    public static string DescribeStatus(AiWorkerStatus status) => status.State switch
    {
        AiWorkerState.Active or AiWorkerState.Disabled =>
            $"worker={status.State}; failures={status.ConsecutiveFailures}; ceiling={status.Ceiling}",
        AiWorkerState.InitFailed => $"worker=InitFailed; detail={status.Detail}",
        _ => $"worker={status.State}",
    };

    /// <summary>The answer to a restart, from the return code of the <c>xpe_ai_init</c> that follows the shutdown.</summary>
    public static AiRestartResult InterpretRestart(int initCode) =>
        initCode == Ok
            ? new AiRestartResult(true, "AI session restarted: a new session with a clean failure count.")
            : new AiRestartResult(false, $"AI session could not be restarted: xpe_ai_init refused the configuration (code {initCode}).");

    private static string RefusalMeaning(int code) => code switch
    {
        InvalidInput => "the input or its size was rejected.",
        NotInitialized => "the AI module is not initialised.",
        UnsupportedFormat => "the pixel format or dimensions are not supported.",
        _ => "the input was rejected.",
    };
}
