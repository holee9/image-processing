// #149 G-4 (GUI-C-54): can a local difference be told apart from its background on screen?
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using ImageProcTest.Controls;
using Xunit;
using Xunit.Abstractions;

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
/// <para><b>These numbers pin current behaviour on purpose.</b> If the renderer changes, these tests
/// fail — that is the signal, not a nuisance. Updating the numbers to make a failure go away removes
/// the only thing standing between a silent rendering change and a reader who trusts the picture.
/// Re-measure and report; do not re-baseline.</para>
/// </summary>
[Trait("Category", "Rendering")]
public sealed class DifferenceLocalContrastTests(ITestOutputHelper output)
{
    /// <summary>
    /// Canvas size. 256 rather than 64: at 64 the control letterboxed the image into roughly a third
    /// of the frame and painted its HUD plate and label inside the drawn area, so a fixed sample
    /// region mixed backdrop, chrome and image (measured in GUI-C-54 by profiling the output).
    /// </summary>
    private const int Size = 256;

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

    /// <summary>Patch and background pixel masks located in the reference render.</summary>
    private readonly record struct RegionMasks(bool[] Patch, bool[] Background, int PatchPixels);

    /// <summary>
    /// Mean patch brightness minus mean background brightness, over masks LOCATED in the reference
    /// render rather than assumed from the bitmap's geometry.
    ///
    /// The first version of this measurement assumed the patch sat at the centre of the frame.
    /// Profiling showed that is false — the control letterboxes the image and draws chrome inside it —
    /// so those numbers described the frame layout, not the difference. Same failure mode as
    /// comparing absolute channels against a tinted backdrop in GUI-C-46.
    /// </summary>
    private static double Separation(byte[] pixels, RegionMasks masks)
    {
        double patchSum = 0, patchCount = 0, backSum = 0, backCount = 0;

        for (var p = 0; p < masks.Patch.Length; p++)
        {
            var i = p * 4;
            var luma = (pixels[i] + pixels[i + 1] + pixels[i + 2]) / 3.0;

            if (masks.Patch[p]) { patchSum += luma; patchCount++; }
            else if (masks.Background[p]) { backSum += luma; backCount++; }
        }

        return patchCount == 0 || backCount == 0
            ? double.NaN
            : (patchSum / patchCount) - (backSum / backCount);
    }

    /// <summary>
    /// Builds the masks by DIFFERENCING the reference render against a patch-free render of the same
    /// scene, so the mask is whatever the patch changed and nothing else.
    ///
    /// A brightness threshold was tried first and was wrong: with a subtle patch the reference's own
    /// patch (luma 8 or 64) never crossed the threshold, so the "patch" it located was the HUD plate
    /// and the label — 727 px for both Δ8 and Δ64, identical because it was chrome both times.
    /// Differencing against the patch-free render cancels the chrome exactly and works at any Δ.
    /// </summary>
    private static RegionMasks Masks(byte[] reference, byte[] patchFreeReference)
    {
        var count = Size * Size;
        var bright = new bool[count];

        for (var p = 0; p < count; p++)
        {
            var i = p * 4;
            var changed = Math.Abs(reference[i] - patchFreeReference[i])
                        + Math.Abs(reference[i + 1] - patchFreeReference[i + 1])
                        + Math.Abs(reference[i + 2] - patchFreeReference[i + 2]);
            bright[p] = changed > 6;   // above the renderer's own rounding, below any real patch
        }

        var background = new bool[count];
        for (var y = 0; y < Size; y++)
        {
            for (var x = 0; x < Size; x++)
            {
                var p = (y * Size) + x;
                if (bright[p]) continue;

                var clear = true;
                for (var dy = -4; dy <= 4 && clear; dy++)
                {
                    for (var dx = -4; dx <= 4; dx++)
                    {
                        var ny = y + dy;
                        var nx = x + dx;
                        if (ny < 0 || ny >= Size || nx < 0 || nx >= Size) continue;
                        if (bright[(ny * Size) + nx]) { clear = false; break; }
                    }
                }

                background[p] = clear;
            }
        }

        return new RegionMasks(bright, background, bright.Count(v => v));
    }

    /// <summary>Renders the heatmap and the true-difference reference for one patch configuration.</summary>
    private static (byte[] Actual, byte[] Reference, byte[] PatchFree) RenderPair(int delta, int patch) =>
        OnStaThread(() =>
        {
            var source = BuildImage((_, _) => Background, 0, 0);
            var processed = BuildImage((_, _) => Background, delta, patch);
            var expected = BuildImage((_, _) => 0, delta, patch);
            var patchFree = BuildImage((_, _) => 0, 0, 0);

            return (
                RenderAll(source, processed, "DifferenceHeatmap"),
                RenderAll(expected, expected, "SourceOnly"),
                RenderAll(patchFree, patchFree, "SourceOnly"));
        });

    /// <summary>Renders the control at one mode and returns the whole bitmap.</summary>
    private static byte[] RenderAll(ImageSource source, ImageSource processed, string mode)
    {
        var viewport = new ImageComparisonViewport
        {
            SourceImage = source,
            ProcessedImage = processed,
            CompareMode = mode,
            Width = Size,
            Height = Size,
        };

        viewport.Measure(new Size(Size, Size));
        viewport.Arrange(new Rect(0, 0, Size, Size));
        viewport.UpdateLayout();

        var target = new RenderTargetBitmap(Size, Size, 96, 96, PixelFormats.Pbgra32);
        target.Render(viewport);

        var pixels = new byte[Size * Size * 4];
        target.CopyPixels(pixels, Size * 4, 0);
        return pixels;
    }

    /// <summary>A left-to-right ramp, optionally with a brighter square at its centre.</summary>
    private static BitmapSource GradientImage(int patchDelta = 0, int patchSize = 0) =>
        BuildImage((x, _) => (byte)(x * 255 / (Size - 1)), patchDelta, patchSize);

    private static BitmapSource BuildImage(Func<int, int, byte> level, int patchDelta, int patchSize)
    {
        var stride = Size * 4;
        var pixels = new byte[stride * Size];
        var half = patchSize / 2;
        var centre = Size / 2;

        for (var y = 0; y < Size; y++)
        {
            for (var x = 0; x < Size; x++)
            {
                var value = level(x, y);
                if (patchSize > 0 && Math.Abs(x - centre) < half && Math.Abs(y - centre) < half)
                {
                    value = (byte)Math.Clamp(value + patchDelta, 0, 255);
                }

                var i = ((y * Size) + x) * 4;
                pixels[i] = value;
                pixels[i + 1] = value;
                pixels[i + 2] = value;
                pixels[i + 3] = 255;
            }
        }

        var bitmap = BitmapSource.Create(Size, Size, 96, 96, PixelFormats.Bgra32, null, pixels, stride);
        bitmap.Freeze();
        return bitmap;
    }

    /// <summary>Runs the body on an STA thread — WPF visuals cannot be built on xUnit's MTA thread.</summary>
    private static T OnStaThread<T>(Func<T> body)
    {
        T result = default!;
        Exception? failure = null;

        var thread = new Thread(() =>
        {
            try { result = body(); }
            catch (Exception ex) { failure = ex; }
        });

        thread.SetApartmentState(ApartmentState.STA);
        thread.Start();
        thread.Join();

        if (failure is not null)
        {
            throw new InvalidOperationException("Rendering on the STA thread failed.", failure);
        }

        return result;
    }
}
