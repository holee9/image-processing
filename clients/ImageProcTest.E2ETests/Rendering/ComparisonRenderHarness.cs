// #149 G-4: the rendering and measurement code shared by the G-4 measurements and their falsification.
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using ImageProcTest.Controls;

namespace ImageProcTest.E2ETests.Rendering;

/// <summary>
/// Renders <see cref="ImageComparisonViewport"/> off-screen and measures patch-versus-background
/// separation.
///
/// <para>Extracted in GUI-C-55 so the falsification exercises <b>the same code</b> the measurements
/// use. A copy would test the copy: the thing under test is this metric, and a second implementation
/// of it proves nothing about the numbers already reported.</para>
/// </summary>
internal static class ComparisonRenderHarness
{
    /// <summary>
    /// Canvas size. 256 rather than 64: at 64 the control letterboxed the image into roughly a third
    /// of the frame and painted its HUD plate and label inside the drawn area, so a fixed sample
    /// region mixed backdrop, chrome and image (measured in GUI-C-54 by profiling the output).
    /// </summary>
    public const int Size = 256;

    /// <summary>Patch and background pixel masks located in the reference render.</summary>
    public readonly record struct RegionMasks(bool[] Patch, bool[] Background, int PatchPixels);

    /// <summary>
    /// Mean patch brightness minus mean background brightness, over masks LOCATED in the reference
    /// render rather than assumed from the bitmap's geometry.
    ///
    /// GUI-C-54's first version assumed the patch sat at the centre of the frame. Profiling showed
    /// that is false — the control letterboxes the image and draws chrome inside it — so those
    /// numbers described the frame layout, not the difference.
    /// </summary>
    public static double Separation(byte[] pixels, RegionMasks masks)
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
    /// Differencing cancels the chrome exactly and works at any Δ.
    /// </summary>
    public static RegionMasks Masks(byte[] reference, byte[] patchFreeReference)
    {
        var count = Size * Size;
        var changedMask = new bool[count];

        for (var p = 0; p < count; p++)
        {
            var i = p * 4;
            var changed = Math.Abs(reference[i] - patchFreeReference[i])
                        + Math.Abs(reference[i + 1] - patchFreeReference[i + 1])
                        + Math.Abs(reference[i + 2] - patchFreeReference[i + 2]);
            changedMask[p] = changed > 6;   // above the renderer's own rounding, below any real patch
        }

        // Background: unchanged pixels at least 4 px from anything changed, and outside the chrome
        // the control paints on top of the image (see Chrome).
        var background = new bool[count];
        for (var y = 0; y < Size; y++)
        {
            for (var x = 0; x < Size; x++)
            {
                var p = (y * Size) + x;
                if (changedMask[p] || Chrome(x, y)) continue;

                var clear = true;
                for (var dy = -4; dy <= 4 && clear; dy++)
                {
                    for (var dx = -4; dx <= 4; dx++)
                    {
                        var ny = y + dy;
                        var nx = x + dx;
                        if (ny < 0 || ny >= Size || nx < 0 || nx >= Size) continue;
                        if (changedMask[(ny * Size) + nx]) { clear = false; break; }
                    }
                }

                background[p] = clear;
            }
        }

        return new RegionMasks(changedMask, background, changedMask.Count(v => v));
    }

    /// <summary>
    /// The regions the control paints regardless of the image: the HUD plate at the top-left and the
    /// mode label along the bottom.
    ///
    /// <para>Added in GUI-C-57. The masks are located in a <c>SourceOnly</c> reference render, which
    /// carries the HUD plate but NOT the difference mode's own label, so that label was only ever
    /// present on one side of the comparison — it landed in the BACKGROUND mask and lifted the
    /// background mean of every heatmap reading. That was tolerable while the heatmap drew a
    /// mid-grey composite; once the mode draws a true difference, an unchanged image renders black
    /// and the label becomes the brightest thing in the background. Measured: the identical-input
    /// floor read <b>-5.6/255</b> with the chrome included and <b>0.0/255</b> with it excluded.</para>
    ///
    /// <para>The rectangles are deliberately generous. Being a few pixels too wide costs a handful of
    /// background samples out of ~60 000; being a pixel too narrow puts white text back into the
    /// measurement.</para>
    /// </summary>
    private static bool Chrome(int x, int y) =>
        (x < 240 && y < 48)          // HUD plate, drawn at (10,10)
        || y >= Size - 36;           // mode label, drawn 12 px up from the bottom edge

    /// <summary>Renders the control at one mode and returns the whole bitmap.</summary>
    public static byte[] RenderAll(ImageSource source, ImageSource processed, string mode)
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

    /// <summary>A left-to-right ramp, optionally with a square of a different value at its centre.</summary>
    public static BitmapSource GradientImage(int patchDelta = 0, int patchSize = 0) =>
        BuildImage((x, _) => (byte)(x * 255 / (Size - 1)), patchDelta, patchSize);

    /// <summary>An image built from a per-pixel level, with an optional square patch.</summary>
    public static BitmapSource BuildImage(
        Func<int, int, byte> level, int patchDelta, int patchSize, int offsetX = 0, int offsetY = 0)
    {
        var stride = Size * 4;
        var pixels = new byte[stride * Size];
        var half = patchSize / 2;
        var centreX = (Size / 2) + offsetX;
        var centreY = (Size / 2) + offsetY;

        for (var y = 0; y < Size; y++)
        {
            for (var x = 0; x < Size; x++)
            {
                var value = level(x, y);
                if (patchSize > 0 && Math.Abs(x - centreX) < half && Math.Abs(y - centreY) < half)
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
    public static T OnStaThread<T>(Func<T> body)
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
