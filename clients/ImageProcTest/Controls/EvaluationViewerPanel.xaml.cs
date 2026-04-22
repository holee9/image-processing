using System;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Threading;
using ImageProcTest.ViewModels;

namespace ImageProcTest.Controls
{
    public partial class EvaluationViewerPanel : UserControl
    {
        private readonly ViewportViewModel viewport = new();
        private RawPreviewResult? currentPreview;
        private NativePreprocessPreviewResult? nativePreview;
        private IReadOnlyList<float>? algorithmPreviewPixels;
        private bool isDraggingComparisonSwipe;
        private bool isPanningViewport;
        private Point panStartPoint;
        private double panStartHorizontalOffset;
        private double panStartVerticalOffset;
        private bool isUpdatingViewerControls;
        private bool isUpdatingHistogramRanges;
        private ViewportRenderParams originalViewportParams = ViewportRenderParams.Default;
        private ViewportRenderParams processedViewportParams = ViewportRenderParams.Default;

        public EvaluationViewerPanel()
        {
            InitializeComponent();
            DataContext = viewport;
        }

        internal void PrepareForRawLoad(string path)
        {
            RawPreviewTitleText.Text = $"Raw preview: {Path.GetFileName(path)}";
            RawPreviewInfoText.Text = "Loading preview and SHA-256...";
            RawPreviewHashText.Text = "SHA-256: calculating";
            currentPreview = null;
            nativePreview = null;
            algorithmPreviewPixels = null;
            ClearViewerRenderState();
            RawZoomSlider.Value = 1;
            CompareSwipeSlider.Value = 0.5;
            AfterPreviewLabelText.Text = "After preview";
            ProcessingScaffoldText.Text = "Loading target raw image for calibration evaluation.";
        }

        internal void LoadRawPreview(RawPreviewResult preview, bool nativeReady)
        {
            currentPreview = preview;
            nativePreview = null;
            algorithmPreviewPixels = null;
            ComparisonCanvas.Width = preview.PreviewWidth;
            ComparisonCanvas.Height = preview.PreviewHeight;
            ResetViewerParamsForPreview(preview);
            RenderComparisonViewports();
            SyncViewerControlsFromActive();
            RawPreviewTitleText.Text = $"Raw preview: {Path.GetFileName(preview.FilePath)}";
            RawPreviewInfoText.Text =
                $"Source={preview.Width}x{preview.Height} uint16, file={RawFileDescriptor.FormatBytes(preview.FileSizeBytes)}, " +
                $"preview={preview.PreviewWidth}x{preview.PreviewHeight}, stride={preview.SampleStride}, " +
                $"min={preview.MinValue}, max={preview.MaxValue}";
            RawPreviewHashText.Text = $"SHA-256: {preview.Sha256}";
            ProcessingScaffoldText.Text = nativeReady
                ? "Before=original. Run the active calibration algorithm chain to update After."
                : "Before=original, After=identity placeholder. Native correction remains disabled until xpe_preprocess.dll export readiness passes.";
            Dispatcher.BeginInvoke(new Action(FitComparisonToViewport), DispatcherPriority.Loaded);
        }

        internal void ClearRawPreview(string message)
        {
            currentPreview = null;
            nativePreview = null;
            algorithmPreviewPixels = null;
            ClearViewerRenderState();
            RawPreviewTitleText.Text = "Raw preview: no file loaded";
            RawPreviewInfoText.Text = message;
            RawPreviewHashText.Text = "SHA-256: unavailable";
            AfterPreviewLabelText.Text = "After preview";
            ProcessingScaffoldText.Text = "Load a target raw image before running calibration evaluation.";
        }

        internal void ClearNativePreview()
        {
            nativePreview = null;
            algorithmPreviewPixels = null;
            AfterPreviewLabelText.Text = "After preview";
            ProcessingScaffoldText.Text = "Correction output cleared. After shows bypass output until the selected stages are run.";
            RenderComparisonViewports();
        }

        internal void SetNativePreview(NativePreprocessPreviewResult result, string calibrationDirectoryPath)
        {
            nativePreview = result;
            algorithmPreviewPixels = null;
            if (IsViewerLinked())
            {
                processedViewportParams = originalViewportParams;
            }
            else if (result.OutputPixels is { Count: > 0 } outputPixels)
            {
                processedViewportParams = PreserveRenderFlags(
                    ViewportRenderService.AutoFit(outputPixels as float[] ?? outputPixels.ToArray()),
                    processedViewportParams);
            }

            ConfigureViewerSliderBounds();
            RenderComparisonViewports();
            SyncViewerControlsFromActive();
            AfterPreviewLabelText.Text = "Native after";
            ProcessingScaffoldText.Text =
                $"Calibration applied from {calibrationDirectoryPath}; artifacts={result.ArtifactDirectory}. " +
                "Before/After uses the loaded raw image dimensions and current render window only.";
        }

        internal void SetBypassPreview(NativePreprocessPreviewResult result, string reason)
        {
            nativePreview = result;
            algorithmPreviewPixels = null;
            processedViewportParams = originalViewportParams;
            ConfigureViewerSliderBounds();
            RenderComparisonViewports();
            SyncViewerControlsFromActive();
            AfterPreviewLabelText.Text = "Bypass after";
            ProcessingScaffoldText.Text = $"Bypass output: {reason} Before and After use separate buffers, but no correction stage modifies the output.";
        }

        internal void SetAlgorithmPreview(IReadOnlyList<float> outputPixels, string afterLabel, string details)
        {
            nativePreview = null;
            algorithmPreviewPixels = outputPixels;
            if (IsViewerLinked())
            {
                processedViewportParams = originalViewportParams;
            }
            else if (outputPixels.Count > 0)
            {
                processedViewportParams = PreserveRenderFlags(
                    ViewportRenderService.AutoFit(outputPixels as float[] ?? outputPixels.ToArray()),
                    processedViewportParams);
            }

            ConfigureViewerSliderBounds();
            RenderComparisonViewports();
            SyncViewerControlsFromActive();
            AfterPreviewLabelText.Text = afterLabel;
            ProcessingScaffoldText.Text = details;
        }

        internal void UpdateNativeReadiness(bool nativeReady)
        {
            if (currentPreview is null)
            {
                ProcessingScaffoldText.Text = "Load a target raw image before running calibration evaluation.";
                return;
            }

            if (nativePreview is not null || algorithmPreviewPixels is not null)
            {
                return;
            }

            ProcessingScaffoldText.Text = nativeReady
                ? "Before=original. Run the active calibration algorithm chain to update After."
                : "Before=original, After=identity placeholder. Native correction remains disabled until xpe_preprocess.dll export readiness passes.";
        }

        internal void SetPreviewSelectionMessage(string title, string info, string hashText = "SHA-256: not calculated until preview load")
        {
            RawPreviewTitleText.Text = title;
            RawPreviewInfoText.Text = info;
            RawPreviewHashText.Text = hashText;
        }

        internal void ResetComparisonLayout()
        {
            CompareSwipeSlider.Value = 0.5;
            SetComparisonZoom(1.0);
            if (currentPreview is not null)
            {
                FitComparisonToViewport();
            }
        }

        internal void FitComparisonToViewport()
        {
            if (currentPreview is null)
            {
                return;
            }

            var viewportWidth = RawImageScrollViewer.ViewportWidth > 0
                ? RawImageScrollViewer.ViewportWidth
                : RawImageScrollViewer.ActualWidth;
            var viewportHeight = RawImageScrollViewer.ViewportHeight > 0
                ? RawImageScrollViewer.ViewportHeight
                : RawImageScrollViewer.ActualHeight;

            if (viewportWidth <= 0 || viewportHeight <= 0)
            {
                return;
            }

            var widthScale = Math.Max(0.01, (viewportWidth - 24) / currentPreview.PreviewWidth);
            var heightScale = Math.Max(0.01, (viewportHeight - 24) / currentPreview.PreviewHeight);
            var fitScale = Math.Min(1.0, Math.Min(widthScale, heightScale));
            RawZoomSlider.Value = Math.Clamp(fitScale, RawZoomSlider.Minimum, RawZoomSlider.Maximum);
        }

        internal void AdjustComparisonZoom(double factor)
        {
            SetComparisonZoom(RawZoomSlider.Value * factor);
        }

        internal void SetComparisonZoom(double value)
        {
            RawZoomSlider.Value = Math.Clamp(value, RawZoomSlider.Minimum, RawZoomSlider.Maximum);
        }

        internal ViewportRenderStateSnapshot CreateSnapshot()
        {
            return new ViewportRenderStateSnapshot(
                originalViewportParams.WindowCenter,
                originalViewportParams.WindowWidth,
                originalViewportParams.Invert,
                originalViewportParams.Lut.ToString(),
                processedViewportParams.WindowCenter,
                processedViewportParams.WindowWidth,
                processedViewportParams.Invert,
                processedViewportParams.Lut.ToString(),
                IsViewerLinked(),
                IsAfterViewportTarget() ? "After" : "Original",
                RawZoomSlider.Value,
                CompareSwipeSlider.Value,
                viewport.OriginalHistogram.Summary,
                viewport.ProcessedHistogram.Summary);
        }

        private void RawZoomSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (RawImageScaleTransform is null || ZoomValueText is null)
            {
                return;
            }

            var scale = Math.Max(0.1, e.NewValue);
            viewport.Zoom = scale;
            RawImageScaleTransform.ScaleX = scale;
            RawImageScaleTransform.ScaleY = scale;
            ZoomValueText.Text = $"{scale * 100:0}%";
        }

        private void ZoomFitButton_Click(object sender, RoutedEventArgs e)
        {
            FitComparisonToViewport();
        }

        private void ZoomActualButton_Click(object sender, RoutedEventArgs e)
        {
            SetComparisonZoom(1.0);
        }

        private void ZoomInButton_Click(object sender, RoutedEventArgs e)
        {
            AdjustComparisonZoom(1.25);
        }

        private void ZoomOutButton_Click(object sender, RoutedEventArgs e)
        {
            AdjustComparisonZoom(0.8);
        }

        private void CompareSwipeSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            viewport.SwipeFraction = e.NewValue;
            UpdateComparisonClip();
        }

        private void ComparisonCanvas_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            UpdateComparisonClip();
        }

        private void ComparisonCanvas_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            if (isPanningViewport)
            {
                return;
            }

            isDraggingComparisonSwipe = true;
            ComparisonCanvas.CaptureMouse();
            UpdateComparisonSwipeFromPoint(e.GetPosition(ComparisonCanvas).X);
            e.Handled = true;
        }

        private void ComparisonCanvas_MouseRightButtonDown(object sender, MouseButtonEventArgs e)
        {
            if (isDraggingComparisonSwipe)
            {
                return;
            }

            isPanningViewport = true;
            panStartPoint = e.GetPosition(RawImageScrollViewer);
            panStartHorizontalOffset = RawImageScrollViewer.HorizontalOffset;
            panStartVerticalOffset = RawImageScrollViewer.VerticalOffset;
            ComparisonCanvas.Cursor = Cursors.SizeAll;
            ComparisonCanvas.CaptureMouse();
            e.Handled = true;
        }

        private void ComparisonCanvas_MouseMove(object sender, MouseEventArgs e)
        {
            if (isPanningViewport)
            {
                if (e.RightButton != MouseButtonState.Pressed)
                {
                    EndViewportPan();
                    return;
                }

                var currentPoint = e.GetPosition(RawImageScrollViewer);
                RawImageScrollViewer.ScrollToHorizontalOffset(
                    panStartHorizontalOffset + panStartPoint.X - currentPoint.X);
                RawImageScrollViewer.ScrollToVerticalOffset(
                    panStartVerticalOffset + panStartPoint.Y - currentPoint.Y);
                e.Handled = true;
                return;
            }

            if (!isDraggingComparisonSwipe || e.LeftButton != MouseButtonState.Pressed)
            {
                return;
            }

            UpdateComparisonSwipeFromPoint(e.GetPosition(ComparisonCanvas).X);
            e.Handled = true;
        }

        private void ComparisonCanvas_MouseLeftButtonUp(object sender, MouseButtonEventArgs e)
        {
            if (!isDraggingComparisonSwipe)
            {
                return;
            }

            UpdateComparisonSwipeFromPoint(e.GetPosition(ComparisonCanvas).X);
            EndComparisonSwipeDrag();
            e.Handled = true;
        }

        private void ComparisonCanvas_MouseRightButtonUp(object sender, MouseButtonEventArgs e)
        {
            if (!isPanningViewport)
            {
                return;
            }

            EndViewportPan();
            e.Handled = true;
        }

        private void ComparisonCanvas_MouseLeave(object sender, MouseEventArgs e)
        {
            if (isDraggingComparisonSwipe && e.LeftButton != MouseButtonState.Pressed)
            {
                EndComparisonSwipeDrag();
            }

            if (isPanningViewport && e.RightButton != MouseButtonState.Pressed)
            {
                EndViewportPan();
            }
        }

        private void ComparisonCanvas_MouseWheel(object sender, MouseWheelEventArgs e)
        {
            AdjustComparisonZoom(e.Delta > 0 ? 1.1 : 1.0 / 1.1);
            e.Handled = true;
        }

        private void ViewerWindowSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (isUpdatingViewerControls || ViewerWindowCenterText is null || ViewerWindowWidthText is null)
            {
                return;
            }

            ApplyViewerControlsToParams();
            RenderComparisonViewports();
        }

        private void ViewerControl_Changed(object sender, RoutedEventArgs e)
        {
            if (isUpdatingViewerControls || ViewerWindowCenterSlider is null || ViewerWindowWidthSlider is null)
            {
                return;
            }

            ApplyViewerControlsToParams();
            RenderComparisonViewports();
        }

        private void OriginalHistogramControl_RangeChanged(object sender, HistogramRangeChangedEventArgs e)
        {
            ApplyHistogramRangeToParams(viewport.OriginalHistogram, e, afterTarget: false);
        }

        private void AfterHistogramControl_RangeChanged(object sender, HistogramRangeChangedEventArgs e)
        {
            ApplyHistogramRangeToParams(viewport.ProcessedHistogram, e, afterTarget: true);
        }

        private void ViewerTargetComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (isUpdatingViewerControls)
            {
                return;
            }

            viewport.ActiveTarget = IsAfterViewportTarget() ? "After" : "Original";
            SyncViewerControlsFromActive();
        }

        private void ViewerPresetComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (isUpdatingViewerControls || currentPreview is null)
            {
                return;
            }

            var selectedPreset = GetSelectedComboText(ViewerPresetComboBox);
            if (string.IsNullOrWhiteSpace(selectedPreset))
            {
                return;
            }

            var current = GetActiveViewportParams();
            var next = string.Equals(selectedPreset, "Auto Fit", StringComparison.OrdinalIgnoreCase)
                ? CreateAutoFitForActiveTarget(current)
                : ViewportRenderService.Presets.TryGetValue(selectedPreset, out var preset)
                    ? PreserveRenderFlags(preset, current)
                    : current;

            if (IsViewerLinked())
            {
                originalViewportParams = next;
                processedViewportParams = next;
            }
            else
            {
                SetActiveViewportParams(next);
            }

            ConfigureViewerSliderBounds();
            SyncViewerControlsFromActive();
            RenderComparisonViewports();
        }

        private void ResetViewerParamsForPreview(RawPreviewResult preview)
        {
            originalViewportParams = ViewportRenderService.AutoFit(preview.SampledPixels);
            processedViewportParams = originalViewportParams;
            viewport.OriginalParams = originalViewportParams;
            viewport.ProcessedParams = processedViewportParams;
            ConfigureViewerSliderBounds();
        }

        private void ConfigureViewerSliderBounds()
        {
            if (ViewerWindowCenterSlider is null || ViewerWindowWidthSlider is null || currentPreview is null)
            {
                return;
            }

            var min = (double)currentPreview.MinValue;
            var max = (double)currentPreview.MaxValue;
            if (nativePreview is not null)
            {
                min = Math.Min(min, nativePreview.OutputMin);
                max = Math.Max(max, nativePreview.OutputMax);
            }
            else if (algorithmPreviewPixels is { Count: > 0 })
            {
                foreach (var value in algorithmPreviewPixels)
                {
                    if (!float.IsFinite(value))
                    {
                        continue;
                    }

                    min = Math.Min(min, value);
                    max = Math.Max(max, value);
                }
            }

            var range = Math.Max(1.0, max - min);
            isUpdatingViewerControls = true;
            try
            {
                ViewerWindowCenterSlider.Minimum = Math.Floor(min - range);
                ViewerWindowCenterSlider.Maximum = Math.Ceiling(max + range);
                ViewerWindowWidthSlider.Minimum = 1;
                ViewerWindowWidthSlider.Maximum = Math.Ceiling(Math.Max(4096.0, range * 4.0));
            }
            finally
            {
                isUpdatingViewerControls = false;
            }
        }

        private void ApplyViewerControlsToParams()
        {
            if (ViewerWindowCenterSlider is null || ViewerWindowWidthSlider is null)
            {
                return;
            }

            var next = new ViewportRenderParams(
                (float)ViewerWindowCenterSlider.Value,
                (float)Math.Max(1.0, ViewerWindowWidthSlider.Value),
                ViewerInvertCheckBox?.IsChecked == true,
                ViewerLutComboBox?.SelectedIndex == 1 ? ViewportLutType.Sigmoid : ViewportLutType.Linear).WithSafeWidth();

            if (IsViewerLinked())
            {
                originalViewportParams = next;
                processedViewportParams = next;
            }
            else
            {
                SetActiveViewportParams(next);
            }

            viewport.LinkedWindowLevel = IsViewerLinked();
            viewport.OriginalParams = originalViewportParams;
            viewport.ProcessedParams = processedViewportParams;

            if (ViewerWindowCenterText is not null)
            {
                ViewerWindowCenterText.Text = $"{next.WindowCenter:0.###}";
            }

            if (ViewerWindowWidthText is not null)
            {
                ViewerWindowWidthText.Text = $"{next.WindowWidth:0.###}";
            }
        }

        private void SyncViewerControlsFromActive()
        {
            if (ViewerWindowCenterSlider is null ||
                ViewerWindowWidthSlider is null ||
                ViewerWindowCenterText is null ||
                ViewerWindowWidthText is null)
            {
                return;
            }

            var parameters = GetActiveViewportParams().WithSafeWidth();
            var center = ClampSliderValue(ViewerWindowCenterSlider, parameters.WindowCenter);
            var width = ClampSliderValue(ViewerWindowWidthSlider, parameters.WindowWidth);

            isUpdatingViewerControls = true;
            try
            {
                ViewerWindowCenterSlider.Value = center;
                ViewerWindowWidthSlider.Value = width;
                ViewerWindowCenterText.Text = $"{center:0.###}";
                ViewerWindowWidthText.Text = $"{width:0.###}";
                if (ViewerInvertCheckBox is not null)
                {
                    ViewerInvertCheckBox.IsChecked = parameters.Invert;
                }

                if (ViewerLutComboBox is not null)
                {
                    ViewerLutComboBox.SelectedIndex = parameters.Lut == ViewportLutType.Sigmoid ? 1 : 0;
                }
            }
            finally
            {
                isUpdatingViewerControls = false;
            }
        }

        private ViewportRenderParams GetActiveViewportParams()
        {
            return IsAfterViewportTarget() ? processedViewportParams : originalViewportParams;
        }

        private ViewportRenderParams GetViewportParams(bool afterTarget)
        {
            return afterTarget ? processedViewportParams : originalViewportParams;
        }

        private void SetActiveViewportParams(ViewportRenderParams parameters)
        {
            SetViewportParams(IsAfterViewportTarget(), parameters);
        }

        private void SetViewportParams(bool afterTarget, ViewportRenderParams parameters)
        {
            if (afterTarget)
            {
                processedViewportParams = parameters;
                viewport.ProcessedParams = parameters;
            }
            else
            {
                originalViewportParams = parameters;
                viewport.OriginalParams = parameters;
            }
        }

        private void ApplyHistogramRangeToParams(
            HistogramData histogram,
            HistogramRangeChangedEventArgs range,
            bool afterTarget)
        {
            if (isUpdatingHistogramRanges ||
                histogram.TotalCount == 0 ||
                histogram.SourceMax <= histogram.SourceMin)
            {
                return;
            }

            var sourceRange = histogram.SourceMax - histogram.SourceMin;
            var lower = histogram.SourceMin + (sourceRange * (float)Math.Clamp(range.RangeStart, 0.0, 1.0));
            var upper = histogram.SourceMin + (sourceRange * (float)Math.Clamp(range.RangeEnd, 0.0, 1.0));
            var width = Math.Max(1f, upper - lower);
            var current = GetViewportParams(afterTarget);
            var next = current with
            {
                WindowCenter = lower + (width / 2f),
                WindowWidth = width
            };

            if (IsViewerLinked())
            {
                originalViewportParams = next;
                processedViewportParams = next;
                viewport.OriginalParams = next;
                viewport.ProcessedParams = next;
            }
            else
            {
                SetViewportParams(afterTarget, next);
            }

            ConfigureViewerSliderBounds();
            SyncViewerControlsFromActive();
            RenderComparisonViewports();
        }

        private void SyncHistogramRanges()
        {
            if (OriginalHistogramControl is null || AfterHistogramControl is null)
            {
                return;
            }

            isUpdatingHistogramRanges = true;
            try
            {
                SetHistogramRange(OriginalHistogramControl, viewport.OriginalHistogram, originalViewportParams);
                SetHistogramRange(AfterHistogramControl, viewport.ProcessedHistogram, processedViewportParams);
            }
            finally
            {
                isUpdatingHistogramRanges = false;
            }
        }

        private static void SetHistogramRange(
            HistogramControl control,
            HistogramData histogram,
            ViewportRenderParams parameters)
        {
            if (histogram.TotalCount == 0 || histogram.SourceMax <= histogram.SourceMin)
            {
                control.RangeStart = 0.0;
                control.RangeEnd = 1.0;
                return;
            }

            var lower = parameters.WindowCenter - (parameters.WindowWidth / 2f);
            var upper = parameters.WindowCenter + (parameters.WindowWidth / 2f);
            var sourceRange = histogram.SourceMax - histogram.SourceMin;
            var start = Math.Clamp((lower - histogram.SourceMin) / sourceRange, 0f, 1f);
            var end = Math.Clamp((upper - histogram.SourceMin) / sourceRange, 0f, 1f);

            control.RangeStart = Math.Min(start, end);
            control.RangeEnd = Math.Max(start, end);
        }

        private bool IsViewerLinked()
        {
            return ViewerLinkCheckBox?.IsChecked != false;
        }

        private bool IsAfterViewportTarget()
        {
            return ViewerTargetComboBox?.SelectedIndex == 1;
        }

        private void RenderComparisonViewports()
        {
            if (currentPreview is null)
            {
                ClearViewerRenderState();
                return;
            }

            var before = ViewportRenderService.Render(
                currentPreview.SampledPixels,
                currentPreview.PreviewWidth,
                currentPreview.PreviewHeight,
                originalViewportParams);
            originalViewportParams = before.Params;
            viewport.OriginalParams = before.Params;
            viewport.OriginalHistogram = before.Histogram;
            BeforePreviewImage.Source = before.Bitmap;
            OriginalHistogramControl.Data = before.Histogram;
            OriginalHistogramText.Text = $"Original histogram: {before.Histogram.Summary}";

            if (IsViewerLinked())
            {
                processedViewportParams = originalViewportParams;
            }

            ViewportRenderResult after;
            if (TryGetProcessedOutputPixels(out var outputPixels))
            {
                after = ViewportRenderService.Render(
                    outputPixels,
                    currentPreview.PreviewWidth,
                    currentPreview.PreviewHeight,
                    processedViewportParams);
            }
            else
            {
                after = ViewportRenderService.Render(
                    currentPreview.SampledPixels,
                    currentPreview.PreviewWidth,
                    currentPreview.PreviewHeight,
                    processedViewportParams);
            }

            processedViewportParams = after.Params;
            viewport.ProcessedParams = after.Params;
            viewport.ProcessedHistogram = after.Histogram;
            AfterPreviewImage.Source = after.Bitmap;
            AfterHistogramControl.Data = after.Histogram;
            AfterHistogramText.Text = $"After histogram: {after.Histogram.Summary}";
            SyncHistogramRanges();
            UpdateComparisonClip();
        }

        private bool TryGetProcessedOutputPixels(out float[] pixels)
        {
            var expectedPixelCount = currentPreview is null
                ? 0
                : checked(currentPreview.PreviewWidth * currentPreview.PreviewHeight);

            if (algorithmPreviewPixels is { Count: > 0 } algorithmValues)
            {
                return TryCopyProcessedOutputPixels(
                    algorithmValues,
                    expectedPixelCount,
                    "Algorithm after output",
                    out pixels);
            }

            if (nativePreview?.OutputPixels is { Count: > 0 } values)
            {
                return TryCopyProcessedOutputPixels(
                    values,
                    expectedPixelCount,
                    "Native after output",
                    out pixels);
            }

            pixels = [];
            return false;
        }

        private bool TryCopyProcessedOutputPixels(
            IReadOnlyList<float> values,
            int expectedPixelCount,
            string label,
            out float[] pixels)
        {
            if (expectedPixelCount > 0 && values.Count != expectedPixelCount)
            {
                pixels = [];
                ProcessingScaffoldText.Text =
                    $"{label} ignored: pixel count {values.Count} does not match the active preview count {expectedPixelCount}.";
                return false;
            }

            pixels = values as float[] ?? values.ToArray();
            return true;
        }

        private ViewportRenderParams CreateAutoFitForActiveTarget(ViewportRenderParams current)
        {
            if (IsAfterViewportTarget() && TryGetProcessedOutputPixels(out var outputPixels))
            {
                return PreserveRenderFlags(ViewportRenderService.AutoFit(outputPixels), current);
            }

            return currentPreview is null
                ? current
                : PreserveRenderFlags(ViewportRenderService.AutoFit(currentPreview.SampledPixels), current);
        }

        private static ViewportRenderParams PreserveRenderFlags(
            ViewportRenderParams next,
            ViewportRenderParams current)
        {
            return next with
            {
                Invert = current.Invert,
                Lut = current.Lut
            };
        }

        private static float ClampSliderValue(Slider slider, float value)
        {
            return (float)Math.Clamp(value, slider.Minimum, slider.Maximum);
        }

        private static string GetSelectedComboText(ComboBox comboBox)
        {
            return (comboBox.SelectedItem as ComboBoxItem)?.Content?.ToString() ?? string.Empty;
        }

        private void ClearViewerRenderState()
        {
            if (ComparisonCanvas is not null)
            {
                ComparisonCanvas.Width = double.NaN;
                ComparisonCanvas.Height = double.NaN;
            }

            if (AfterPreviewClip is not null)
            {
                AfterPreviewClip.Rect = Rect.Empty;
            }

            if (BeforePreviewImage is not null)
            {
                BeforePreviewImage.Source = null;
            }

            if (AfterPreviewImage is not null)
            {
                AfterPreviewImage.Source = null;
            }

            if (OriginalHistogramControl is not null)
            {
                OriginalHistogramControl.Data = HistogramData.Empty;
                OriginalHistogramControl.RangeStart = 0.0;
                OriginalHistogramControl.RangeEnd = 1.0;
            }

            if (AfterHistogramControl is not null)
            {
                AfterHistogramControl.Data = HistogramData.Empty;
                AfterHistogramControl.RangeStart = 0.0;
                AfterHistogramControl.RangeEnd = 1.0;
            }

            viewport.OriginalHistogram = HistogramData.Empty;
            viewport.ProcessedHistogram = HistogramData.Empty;

            if (OriginalHistogramText is not null)
            {
                OriginalHistogramText.Text = "Original histogram: empty";
            }

            if (AfterHistogramText is not null)
            {
                AfterHistogramText.Text = "After histogram: empty";
            }
        }

        private void UpdateComparisonClip()
        {
            if (AfterPreviewClip is null || SwipeLine is null || SwipeValueText is null || ComparisonCanvas is null)
            {
                return;
            }

            var width = GetComparisonCanvasWidth();
            var height = GetComparisonCanvasHeight();

            if (width <= 0 || height <= 0)
            {
                return;
            }

            var fraction = Math.Clamp(CompareSwipeSlider.Value, 0, 1);
            var x = width * fraction;
            viewport.SwipeFraction = fraction;
            AfterPreviewClip.Rect = new Rect(0, 0, x, height);
            SwipeLine.Height = height;
            SwipeLine.Margin = new Thickness(Math.Max(0, x - 1), 0, 0, 0);
            SwipeValueText.Text = $"{fraction * 100:0}%";
        }

        private void UpdateComparisonSwipeFromPoint(double x)
        {
            var width = GetComparisonCanvasWidth();
            if (width <= 0)
            {
                return;
            }

            CompareSwipeSlider.Value = Math.Clamp(x / width, 0, 1);
        }

        private void EndComparisonSwipeDrag()
        {
            isDraggingComparisonSwipe = false;
            ComparisonCanvas.ReleaseMouseCapture();
        }

        private void EndViewportPan()
        {
            isPanningViewport = false;
            ComparisonCanvas.Cursor = Cursors.SizeWE;
            ComparisonCanvas.ReleaseMouseCapture();
        }

        private double GetComparisonCanvasWidth()
        {
            return !double.IsNaN(ComparisonCanvas.Width) && ComparisonCanvas.Width > 0
                ? ComparisonCanvas.Width
                : ComparisonCanvas.ActualWidth;
        }

        private double GetComparisonCanvasHeight()
        {
            return !double.IsNaN(ComparisonCanvas.Height) && ComparisonCanvas.Height > 0
                ? ComparisonCanvas.Height
                : ComparisonCanvas.ActualHeight;
        }
    }
}
