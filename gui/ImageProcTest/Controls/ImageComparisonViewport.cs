using System.Globalization;
using System.Windows;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using ImageProcTest.Models;
using WpfBrush = System.Windows.Media.Brush;
using WpfBrushes = System.Windows.Media.Brushes;
using WpfColor = System.Windows.Media.Color;
using WpfFlowDirection = System.Windows.FlowDirection;
using WpfMouseEventArgs = System.Windows.Input.MouseEventArgs;
using WpfPen = System.Windows.Media.Pen;
using WpfPoint = System.Windows.Point;

namespace ImageProcTest.Controls;

public sealed class ImageComparisonViewport : FrameworkElement
{
    public static readonly DependencyProperty SourceImageProperty =
        DependencyProperty.Register(
            nameof(SourceImage),
            typeof(ImageSource),
            typeof(ImageComparisonViewport),
            new FrameworkPropertyMetadata(null, FrameworkPropertyMetadataOptions.AffectsRender, OnSourceImageChanged));

    public static readonly DependencyProperty ProcessedImageProperty =
        DependencyProperty.Register(
            nameof(ProcessedImage),
            typeof(ImageSource),
            typeof(ImageComparisonViewport),
            new FrameworkPropertyMetadata(null, FrameworkPropertyMetadataOptions.AffectsRender, OnProcessedImageChanged));

    /// <summary>
    /// The pixel chain of the image being shown (#180, GUI-C-101), drawn in the HUD so the operator sees
    /// what produced these pixels without opening a panel. Empty draws nothing extra.
    /// </summary>
    public static readonly DependencyProperty ChainStatusProperty =
        DependencyProperty.Register(
            nameof(ChainStatus),
            typeof(string),
            typeof(ImageComparisonViewport),
            new FrameworkPropertyMetadata(string.Empty, FrameworkPropertyMetadataOptions.AffectsRender));

    public static readonly DependencyProperty CompareModeProperty =
        DependencyProperty.Register(
            nameof(CompareMode),
            typeof(string),
            typeof(ImageComparisonViewport),
            new FrameworkPropertyMetadata("SwipeVertical", FrameworkPropertyMetadataOptions.AffectsRender));

    public static readonly DependencyProperty ZoomScaleProperty =
        DependencyProperty.Register(
            nameof(ZoomScale),
            typeof(double),
            typeof(ImageComparisonViewport),
            new FrameworkPropertyMetadata(0.0, FrameworkPropertyMetadataOptions.BindsTwoWayByDefault | FrameworkPropertyMetadataOptions.AffectsRender));

    public static readonly DependencyProperty PanXProperty =
        DependencyProperty.Register(
            nameof(PanX),
            typeof(double),
            typeof(ImageComparisonViewport),
            new FrameworkPropertyMetadata(0.0, FrameworkPropertyMetadataOptions.BindsTwoWayByDefault | FrameworkPropertyMetadataOptions.AffectsRender));

    public static readonly DependencyProperty PanYProperty =
        DependencyProperty.Register(
            nameof(PanY),
            typeof(double),
            typeof(ImageComparisonViewport),
            new FrameworkPropertyMetadata(0.0, FrameworkPropertyMetadataOptions.BindsTwoWayByDefault | FrameworkPropertyMetadataOptions.AffectsRender));

    public static readonly DependencyProperty SwipePositionProperty =
        DependencyProperty.Register(
            nameof(SwipePosition),
            typeof(double),
            typeof(ImageComparisonViewport),
            new FrameworkPropertyMetadata(0.5, FrameworkPropertyMetadataOptions.BindsTwoWayByDefault | FrameworkPropertyMetadataOptions.AffectsRender));

    public static readonly DependencyProperty OverlayOpacityProperty =
        DependencyProperty.Register(
            nameof(OverlayOpacity),
            typeof(double),
            typeof(ImageComparisonViewport),
            new FrameworkPropertyMetadata(0.5, FrameworkPropertyMetadataOptions.BindsTwoWayByDefault | FrameworkPropertyMetadataOptions.AffectsRender));

    // #172 (GUI-C-79): how many times each image was replaced. Read only by the automation peer; it
    // changes nothing about rendering. A version, not a pixel hash: the question the tests ask is
    // "was the viewport handed a new image", and a counter answers it at no cost.
    private int _sourceVersion;
    private int _processedVersion;

    private WpfPoint _lastDragPoint;
    private DragMode _dragMode = DragMode.None;

    public ImageComparisonViewport()
    {
        Focusable = true;
        ClipToBounds = true;
        RenderOptions.SetBitmapScalingMode(this, BitmapScalingMode.NearestNeighbor);
    }

    /// <summary>
    /// What this control has actually been given, for UI automation (#172).
    ///
    /// <para>The main viewport showed nothing from 54a3ae7 until #172 because every check looked at something
    /// other than the images this control received — the shell's presence, a constant, the view model's
    /// own properties (GUI-C-78). This is the one place that cannot be wrong about it: it reads the
    /// dependency properties the renderer reads. A null image reads <c>none</c>; a binding that failed to
    /// resolve also leaves the property null, so it reads <c>none</c> as well — both are defects here.</para>
    /// </summary>
    /// <summary>
    /// The mode the last frame was actually drawn in, or <c>none</c> when that frame had no source image
    /// and <c>not rendered</c> before the first frame (#149, GUI-C-96). A test that reads
    /// <see cref="CompareMode"/> or the settings value learns what was requested; this is what was drawn.
    /// </summary>
    public string RenderedMode => _renderedMode;

    private string _renderedMode = "not rendered";

    /// <summary>
    /// What the last frame was drawn with, beyond the mode (#182 follow-up, GUI-C-98). Every value is
    /// captured where the drawing code uses it, so a setting that never reached the control cannot
    /// appear here: <c>zoom</c> is the ZoomScale the frame read (<c>fit</c> for 0), <c>scale</c> and
    /// <c>offset</c> are the drawn image's scale and centre offset from the viewport centre (pan),
    /// <c>swipe</c> is the divider fraction drawn and <c>opacity</c> the overlay opacity pushed —
    /// <c>-</c> when the frame's mode draws neither.
    /// </summary>
    public string RenderedState => _renderedState;

    private string _renderedState = string.Empty;
    private double? _renderedSwipe;
    private double? _renderedOpacity;

    public string DescribeReceivedImages() =>
        string.Create(CultureInfo.InvariantCulture,
            $"source={Describe(SourceImage)} v{_sourceVersion}; processed={Describe(ProcessedImage)} v{_processedVersion}");

    private static string Describe(ImageSource? image) => image switch
    {
        null => "none",
        BitmapSource bitmap => $"{bitmap.PixelWidth}x{bitmap.PixelHeight}",
        _ => image.GetType().Name,
    };

    private ImageSource? _hashedImage;
    private string _hashedValue = "-";

    /// <summary>
    /// FNV-1a over the pixels of the processed layer this frame drew (#180, GUI-C-99), so a test can tell
    /// whether the chain changed what is on screen. Cached per image object: a render does not re-hash.
    /// </summary>
    private string ProcessedPixelHash(ImageSource image)
    {
        if (ReferenceEquals(image, _hashedImage))
        {
            return _hashedValue;
        }

        _hashedImage = image;
        _hashedValue = "-";
        if (image is BitmapSource bitmap)
        {
            var converted = bitmap.Format == PixelFormats.Bgra32 ? bitmap : new FormatConvertedBitmap(bitmap, PixelFormats.Bgra32, null, 0);
            var stride = converted.PixelWidth * 4;
            var buffer = new byte[stride * converted.PixelHeight];
            converted.CopyPixels(buffer, stride, 0);
            var hash = 14695981039346656037UL;
            foreach (var b in buffer)
            {
                hash = (hash ^ b) * 1099511628211UL;
            }

            _hashedValue = hash.ToString("x16", CultureInfo.InvariantCulture);
        }

        return _hashedValue;
    }

    private static void OnSourceImageChanged(DependencyObject d, DependencyPropertyChangedEventArgs e) =>
        ((ImageComparisonViewport)d)._sourceVersion++;

    private static void OnProcessedImageChanged(DependencyObject d, DependencyPropertyChangedEventArgs e) =>
        ((ImageComparisonViewport)d)._processedVersion++;

    protected override System.Windows.Automation.Peers.AutomationPeer OnCreateAutomationPeer() =>
        new ImageComparisonViewportAutomationPeer(this);

    public ImageSource? SourceImage
    {
        get => (ImageSource?)GetValue(SourceImageProperty);
        set => SetValue(SourceImageProperty, value);
    }

    public ImageSource? ProcessedImage
    {
        get => (ImageSource?)GetValue(ProcessedImageProperty);
        set => SetValue(ProcessedImageProperty, value);
    }

    public string CompareMode
    {
        get => (string)GetValue(CompareModeProperty);
        set => SetValue(CompareModeProperty, value);
    }

    public string ChainStatus
    {
        get => (string)GetValue(ChainStatusProperty);
        set => SetValue(ChainStatusProperty, value);
    }

    public double ZoomScale
    {
        get => (double)GetValue(ZoomScaleProperty);
        set => SetValue(ZoomScaleProperty, Math.Clamp(value, 0.0, 16.0));
    }

    public double PanX
    {
        get => (double)GetValue(PanXProperty);
        set => SetValue(PanXProperty, value);
    }

    public double PanY
    {
        get => (double)GetValue(PanYProperty);
        set => SetValue(PanYProperty, value);
    }

    public double SwipePosition
    {
        get => (double)GetValue(SwipePositionProperty);
        set => SetValue(SwipePositionProperty, Math.Clamp(value, 0.0, 1.0));
    }

    public double OverlayOpacity
    {
        get => (double)GetValue(OverlayOpacityProperty);
        set => SetValue(OverlayOpacityProperty, Math.Clamp(value, 0.0, 1.0));
    }

    // @MX:NOTE: Renders 7 comparison modes via custom DrawingContext. DifferenceHeatmap draws a
    // true per-pixel |source - processed| image in linear grey (GUI-C-57, #149 G-4).
    protected override void OnRender(DrawingContext drawingContext)
    {
        base.OnRender(drawingContext);

        var viewport = new Rect(0, 0, ActualWidth, ActualHeight);
        drawingContext.DrawRectangle(new SolidColorBrush(WpfColor.FromRgb(8, 13, 23)), null, viewport);

        if (SourceImage is null)
        {
            _renderedMode = "none";
            _renderedState = string.Empty;
            DrawCenteredText(drawingContext, "Load a RAW image to compare source and processed output.", viewport);
            return;
        }

        var processed = ProcessedImage ?? SourceImage;
        var imageRect = GetImageRect(SourceImage);
        var mode = NormalizeMode(CompareMode);
        _renderedMode = mode;
        _renderedSwipe = null;
        _renderedOpacity = null;

        drawingContext.PushClip(new RectangleGeometry(viewport));
        switch (mode)
        {
            case "ProcessedOnly":
                DrawImage(drawingContext, processed, imageRect);
                break;
            case "SplitLocked":
                DrawSplitLocked(drawingContext, viewport, imageRect, processed);
                break;
            case "OverlayOpacity":
                DrawImage(drawingContext, SourceImage, imageRect);
                _renderedOpacity = Math.Clamp(OverlayOpacity, 0.0, 1.0);
                drawingContext.PushOpacity(_renderedOpacity.Value);
                DrawImage(drawingContext, processed, imageRect);
                drawingContext.Pop();
                break;
            case "DifferenceHeatmap":
                DrawImage(drawingContext, DifferenceImage(SourceImage, processed) ?? SourceImage, imageRect);
                break;
            case "SourceOnly":
                DrawImage(drawingContext, SourceImage, imageRect);
                break;
            case "SwipeHorizontal":
                DrawImage(drawingContext, SourceImage, imageRect);
                DrawHorizontalSwipe(drawingContext, viewport, imageRect, processed);
                break;
            case "SwipeVertical":
            default:
                DrawImage(drawingContext, SourceImage, imageRect);
                DrawVerticalSwipe(drawingContext, viewport, imageRect, processed);
                break;
        }

        drawingContext.Pop();
        _renderedState = string.Create(CultureInfo.InvariantCulture,
            $"zoom={(ZoomScale <= 0.0 ? "fit" : ZoomScale.ToString("0.####", CultureInfo.InvariantCulture))}; " +
            $"scale={imageRect.Width / Math.Max(1.0, SourceImage.Width):0.####}; " +
            $"offset={imageRect.X + (imageRect.Width / 2.0) - (ActualWidth / 2.0):0.#},{imageRect.Y + (imageRect.Height / 2.0) - (ActualHeight / 2.0):0.#}; " +
            $"swipe={(_renderedSwipe is { } sw ? sw.ToString("0.####", CultureInfo.InvariantCulture) : "-")}; " +
            $"opacity={(_renderedOpacity is { } op ? op.ToString("0.####", CultureInfo.InvariantCulture) : "-")}; " +
            $"processed={ProcessedPixelHash(processed)}");
        DrawHud(drawingContext, viewport, mode);
    }

    protected override void OnMouseDown(MouseButtonEventArgs e)
    {
        Focus();
        CaptureMouse();
        _lastDragPoint = e.GetPosition(this);

        if (e.ChangedButton == MouseButton.Right || e.ChangedButton == MouseButton.Middle)
        {
            _dragMode = DragMode.Pan;
        }
        else
        {
            _dragMode = IsSwipeMode(NormalizeMode(CompareMode)) ? DragMode.Swipe : DragMode.Pan;
        }

        e.Handled = true;
    }

    protected override void OnMouseMove(WpfMouseEventArgs e)
    {
        if (_dragMode == DragMode.None || !IsMouseCaptured)
        {
            return;
        }

        var point = e.GetPosition(this);
        var delta = point - _lastDragPoint;
        _lastDragPoint = point;

        if (_dragMode == DragMode.Swipe)
        {
            if (NormalizeMode(CompareMode) == "SwipeHorizontal")
            {
                SwipePosition = ActualHeight <= 0 ? SwipePosition : point.Y / ActualHeight;
            }
            else
            {
                SwipePosition = ActualWidth <= 0 ? SwipePosition : point.X / ActualWidth;
            }
        }
        else
        {
            PanX += delta.X;
            PanY += delta.Y;
        }

        e.Handled = true;
    }

    protected override void OnMouseUp(MouseButtonEventArgs e)
    {
        _dragMode = DragMode.None;
        ReleaseMouseCapture();
        e.Handled = true;
    }

    protected override void OnMouseWheel(MouseWheelEventArgs e)
    {
        var currentScale = GetEffectiveScale();
        var factor = e.Delta > 0 ? 1.20 : 1.0 / 1.20;
        ZoomScale = Math.Clamp(currentScale * factor, 0.01, 16.0);
        e.Handled = true;
    }

    private void DrawVerticalSwipe(DrawingContext drawingContext, Rect viewport, Rect imageRect, ImageSource processed)
    {
        _renderedSwipe = Math.Clamp(SwipePosition, 0.0, 1.0);
        var dividerX = _renderedSwipe.Value * viewport.Width;
        drawingContext.PushClip(new RectangleGeometry(new Rect(dividerX, 0, Math.Max(0, viewport.Width - dividerX), viewport.Height)));
        DrawImage(drawingContext, processed, imageRect);
        drawingContext.Pop();
        DrawDivider(drawingContext, new WpfPoint(dividerX, 0), new WpfPoint(dividerX, viewport.Height));
    }

    private void DrawHorizontalSwipe(DrawingContext drawingContext, Rect viewport, Rect imageRect, ImageSource processed)
    {
        _renderedSwipe = Math.Clamp(SwipePosition, 0.0, 1.0);
        var dividerY = _renderedSwipe.Value * viewport.Height;
        drawingContext.PushClip(new RectangleGeometry(new Rect(0, dividerY, viewport.Width, Math.Max(0, viewport.Height - dividerY))));
        DrawImage(drawingContext, processed, imageRect);
        drawingContext.Pop();
        DrawDivider(drawingContext, new WpfPoint(0, dividerY), new WpfPoint(viewport.Width, dividerY));
    }

    private void DrawSplitLocked(DrawingContext drawingContext, Rect viewport, Rect imageRect, ImageSource processed)
    {
        var halfWidth = viewport.Width / 2.0;
        drawingContext.PushClip(new RectangleGeometry(new Rect(0, 0, halfWidth, viewport.Height)));
        DrawImage(drawingContext, SourceImage!, imageRect);
        drawingContext.Pop();

        drawingContext.PushClip(new RectangleGeometry(new Rect(halfWidth, 0, halfWidth, viewport.Height)));
        DrawImage(drawingContext, processed, imageRect);
        drawingContext.Pop();

        DrawDivider(drawingContext, new WpfPoint(halfWidth, 0), new WpfPoint(halfWidth, viewport.Height));
    }

    /// <summary>
    /// The per-pixel absolute difference of the two layers, as a linear grey image.
    ///
    /// <para><b>Value</b>: <c>|source - processed|</c> (MENU-001 L288; COMPARE-001 L68's "signed or
    /// absolute" admits it). <b>Colour</b>: linear grey, 0 = black, 255 = white — decided for #149
    /// because this product's display path is greyscale, and a rainbow map draws an edge where the
    /// values are continuous, which reads as a defect that is not there.</para>
    ///
    /// <para>Grey is taken from the LARGEST of the three channel differences rather than per channel,
    /// so a change confined to one channel still shows at its full size and the output is grey for
    /// colour input too. For the greyscale images this product shows, the two are identical.</para>
    ///
    /// <para>What it replaced: the source drawn under the processed layer at 42 % opacity plus a red
    /// wash. That composite is why identical inputs used to render differently depending on the
    /// image (measured 255/255 deviation), why a darker change read differently from a brighter one
    /// of the same size (gap 38.0), and why a local change separated by only 16 % of what the true
    /// difference gives (GUI-C-53/54/55).</para>
    ///
    /// <para>Cached on the two source references: <c>OnRender</c> runs on every pan, zoom and resize,
    /// and the difference does not depend on any of them.</para>
    /// </summary>
    private ImageSource? DifferenceImage(ImageSource source, ImageSource processed)
    {
        if (ReferenceEquals(_differenceKey.Source, source)
            && ReferenceEquals(_differenceKey.Processed, processed))
        {
            return _differenceValue;
        }

        _differenceKey = (source, processed);
        _differenceValue = ComputeDifference(source, processed);
        return _differenceValue;
    }

    private (ImageSource? Source, ImageSource? Processed) _differenceKey;
    private ImageSource? _differenceValue;

    /// <summary>
    /// Builds the difference bitmap, or null when the two layers cannot be differenced pixel for
    /// pixel — different dimensions, or a source that is not a bitmap. The caller then draws the
    /// source layer, which is wrong but visible; silently drawing black would look like "no
    /// difference", the one answer that must never be faked.
    /// </summary>
    private static ImageSource? ComputeDifference(ImageSource source, ImageSource processed)
    {
        if (source is not BitmapSource left || processed is not BitmapSource right) return null;
        if (left.PixelWidth != right.PixelWidth || left.PixelHeight != right.PixelHeight) return null;
        if (left.PixelWidth == 0 || left.PixelHeight == 0) return null;

        var width = left.PixelWidth;
        var height = left.PixelHeight;
        var stride = width * 4;

        var lhs = new byte[stride * height];
        var rhs = new byte[stride * height];
        new FormatConvertedBitmap(left, PixelFormats.Bgra32, null, 0).CopyPixels(lhs, stride, 0);
        new FormatConvertedBitmap(right, PixelFormats.Bgra32, null, 0).CopyPixels(rhs, stride, 0);

        var difference = new byte[stride * height];
        for (var i = 0; i < difference.Length; i += 4)
        {
            var grey = (byte)Math.Max(
                Math.Abs(lhs[i] - rhs[i]),
                Math.Max(Math.Abs(lhs[i + 1] - rhs[i + 1]), Math.Abs(lhs[i + 2] - rhs[i + 2])));

            difference[i] = grey;
            difference[i + 1] = grey;
            difference[i + 2] = grey;
            difference[i + 3] = 255;
        }

        var bitmap = BitmapSource.Create(
            width, height, left.DpiX, left.DpiY, PixelFormats.Bgra32, null, difference, stride);
        bitmap.Freeze();
        return bitmap;
    }

    private static void DrawImage(DrawingContext drawingContext, ImageSource image, Rect imageRect)
    {
        drawingContext.DrawImage(image, imageRect);
    }

    private void DrawDivider(DrawingContext drawingContext, WpfPoint start, WpfPoint end)
    {
        var pen = new WpfPen(new SolidColorBrush(WpfColor.FromRgb(248, 250, 252)), 2.0);
        drawingContext.DrawLine(pen, start, end);
    }

    private Rect GetImageRect(ImageSource image)
    {
        var scale = GetEffectiveScale();
        var width = Math.Max(1.0, image.Width) * scale;
        var height = Math.Max(1.0, image.Height) * scale;
        var x = ((ActualWidth - width) / 2.0) + PanX;
        var y = ((ActualHeight - height) / 2.0) + PanY;
        return new Rect(x, y, width, height);
    }

    // @MX:NOTE: [AUTO] ZoomScale == 0 means fit-to-viewport; returns min(fitX, fitY) to preserve aspect ratio; called by OnRender and OnMouseWheel
    private double GetEffectiveScale()
    {
        if (SourceImage is null || ActualWidth <= 0 || ActualHeight <= 0)
        {
            return 1.0;
        }

        if (ZoomScale > 0.0)
        {
            return ZoomScale;
        }

        var fitX = ActualWidth / Math.Max(1.0, SourceImage.Width);
        var fitY = ActualHeight / Math.Max(1.0, SourceImage.Height);
        return Math.Max(0.01, Math.Min(fitX, fitY));
    }

    private void DrawHud(DrawingContext drawingContext, Rect viewport, string mode)
    {
        var zoomText = ZoomScale <= 0.0 ? "fit" : $"{ZoomScale * 100.0:0}%";
        var chain = string.IsNullOrWhiteSpace(ChainStatus) ? string.Empty : $" | {ChainStatus}";
        var text = $"{mode} | zoom {zoomText} | pan {PanX:0},{PanY:0} | swipe {SwipePosition:P0}{chain}";
        var formatted = CreateText(text, 12, WpfBrushes.White);
        var padding = new Thickness(8, 4, 8, 4);
        var hudRect = new Rect(10, 10, formatted.Width + padding.Left + padding.Right, formatted.Height + padding.Top + padding.Bottom);
        drawingContext.DrawRoundedRectangle(new SolidColorBrush(WpfColor.FromArgb(180, 15, 23, 42)), null, hudRect, 5, 5);
        drawingContext.DrawText(formatted, new WpfPoint(hudRect.X + padding.Left, hudRect.Y + padding.Top));

        if (mode == "DifferenceHeatmap")
        {
            var label = CreateText("Difference |source - processed|", 12, WpfBrushes.White);
            drawingContext.DrawText(label, new WpfPoint(12, viewport.Bottom - label.Height - 12));
        }
    }

    private void DrawCenteredText(DrawingContext drawingContext, string text, Rect viewport)
    {
        var formatted = CreateText(text, 16, new SolidColorBrush(WpfColor.FromRgb(203, 213, 225)));
        var point = new WpfPoint(
            Math.Max(0, (viewport.Width - formatted.Width) / 2.0),
            Math.Max(0, (viewport.Height - formatted.Height) / 2.0));
        drawingContext.DrawText(formatted, point);
    }

    private FormattedText CreateText(string text, double size, WpfBrush brush)
    {
        return new FormattedText(
            text,
            CultureInfo.CurrentCulture,
            WpfFlowDirection.LeftToRight,
            new Typeface("Segoe UI"),
            size,
            brush,
            VisualTreeHelper.GetDpi(this).PixelsPerDip);
    }

    /// <summary>
    /// The mode this control will draw, resolved through the one rule (#161, GUI-C-60).
    ///
    /// This used to be a switch of its own, which is why the HUD could say one mode while the
    /// automation HelpText said another: two resolutions of the same string. It delegates now, and
    /// the settings property only ever holds a supported mode anyway — so for values that came from
    /// the app this is the identity. It still guards, because the control is a public element and a
    /// caller can set CompareMode directly.
    /// </summary>
    private static string NormalizeMode(string? mode) => ComparisonModes.Normalize(mode);

    private static bool IsSwipeMode(string mode) => mode is "SwipeVertical" or "SwipeHorizontal";

    private enum DragMode
    {
        None,
        Pan,
        Swipe
    }
}

/// <summary>
/// Makes the comparison viewport visible to UI automation, with what it received as its status (#172).
/// Before this the control had no peer at all, so neither tests nor assistive technology could see it.
/// </summary>
internal sealed class ImageComparisonViewportAutomationPeer(ImageComparisonViewport owner)
    : System.Windows.Automation.Peers.FrameworkElementAutomationPeer(owner)
{
    protected override string GetClassNameCore() => nameof(ImageComparisonViewport);

    protected override System.Windows.Automation.Peers.AutomationControlType GetAutomationControlTypeCore() =>
        System.Windows.Automation.Peers.AutomationControlType.Image;

    protected override string GetNameCore()
    {
        var name = base.GetNameCore();
        return string.IsNullOrEmpty(name) ? "Comparison viewport" : name;
    }

    protected override string GetItemStatusCore() => ((ImageComparisonViewport)Owner).DescribeReceivedImages();

    /// <summary><c>rendered=&lt;mode&gt;</c> — the mode of the last drawn frame (#149, GUI-C-96).</summary>
    protected override string GetHelpTextCore()
    {
        var owner = (ImageComparisonViewport)Owner;
        return string.IsNullOrEmpty(owner.RenderedState)
            ? $"rendered={owner.RenderedMode}"
            : $"rendered={owner.RenderedMode}; {owner.RenderedState}";
    }
}
