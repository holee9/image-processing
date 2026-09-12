// #149 G-4 (GUI-C-46): what DifferenceHeatmap actually draws, observed by rendering it.
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using ImageProcTest.Controls;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Rendering;

/// <summary>
/// Renders <see cref="ImageComparisonViewport"/> off-screen and reads the pixels it produced.
///
/// GUI-C-45 reported that DifferenceHeatmap is "a tint approximation, not a true per-pixel
/// difference image" — on the authority of a code comment. A comment is somebody's reading of the
/// code, which is the same class of evidence that put "구현 불가" into the GUI-C-43 report. So this
/// runs the renderer instead.
///
/// The experiment: feed the SAME image as source and processed. A true difference of an image with
/// itself is zero everywhere, so a difference renderer would produce black. Whatever comes out
/// instead is the measurement.
///
/// No pixel value is asserted as "correct" — the mode's contract (GUI-CMP-FR-005) only says the
/// viewer exposes a difference heatmap mode. What is asserted is the property that distinguishes a
/// difference image from a tinted overlay, and the observed values are printed either way.
/// </summary>
[Trait("Category", "Rendering")]
public sealed class ComparisonRenderObservationTests(ITestOutputHelper output)
{
    private const int Size = 64;

    /// <summary>
    /// For identical inputs a true difference is zero WHATEVER the inputs are. Measured: the output
    /// changes with the input, so it is not a difference of them.
    ///
    /// This is stronger than "the output is not black". The control renders over a dark backdrop, so
    /// absolute values are damped and "not black" alone could be read as an artefact of that
    /// backdrop. Input-dependence cannot: difference(x, x) = 0 for every x, so an output that moves
    /// when x moves is computing something else.
    ///
    /// Run on an STA thread — WPF visuals cannot be created on the MTA thread xUnit provides.
    /// </summary>
    [Fact]
    public void DifferenceHeatmap_OfAnImageWithItself_DependsOnTheImage()
    {
        var (whiteDiff, blackDiff, whiteSource, blackSource) = OnStaThread(() =>
        {
            var white = SolidImage(Colors.White);
            var black = SolidImage(Colors.Black);
            return (
                RenderCentrePixel(white, white, "DifferenceHeatmap"),
                RenderCentrePixel(black, black, "DifferenceHeatmap"),
                RenderCentrePixel(white, white, "SourceOnly"),
                RenderCentrePixel(black, black, "SourceOnly"));
        });

        output.WriteLine($"SourceOnly        white->{Describe(whiteSource)}  black->{Describe(blackSource)}");
        output.WriteLine($"DifferenceHeatmap white->{Describe(whiteDiff)}  black->{Describe(blackDiff)}");

        // The control draws the image (SourceOnly tracks it), so the sample point is on the image
        // and not on some placeholder — without this the comparison below would measure nothing.
        Assert.True(
            whiteSource != blackSource,
            $"SourceOnly rendered the same pixel for white and black input ({Describe(whiteSource)}); " +
            "the sample point is not on the image, so this experiment measures nothing.");

        Assert.True(
            whiteDiff != blackDiff,
            "DifferenceHeatmap produced the same pixel for two different identical-input pairs, " +
            "which is what a true per-pixel difference does. If the renderer changed, #149 G-4 must " +
            "be re-measured and this observation rewritten — do not simply relax it.");
    }

    /// <summary>
    /// The difference mode differs from the source-only mode only by a tint: identical inputs leave
    /// the image visible underneath rather than cancelling it.
    ///
    /// Reported as measured channel values so the report carries numbers, not adjectives.
    /// </summary>
    [Fact]
    public void DifferenceHeatmap_LeavesTheImageVisible_AndShiftsItTowardRed()
    {
        var (difference, sourceOnly) = OnStaThread(() =>
        {
            var image = SolidImage(Colors.White);
            return (
                RenderCentrePixel(image, image, "DifferenceHeatmap"),
                RenderCentrePixel(image, image, "SourceOnly"));
        });

        output.WriteLine(
            $"delta vs SourceOnly: R {difference.R - sourceOnly.R}, " +
            $"G {difference.G - sourceOnly.G}, B {difference.B - sourceOnly.B}");

        // The image survives — a difference image of identical inputs would not.
        Assert.True(
            difference.R > 0 || difference.G > 0 || difference.B > 0,
            "Nothing was drawn at all; the experiment measured nothing.");

        // Compared as a DELTA against the same inputs in SourceOnly, not as absolute dominance: the
        // control composites over a blue-grey backdrop, so the result is blue-dominant in absolute
        // terms even under a red wash. The first version of this assertion compared absolute
        // channels and failed on that backdrop — measuring the backdrop, not the mode.
        var deltaR = difference.R - sourceOnly.R;
        var deltaG = difference.G - sourceOnly.G;
        var deltaB = difference.B - sourceOnly.B;

        Assert.True(
            deltaR > deltaG && deltaR > deltaB,
            $"Expected red to be preserved relative to green and blue (a red wash), measured " +
            $"deltas R {deltaR}, G {deltaG}, B {deltaB}.");
    }

    /// <summary>
    /// #149 G-3: the renderer honours SourceOnly and ProcessedOnly even though no control selects
    /// them.
    ///
    /// Reachability was measured first: the two modes appear in the view model's option array (bound
    /// to nothing) and in the renderer, and <c>appsettings.json</c> carries <c>comparisonMode</c> —
    /// so a hand-edited settings file reaches them. That path is NOT driven here, because an E2E
    /// launch reads and writes the shipped settings file and a test that edits it would overwrite a
    /// developer's own settings — the same hazard that kept W-08 unimplemented in GUI-C-43.
    ///
    /// Rendering the control directly answers the question the card asked — do these modes do
    /// anything? — without touching anyone's configuration.
    /// </summary>
    [Fact]
    public void SourceOnlyAndProcessedOnly_RenderTheLayerTheyName()
    {
        var (sourceOnly, processedOnly) = OnStaThread(() =>
        {
            var source = SolidImage(Colors.White);
            var processed = SolidImage(Colors.Black);
            return (
                RenderCentrePixel(source, processed, "SourceOnly"),
                RenderCentrePixel(source, processed, "ProcessedOnly"));
        });

        output.WriteLine($"source=white processed=black: SourceOnly -> {Describe(sourceOnly)}");
        output.WriteLine($"source=white processed=black: ProcessedOnly -> {Describe(processedOnly)}");

        // The white source must render brighter than the black processed layer. Comparing the two
        // modes against each other rather than against fixed values keeps the backdrop out of it.
        Assert.True(
            sourceOnly.R > processedOnly.R && sourceOnly.G > processedOnly.G && sourceOnly.B > processedOnly.B,
            $"SourceOnly ({Describe(sourceOnly)}) should be brighter than ProcessedOnly " +
            $"({Describe(processedOnly)}) when the source is white and the processed layer black.");
    }

    /// <summary>Renders the control at one mode and returns the colour at the centre of the viewport.</summary>
    private static Color RenderCentrePixel(ImageSource source, ImageSource processed, string mode)
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

        var pixels = new byte[4];
        target.CopyPixels(new Int32Rect(Size / 2, Size / 2, 1, 1), pixels, 4, 0);
        return Color.FromArgb(pixels[3], pixels[2], pixels[1], pixels[0]);
    }

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

    private static string Describe(Color color) => $"#{color.R:X2}{color.G:X2}{color.B:X2} (A={color.A})";

    /// <summary>
    /// Runs the body on an STA thread and rethrows whatever it threw.
    ///
    /// xUnit runs tests on MTA threads; constructing a WPF visual there throws, so a failure here
    /// would otherwise look like a defect in the control rather than in the harness.
    /// </summary>
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
