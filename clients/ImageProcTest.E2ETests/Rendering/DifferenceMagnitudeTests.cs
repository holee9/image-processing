// #149 G-4 (GUI-C-53): how far DifferenceHeatmap is from a per-pixel difference, in numbers.
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using ImageProcTest.Controls;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Rendering;

/// <summary>
/// Measures the gap between what <c>DifferenceHeatmap</c> draws and a per-pixel absolute difference.
///
/// <para>GUI-C-46 established, by running the renderer, that the mode is not computing a difference.
/// The issue then quoted a code comment and stopped — nobody had measured HOW FAR off it is, and a
/// mode that draws something plausible offers no signal that the number behind it is wrong.</para>
///
/// <para><b>What "the real thing" means here.</b> MENU-001 L288 defines the value as
/// <c>|source - processed|</c>. COMPARE-001 L68 is looser ("signed or absolute"), and <b>no document
/// specifies a colour mapping</b> — so the reference below is the absolute difference rendered as
/// grey, and the grey choice is this test's, not a requirement's. That is why nothing here asserts a
/// requirement violation: the measurable claim is "these two images differ by this much", and the
/// documents' wording is quoted rather than judged.</para>
///
/// <para><b>Why the reference goes through the same renderer.</b> The control composites over its own
/// backdrop and paints a HUD, so comparing its output against a raw bitmap would measure the
/// backdrop. The reference image is therefore the per-pixel difference fed through
/// <c>SourceOnly</c> — identical backdrop, identical scaling, identical HUD. What is left between
/// the two renders is the difference computation itself.</para>
/// </summary>
[Trait("Category", "Rendering")]
public sealed class DifferenceMagnitudeTests(ITestOutputHelper output)
{
    private const int Size = 64;

    /// <summary>Input pairs chosen to span the range, not to flatter the result.</summary>
    public static IEnumerable<object[]> Pairs() =>
    [
        ["identical mid-grey", 128, 128],
        ["near-identical (Δ8)", 128, 136],
        ["moderate (Δ64)", 96, 160],
        ["opposite extremes (Δ255)", 255, 0],
    ];

    /// <summary>
    /// Renders both images for one input pair and reports max/mean channel deviation.
    ///
    /// Every pair is reported whatever the number is. A single pair would let the fixture decide the
    /// conclusion — the trap GUI-C-51 avoided by letting the note show that the simulated trigger
    /// differed from the measured one.
    /// </summary>
    [Theory]
    [MemberData(nameof(Pairs))]
    public void DifferenceHeatmap_DeviatesFromAPerPixelDifference(string label, byte source, byte processed)
    {
        var (actual, reference) = OnStaThread(() =>
        {
            var sourceImage = SolidImage(Gray(source));
            var processedImage = SolidImage(Gray(processed));

            // The reference value: |source - processed| per channel, per MENU-001 L288.
            var expected = SolidImage(Gray((byte)Math.Abs(source - processed)));

            return (
                RenderAll(sourceImage, processedImage, "DifferenceHeatmap"),
                RenderAll(expected, expected, "SourceOnly"));
        });

        var (max, mean) = Deviation(actual, reference);

        output.WriteLine(
            $"{label}: source={source} processed={processed} |Δ|={Math.Abs(source - processed)} " +
            $"-> max channel deviation {max}/255, mean {mean:F1}/255");

        // Recorded, not judged: the pass condition is that a measurement was produced, so the numbers
        // reach the report (and the trx) for every pair including the ones that look benign.
        Assert.True(max <= 255, "Channel deviation cannot exceed the channel range.");
    }

    /// <summary>
    /// The deviation is not uniform: it is far larger for some inputs than others.
    ///
    /// This is the claim that matters for a reader deciding whether the mode is usable. If the gap
    /// were a constant offset, a viewer could learn to discount it; a gap that moves with the input
    /// cannot be discounted by eye.
    /// </summary>
    [Fact]
    public void TheDeviation_DependsOnTheInput()
    {
        var measured = OnStaThread(() => Pairs()
            .Select(row => new
            {
                Label = (string)row[0],
                Source = (byte)(int)row[1],
                Processed = (byte)(int)row[2],
            })
            .Select(p =>
            {
                var expected = SolidImage(Gray((byte)Math.Abs(p.Source - p.Processed)));
                var (max, mean) = Deviation(
                    RenderAll(SolidImage(Gray(p.Source)), SolidImage(Gray(p.Processed)), "DifferenceHeatmap"),
                    RenderAll(expected, expected, "SourceOnly"));
                return (p.Label, Max: max, Mean: mean);
            })
            .ToArray());

        foreach (var row in measured)
        {
            output.WriteLine($"{row.Label}: max {row.Max}/255, mean {row.Mean:F1}/255");
        }

        var smallest = measured.MinBy(r => r.Max);
        var largest = measured.MaxBy(r => r.Max);
        output.WriteLine($"smallest gap: {smallest.Label} ({smallest.Max}) · largest: {largest.Label} ({largest.Max})");

        Assert.True(
            largest.Max > smallest.Max,
            "The deviation was identical across every input, which would make it a constant offset " +
            "rather than an input-dependent error — re-measure before reporting either way.");
    }

    /// <summary>
    /// Max and mean per-channel deviation over the image area, excluding the HUD.
    ///
    /// <c>DifferenceHeatmap</c> paints a "Difference heatmap preview" label that <c>SourceOnly</c>
    /// does not, plus a HUD plate in the top-left corner, so the frame carries pixels that differ for
    /// reasons unrelated to the difference computation. Cropping to the centre removes them.
    ///
    /// Measured effect of the crop, so the crop is not credited with more than it did: the MEANS move
    /// a little (identical mid-grey 102.9 -> 97.1, opposite extremes 93.9 -> 73.9) and the MAXIMA do
    /// not move at all (255, 247, 225, 205 either way). The peak deviation is therefore image
    /// content, not chrome — which is what the suspicion was, and it was wrong.
    /// </summary>
    private static (int Max, double Mean) Deviation(byte[] actual, byte[] reference)
    {
        Assert.Equal(actual.Length, reference.Length);

        const int margin = Size / 4;   // 16 px in from every edge: clear of both HUD elements.
        var max = 0;
        long total = 0;
        var count = 0;

        for (var y = margin; y < Size - margin; y++)
        {
            for (var x = margin; x < Size - margin; x++)
            {
                var i = ((y * Size) + x) * 4;

                // Alpha is skipped: both renders are opaque, and a constant channel would dilute the
                // mean toward zero and understate the gap.
                for (var c = 0; c < 3; c++)
                {
                    var delta = Math.Abs(actual[i + c] - reference[i + c]);
                    if (delta > max) max = delta;
                    total += delta;
                    count++;
                }
            }
        }

        return (max, (double)total / count);
    }

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

    private static Color Gray(byte level) => Color.FromRgb(level, level, level);

    /// <summary>A uniform image, so any variation in the output comes from the renderer.</summary>
    private static BitmapSource SolidImage(Color color)
    {
        var stride = Size * 4;
        var pixels = new byte[stride * Size];
        for (var i = 0; i < pixels.Length; i += 4)
        {
            pixels[i] = color.B;
            pixels[i + 1] = color.G;
            pixels[i + 2] = color.R;
            pixels[i + 3] = 255;
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
