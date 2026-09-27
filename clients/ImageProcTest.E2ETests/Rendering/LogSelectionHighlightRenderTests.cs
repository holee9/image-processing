// GUI-C-142 §C: does the log list's ItemContainerStyle suppress the selection highlight? Rendered in-process.
using System;
using System.Threading;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Rendering;

/// <summary>
/// GUI-C-141 left one question deciding its verdict: when the <c>#206</c> filter moves the selection to
/// another row, can the user SEE which row is selected? Five attempts to answer it from the screen failed,
/// and each failure was caught by a control rather than becoming a finding:
///
/// <list type="number">
/// <item>per-item <c>Capture()</c> — a 260-character row and a 34-character row gave the same mean (the
/// item rectangle is reported 1582 wide against a 356-wide list, unclipped);</item>
/// <item>list-level <c>Capture()</c> — mean 180.5 on a <c>#14171e</c> panel;</item>
/// <item>band difference from a window capture — both bands identical at 217.65;</item>
/// <item>the same after <c>SetForeground()</c> — identical at 180.30;</item>
/// <item>the diagnostic that ended it: the capture is NOT blank (min 0, max 255) but its overall mean is
/// 214.6, so a bright window is being captured rather than this dark app. Screen capture is measuring
/// whatever sits at those coordinates.</item>
/// </list>
///
/// <para><b>So this measures the mechanism instead, with no screen involved.</b> The log list's
/// <c>ItemContainerStyle</c> sets <c>Background="Transparent"</c> on every <c>ListBoxItem</c>
/// (<c>AnalysisPanel.xaml</c>). The question "does that suppress the theme's selection highlight" is
/// answered by rendering two lists that differ ONLY in that setter and comparing the selected row against
/// its neighbour in each. The control is the second list: without the setter, the difference MUST appear,
/// or this render says nothing.</para>
///
/// <para><b>Scope, stated rather than glossed:</b> this renders the style, not the running application. It
/// answers "does this setter suppress the highlight", which is what the verdict hangs on; it does not
/// observe the app's own window. The report says so.</para>
/// </summary>
public sealed class LogSelectionHighlightRenderTests(ITestOutputHelper output)
{
    private const int Width = 356;      // the measured list width in the running app
    private const int RowHeight = 23;   // the measured row height
    private const int Rows = 4;

    [Fact]
    public void TheTransparentBackgroundSetter_DecidesWhetherASelectedRowLooksDifferent()
    {
        double withSetter = -1, withoutSetter = -1, withRed = -1, redNeighbourMean = -1;
        OnStaThread(() =>
        {
            withSetter = RowDelta(Brushes.Transparent, "§C with Background=Transparent (the app's style)", out _);
            withoutSetter = RowDelta(null, "§C CONTROL A without that setter", out _);
            // CONTROL B: is the ItemContainerStyle applied at all? The two numbers above came out exactly
            // equal, which is also what an IGNORED style would produce. An obvious colour settles it.
            withRed = RowDelta(Brushes.Red, "§C CONTROL B with Background=Red", out redNeighbourMean);
        });

        output.WriteLine($"§C VERDICT: app style |Δ|={withSetter:0.00}, control A |Δ|={withoutSetter:0.00}, " +
                         $"control B (red) |Δ|={withRed:0.00} with unselected-row mean {redNeighbourMean:0.00}");

        // CONTROL B: a red item background must move the UNSELECTED row's mean well away from the dark
        // panel's. If it does not, the style is not reaching the items and neither number above counts.
        Assert.True(redNeighbourMean > 60.0,
            $"CONTROL B FAILED: with Background=Red the unselected row's mean is {redNeighbourMean:0.00}, "
          + "so the ItemContainerStyle is not being applied. Two equal numbers would then say nothing about "
          + "the Transparent setter.");

        // The control decides whether the measurement counts at all.
        Assert.True(withoutSetter > 1.0,
            $"CONTROL FAILED: without the Background setter the selected row differs from its neighbour by "
          + $"only {withoutSetter:0.00}. Then this render cannot see a selection highlight at all and the "
          + "other number means nothing — the same trap five screen-capture attempts fell into.");

        output.WriteLine(withSetter > 1.0
            ? "§C the app's style DOES draw a visible selection highlight"
            : "§C the app's style draws NO visible selection highlight");
    }

    /// <summary>
    /// Renders a 4-row list, selects row 0, and returns the mean absolute difference between row 0's band
    /// and row 1's band — inside ONE rendered bitmap, same width, same rows, so "no difference" is an answer
    /// rather than a broken instrument.
    /// </summary>
    private double RowDelta(Brush? itemBackground, string label, out double neighbourMean)
    {
        var style = new Style(typeof(ListBoxItem));
        style.Setters.Add(new Setter(Control.PaddingProperty, new Thickness(4, 2, 4, 2)));
        if (itemBackground is not null)
        {
            // Transparent is the setter under test, copied from AnalysisPanel.xaml. Red is control B.
            style.Setters.Add(new Setter(Control.BackgroundProperty, itemBackground));
        }

        var list = new ListBox
        {
            Width = Width,
            Height = RowHeight * Rows,
            Background = Brushes.Transparent,
            BorderThickness = new Thickness(0),
            Foreground = new SolidColorBrush(Color.FromRgb(0xf0, 0xf2, 0xf5)),
            FontFamily = new FontFamily("Consolas"),
            FontSize = 11,
            ItemContainerStyle = style,
        };

        // The same DataTemplate shape the app uses: one TextBlock bound to the string.
        var template = new DataTemplate();
        var text = new FrameworkElementFactory(typeof(TextBlock));
        text.SetBinding(TextBlock.TextProperty, new System.Windows.Data.Binding());
        text.SetValue(TextBlock.ForegroundProperty, new SolidColorBrush(Color.FromRgb(0xb0, 0xb6, 0xc0)));
        template.VisualTree = text;
        list.ItemTemplate = template;

        for (var i = 0; i < Rows; i++)
        {
            list.Items.Add($"[00:00:0{i}.000] line {i} of the log, long enough to fill the row");
        }

        // A dark host, so the panel behind the rows matches the app's #14171e rather than white.
        var host = new Border
        {
            Background = new SolidColorBrush(Color.FromRgb(0x14, 0x17, 0x1e)),
            Child = list,
            Width = Width,
            Height = RowHeight * Rows,
        };

        host.Measure(new Size(Width, RowHeight * Rows));
        host.Arrange(new Rect(0, 0, Width, RowHeight * Rows));
        host.UpdateLayout();
        list.SelectedIndex = 0;
        host.UpdateLayout();

        var target = new RenderTargetBitmap(Width, RowHeight * Rows, 96, 96, PixelFormats.Pbgra32);
        target.Render(host);

        var stride = Width * 4;
        var pixels = new byte[stride * RowHeight * Rows];
        target.CopyPixels(pixels, stride, 0);

        var selected = BandMean(pixels, stride, 0);
        var neighbour = BandMean(pixels, stride, 1);
        neighbourMean = neighbour;
        var delta = Math.Abs(selected - neighbour);
        output.WriteLine($"{label}: selected row mean={selected:0.00} neighbour mean={neighbour:0.00} |Δ|={delta:0.00}");
        return delta;
    }

    private static double BandMean(byte[] pixels, int stride, int row)
    {
        double total = 0;
        var count = 0;
        for (var y = row * RowHeight; y < (row + 1) * RowHeight; y++)
        {
            for (var x = 0; x < Width; x++)
            {
                var offset = (y * stride) + (x * 4);
                total += (pixels[offset] + pixels[offset + 1] + pixels[offset + 2]) / 3.0;
                count++;
            }
        }

        return count is 0 ? -1 : total / count;
    }

    /// <summary>WPF visuals cannot be built on xUnit's MTA thread — the same helper shape the render suite uses.</summary>
    private static void OnStaThread(Action body)
    {
        Exception? failure = null;
        var thread = new Thread(() =>
        {
            try
            {
                body();
            }
            catch (Exception ex)
            {
                failure = ex;
            }
        });
        thread.SetApartmentState(ApartmentState.STA);
        thread.Start();
        thread.Join();
        if (failure is not null)
        {
            throw new InvalidOperationException("Rendering on the STA thread failed.", failure);
        }
    }
}
