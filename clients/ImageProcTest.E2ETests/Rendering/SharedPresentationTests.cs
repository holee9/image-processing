using System.Reflection;
using System.Windows;
using System.Windows.Automation;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using System.Xml.Linq;
using ImageProcTest.Views;
using ImageProcTest.Controls;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Rendering;

public sealed class SharedPresentationTests
{
    private readonly ITestOutputHelper _output;
    public SharedPresentationTests(ITestOutputHelper output) => _output = output;

    [Fact]
    public void C274_SharedValues_PreserveSystemTypographyAndMeasurementColors_WithoutGlobalControlStyles()
        => OnWindow(window =>
        {
            Assert.Equal(SystemFonts.MessageFontFamily, window.FontFamily);
            Assert.Equal(SystemFonts.MessageFontSize, window.FontSize);
            Assert.Equal(Colors.Black, Brush(window, "UiImageBackground").Color);
            Assert.Equal(Colors.Lime, Brush(window, "UiRegionStroke").Color);
            Assert.Equal(Colors.Yellow, Brush(window, "UiRoiStroke").Color);
            Assert.Equal(12d, window.FindResource("UiMarkerFontSize"));
            Assert.Equal(new Thickness(14), window.FindResource("UiComparisonMargin"));
            var foreground = Assert.IsType<SolidColorBrush>(window.Foreground).Color;
            var background = Assert.IsType<SolidColorBrush>(window.Background).Color;
            var contrast = Contrast(foreground, background);
            _output.WriteLine($"Window chrome {foreground} / {background}: {contrast:F6}:1");
            Assert.True(contrast >= 4.5);
            var shared = Assert.Single(window.Resources.MergedDictionaries);
            Assert.DoesNotContain(shared.Keys.Cast<object>(), key => key is Type);
        });

    [Fact]
    public void C274_ImportantNotices_HaveTextAndAtLeastFourPointFiveContrast()
        => OnWindow(window =>
        {
            var notice = Descendants(window).OfType<TextBlock>()
                .Single(x => AutomationProperties.GetAutomationId(x) == "ComparisonProvenanceNotice");
            Assert.Contains("no vendor measurement", notice.Text);
            var foreground = Assert.IsType<SolidColorBrush>(notice.Foreground).Color;
            var background = Assert.IsType<SolidColorBrush>(notice.Background).Color;
            Assert.True(Contrast(foreground, background) >= 4.5,
                $"Provenance text contrast {Contrast(foreground, background):F4} must be at least 4.5.");
            var warning = Field<TextBlock>(window, "_markerWarning");
            Assert.True(Contrast(((SolidColorBrush)warning.Foreground).Color, Colors.White) >= 4.5);
        });

    [Theory]
    [InlineData("UiNoticeText", "UiMockBackground")]
    [InlineData("UiWarningText", "UiAiWarningBackground")]
    [InlineData("UiProvenanceText", "UiProvenanceBackground")]
    [InlineData("UiRegionStroke", "UiImageBackground")]
    [InlineData("UiFocusBrush", null)]
    public void C274_ChromeAndOverlayText_UseMeasuredContrastIncludingAlphaBackground(string foreground, string? background)
        => OnWindow(window =>
        {
            var fg = Brush(window, foreground).Color;
            var bg = background is null ? SystemColors.WindowColor : Brush(window, background).Color;
            static Color Over(Color color, Color underlay) => Color.FromRgb(
                (byte)Math.Round(color.A / 255d * color.R + (1 - color.A / 255d) * underlay.R),
                (byte)Math.Round(color.A / 255d * color.G + (1 - color.A / 255d) * underlay.G),
                (byte)Math.Round(color.A / 255d * color.B + (1 - color.A / 255d) * underlay.B));
            bg = Over(bg, SystemColors.WindowColor); fg = Over(fg, bg);
            var contrast = Contrast(fg, bg);
            _output.WriteLine($"{foreground} {fg} / {background ?? "SystemColors.WindowColor"} {bg}: {contrast:F6}:1");
            Assert.True(contrast >= 4.5, $"{foreground}/{background}: {contrast:F6}:1 < 4.5.");
        });

    [Fact]
    public void C274_KeyboardFocus_HasAVisibleBorderWithoutChangingItsLayout()
        => OnWindow(window =>
        {
            window.Show();
            var button = Field<Button>(window, "_openButton");
            window.UpdateLayout();
            var before = button.RenderSize;
            Assert.True(button.Focus());
            window.Dispatcher.Invoke(() => { }, DispatcherPriority.ContextIdle);
            Assert.True(button.IsKeyboardFocused);
            Assert.Equal(Brush(window, "UiFocusBrush").Color,
                Assert.IsType<SolidColorBrush>(button.BorderBrush).Color);
            Assert.NotNull(button.FocusVisualStyle);
            Assert.Equal(before, button.RenderSize);
        });

    [Theory]
    [InlineData(false)]
    [InlineData(true)]
    public void C274_MainOnlyMetadata_PreservesTheExistingSecondaryColorAndMeetsContrast(bool onRunSetBadge)
        => OnWindow(window =>
        {
            var foreground = Brush(window, "UiMetadataText").Color;
            Assert.Equal(Color.FromRgb(0x8a, 0x90, 0x9c), foreground);
            var background = Color.FromRgb(0x14, 0x17, 0x1e);
            if (onRunSetBadge)
            {
                const double alpha = 13d / 255;
                background = Color.FromRgb(
                    (byte)Math.Round(alpha * 255 + (1 - alpha) * background.R),
                    (byte)Math.Round(alpha * 255 + (1 - alpha) * background.G),
                    (byte)Math.Round(alpha * 255 + (1 - alpha) * background.B));
            }
            var contrast = Contrast(foreground, background);
            _output.WriteLine($"Main metadata {foreground} / {background}, badge={onRunSetBadge}: {contrast:F6}:1");
            Assert.True(contrast >= 4.5);
        });

    [Fact]
    public void C274_MainMetricsHeaders_HaveReadableTextOnTheirExistingDarkPanel()
        => OnWindow(window =>
        {
            var directory = new DirectoryInfo(AppContext.BaseDirectory);
            while (directory is not null && !File.Exists(Path.Combine(directory.FullName, "gui", "ImageProcTest", "Views", "AnalysisPanel.xaml")))
                directory = directory.Parent;
            Assert.NotNull(directory);
            var document = XDocument.Load(Path.Combine(directory!.FullName, "gui", "ImageProcTest", "Views", "AnalysisPanel.xaml"));
            var background = (Color)ColorConverter.ConvertFromString(document.Root!.Attribute("Background")!.Value);
            var headers = document.Descendants().Where(x => x.Name.LocalName == "TextBlock" && x.Attribute("Text")?.Value is "METRIC" or "Delta").ToArray();
            Assert.Equal(2, headers.Length);
            foreach (var header in headers)
            {
                var color = header.Attribute("Foreground")!.Value;
                var foreground = color.StartsWith("{StaticResource ", StringComparison.Ordinal)
                    ? Brush(window, color["{StaticResource ".Length..^1]).Color
                    : (Color)ColorConverter.ConvertFromString(color);
                var contrast = Contrast(foreground, background);
                _output.WriteLine($"Metrics header {header.Attribute("Text")!.Value}: {foreground} / {background}: {contrast:F6}:1");
                Assert.True(contrast >= 4.5, $"Metrics header contrast {contrast:F6}:1 < 4.5.");
            }
        });

    [Fact]
    public void C274_ViewportBackground_ChangesOnlyTheCanvas_AndInvalidatesRendering()
        => OnWindow(window =>
        {
            var viewport = new ImageComparisonViewport { ZoomScale = 1, CompareMode = "SourceOnly" };
            var original = Enumerable.Repeat((byte)17, 16).ToArray();
            viewport.SourceImage = BitmapSource.Create(4, 4, 96, 96, PixelFormats.Gray8, null, original, 4);
            Assert.Equal(new byte[] { 23, 13, 8, 255 }, RenderViewport(viewport)[..4]);
            viewport.SetValue(Control.BackgroundProperty, Brush(window, "UiImageBackground"));
            var rendered = RenderViewport(viewport);
            Assert.Equal(new byte[] { 0, 0, 0, 255 }, rendered[..4]);
            var center = (128 * 256 + 128) * 4;
            Assert.Equal(new byte[] { 17, 17, 17, 255 }, rendered[center..(center + 4)]);
            var untouched = new byte[16]; ((BitmapSource)viewport.SourceImage).CopyPixels(untouched, 4, 0);
            Assert.Equal(original, untouched);
            var metadata = Assert.IsType<FrameworkPropertyMetadata>(Control.BackgroundProperty.GetMetadata(typeof(ImageComparisonViewport)));
            Assert.True(metadata.AffectsRender);
        });

    [Fact]
    public void C274_UnstyledDetachedViewport_KeepsTheLegacyBackground_WithASharedFrozenBrush()
    {
        static SolidColorBrush DefaultBrush() => ComparisonRenderHarness.OnStaThread(() =>
        {
            var viewport = new ImageComparisonViewport();
            Assert.Equal(new byte[] { 23, 13, 8, 255 }, RenderViewport(viewport)[..4]);
            return Assert.IsType<SolidColorBrush>(viewport.GetValue(Control.BackgroundProperty));
        });
        var first = DefaultBrush(); var second = DefaultBrush();
        Assert.True(first.IsFrozen); Assert.Same(first, second);
        Assert.Equal(Color.FromRgb(8, 13, 23), first.Color);
    }

    [Theory]
    [InlineData("Viewport")]
    [InlineData("LaneAViewport")]
    [InlineData("LaneBViewport")]
    public void C274_EachMainOnlyViewport_ExplicitlyUsesTheNeutralSharedBrush(string name)
        => OnWindow(window =>
        {
            var directory = new DirectoryInfo(AppContext.BaseDirectory);
            while (directory is not null && !File.Exists(Path.Combine(directory.FullName, "gui", "ImageProcTest", "Views", "ViewportShell.xaml")))
                directory = directory.Parent;
            Assert.NotNull(directory);
            var document = XDocument.Load(Path.Combine(directory!.FullName, "gui", "ImageProcTest", "Views", "ViewportShell.xaml"));
            var element = document.Descendants().Single(x => x.Name.LocalName == "ImageComparisonViewport" && x.Attributes().Any(a => a.Name.LocalName == "Name" && a.Value == name));
            Assert.Equal("{DynamicResource UiImageBackground}", element.Attribute("Background")?.Value);
            Assert.Equal(Colors.Black, Brush(window, "UiImageBackground").Color);
        });

    private static byte[] RenderViewport(ImageComparisonViewport viewport)
    {
        viewport.Width = viewport.Height = 256;
        viewport.Measure(new Size(256, 256)); viewport.Arrange(new Rect(0, 0, 256, 256)); viewport.UpdateLayout();
        var bitmap = new RenderTargetBitmap(256, 256, 96, 96, PixelFormats.Pbgra32); bitmap.Render(viewport);
        var bytes = new byte[256 * 256 * 4]; bitmap.CopyPixels(bytes, 256 * 4, 0); return bytes;
    }

    private static SolidColorBrush Brush(Window window, string key)
        => Assert.IsType<SolidColorBrush>(window.FindResource(key));

    private static double Contrast(Color a, Color b)
    {
        static double Linear(byte value) { var s = value / 255d; return s <= .04045 ? s / 12.92 : Math.Pow((s + .055) / 1.055, 2.4); }
        static double L(Color color) => .2126 * Linear(color.R) + .7152 * Linear(color.G) + .0722 * Linear(color.B);
        return (Math.Max(L(a), L(b)) + .05) / (Math.Min(L(a), L(b)) + .05);
    }

    private static IEnumerable<DependencyObject> Descendants(DependencyObject parent)
    {
        foreach (var child in LogicalTreeHelper.GetChildren(parent).OfType<DependencyObject>())
        {
            yield return child;
            foreach (var descendant in Descendants(child)) yield return descendant;
        }
    }

    private static T Field<T>(object obj, string name) => (T)obj.GetType()
        .GetField(name, BindingFlags.Instance | BindingFlags.NonPublic)!.GetValue(obj)!;

    private static void OnWindow(Action<ComparisonImportWindow> body)
        => ComparisonRenderHarness.OnStaThread(() =>
        {
            var window = new ComparisonImportWindow((_, _, _) => throw new InvalidOperationException("No backend operation belongs to this presentation check."));
            var temporary = Path.Combine(Path.GetTempPath(), "xpe-presentation-" + Guid.NewGuid().ToString("N") + ".json");
            window.GetType().GetField("_rememberFile", BindingFlags.Instance | BindingFlags.NonPublic)!.SetValue(window, temporary);
            try { body(window); }
            finally { window.Close(); if (File.Exists(temporary)) File.Delete(temporary); }
            return true;
        });
}
