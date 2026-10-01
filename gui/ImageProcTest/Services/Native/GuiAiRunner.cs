// #225 row 10 (GUI-C-184): AI bone suppression, the native side. Declared where the shared resolver sees it.
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
    private static readonly object Gate = new();
    private static bool _started;

    /// <summary>Runs <paramref name="action"/> while no other init, shutdown or state call can run.</summary>
    public static T WithLock<T>(Func<T> action)
    {
        ArgumentNullException.ThrowIfNull(action);
        lock (Gate)
        {
            return action();
        }
    }

    /// <summary>Starts the AI module with the worker path on. Already started is not an error (the module ignores it).</summary>
    public static int Init(string modelDirectory) =>
        WithLock(() =>
        {
            var code = XpeAiNative.xpe_ai_init(modelDirectory, new AiConfig { UseWorker = true }.ToJson());
            if (code == 0)
            {
                _started = true;
            }

            return code;
        });

    /// <summary>Stops the module when this process started it; otherwise does nothing, and never loads the DLL to do so.</summary>
    public static void Shutdown() =>
        WithLock(() =>
        {
            if (!_started)
            {
                return 0;
            }

            _started = false;
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

/// <summary>One AI bone suppression run over a frame: init → convert → <c>xpe_bone_suppress</c> → convert back.</summary>
internal static class GuiAiRunner
{
    public static StageExecution Run(ushort[] input, int width, int height, string modelDirectory)
    {
        var count = checked(width * height);
        if (width <= 0 || height <= 0 || input.Length != count)
        {
            return new StageExecution(false, null, $"AI bone suppression not started: {input.Length} pixels do not fit {width}x{height}.");
        }

        var allocated = new List<Action>();
        try
        {
            var initCode = GuiAiSession.Init(modelDirectory);
            if (initCode != 0)
            {
                return AiBoneSuppressionStage.InterpretInit(initCode);
            }

            if (!TryAlloc(width, height, out var source, allocated, out var reason) ||
                !TryAlloc(width, height, out var target, allocated, out reason))
            {
                return new StageExecution(false, null, $"AI bone suppression not started: {reason}");
            }

            Marshal.Copy(AiBoneSuppressionStage.ToFloat(input), 0, source.Data, count);

            var code = XpeAiNative.xpe_bone_suppress(ref source, ref target, null);

            // The output buffer is read ONLY for code 0. After a refusal it was not written, and after a failed worker
            // call it equals the input; neither is a result (AiBoneSuppressionStage).
            float[]? output = null;
            if (code == AiBoneSuppressionStage.Ok)
            {
                output = new float[count];
                Marshal.Copy(target.Data, output, 0, count);
            }

            return AiBoneSuppressionStage.Interpret(code, output);
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
