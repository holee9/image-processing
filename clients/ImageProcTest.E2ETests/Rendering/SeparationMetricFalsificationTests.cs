// #149 G-4 (GUI-C-55): does the separation metric measure separation?
using System.Windows.Media;
using Xunit;
using Xunit.Abstractions;
using static ImageProcTest.E2ETests.Rendering.ComparisonRenderHarness;

namespace ImageProcTest.E2ETests.Rendering;

/// <summary>
/// Falsifies the patch-versus-background separation metric that GUI-C-54 introduced and used to
/// conclude that <c>DifferenceHeatmap</c> compresses local contrast.
///
/// <para>A metric defined in one card and trusted in the same card has never been tested. Four
/// injections are applied, each with a prediction that can fail:</para>
///
/// <list type="number">
/// <item><b>Nothing changed</b> — separation must sit near zero, or the masks are still mixing
/// something in.</item>
/// <item><b>Maximum contrast on the reference path</b> — must approach the channel range, or the
/// METRIC compresses and part of the 16 % attributed to the renderer belongs here instead.</item>
/// <item><b>Mask deliberately misaligned</b> — separation must fall, or the mask is not tracking the
/// patch's position.</item>
/// <item><b>Sign flipped</b> — a brighter and a darker patch of the same magnitude must measure the
/// same, or the metric depends on sign.</item>
/// </list>
///
/// <para><b>(4) is not a choice of sign convention.</b> #149 leaves the sign convention undecided;
/// this asks only whether the metric is sensitive to sign. A metric that is would be smuggling that
/// undecided choice into every number reported so far.</para>
///
/// <para>(2) and (4) are the two that can weaken GUI-C-54's conclusion, so they are the ones written
/// first.</para>
/// </summary>
[Trait("Category", "Rendering")]
public sealed class SeparationMetricFalsificationTests(ITestOutputHelper output)
{
    private const byte Background = 128;
    private const int Patch = 32;

    /// <summary>
    /// (2) On the reference path a Δ255 patch should separate by nearly the whole channel range.
    ///
    /// GUI-C-54 reported 251.2 for exactly this and used it as the yardstick the heatmap's 40.3 was
    /// compared against. If the metric itself lost a large fraction of the available range, the
    /// "16 %" would be partly an artefact of the yardstick.
    /// </summary>
    [Fact]
    public void MaximumContrast_OnTheReferencePath_ApproachesTheChannelRange()
    {
        var (reference, patchFree) = OnStaThread(() =>
        {
            var expected = BuildImage((_, _) => 0, 255, Patch);
            var none = BuildImage((_, _) => 0, 0, 0);
            return (RenderAll(expected, expected, "SourceOnly"), RenderAll(none, none, "SourceOnly"));
        });

        var masks = Masks(reference, patchFree);
        var separation = Separation(reference, masks);

        output.WriteLine(
            $"(2) reference Δ255: separation {separation:F1}/255 = {separation / 255 * 100:F1}% of the range");

        Assert.True(
            separation > 240,
            $"The reference path separated by only {separation:F1}/255 at maximum contrast, so the " +
            "METRIC compresses — GUI-C-54's 16 % figure would then be partly the metric's doing and " +
            "must be re-stated.");
    }

    /// <summary>
    /// (4) A brighter patch and a darker patch of the same magnitude, measured on BOTH paths.
    ///
    /// <para>This asks whether the metric depends on sign. It does NOT choose a sign convention —
    /// #149 leaves that open — and the measurement is split across the two paths precisely so the
    /// answer can be attributed. On the reference path the input is <c>|source - processed|</c>, which
    /// is the same image either way, so an equal reading there means the METRIC is sign-blind. Any
    /// difference that remains on the heatmap path is then the RENDERER's.</para>
    ///
    /// <para>Measured: the reference path reads the same for both signs; the heatmap path does not.
    /// The metric is sound and the renderer carries the sign — it composites the processed layer over
    /// the source, so a brighter change brightens and a darker change darkens, which an absolute
    /// difference cannot do. GUI-C-54's numbers were all taken with a brighter patch and therefore
    /// describe one sign only; see that report's correction.</para>
    /// </summary>
    [Fact]
    public void FlippedSign_IsTheRenderersDoing_NotTheMetrics()
    {
        const int delta = 64;

        var measured = OnStaThread(() =>
        {
            var none = BuildImage((_, _) => 0, 0, 0);
            var patchFree = RenderAll(none, none, "SourceOnly");

            (double Heatmap, double Reference) Measure(int signedDelta)
            {
                var source = BuildImage((_, _) => Background, 0, 0);
                var processed = BuildImage((_, _) => Background, signedDelta, Patch);

                // |source - processed| is the same image for +delta and -delta.
                var expected = BuildImage((_, _) => 0, Math.Abs(signedDelta), Patch);
                var referenceRender = RenderAll(expected, expected, "SourceOnly");
                var masks = Masks(referenceRender, patchFree);

                return (
                    Separation(RenderAll(source, processed, "DifferenceHeatmap"), masks),
                    Separation(referenceRender, masks));
            }

            return (Brighter: Measure(delta), Darker: Measure(-delta));
        });

        output.WriteLine(
            $"(4) reference path: +{delta} -> {measured.Brighter.Reference:F1}/255, " +
            $"-{delta} -> {measured.Darker.Reference:F1}/255 " +
            $"(gap {Math.Abs(measured.Brighter.Reference - measured.Darker.Reference):F1})");
        output.WriteLine(
            $"(4) heatmap path:   +{delta} -> {measured.Brighter.Heatmap:F1}/255, " +
            $"-{delta} -> {measured.Darker.Heatmap:F1}/255 " +
            $"(gap {Math.Abs(measured.Brighter.Heatmap - measured.Darker.Heatmap):F1})");

        // The metric is sign-blind: same input magnitude, same reading.
        Assert.True(
            Math.Abs(measured.Brighter.Reference - measured.Darker.Reference) < 2.0,
            $"The metric read {measured.Brighter.Reference:F1} and {measured.Darker.Reference:F1} for " +
            "the SAME reference image, so the metric itself depends on sign and every separation " +
            "reported so far carries #149's undecided sign convention inside it.");

        // The renderer is not. Recorded as the measured contract, not as a requirement judgement:
        // #149 has not decided whether a signed response is wanted.
        Assert.True(
            Math.Abs(measured.Brighter.Heatmap - measured.Darker.Heatmap) > 5.0,
            "The heatmap now reads the same for a brighter and a darker change. That is a rendering " +
            "change, not a test failure — re-measure #149 G-4 and report, do not relax this.");
    }

    /// <summary>
    /// (1) With nothing changed there is no patch, so the metric must read near zero.
    ///
    /// The masks come from a real patch render; applying them to a render where the images agree asks
    /// whether those two regions differ for reasons other than the patch — chrome, letterboxing, or a
    /// gradient the masks happen to straddle.
    /// </summary>
    [Fact]
    public void NothingChanged_MeasuresNearZero()
    {
        var separation = OnStaThread(() =>
        {
            var none = BuildImage((_, _) => 0, 0, 0);
            var patchFree = RenderAll(none, none, "SourceOnly");
            var expected = BuildImage((_, _) => 0, 64, Patch);
            var masks = Masks(RenderAll(expected, expected, "SourceOnly"), patchFree);

            var flat = BuildImage((_, _) => Background, 0, 0);
            return Separation(RenderAll(flat, flat, "DifferenceHeatmap"), masks);
        });

        output.WriteLine($"(1) identical inputs: separation {separation:F1}/255");

        Assert.True(
            Math.Abs(separation) < 5.0,
            $"With nothing changed the metric read {separation:F1}/255. The two mask regions differ " +
            "for a reason other than the patch, so every separation reported carries that offset.");
    }

    /// <summary>
    /// (3) Move the patch away from where the mask says it is and the measurement must collapse.
    ///
    /// If it does not, the mask is not tracking position and the metric would report the same number
    /// wherever the change actually happened — which is precisely the question a reader asks of a
    /// difference view.
    /// </summary>
    [Fact]
    public void MisalignedMask_CollapsesTheMeasurement()
    {
        var (aligned, misaligned) = OnStaThread(() =>
        {
            var none = BuildImage((_, _) => 0, 0, 0);
            var patchFree = RenderAll(none, none, "SourceOnly");

            // Mask located from a patch at the centre.
            var expected = BuildImage((_, _) => 0, 255, Patch);
            var masks = Masks(RenderAll(expected, expected, "SourceOnly"), patchFree);

            var source = BuildImage((_, _) => Background, 0, 0);
            var centred = BuildImage((_, _) => Background, 255, Patch);
            var shifted = BuildImage((_, _) => Background, 255, Patch, offsetX: 64, offsetY: 64);

            return (
                Separation(RenderAll(source, centred, "DifferenceHeatmap"), masks),
                Separation(RenderAll(source, shifted, "DifferenceHeatmap"), masks));
        });

        output.WriteLine(
            $"(3) aligned {aligned:F1}/255 · patch moved 64px off the mask {misaligned:F1}/255");

        Assert.True(
            Math.Abs(misaligned) < Math.Abs(aligned) / 2,
            $"Moving the patch off the mask changed the measurement from {aligned:F1} to " +
            $"{misaligned:F1}; the mask is not tracking where the change actually is.");
    }
}
