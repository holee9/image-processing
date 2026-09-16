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
    /// P1 (#149 decision, GUI-C-57): for identical inputs the mode renders ONE colour, whatever the
    /// inputs are.
    ///
    /// <para><b>This assertion is inverted from what GUI-C-46 wrote here, on purpose.</b> That test
    /// asserted the output <i>changes</i> with the input — the property that told a tint from a
    /// difference — and it was the evidence that the mode was not computing one. Measured then:
    /// white input rendered <c>#877E92</c> and black <c>#372A3B</c>. The mode now draws
    /// <c>|source - processed|</c>, so both must land on the same colour, and the old expectation is
    /// recorded here rather than deleted.</para>
    ///
    /// <para>P1 holds whatever colour mapping is chosen: identical inputs hand the mapping a value of
    /// zero everywhere, and one value maps to one colour. It is the leader's greyscale decision that
    /// makes that colour black; the assertion does not depend on it.</para>
    ///
    /// <para>Run on an STA thread — WPF visuals cannot be created on the MTA thread xUnit provides.
    /// </para>
    /// </summary>
    [Fact]
    public void DifferenceHeatmap_OfAnImageWithItself_IsOneColour()
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
            whiteDiff == blackDiff,
            $"DifferenceHeatmap rendered {Describe(whiteDiff)} for identical white input and " +
            $"{Describe(blackDiff)} for identical black input. A difference of an image with itself " +
            "is zero whatever the image is, so the mode is reading the inputs themselves again — " +
            "re-measure #149 G-4 and report; do not relax this.");
    }

    /// <summary>
    /// The colour identical inputs land on is black — the leader's mapping, stated as a test so a
    /// change to it is visible rather than silent.
    ///
    /// <para>Separate from P1 above <b>because it is the one claim here that depends on the colour
    /// mapping.</b> P1 would still hold under a rainbow map; this would not. Keeping them apart means
    /// a future mapping decision fails exactly one test, and the failure names the decision.</para>
    ///
    /// <para>Replaces <c>DifferenceHeatmap_LeavesTheImageVisible_AndShiftsItTowardRed</c>, which
    /// pinned the old red wash: measured deltas against <c>SourceOnly</c> were R -12, G -49, B -49
    /// for white input. The wash is gone.</para>
    ///
    /// <para>Compared against the backdrop the control paints when it has nothing to draw, not
    /// against a literal 0: the render is composited over that backdrop, so "black" means "the
    /// backdrop shows through unchanged".</para>
    /// </summary>
    [Fact]
    public void DifferenceHeatmap_OfIdenticalInputs_IsBlack()
    {
        var (difference, backdrop) = OnStaThread(() =>
        {
            var image = SolidImage(Colors.White);
            var pitch = SolidImage(Colors.Black);
            return (
                RenderCentrePixel(image, image, "DifferenceHeatmap"),
                RenderCentrePixel(pitch, pitch, "SourceOnly"));
        });

        output.WriteLine($"identical white input -> {Describe(difference)}; black image -> {Describe(backdrop)}");

        Assert.True(
            difference == backdrop,
            $"Identical inputs rendered {Describe(difference)} where a black image renders " +
            $"{Describe(backdrop)}. Zero difference must map to black (#149's colour decision: " +
            "linear grey, 0 = black) — if the mapping changed, say so and re-measure.");
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
