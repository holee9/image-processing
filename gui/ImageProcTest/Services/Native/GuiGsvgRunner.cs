// #180 (GUI-C-101): run the GSVG stage and say what it did.
using System.IO;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.Json.Serialization;
using ImageProcTest.Models;

namespace ImageProcTest.Services.Native;

/// <summary>
/// The config gsvg reads (<c>gsvg_api.h</c> §init). Written as an object and serialized, never as an
/// interpolated string: the module silently disables a feature whose key is misspelled
/// (gsvg_api.h:61-66), and the settings survey does not count a value that only appears inside a string
/// literal as a processing read (GUI-C-97 risk (b), decided in GUI-C-99).
/// </summary>
internal sealed record GsvgConfig
{
    [JsonPropertyName("grid_suppression")] public bool GridSuppression { get; init; }

    [JsonPropertyName("virtual_grid")] public bool VirtualGrid { get; init; }

    [JsonPropertyName("vg_table_path")] public string? TablePath { get; init; }

    [JsonPropertyName("vg_kvp")] public double? Kvp { get; init; }

    [JsonPropertyName("vg_grid_ratio")] public double? GridRatio { get; init; }

    [JsonPropertyName("vg_grid_frequency_per_cm")] public double? GridFrequencyPerCm { get; init; }

    [JsonPropertyName("vg_pixel_pitch_mm")] public double? PixelPitchMm { get; init; }

    [JsonPropertyName("vg_air_signal")] public double? AirSignal { get; init; }

    [JsonPropertyName("vg_iterations")] public int? Iterations { get; init; }

    /// <summary>Laplacian pyramid levels; 0 (sent as absent) leaves the pyramid and de-noise steps off.</summary>
    [JsonPropertyName("vg_pyramid_levels")] public int? PyramidLevels { get; init; }

    /// <summary>Detail gain; only meaningful with the pyramid on, and 1.0 makes the pyramid a no-op.</summary>
    [JsonPropertyName("vg_pyramid_gain")] public double? PyramidGain { get; init; }

    /// <summary>Soft-threshold strength on the finest band; only meaningful with the pyramid on.</summary>
    [JsonPropertyName("vg_denoise_k")] public double? DenoiseK { get; init; }

    private static readonly JsonSerializerOptions Options = new()
    {
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
    };

    public string ToJson() => JsonSerializer.Serialize(this, Options);
}

/// <summary>
/// One GSVG run over a frame: init → <c>xpe_gsvg_process_ex</c> → shutdown.
///
/// <para>The reason the module reports becomes the chain's status (GUI-C-100 §3.1, accepted by the lead):
/// <c>APPLIED</c> and <c>NO_GRID_DETECTED</c> come back as "it ran" and the chain runner then decides
/// Applied vs AppliedNoChange by comparing pixels; every other reason comes back as "it did not run", so
/// the chain falls back to this stage's input and shows the reason.</para>
/// </summary>
internal static class GuiGsvgRunner
{
    private const int XpeOk = 0;

    /// <summary>The product table shipped beside gsvg.dll (GUI-C-101 decision: setting, default here).</summary>
    internal const string ProductTableFileName = "vg_table_water_csi600_victre.csv";

    /// <summary>What one run did: whether the chain should take the pixels, and the line the GUI shows.</summary>
    internal sealed record Result(bool Ran, ushort[]? Pixels, string Message);

    /// <summary>The default table path: beside the gsvg.dll this process resolved, when it can be found.</summary>
    public static string? DefaultTablePath()
    {
        var dll = GuiNativeLibraryResolver.ResolvedPath("gsvg.dll")
            ?? NativeModuleLibraryLocator.TryFindDll("gsvg.dll", "image-processing");
        var directory = dll is null ? null : Path.GetDirectoryName(dll);
        return string.IsNullOrEmpty(directory) ? null : Path.Combine(directory, ProductTableFileName);
    }

    public static Result Run(ushort[] input, int width, int height, AppSettings settings)
    {
        var mode = GsvgModes.Normalize(settings.GsvgMode);
        if (mode == GsvgModes.None)
        {
            return new Result(false, null, "GSVG is switched off.");
        }

        var virtualGrid = mode == GsvgModes.VirtualGrid;
        var tablePath = virtualGrid ? ResolveTablePath(settings) : null;
        if (virtualGrid)
        {
            if (tablePath is null)
            {
                return new Result(false, null,
                    "The virtual grid needs a parameter table; none was configured and none was found beside gsvg.dll.");
            }

            if (!File.Exists(tablePath))
            {
                return new Result(false, null, $"The virtual-grid table was not found: {tablePath}");
            }
        }

        var config = new GsvgConfig
        {
            GridSuppression = mode == GsvgModes.GridSuppression,
            VirtualGrid = virtualGrid,
            TablePath = tablePath,
            Kvp = virtualGrid ? settings.ExposureKvp : null,
            GridRatio = virtualGrid ? settings.GsvgGridRatio : null,
            GridFrequencyPerCm = virtualGrid ? settings.GsvgGridFrequencyPerCm : null,
            PixelPitchMm = virtualGrid ? settings.PixelPitchMm : null,
            AirSignal = virtualGrid ? settings.GsvgAirSignal : null,
            Iterations = virtualGrid ? settings.GsvgIterations : null,
            // All three keys travel together, ALWAYS — including the off case, which sends levels 0 with
            // the exact companions the module demands there (gain 1.0, k 0).
            //
            // Omitting them to mean "off" was the earlier design and it was wrong: an omitted key means
            // the module's default, and when that default changed from 0 to 4/1.3/2 the GUI's "off"
            // silently became "on with the module's settings". Measured — with the new DLL, levels 0
            // drew the same pixels as the module's default pyramid, not the same as no pyramid.
            PyramidLevels = virtualGrid ? settings.GsvgPyramidLevels : null,
            PyramidGain = virtualGrid ? (settings.GsvgPyramidLevels > 0 ? settings.GsvgPyramidGain : 1.0) : null,
            DenoiseK = virtualGrid ? (settings.GsvgPyramidLevels > 0 ? settings.GsvgDenoiseK : 0.0) : null,
        };

        var handle = IntPtr.Zero;
        try
        {
            var initCode = XpeGsvgNative.xpe_gsvg_init(out handle, config.ToJson());
            if (initCode != XpeOk || handle == IntPtr.Zero)
            {
                return new Result(false, null, $"xpe_gsvg_init refused the configuration ({initCode}). See the alerts for the reason.");
            }

            var count = checked(width * height);
            var output = new ushort[count];
            var native = XpeGsvgResultNative.Create();
            var code = XpeGsvgNative.xpe_gsvg_process_ex(
                handle, input, (nuint)count, output, (nuint)count, width, height,
                null, 0, null, 0, ref native);

            return Interpret(code, native, output);
        }
        finally
        {
            if (handle != IntPtr.Zero)
            {
                XpeGsvgNative.xpe_gsvg_shutdown(handle);
            }
        }
    }

    /// <summary>Where the table comes from: the setting when it is filled in, otherwise beside the DLL.</summary>
    private static string? ResolveTablePath(AppSettings settings) =>
        string.IsNullOrWhiteSpace(settings.GsvgTablePath) ? DefaultTablePath() : settings.GsvgTablePath;

    /// <summary>The module's answer, as the chain's stage executor sees it (GUI-C-100 §3.1).</summary>
    internal static Result Interpret(int code, XpeGsvgResultNative native, ushort[] output)
    {
        var reason = (XpeGsvgReasonNative)native.Reason;
        var detail =
            $"reason={reason} ({native.Reason}), vignette={native.VignetteApplied}, grid={native.GridSuppressed}, " +
            $"virtualGrid={native.VirtualGridApplied}, restoredOriginal={native.RestoredOriginal}, code={code}";

        if (code != XpeOk)
        {
            return new Result(false, null, $"GSVG refused this image: {detail}");
        }

        return reason switch
        {
            XpeGsvgReasonNative.Applied => new Result(true, output, $"GSVG applied: {detail}"),

            // The step ran and found nothing to remove. Taking the pixels lets the chain runner compare
            // them and record AppliedNoChange — the case GUI-C-97 risk (a) asked to keep apart from Applied.
            XpeGsvgReasonNative.NoGridDetected => new Result(true, output, $"GSVG found no grid: {detail}"),

            XpeGsvgReasonNative.NotConfigured => new Result(false, null,
                $"GSVG was requested but no correction was enabled in its config — check the key names. {detail}"),
            XpeGsvgReasonNative.ImageTooSmall => new Result(false, null, $"The image is too small for GSVG: {detail}"),
            XpeGsvgReasonNative.GridNotInSubbands => new Result(false, null,
                $"GSVG saw a grid peak but no sub-band confirmed it; nothing was filtered and a grid may remain. {detail}"),
            XpeGsvgReasonNative.VirtualGridRefused => new Result(false, null,
                $"The virtual grid refused this exposure or these settings; the module restored the original. {detail}"),
            _ => new Result(false, null, $"GSVG returned a reason this build does not know: {detail}"),
        };
    }

    /// <summary>The module version, or null when gsvg.dll cannot be loaded.</summary>
    public static string? TryGetVersion()
    {
        try
        {
            var ptr = XpeGsvgNative.xpe_gsvg_version();
            return ptr == IntPtr.Zero ? null : Marshal.PtrToStringAnsi(ptr);
        }
        catch (Exception)
        {
            return null;
        }
    }
}
