// #149 G-4 (GUI-C-54): can a local difference be told apart from its background on screen?
using Xunit;
using Xunit.Abstractions;
using static ImageProcTest.E2ETests.Rendering.ComparisonRenderHarness;

namespace ImageProcTest.E2ETests.Rendering;

/// <summary>
/// Measures whether <c>DifferenceHeatmap</c> separates a small changed region from an unchanged
/// background — the shape a reader actually looks for.
///
/// <para>GUI-C-53 measured uniform inputs and found the deviation largest where the inputs were
/// IDENTICAL. Uniform images cannot express the case that matters in practice: mostly the same, one
/// spot different. This measures that case.</para>
///
/// <para><b>What is asserted and what is not.</b> No requirement is judged here. MENU-001 L288 gives
/// the value (<c>|source - processed|</c>); COMPARE-001 L68 leaves the sign open; no document
/// specifies a colour mapping. The reference is therefore the absolute difference rendered through
/// the same control in <c>SourceOnly</c> — same backdrop, same scaling, same chrome — and the claim
/// is only "patch and background separate by this much in each".</para>
///
/// <para><b>Scope note, superseded (GUI-C-57).</b> Until #149 was decided, every patch here had to be
/// BRIGHTER than its background, because the renderer's answer depended on the sign (22.3 for a +64
/// patch versus -15.7 for a -64 one, GUI-C-55). The mode now draws <c>|source - processed|</c>, which
/// cannot carry a sign, and the measured gap between the two signs is 0.0 — so these numbers describe
/// both.</para>
///
/// <para><b>These numbers pin current behaviour on purpose.</b> If the renderer changes, these tests
/// fail — that is the signal, not a nuisance. Updating the numbers to make a failure go away removes
/// the only thing standing between a silent rendering change and a reader who trusts the picture.
/// Re-measure and report; do not re-baseline.</para>
/// </summary>
[Trait("Category", "Rendering")]
public sealed class DifferenceLocalContrastTests(ITestOutputHelper output)
{
    private const byte Background = 128;

    /// <summary>Patch deltas spanning subtle to extreme.</summary>
    public static IEnumerable<object[]> PatchDeltas() => [[8], [64], [255]];

    /// <summary>Patch sizes, to find where a region stops being resolvable at all.</summary>
    public static IEnumerable<object[]> PatchSizes() => [[4], [8], [16], [32], [64]];

    /// <summary>
    /// A local change of Δ separates from its background by THIS much on screen, next to the same
    /// measurement on a true difference image.
    ///
    /// Separation, not max/mean deviation: GUI-C-53's numbers say how far the whole frame is from a
    /// difference image, which cannot answer "would a reader see this spot".
    /// </summary>
    [Theory]
    [MemberData(nameof(PatchDeltas))]
    public void LocalChange_SeparatesFromBackground_ByThisMuch(int delta)
    {
        const int patch = 32;
        var (actual, reference, patchFree) = RenderPair(delta, patch);
        var masks = Masks(reference, patchFree);

        output.WriteLine(
            $"Δ{delta} patch {patch}px ({masks.PatchPixels}px located): " +
            $"heatmap separation {Separation(actual, masks):F1}/255, " +
            $"true-difference separation {Separation(reference, masks):F1}/255");

        // Recorded, not judged: every delta reaches the report, including the benign-looking ones.
        // One row alone would let the fixture pick the conclusion.
        Assert.True(masks.PatchPixels > 0, "The patch was not located in the reference render.");
    }

    /// <summary>
    /// The question this card exists for: is a real Δ8 change buried under the renderer's own
    /// variation?
    ///
    /// The floor is measured with the SAME masks on a render where nothing changed. A signal that
    /// does not clear that floor is one a reader cannot attribute to the image.
    /// </summary>
    [Fact]
    public void SubtleChange_MeasuredAgainstTheRenderersOwnFloor()
    {
        const int patch = 32;

        var (subtleActual, subtleReference, patchFree) = RenderPair(delta: 8, patch);
        var (identicalActual, _, _) = RenderPair(delta: 0, patch);
        var masks = Masks(subtleReference, patchFree);

        var subtle = Separation(subtleActual, masks);
        var trueSubtle = Separation(subtleReference, masks);
        var floor = Math.Abs(Separation(identicalActual, masks));

        output.WriteLine(
            $"Δ8 ({masks.PatchPixels}px located): heatmap separation {subtle:F1}/255 · " +
            $"true-difference separation {trueSubtle:F1}/255 · identical-input floor {floor:F1}/255 · " +
            $"signal-to-floor {Math.Abs(subtle) / Math.Max(floor, 0.01):F2}x");

        Assert.True(masks.PatchPixels > 0, "The patch was not located in the reference render.");
    }

    /// <summary>
    /// Where does a patch stop being resolvable? Reported so "not distinguishable" can be attributed
    /// to the renderer rather than to a patch too small to survive scaling — the fixture trap the
    /// card names.
    /// </summary>
    [Theory]
    [MemberData(nameof(PatchSizes))]
    public void PatchSize_ChangesWhatIsResolvable(int patch)
    {
        var (actual, reference, patchFree) = RenderPair(delta: 255, patch);
        var masks = Masks(reference, patchFree);

        output.WriteLine(
            $"patch {patch}px (Δ255, {masks.PatchPixels}px located): " +
            $"heatmap separation {Separation(actual, masks):F1}/255, " +
            $"true-difference separation {Separation(reference, masks):F1}/255");

        Assert.True(patch > 0, "A patch has a size.");
    }

    /// <summary>
    /// A gradient background, which a uniform image cannot express: the mapping may behave
    /// differently where the underlying value varies.
    /// </summary>
    [Fact]
    public void GradientBackground_WithALocalChange()
    {
        const int patch = 32;
        const int delta = 64;

        var (actual, reference, patchFree) = OnStaThread(() =>
        {
            var source = GradientImage();
            var processed = GradientImage(patchDelta: delta, patchSize: patch);
            var expected = BuildImage((_, _) => 0, delta, patch);
            var none = BuildImage((_, _) => 0, 0, 0);

            return (
                RenderAll(source, processed, "DifferenceHeatmap"),
                RenderAll(expected, expected, "SourceOnly"),
                RenderAll(none, none, "SourceOnly"));
        });

        var masks = Masks(reference, patchFree);
        output.WriteLine(
            $"gradient background, Δ{delta} patch {patch}px ({masks.PatchPixels}px located): " +
            $"heatmap separation {Separation(actual, masks):F1}/255, " +
            $"true-difference separation {Separation(reference, masks):F1}/255");

        Assert.True(masks.PatchPixels > 0, "The patch was not located in the reference render.");
    }

    /// <summary>
    /// P3 (#149 decision, GUI-C-57): the mode must separate a local change from its background by
    /// what the true difference separates it by — not by a fraction of it.
    ///
    /// <para><b>Measured before the fix</b> (GUI-C-54, same metric, same masks): Δ64 read 22.3 where
    /// the true difference read 60.2 — <b>37 %</b>; Δ255 read 40.3. The fix is judged by this number
    /// moving to ~100 %, and the assertion is a ratio rather than an absolute so it keeps meaning if
    /// the fixture's contrast changes.</para>
    ///
    /// <para>Δ255 is excluded from the ratio for a fixture reason, not a convenient one: +255 on a
    /// background of 128 clamps, so the images differ by 127 and the row measures clamping as much as
    /// rendering. Δ8 is excluded because its true separation (4.2) is close enough to the floor that
    /// a ratio over it is noise, not a measurement — it is reported by the test above instead.</para>
    /// </summary>
    [Fact]
    public void LocalSeparation_MatchesTheTrueDifference()
    {
        const int patch = 32;
        const int delta = 64;

        var (actual, reference, patchFree) = RenderPair(delta, patch);
        var masks = Masks(reference, patchFree);

        var heatmap = Separation(actual, masks);
        var truth = Separation(reference, masks);
        var ratio = heatmap / truth;

        output.WriteLine(
            $"P3 Δ{delta} patch {patch}px: heatmap {heatmap:F1}/255 · true difference {truth:F1}/255 " +
            $"· ratio {ratio:P1} (GUI-C-54 measured 37 % here)");

        Assert.True(
            ratio > 0.9,
            $"The mode separated the patch by {heatmap:F1} where the true difference separates it by " +
            $"{truth:F1} ({ratio:P1}). It is compressing local contrast again — re-measure #149 G-4 " +
            "and report; do not lower this ratio.");
    }

    /// <summary>
    /// Renders the heatmap and the true-difference reference for one patch configuration.
    ///
    /// <para><b>The reference uses the difference the images actually carry, not the requested
    /// delta.</b> A patch of +255 on a background of 128 clamps to 255, so the real difference is
    /// 127 — building the reference from 255 would compare the render against a difference that is
    /// not in the inputs. GUI-C-54 built it from the requested delta, which is why its Δ255 rows
    /// reported a true-difference separation of 251 where the images only ever differed by 127; the
    /// ratios in that report are understated for those rows. Corrected here (GUI-C-57).</para>
    /// </summary>
    private static (byte[] Actual, byte[] Reference, byte[] PatchFree) RenderPair(int delta, int patch) =>
        OnStaThread(() =>
        {
            var carried = Math.Clamp(Background + delta, 0, 255) - Background;

            var source = BuildImage((_, _) => Background, 0, 0);
            var processed = BuildImage((_, _) => Background, delta, patch);
            var expected = BuildImage((_, _) => 0, carried, patch);
            var patchFree = BuildImage((_, _) => 0, 0, 0);

            return (
                RenderAll(source, processed, "DifferenceHeatmap"),
                RenderAll(expected, expected, "SourceOnly"),
                RenderAll(patchFree, patchFree, "SourceOnly"));
        });
}
