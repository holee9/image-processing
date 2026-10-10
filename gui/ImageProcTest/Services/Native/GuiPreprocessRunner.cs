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
        string bodyPart,
        float kVp,
        float pixelPitchMm,
        bool measureExposureIndex = false)
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

            // GUI-C-232b (SRS-CALIB-SAFE-004, leader decision): the operator app has ONE preprocess path, xpe_preprocess_pipeline_out, which never writes the input frame. A DLL without that
            // export is a FAILURE with an instruction, not a reason to run the older in-place stage calls.
            try
            {
                return RunPipelineOut(rawPixels, width, height, bodyPart, kVp, pixelPitchMm, measureExposureIndex);
            }
            catch (EntryPointNotFoundException)
            {
                return new PreprocessRunResult(false, "Preprocessing not run: this xpe_preprocess.dll has no xpe_preprocess_pipeline_out (the DLL is too old). Update the native DLLs and start the app again.", null);
            }
        }
        finally
        {
            XpePreprocessNative.xpe_preprocess_shutdown();
        }
    }

    /// <summary>
    /// The configuration the app passes to the pipeline: the product path of the first stage, as the module's own measurements use it (modules/preprocess/tests/test_zz_a241_measure.cpp, cfgFinal).
    /// Temperature compensation, nonlinearity and binning are bypassed because the app has no temperature, no linearity table and no binning setting to give them; ghost is off because no ghost handle is given.
    /// </summary>
    internal const string PipelineConfigJson = "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true}";

    /// <summary>
    /// The calibration maps are already loaded by <see cref="Run"/>. The input frame goes in read-only and the corrected float32 frame comes out in a separate buffer
    /// (<c>xpe_preprocess_pipeline_out</c>): the raw image the user opened is never written (SRS-CALIB-SAFE-004).
    /// </summary>
    private static PreprocessRunResult RunPipelineOut(ushort[] rawPixels, int width, int height, string bodyPart,
        float kVp, float pixelPitchMm, bool measureExposureIndex)
    {
        var metadata = XpeImageMetadataNative.Create(bodyPart, kVp: kVp, mAs: 2.0f, sidMm: 1000.0f, pixelPitchMm: pixelPitchMm);

        var input = default(XpeImageBufferNative);
        var output = default(XpeImageBufferNative);
        var allocated = new List<Action>();

        try
        {
            if (!TryAlloc(width, height, XpePixelFormatNative.UInt16, out input, allocated, out var reason) ||
                !TryAlloc(width, height, XpePixelFormatNative.Float32, out output, allocated, out reason))
            {
                return new PreprocessRunResult(false, reason, null);
            }

            var count = width * height;
            var signed = new short[count];
            Buffer.BlockCopy(rawPixels, 0, signed, 0, count * sizeof(ushort));
            Marshal.Copy(signed, 0, input.Data, count);

            var code = XpePreprocessNative.xpe_preprocess_pipeline_out(ref input, ref output, ref metadata, IntPtr.Zero, IntPtr.Zero, PipelineConfigJson);
            if (code != XpeOk)
            {
                return new PreprocessRunResult(false, $"xpe_preprocess_pipeline_out failed ({code}).", null);
            }

            var exposure = measureExposureIndex ? MeasureUncalibratedExposureIndex(ref output, ref metadata) : string.Empty;
            var floats = ReadFloats(output.Data, count);
            var nonFinite = BaselineStageAdapters.CountPreprocessNonFinite(ReadOnlySpan<float>.Empty, floats);   // the gain stage's intermediate is internal to the pipeline: only the final image is counted
            return new PreprocessRunResult(
                true,
                $"Preprocess: xpe_preprocess_pipeline_out (offset -> gain -> defect, input kept) on {width}x{height} ({bodyPart}).{exposure}",
                ScaleToUInt16(floats),
                nonFinite,
                floats);
        }
        finally
        {
            for (var i = allocated.Count - 1; i >= 0; i--)
            {
                allocated[i]();
            }
        }
    }

    /// <summary>
    /// EI and DI of the corrected float image, as text for the stage's summary. A MEASUREMENT, never a pass criterion: the module's S0 reference (1000) has not
    /// been checked against the gui's gain scale, so the value is labelled "uncalibrated EI" (the leader's wording: 보정 안 된 EI). A failure to measure is
    /// said, not hidden, and does not fail the stage.
    /// </summary>
    private static string MeasureUncalibratedExposureIndex(ref XpeImageBufferNative image, ref XpeImageMetadataNative metadata)
    {
        const string Label = " uncalibrated EI (보정 안 된 EI)";
        try
        {
            var code = XpeExposureIndexNative.xpe_calc_exposure_index(ref image, ref metadata, out var ei, out var di);
            return code == XpeOk
                ? string.Create(System.Globalization.CultureInfo.InvariantCulture, $"{Label} = {ei:0.##}, DI = {di:0.##} (measured, not a pass criterion; S0 reference not verified against the gui gain scale).")
                : $"{Label} not measured: xpe_calc_exposure_index returned {code}.";
        }
        catch (Exception ex) when (ex is DllNotFoundException or EntryPointNotFoundException)
        {
            return $"{Label} not measured: {ex.Message}";
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

    private static float[] ReadFloats(IntPtr source, int count)
    {
        var floats = new float[count];
        Marshal.Copy(source, floats, 0, count);
        return floats;
    }

    /// <summary>
    /// Float32 output scaled back into UInt16 for the preview. The scale is display-only — the
    /// corrected values themselves stay in the native buffer's domain. Counting happens before this (the caller): the
    /// conversion cannot represent NaN or an infinity, so afterwards they look like ordinary pixels.
    /// </summary>
    private static ushort[] ScaleToUInt16(float[] floats)
    {
        var count = floats.Length;
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
