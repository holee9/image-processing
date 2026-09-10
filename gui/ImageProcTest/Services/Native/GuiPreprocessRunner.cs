// #141 (Phase 1a): run the preprocess stages against a loaded frame.
using System.IO;
using System.Runtime.InteropServices;
using ImageProcTest.Models;

namespace ImageProcTest.Services.Native;

/// <summary>
/// Drives <c>xpe_preprocess.dll</c> stage by stage: init → load calibration → offset → gain →
/// defect → shutdown.
///
/// Written for the gui rather than shared from clients. The clients-side implementation
/// (<c>NativePreprocessPreviewService</c>) reaches into <c>XpeCommonApi</c> 49 times, and that type's
/// static constructor registers a DllImport resolver; gui already registers its own (GUI-C-32) and
/// the runtime allows exactly one per assembly, so linking it in would throw on first touch — the
/// wall measured in GUI-C-13. Only what the gui calls lives here.
///
/// Calibration files are NOT in the repository. They are produced at run time by
/// <c>xpe_calib_fixture_gen</c> (QA-A-36) into a directory the settings point at, which is why a
/// missing file is reported as a plain reason rather than treated as a defect.
/// </summary>
internal static class GuiPreprocessRunner
{
    private const int XpeOk = 0;

    /// <summary>Calibration file names the generator writes.</summary>
    private const string OffsetFile = "offset.xcal";
    private const string GainFile = "gain.xcal";
    private const string DefectFile = "defect.xcal";

    /// <summary>
    /// Runs the three correction stages. Every failure path returns a reason instead of throwing:
    /// the caller surfaces it as an alert, and a missing calibration set is an expected state.
    /// </summary>
    public static PreprocessRunResult Run(
        ushort[] rawPixels,
        int width,
        int height,
        string offsetDirectory,
        string gainDirectory,
        string defectDirectory,
        string bodyPart)
    {
        var offset = Path.Combine(offsetDirectory, OffsetFile);
        var gain = Path.Combine(gainDirectory, GainFile);
        var defect = Path.Combine(defectDirectory, DefectFile);

        var missing = new[] { offset, gain, defect }.Where(p => !File.Exists(p)).ToArray();
        if (missing.Length > 0)
        {
            return new PreprocessRunResult(
                false,
                $"Preprocessing skipped: calibration file(s) not found — {string.Join(", ", missing)}. " +
                "Generate a set with xpe_calib_fixture_gen --out <dir> and point the calibration " +
                "directories at it.",
                null);
        }

        var initCode = XpePreprocessNative.xpe_preprocess_init(null);
        if (initCode != XpeOk)
        {
            return new PreprocessRunResult(false, $"xpe_preprocess_init failed ({initCode}).", null);
        }

        try
        {
            foreach (var (path, load, name) in new (string, Func<string, int>, string)[]
                     {
                         (offset, XpePreprocessNative.xpe_calib_load_offset, "offset"),
                         (gain, XpePreprocessNative.xpe_calib_load_gain, "gain"),
                         (defect, XpePreprocessNative.xpe_calib_load_defect_map, "defect"),
                     })
            {
                var code = load(path);
                if (code != XpeOk)
                {
                    return new PreprocessRunResult(false, $"Loading {name} calibration failed ({code}).", null);
                }
            }

            return RunStages(rawPixels, width, height, bodyPart);
        }
        finally
        {
            XpePreprocessNative.xpe_preprocess_shutdown();
        }
    }

    /// <summary>
    /// offset (UInt16→UInt16) → gain (UInt16→Float32) → defect (Float32→Float32).
    ///
    /// The formats are the header's, not a guess: each stage declares its input and output format
    /// (preprocess_api.h), and allocating the wrong one is a silent wrong answer rather than an error.
    /// </summary>
    private static PreprocessRunResult RunStages(ushort[] rawPixels, int width, int height, string bodyPart)
    {
        var metadata = XpeImageMetadataNative.Create(bodyPart, kVp: 70.0f, mAs: 2.0f, sidMm: 1000.0f, pixelPitchMm: 0.14f);

        var input = default(XpeImageBufferNative);
        var offsetOut = default(XpeImageBufferNative);
        var gainOut = default(XpeImageBufferNative);
        var defectOut = default(XpeImageBufferNative);
        var allocated = new List<Action>();

        try
        {
            if (!TryAlloc(width, height, XpePixelFormatNative.UInt16, out input, allocated, out var reason) ||
                !TryAlloc(width, height, XpePixelFormatNative.UInt16, out offsetOut, allocated, out reason) ||
                !TryAlloc(width, height, XpePixelFormatNative.Float32, out gainOut, allocated, out reason) ||
                !TryAlloc(width, height, XpePixelFormatNative.Float32, out defectOut, allocated, out reason))
            {
                return new PreprocessRunResult(false, reason, null);
            }

            var count = width * height;
            var signed = new short[count];
            Buffer.BlockCopy(rawPixels, 0, signed, 0, count * sizeof(ushort));
            Marshal.Copy(signed, 0, input.Data, count);

            var offsetCode = XpePreprocessNative.xpe_offset_correct(ref input, ref offsetOut, ref metadata);
            if (offsetCode != XpeOk)
            {
                return new PreprocessRunResult(false, $"xpe_offset_correct failed ({offsetCode}).", null);
            }

            var gainCode = XpePreprocessNative.xpe_gain_correct(ref offsetOut, ref gainOut, ref metadata);
            if (gainCode != XpeOk)
            {
                return new PreprocessRunResult(false, $"xpe_gain_correct failed ({gainCode}).", null);
            }

            var defectCode = XpePreprocessNative.xpe_defect_correct(ref gainOut, ref defectOut, ref metadata);
            if (defectCode != XpeOk)
            {
                return new PreprocessRunResult(false, $"xpe_defect_correct failed ({defectCode}).", null);
            }

            return new PreprocessRunResult(
                true,
                $"Preprocess: offset -> gain -> defect on {width}x{height} ({bodyPart}).",
                ReadFloatsAsUInt16(defectOut.Data, count));
        }
        finally
        {
            for (var i = allocated.Count - 1; i >= 0; i--)
            {
                allocated[i]();
            }
        }
    }

    private static bool TryAlloc(
        int width,
        int height,
        XpePixelFormatNative format,
        out XpeImageBufferNative buffer,
        List<Action> allocated,
        out string reason)
    {
        buffer = default;
        var code = XpeCommonNative.xpe_alloc_image((uint)width, (uint)height, format, out buffer);
        if (code != XpeOk)
        {
            reason = $"xpe_alloc_image({format}) failed ({code}).";
            return false;
        }

        var local = buffer;
        allocated.Add(() => XpeCommonNative.xpe_free_image(ref local));
        reason = string.Empty;
        return true;
    }

    /// <summary>
    /// Float32 output scaled back into UInt16 for the preview. The scale is display-only — the
    /// corrected values themselves stay in the native buffer's domain.
    /// </summary>
    private static ushort[] ReadFloatsAsUInt16(IntPtr source, int count)
    {
        var floats = new float[count];
        Marshal.Copy(source, floats, 0, count);

        var max = 0.0f;
        for (var i = 0; i < count; i++)
        {
            if (floats[i] > max) max = floats[i];
        }

        var scale = max > 0.0f ? ushort.MaxValue / max : 0.0f;
        var pixels = new ushort[count];
        for (var i = 0; i < count; i++)
        {
            var value = floats[i] * scale;
            pixels[i] = value <= 0.0f ? (ushort)0
                : value >= ushort.MaxValue ? ushort.MaxValue
                : (ushort)value;
        }

        return pixels;
    }
}
