// #225 row 9 (GUI-C-196 M6, Codex #73 finding 1): the hops that carry a stage's non-finite count from the stage to the chain result. Free of WPF so the integration
// tests link the SAME functions the real backend calls, instead of building a verdict input by hand.
namespace ImageProcTest.Services;

public static class BaselineStageAdapters
{
    /// <summary>
    /// How many values are NaN or an infinity (exponent bits all ones). Counted on the float image BEFORE it is scaled to 16 bits: the conversion cannot represent
    /// them, so afterwards they are indistinguishable from ordinary pixels.
    /// </summary>
    public static long CountNonFinite(ReadOnlySpan<float> values)
    {
        long count = 0;
        foreach (var value in values)
        {
            if ((BitConverter.SingleToInt32Bits(value) & 0x7F800000) == 0x7F800000)
            {
                count++;
            }
        }

        return count;
    }

    /// <summary>
    /// The preprocess stage's count (Codex #76 finding 2): the gain stage's float output AND the defect stage's. The gain output feeds the defect stage, which can fill a
    /// bad pixel with a finite value, so counting only the final image would let a non-finite gain result pass.
    /// </summary>
    public static long CountPreprocessNonFinite(ReadOnlySpan<float> gainOut, ReadOnlySpan<float> defectOut) => CountNonFinite(gainOut) + CountNonFinite(defectOut);

    /// <summary>The enhance stage's result as the chain runner takes it, with its non-finite count carried.</summary>
    internal static StageExecution FromEnhance(EnhanceBasicResult result) =>
        new(result.Ran, result.Pixels, result.Summary, result.NaNInfCount);

    /// <summary>The preprocess stage's result as the chain runner takes it, with the count the runner made on its float image.</summary>
    public static StageExecution FromPreprocess(bool ran, ushort[]? pixels, string summary, long nonFiniteCount) =>
        new(ran, pixels, summary, nonFiniteCount);
}
