// #225 row 9 (GUI-C-196 M2): the basic enhancement stage's logic, apart from the native calls. Free of WPF and of native code so the integration tests link it.
using System.Diagnostics;
using System.Globalization;

namespace ImageProcTest.Services;

/// <summary>
/// A float32 working image held by the enhancement module (design D2): the whole stage runs on it and it is read back once, at the end.
/// The native implementation keeps one buffer allocated by xpe_alloc_image; a test supplies a fake.
/// </summary>
internal interface IEnhanceImage : IDisposable
{
    /// <summary>xpe_log_transform: <c>normFactor * log10(x + 1)</c>. Returns the module's return code (0 = OK).</summary>
    int LogTransform(float normFactor);

    /// <summary>xpe_noise_reduce, bilateral mode. Returns the module's return code.</summary>
    int NoiseReduceBilateral(float sigmaSpace, float sigmaRange);

    /// <summary>xpe_contrast_enhance (CLAHE). Returns the module's return code.</summary>
    int ContrastEnhance(float clipLimit, int tileWidth, int tileHeight);

    /// <summary>xpe_edge_enhance (unsharp masking). Returns the module's return code.</summary>
    int EdgeEnhance(float amount, float radius, float threshold);

    /// <summary>How many pixels hold a NaN or an infinity right now.</summary>
    long CountNonFinite();

    /// <summary>A copy of the pixels.</summary>
    float[] ReadFloats();
}

/// <summary>Opens a float32 working image from 16-bit pixels.</summary>
internal interface IEnhanceBasicBackend
{
    IEnhanceImage Open(ushort[] input, int width, int height);

    /// <summary>GUI-C-234: opens the working image from float values (the corrected image the preprocess stage made, before its 16-bit scaling). A backend that cannot says so; the stage then uses the 16-bit input.</summary>
    IEnhanceImage OpenFloats(float[] input, int width, int height) => throw new NotSupportedException("this backend opens 16-bit input only");
}

/// <summary>What the stage did: the pixels when it ran, the one-line account either way, and the non-finite count the baseline's verdict needs.</summary>
internal sealed record EnhanceBasicResult(bool Ran, ushort[]? Pixels, string Summary, long NaNInfCount, float[]? Floats = null);

/// <summary>
/// The <c>enhance_basic</c> stage (design D2/D5): log transform, noise reduction, contrast enhancement and edge enhancement, in that order, on ONE float32
/// image, converted to 16 bits ONCE at the end. If any step is refused, or leaves a non-finite value, the whole stage is refused and no pixels are returned:
/// the later steps depend on the earlier ones, so a partial result would not be the pipeline's.
///
/// <para><b>The one conversion back to 16 bits rounds half to even and clamps to 0..65535.</b> That is this stage's rule. The preprocess stage's own conversion
/// (GuiPreprocessRunner.ReadFloatsAsUInt16) normalises by the frame maximum and truncates; the two are different rules on purpose and neither was changed for the
/// other (decision of the GUI-C-196 review: each module keeps its existing rule, the ordinary Apply's output stays as it is). The numbers of pixels the clamp
/// moved are in the summary, so a saturating enhancement is visible.</para>
/// </summary>
internal static class EnhanceBasicStage
{
    /// <param name="floatInput">
    /// GUI-C-234: when given (and as long as <paramref name="input"/>), the working image is opened from these values instead of from <paramref name="input"/>. The ordinary chain passes the preprocess
    /// stage's corrected float image so that the enhancement runs on what the module produced, not on its 16-bit rescaling (the rescaling changes the log transform); <paramref name="input"/> still
    /// carries the length check and the fallback. The Deterministic Baseline passes none: its rule (each stage keeps its own conversion) is unchanged.
    /// </param>
    public static EnhanceBasicResult Run(ushort[] input, int width, int height, IEnhanceBasicBackend backend, float[]? floatInput = null)
    {
        ArgumentNullException.ThrowIfNull(input);
        ArgumentNullException.ThrowIfNull(backend);

        if (width <= 0 || height <= 0 || input.Length != checked(width * height))
        {
            return new EnhanceBasicResult(false, null, $"enhance_basic: the input has {input.Length} pixels for {width} x {height}.", 0);
        }

        var parts = new List<string>();
        long nanInf = 0;
        IEnhanceImage image;
        try
        {
            image = floatInput is not null && floatInput.Length == input.Length ? backend.OpenFloats(floatInput, width, height) : backend.Open(input, width, height);
        }
        catch (Exception ex)
        {
            return new EnhanceBasicResult(false, null, $"enhance_basic: could not open the working image: {ex.Message}", 0);
        }

        using (image)
        {
            var steps = new (string Name, Func<int> Call)[]
            {
                ("log", () => image.LogTransform(BaselineParameters.LogNormFactor)),
                ($"noise(bilateral {F(BaselineParameters.NoiseSigmaSpace)}/{F(BaselineParameters.NoiseSigmaRange)})", () => image.NoiseReduceBilateral(BaselineParameters.NoiseSigmaSpace, BaselineParameters.NoiseSigmaRange)),
                ($"contrast(clahe {F(BaselineParameters.ClaheClipLimit)} {BaselineParameters.ClaheTileWidth}x{BaselineParameters.ClaheTileHeight})", () => image.ContrastEnhance(BaselineParameters.ClaheClipLimit, BaselineParameters.ClaheTileWidth, BaselineParameters.ClaheTileHeight)),
                ($"edge(usm {F(BaselineParameters.UsmAmount)}/{F(BaselineParameters.UsmRadius)}/{F(BaselineParameters.UsmThreshold)})", () => image.EdgeEnhance(BaselineParameters.UsmAmount, BaselineParameters.UsmRadius, BaselineParameters.UsmThreshold)),
            };

            foreach (var (name, call) in steps)
            {
                var watch = Stopwatch.StartNew();
                int code;
                try
                {
                    code = call();
                }
                catch (Exception ex)
                {
                    return new EnhanceBasicResult(false, null, $"enhance_basic: {name} threw {ex.GetType().Name}: {ex.Message}", nanInf);
                }

                if (code != 0)
                {
                    return new EnhanceBasicResult(false, null, $"enhance_basic: {name} was refused by the module (return code {code}); {string.Join("; ", parts)}".TrimEnd(' ', ';'), nanInf);
                }

                var bad = image.CountNonFinite();
                nanInf += bad;
                if (bad != 0)
                {
                    return new EnhanceBasicResult(false, null, $"enhance_basic: {name} left {bad} non-finite value(s); nothing was returned", nanInf);
                }

                parts.Add($"{name} ok {watch.Elapsed.TotalMilliseconds:0} ms");
            }

            var floats = image.ReadFloats();
            if (floats.Length != input.Length)
            {
                return new EnhanceBasicResult(false, null, $"enhance_basic: the working image has {floats.Length} pixels, not {input.Length}", nanInf);
            }

            var output = new ushort[floats.Length];
            long low = 0, high = 0;
            for (var i = 0; i < floats.Length; i++)
            {
                var rounded = MathF.Round(floats[i], MidpointRounding.ToEven);
                if (rounded < 0f)
                {
                    low++;
                    output[i] = 0;
                }
                else if (rounded > ushort.MaxValue)
                {
                    high++;
                    output[i] = ushort.MaxValue;
                }
                else
                {
                    output[i] = (ushort)rounded;
                }
            }

            parts.Add($"to 16 bit: round-half-even, clamped below {low} above {high}");
            return new EnhanceBasicResult(true, output, "enhance_basic: " + string.Join("; ", parts) + (floatInput is not null ? "; input: the corrected float image" : string.Empty), nanInf, floats);
        }
    }

    private static string F(float value) => value.ToString("0.###", CultureInfo.InvariantCulture);
}
