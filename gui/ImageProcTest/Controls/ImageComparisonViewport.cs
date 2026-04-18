using System.Globalization;
using System.Windows;
using System.Windows.Input;
using System.Windows.Media;
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
            new FrameworkPropertyMetadata(null, FrameworkPropertyMetadataOptions.AffectsRender));

    public static readonly DependencyProperty ProcessedImageProperty =
        DependencyProperty.Register(
            nameof(ProcessedImage),
            typeof(ImageSource),
            typeof(ImageComparisonViewport),
            new FrameworkPropertyMetadata(null, FrameworkPropertyMetadataOptions.AffectsRender));

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

    private WpfPoint _lastDragPoint;
    private DragMode _dragMode = DragMode.None;

    public ImageComparisonViewport()
    {
        Focusable = true;
        ClipToBounds = true;
        RenderOptions.SetBitmapScalingMode(this, BitmapScalingMode.NearestNeighbor);
    }

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

    protected override void OnRender(DrawingContext drawingContext)
    {
        base.OnRender(drawingContext);

        var viewport = new Rect(0, 0, ActualWidth, ActualHeight);
        drawingContext.DrawRectangle(new SolidColorBrush(WpfColor.FromRgb(8, 13, 23)), null, viewport);

        if (SourceImage is null)
        {
            DrawCenteredText(drawingContext, "Load a RAW image to compare source and processed output.", viewport);
            return;
        }

        var processed = ProcessedImage ?? SourceImage;
        var imageRect = GetImageRect(SourceImage);
        var mode = NormalizeMode(CompareMode);

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
                drawingContext.PushOpacity(Math.Clamp(OverlayOpacity, 0.0, 1.0));
                DrawImage(drawingContext, processed, imageRect);
                drawingContext.Pop();
                break;
            case "DifferenceHeatmap":
                DrawImage(drawingContext, SourceImage, imageRect);
                drawingContext.PushOpacity(0.42);
                DrawImage(drawingContext, processed, imageRect);
                drawingContext.Pop();
                drawingContext.DrawRectangle(new SolidColorBrush(WpfColor.FromArgb(76, 220, 38, 38)), null, viewport);
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
        var dividerX = Math.Clamp(SwipePosition, 0.0, 1.0) * viewport.Width;
        drawingContext.PushClip(new RectangleGeometry(new Rect(dividerX, 0, Math.Max(0, viewport.Width - dividerX), viewport.Height)));
        DrawImage(drawingContext, processed, imageRect);
        drawingContext.Pop();
        DrawDivider(drawingContext, new WpfPoint(dividerX, 0), new WpfPoint(dividerX, viewport.Height));
    }

    private void DrawHorizontalSwipe(DrawingContext drawingContext, Rect viewport, Rect imageRect, ImageSource processed)
    {
        var dividerY = Math.Clamp(SwipePosition, 0.0, 1.0) * viewport.Height;
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
        var text = $"{mode} | zoom {zoomText} | pan {PanX:0},{PanY:0} | swipe {SwipePosition:P0}";
        var formatted = CreateText(text, 12, WpfBrushes.White);
        var padding = new Thickness(8, 4, 8, 4);
        var hudRect = new Rect(10, 10, formatted.Width + padding.Left + padding.Right, formatted.Height + padding.Top + padding.Bottom);
        drawingContext.DrawRoundedRectangle(new SolidColorBrush(WpfColor.FromArgb(180, 15, 23, 42)), null, hudRect, 5, 5);
        drawingContext.DrawText(formatted, new WpfPoint(hudRect.X + padding.Left, hudRect.Y + padding.Top));

        if (mode == "DifferenceHeatmap")
        {
            var label = CreateText("Difference heatmap preview", 12, WpfBrushes.White);
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

    private static string NormalizeMode(string? mode) => mode switch
    {
        "SwipeHorizontal" => "SwipeHorizontal",
        "SplitLocked" => "SplitLocked",
        "OverlayOpacity" => "OverlayOpacity",
        "DifferenceHeatmap" => "DifferenceHeatmap",
        "SourceOnly" => "SourceOnly",
        "ProcessedOnly" => "ProcessedOnly",
        _ => "SwipeVertical"
    };

    private static bool IsSwipeMode(string mode) => mode is "SwipeVertical" or "SwipeHorizontal";

    private enum DragMode
    {
        None,
        Pan,
        Swipe
    }
}
