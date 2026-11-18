using System.ComponentModel;
using System.Globalization;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Automation;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Markup;
using System.Xml.Linq;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Rendering;

public sealed class ReworkPresentationTests
{
    private readonly ITestOutputHelper _output;
    public ReworkPresentationTests(ITestOutputHelper output) => _output = output;

    [Theory]
    [InlineData("LaneAAlgorithmPicker", true)]
    [InlineData("LaneBAlgorithmPicker", true)]
    [InlineData("LaneAAlgorithmPicker", false)]
    [InlineData("LaneBAlgorithmPicker", false)]
    public void C274R1_ActualPickerTemplate_IsReadableInNormalAndDisabledStates(string id, bool enabled)
        => ComparisonRenderHarness.OnStaThread(() =>
        {
            var shell = new Window();
            try
            {
                var resources = SharedResources();
                var source = XDocument.Load(Source("Views/AlgorithmBar.xaml"));
                var element = source.Descendants().Single(x => x.Name.LocalName == "ComboBox" && x.Attributes().Any(a => a.Name.LocalName == "AutomationProperties.AutomationId" && a.Value == id));
                var picker = new ComboBox { Width = 260, Height = 32, IsEnabled = enabled };
                picker.Resources.MergedDictionaries.Add(resources);
                picker.Style = element.Attribute("Style") is { } style
                    ? (Style)resources[Key(style.Value)]
                    : new Style(typeof(ComboBox), (Style)resources["UiFocusableControl"]);
                Set(picker, element, nameof(Control.Foreground), Control.ForegroundProperty, typeof(Brush));
                Set(picker, element, nameof(Control.Background), Control.BackgroundProperty, typeof(Brush));
                Set(picker, element, nameof(Control.BorderThickness), Control.BorderThicknessProperty, typeof(Thickness));
                Set(picker, element, nameof(Control.FontSize), Control.FontSizeProperty, typeof(double));
                Set(picker, element, nameof(Control.FontWeight), Control.FontWeightProperty, typeof(FontWeight));
                Set(picker, element, nameof(Control.Padding), Control.PaddingProperty, typeof(Thickness));
                picker.Items.Add("No grid correction"); picker.SelectedIndex = 0;
                var host = new Grid { Width = 280, Height = 52, Background = new SolidColorBrush(Color.FromRgb(20, 23, 30)) };
                shell.Content = host;
                host.Children.Add(picker);
                host.Measure(new Size(280, 52)); host.Arrange(new Rect(0, 0, 280, 52)); host.UpdateLayout(); picker.ApplyTemplate(); host.UpdateLayout();
                var glyph = Descendants(picker).OfType<TextBlock>().Single(x => x.Text == "No grid correction");
                var opacity = 1d; var chain = new List<string>();
                for (DependencyObject? node = glyph; node is not null; node = VisualTreeHelper.GetParent(node))
                    if (node is UIElement visual) { opacity *= visual.Opacity; chain.Add($"{node.GetType().Name}:{visual.Opacity:F3}"); }
                var declared = Assert.IsType<SolidColorBrush>(glyph.Foreground);
                opacity *= declared.Opacity * declared.Color.A / 255d;
                var bitmap = new RenderTargetBitmap(280, 52, 96, 96, PixelFormats.Pbgra32); bitmap.Render(host);
                var bytes = new byte[280 * 52 * 4]; bitmap.CopyPixels(bytes, 280 * 4, 0);
                var origin = picker.TranslatePoint(new Point(), host);
                var index = ((int)(origin.Y + picker.ActualHeight / 2) * 280 + (int)(origin.X + picker.ActualWidth - 32)) * 4;
                var background = Color.FromRgb(bytes[index + 2], bytes[index + 1], bytes[index]);
                var foreground = Over(declared.Color, opacity, background);
                var contrast = Contrast(foreground, background);
                _output.WriteLine($"{id} enabled={picker.IsEnabled}, foreground={declared.Color}, effectiveOpacity={opacity:F6}, composed={foreground}, renderedBackground={background}, contrast={contrast:F6}:1; {string.Join(",", chain)}");
                Assert.True(contrast >= 4.5, $"{id} enabled={enabled}: {contrast:F6}:1 < 4.5.");
                return true;
            }
            finally { shell.Close(); }
        });

    [Theory]
    [InlineData("LaneAAlgorithmPicker")]
    [InlineData("LaneBAlgorithmPicker")]
    public void C274R1_SelectedPopupText_IsReadable(string id)
    {
        System.Windows.Threading.Dispatcher? ownedDispatcher = null;
        ComparisonRenderHarness.OnStaThread(() =>
        {
            var shell = new Window { Width = 300, Height = 100, ShowActivated = false, ShowInTaskbar = false, Left = -10000, Top = -10000 };
            ownedDispatcher = shell.Dispatcher;
            ComboBox? picker = null;
            Exception? bodyFailure = null;
            try
            {
                var resources = SharedResources();
                var element = XDocument.Load(Source("Views/AlgorithmBar.xaml")).Descendants().Single(x => x.Name.LocalName == "ComboBox" && Id(x) == id);
                picker = new ComboBox { Width = 260, Height = 32 };
                picker.Resources.MergedDictionaries.Add(resources);
                picker.Style = new Style(typeof(ComboBox), (Style)resources["UiFocusableControl"]);
                Set(picker, element, nameof(Control.Foreground), Control.ForegroundProperty, typeof(Brush));
                Set(picker, element, nameof(Control.Background), Control.BackgroundProperty, typeof(Brush));
                picker.Items.Add("No grid correction");
                picker.SelectedIndex = 0;
                shell.Content = picker;
                shell.Show();
                picker.IsDropDownOpen = true;
                shell.UpdateLayout();
                var popup = Assert.IsType<System.Windows.Controls.Primitives.Popup>(picker.Template.FindName("PART_Popup", picker));
                var root = Assert.IsAssignableFrom<FrameworkElement>(popup.Child);
                root.UpdateLayout();
                var item = Assert.IsType<ComboBoxItem>(picker.ItemContainerGenerator.ContainerFromIndex(0));
                Assert.True(item.IsSelected);
                var glyph = Descendants(item).OfType<TextBlock>().Single(x => x.Text == "No grid correction");
                var brush = Assert.IsType<SolidColorBrush>(glyph.Foreground);
                var opacity = brush.Opacity * brush.Color.A / 255d;
                for (DependencyObject? node = glyph; node is not null; node = VisualTreeHelper.GetParent(node))
                    if (node is UIElement visual) opacity *= visual.Opacity;
                var width = (int)Math.Ceiling(root.ActualWidth);
                var height = (int)Math.Ceiling(root.ActualHeight);
                Assert.True(width > 0 && height > 0);
                var bitmap = new RenderTargetBitmap(width, height, 96, 96, PixelFormats.Pbgra32);
                bitmap.Render(root);
                var bytes = new byte[width * height * 4];
                bitmap.CopyPixels(bytes, width * 4, 0);
                var origin = item.TranslatePoint(new Point(), root);
                var index = ((int)(origin.Y + item.ActualHeight / 2) * width + (int)(origin.X + item.ActualWidth - 12)) * 4;
                Assert.Equal(255, bytes[index + 3]);
                var background = Color.FromRgb(bytes[index + 2], bytes[index + 1], bytes[index]);
                var foreground = Over(brush.Color, opacity, background);
                var contrast = Contrast(foreground, background);
                _output.WriteLine($"{id} popup open={picker.IsDropDownOpen}, selected={item.IsSelected}, actualGlyph={brush.Color}, opacity={opacity:F6}, renderedBackground={background}, contrast={contrast:F6}:1");
                Assert.True(contrast >= 4.5, $"Selected popup text contrast {contrast:F6}:1 < 4.5.");
                return true;
            }
            catch (Exception exception)
            {
                bodyFailure = exception;
                throw;
            }
            finally
            {
                try
                {
                    try { if (picker is not null) picker.IsDropDownOpen = false; }
                    finally { shell.Close(); }
                }
                catch (Exception cleanupFailure) when (bodyFailure is not null)
                {
                    _output.WriteLine($"Popup cleanup also failed: {cleanupFailure}");
                }
                finally
                {
                    try { shell.Dispatcher.InvokeShutdown(); }
                    catch (Exception shutdownFailure) when (bodyFailure is not null)
                    {
                        _output.WriteLine($"Owned Dispatcher shutdown also failed: {shutdownFailure}");
                    }
                }
            }
        });
        Assert.NotNull(ownedDispatcher);
        _output.WriteLine($"{id} ownedDispatcher.HasShutdownFinished={ownedDispatcher.HasShutdownFinished}");
        Assert.True(ownedDispatcher.HasShutdownFinished, "The popup's owned Dispatcher must finish shutdown before its worker ends.");
    }

    [Fact]
    public void C274R1_MetricState_WrapsAtWordBoundariesAndKeepsTheElapsedValue()
        => ComparisonRenderHarness.OnStaThread(() =>
        {
            const string original = "RequestedNotApplied · 0.002 ms";
            var document = XDocument.Load(Source("Views/AnalysisPanel.xaml"));
            var cell = document.Descendants().Single(x => x.Name.LocalName == "TextBlock" && x.Attributes().Any(a => a.Name.LocalName == "AutomationProperties.AutomationId" && a.Value.Contains("MetricA:", StringComparison.Ordinal)));
            var converter = MetricConverter(cell);
            var block = new TextBlock { Width = (380 - 24 - SystemParameters.VerticalScrollBarWidth - 100 - 48) / 2, FontFamily = new FontFamily(cell.Attribute("FontFamily")!.Value), FontSize = double.Parse(cell.Attribute("FontSize")!.Value, CultureInfo.InvariantCulture), TextWrapping = TextWrapping.Wrap };
            block.SetBinding(TextBlock.TextProperty, new Binding("LaneA") { Converter = converter, Mode = BindingMode.OneWay });
            block.DataContext = new { LaneA = original };
            block.Measure(new Size(block.Width, 1000)); block.Arrange(new Rect(0, 0, block.Width, block.DesiredSize.Height)); block.UpdateLayout();
            var bitmap = new RenderTargetBitmap((int)Math.Ceiling(block.Width), (int)Math.Ceiling(block.ActualHeight), 96, 96, PixelFormats.Pbgra32); bitmap.Render(block);
            var characters = Characters(block).ToArray();
            var lines = characters.GroupBy(x => Math.Round(x.Top, 3)).Select(x => string.Concat(x.Select(c => c.Character))).ToArray();
            _output.WriteLine($"Metric width={block.Width:F3}, visible='{block.Text}', actual lines: {string.Join(" / ", lines)}");
            Assert.NotEmpty(characters);
            for (var index = 1; index < characters.Length; index++)
                if (Math.Abs(characters[index].Top - characters[index - 1].Top) > .1)
                    Assert.False(char.IsLetter(characters[index - 1].Character) && char.IsLetter(characters[index].Character), $"A word was split across lines: {string.Join(" / ", lines)}");
            Assert.Equal("Requested / not applied · 0.002 ms", block.Text);
            Assert.Equal("RequestedNotApplied · 0.002 ms", original);
            return true;
        });

    [Theory]
    [InlineData("RequestedNotApplied · 0.002 ms", "Requested / not applied · 0.002 ms")]
    [InlineData("RequestedNotApplied", "Requested / not applied")]
    [InlineData("AppliedNoChange · 1 ms", "Applied / no change · 1 ms")]
    [InlineData("NotRequested · 0 ms", "Not requested · 0 ms")]
    [InlineData("not measured", "not measured")]
    [InlineData("not run", "not run")]
    [InlineData("MyRequestedNotAppliedData", "MyRequestedNotAppliedData")]
    public void C274R1_MetricDisplay_IsOnlyAPresentationAlias(string original, string expected)
        => ComparisonRenderHarness.OnStaThread(() =>
        {
            var document = XDocument.Load(Source("Views/AnalysisPanel.xaml"));
            var cells = document.Descendants().Where(x => x.Name.LocalName == "TextBlock" && x.Attributes().Any(a => a.Name.LocalName == "AutomationProperties.AutomationId" && (a.Value.Contains("MetricA:", StringComparison.Ordinal) || a.Value.Contains("MetricB:", StringComparison.Ordinal)))).ToArray();
            foreach (var cell in cells)
            {
                var converter = MetricConverter(cell);
                var display = converter?.Convert(original, typeof(string), null!, CultureInfo.InvariantCulture) ?? original;
                Assert.Equal(expected, display);
                if (converter is not null) Assert.Same(Binding.DoNothing, converter.ConvertBack(display, typeof(string), null!, CultureInfo.InvariantCulture));
            }
            return true;
        });

    private static IValueConverter? MetricConverter(XElement cell)
    {
        if (!cell.Attribute("Text")!.Value.Contains("MetricCellDisplay", StringComparison.Ordinal)) return null;
        var type = typeof(ReworkPresentationTests).Assembly.GetType("ImageProcTest.Converters.MetricCellDisplayConverter", throwOnError: true)!;
        return (IValueConverter)Activator.CreateInstance(type)!;
    }

    [Theory]
    [InlineData(600)]
    [InlineData(800)]
    public void C274R1_StudyAndModeButtons_HaveSeparateRuntimeLayoutBands(double width)
        => ComparisonRenderHarness.OnStaThread(() =>
        {
            var shell = new Window();
            try
            {
                var resources = SharedResources();
                var document = XDocument.Load(Source("Views/ViewportShell.xaml"));
                var source = document.Root!.Elements().Single(x => x.Name.LocalName == "Grid");
                var nodes = new Dictionary<string, FrameworkElement>();
                var layout = Layout(source, resources, nodes);
                shell.Content = layout;
                layout.Width = width; layout.Height = 400;
                layout.Measure(new Size(width, 400)); layout.Arrange(new Rect(0, 0, width, 400)); layout.UpdateLayout();
                var study = nodes["StudyHud"]; var studyBox = new Rect(study.TranslatePoint(new Point(), layout), study.RenderSize);
                var buttons = Descendants(nodes["ViewportToolbar"]).OfType<Button>()
                    .Where(x => AutomationProperties.GetAutomationId(x) is "CompareButtonSwipeVertical" or "CompareButtonSplitLocked" or "CompareButtonOverlayOpacity" or "CompareButtonDifferenceHeatmap").ToArray();
                Assert.Equal(4, buttons.Length);
                foreach (var button in buttons)
                {
                    var box = new Rect(button.TranslatePoint(new Point(), layout), button.RenderSize);
                    var intersection = Rect.Intersect(studyBox, box);
                    var area = intersection.IsEmpty ? 0 : intersection.Width * intersection.Height;
                    _output.WriteLine($"Shell width={width:F0}; Study={studyBox}; {button.Content}={box}; overlap={area:F3} px²");
                    Assert.True(area <= .01, $"STUDY covers {button.Content}: {area:F3} px².");
                }
                return true;
            }
            finally { shell.Close(); }
        });

    private static Grid Layout(XElement source, ResourceDictionary resources, Dictionary<string, FrameworkElement> nodes)
    {
        var grid = new Grid(); CopyBox(grid, source);
        var rows = source.Elements().FirstOrDefault(x => x.Name.LocalName == "Grid.RowDefinitions");
        if (rows is not null)
            foreach (var row in rows.Elements()) grid.RowDefinitions.Add(new RowDefinition { Height = (GridLength)new GridLengthConverter().ConvertFromInvariantString(row.Attribute("Height")?.Value ?? "*")! });
        foreach (var element in source.Elements().Where(x => !x.Name.LocalName.Contains('.', StringComparison.Ordinal)))
        {
            var id = Id(element);
            if (id is "ViewportToolbar" or "StudyHud")
            {
                var clone = new XElement(element);
                foreach (var node in clone.DescendantsAndSelf())
                {
                    node.Name = XName.Get(node.Name.LocalName.Replace("AutomationBorder", "Border", StringComparison.Ordinal), "http://schemas.microsoft.com/winfx/2006/xaml/presentation");
                    foreach (var attribute in node.Attributes().Where(x => !x.IsNamespaceDeclaration).ToArray())
                    {
                        if (attribute.Value.Contains("{Binding", StringComparison.Ordinal))
                        {
                            if (attribute.Name.LocalName == "Text") attribute.Value = attribute.Value.Contains("ActiveStudyId", StringComparison.Ordinal) ? "Study 001" : attribute.Value.Contains("ActiveImageSummary", StringComparison.Ordinal) ? "1024 x 1024 uint16 RAW measurement sample with a source image and recorded processing" : "100%";
                            else attribute.Remove();
                        }
                        else if (attribute.Value.StartsWith("{StaticResource ", StringComparison.Ordinal) || attribute.Value.StartsWith("{DynamicResource ", StringComparison.Ordinal))
                            attribute.Value = resources[Key(attribute.Value)].ToString()!;
                    }
                }
                var view = (FrameworkElement)XamlReader.Parse(clone.ToString());
                CopyBox(view, element); nodes[id] = view; grid.Children.Add(view);
            }
            else if (element.Name.LocalName == "Grid" && element.Descendants().Any(x => Id(x) is "ViewportToolbar" or "StudyHud"))
                grid.Children.Add(Layout(element, resources, nodes));
        }
        return grid;
    }

    private static string Id(XElement element) => element.Attributes().FirstOrDefault(x => x.Name.LocalName == "AutomationProperties.AutomationId")?.Value ?? string.Empty;

    private static void CopyBox(FrameworkElement target, XElement source)
    {
        if (source.Attribute("Margin") is { } margin) target.Margin = (Thickness)new ThicknessConverter().ConvertFromInvariantString(margin.Value)!;
        if (source.Attribute("HorizontalAlignment") is { } horizontal) target.HorizontalAlignment = Enum.Parse<HorizontalAlignment>(horizontal.Value);
        if (source.Attribute("VerticalAlignment") is { } vertical) target.VerticalAlignment = Enum.Parse<VerticalAlignment>(vertical.Value);
        if (source.Attribute("Grid.Row") is { } row) Grid.SetRow(target, int.Parse(row.Value, CultureInfo.InvariantCulture));
    }

    private static IEnumerable<(char Character, double Top)> Characters(TextBlock block)
    {
        var pointer = block.ContentStart;
        while (pointer is not null && pointer.CompareTo(block.ContentEnd) < 0)
        {
            if (pointer.GetPointerContext(LogicalDirection.Forward) == TextPointerContext.Text)
            {
                var text = pointer.GetTextInRun(LogicalDirection.Forward);
                for (var index = 0; index < text.Length; index++)
                {
                    var rectangle = pointer.GetPositionAtOffset(index)!.GetCharacterRect(LogicalDirection.Forward);
                    yield return (text[index], rectangle.Top);
                }
                pointer = pointer.GetPositionAtOffset(text.Length);
            }
            else pointer = pointer.GetNextContextPosition(LogicalDirection.Forward);
        }
    }

    private static void Set(Control control, XElement source, string name, DependencyProperty property, Type type)
    {
        if (source.Attribute(name) is not { } attribute) return;
        if (attribute.Value.StartsWith("{", StringComparison.Ordinal)) control.SetResourceReference(property, Key(attribute.Value));
        else control.SetValue(property, TypeDescriptor.GetConverter(type).ConvertFromInvariantString(attribute.Value));
    }

    private static string Key(string value) => value[(value.IndexOf(' ') + 1)..^1];

    private static ResourceDictionary SharedResources() => new()
    {
        Source = new Uri($"/{typeof(ReworkPresentationTests).Assembly.GetName().Name};component/Resources/Presentation.xaml", UriKind.Relative),
    };

    private static string Source(string relative)
    {
        var directory = new DirectoryInfo(AppContext.BaseDirectory);
        while (directory is not null && !File.Exists(Path.Combine(directory.FullName, "gui", "ImageProcTest", relative))) directory = directory.Parent;
        return Path.Combine((directory ?? throw new InvalidOperationException("No source checkout for this assembly.")).FullName, "gui", "ImageProcTest", relative);
    }

    private static IEnumerable<DependencyObject> Descendants(DependencyObject parent)
    {
        for (var index = 0; index < VisualTreeHelper.GetChildrenCount(parent); index++)
        {
            var child = VisualTreeHelper.GetChild(parent, index); yield return child;
            foreach (var descendant in Descendants(child)) yield return descendant;
        }
    }

    private static Color Over(Color color, double opacity, Color background) => Color.FromRgb(
        (byte)Math.Round(opacity * color.R + (1 - opacity) * background.R),
        (byte)Math.Round(opacity * color.G + (1 - opacity) * background.G),
        (byte)Math.Round(opacity * color.B + (1 - opacity) * background.B));

    private static double Contrast(Color a, Color b)
    {
        static double Linear(byte value) { var s = value / 255d; return s <= .04045 ? s / 12.92 : Math.Pow((s + .055) / 1.055, 2.4); }
        static double L(Color color) => .2126 * Linear(color.R) + .7152 * Linear(color.G) + .0722 * Linear(color.B);
        return (Math.Max(L(a), L(b)) + .05) / (Math.Min(L(a), L(b)) + .05);
    }
}
