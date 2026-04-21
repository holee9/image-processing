using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Shapes;

namespace ImageProcTest.Controls
{
    public sealed class HistogramRangeChangedEventArgs(double rangeStart, double rangeEnd) : EventArgs
    {
        public double RangeStart { get; } = rangeStart;

        public double RangeEnd { get; } = rangeEnd;
    }

    public partial class HistogramControl : UserControl
    {
        private enum DragHandle
        {
            None,
            Start,
            End
        }

        private const double MinimumRangeSpan = 0.001;
        private DragHandle dragHandle = DragHandle.None;
        private bool isSettingRange;

        public static readonly DependencyProperty DataProperty =
            DependencyProperty.Register(
                nameof(Data),
                typeof(HistogramData),
                typeof(HistogramControl),
                new PropertyMetadata(HistogramData.Empty, OnDataChanged, CoerceHistogramData));

        public static readonly DependencyProperty RangeStartProperty =
            DependencyProperty.Register(
                nameof(RangeStart),
                typeof(double),
                typeof(HistogramControl),
                new PropertyMetadata(0.0, OnRangeDisplayChanged, CoerceRangeValue));

        public static readonly DependencyProperty RangeEndProperty =
            DependencyProperty.Register(
                nameof(RangeEnd),
                typeof(double),
                typeof(HistogramControl),
                new PropertyMetadata(1.0, OnRangeDisplayChanged, CoerceRangeValue));

        public HistogramData Data
        {
            get => (HistogramData)GetValue(DataProperty);
            set => SetValue(DataProperty, value);
        }

        public double RangeStart
        {
            get => (double)GetValue(RangeStartProperty);
            set => SetValue(RangeStartProperty, Clamp01(value));
        }

        public double RangeEnd
        {
            get => (double)GetValue(RangeEndProperty);
            set => SetValue(RangeEndProperty, Clamp01(value));
        }

        public event EventHandler<HistogramRangeChangedEventArgs>? RangeChanged;

        public HistogramControl()
        {
            InitializeComponent();
            SizeChanged += (_, _) => Render();
        }

        private static void OnDataChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            if (d is HistogramControl control)
            {
                control.Render();
            }
        }

        private static void OnRangeDisplayChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            if (d is HistogramControl { isSettingRange: false } control)
            {
                control.Render();
            }
        }

        private static object CoerceHistogramData(DependencyObject d, object baseValue)
        {
            return baseValue is HistogramData data ? data : HistogramData.Empty;
        }

        private static object CoerceRangeValue(DependencyObject d, object baseValue)
        {
            return baseValue is double value ? Clamp01(value) : 0.0;
        }

        private static double Clamp01(double value)
        {
            return Math.Clamp(double.IsFinite(value) ? value : 0.0, 0.0, 1.0);
        }

        private void Render()
        {
            if (HistogramCanvas is null)
            {
                return;
            }

            HistogramCanvas.Children.Clear();
            var data = Data ?? HistogramData.Empty;
            if (data.Bins.Count == 0 || data.MaxCount <= 0)
            {
                return;
            }

            var width = Math.Max(1.0, HistogramCanvas.ActualWidth);
            var height = Math.Max(1.0, HistogramCanvas.ActualHeight);
            var barWidth = Math.Max(1.0, width / data.Bins.Count);
            var fill = new SolidColorBrush(Color.FromRgb(88, 166, 255));
            fill.Freeze();

            for (var i = 0; i < data.Bins.Count; i++)
            {
                var barHeight = Math.Max(1.0, (data.Bins[i] / (double)data.MaxCount) * height);
                var rect = new Rectangle
                {
                    Width = Math.Ceiling(barWidth),
                    Height = barHeight,
                    Fill = fill,
                    Opacity = 0.82
                };
                Canvas.SetLeft(rect, i * barWidth);
                Canvas.SetTop(rect, height - barHeight);
                HistogramCanvas.Children.Add(rect);
            }

            RenderRangeOverlay(width, height);
        }

        private void RenderRangeOverlay(double width, double height)
        {
            var start = Clamp01(Math.Min(RangeStart, RangeEnd));
            var end = Clamp01(Math.Max(RangeStart, RangeEnd));
            var x1 = start * width;
            var x2 = end * width;
            var selectedWidth = Math.Max(1.0, x2 - x1);

            var overlay = new Rectangle
            {
                Width = selectedWidth,
                Height = height,
                Fill = new SolidColorBrush(Color.FromArgb(42, 255, 209, 102))
            };
            Canvas.SetLeft(overlay, x1);
            Canvas.SetTop(overlay, 0);
            HistogramCanvas.Children.Add(overlay);

            AddHandle(x1, height);
            AddHandle(x2, height);
        }

        private void AddHandle(double x, double height)
        {
            var handle = new Rectangle
            {
                Width = 3,
                Height = height,
                Fill = new SolidColorBrush(Color.FromRgb(255, 209, 102)),
                Opacity = 0.95
            };
            Canvas.SetLeft(handle, Math.Clamp(x - 1.5, 0, Math.Max(0, HistogramCanvas.ActualWidth - 3)));
            Canvas.SetTop(handle, 0);
            HistogramCanvas.Children.Add(handle);
        }

        private void HistogramCanvas_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            if ((Data ?? HistogramData.Empty).TotalCount == 0)
            {
                return;
            }

            dragHandle = GetNearestHandle(e.GetPosition(HistogramCanvas).X);
            HistogramCanvas.CaptureMouse();
            UpdateDraggedRange(e.GetPosition(HistogramCanvas).X);
            e.Handled = true;
        }

        private void HistogramCanvas_MouseMove(object sender, MouseEventArgs e)
        {
            if (dragHandle == DragHandle.None || e.LeftButton != MouseButtonState.Pressed)
            {
                return;
            }

            UpdateDraggedRange(e.GetPosition(HistogramCanvas).X);
            e.Handled = true;
        }

        private void HistogramCanvas_MouseLeftButtonUp(object sender, MouseButtonEventArgs e)
        {
            EndDrag();
            e.Handled = true;
        }

        private void HistogramCanvas_MouseLeave(object sender, MouseEventArgs e)
        {
            if (dragHandle != DragHandle.None && e.LeftButton != MouseButtonState.Pressed)
            {
                EndDrag();
            }
        }

        private DragHandle GetNearestHandle(double x)
        {
            var width = Math.Max(1.0, HistogramCanvas.ActualWidth);
            var startX = Clamp01(RangeStart) * width;
            var endX = Clamp01(RangeEnd) * width;
            return Math.Abs(x - startX) <= Math.Abs(x - endX)
                ? DragHandle.Start
                : DragHandle.End;
        }

        private void UpdateDraggedRange(double x)
        {
            var width = Math.Max(1.0, HistogramCanvas.ActualWidth);
            var fraction = Clamp01(x / width);
            var start = Clamp01(RangeStart);
            var end = Clamp01(RangeEnd);

            if (dragHandle == DragHandle.Start)
            {
                start = Math.Min(fraction, end - MinimumRangeSpan);
            }
            else if (dragHandle == DragHandle.End)
            {
                end = Math.Max(fraction, start + MinimumRangeSpan);
            }
            else
            {
                return;
            }

            SetRange(start, end, userInitiated: true);
        }

        private void SetRange(double start, double end, bool userInitiated)
        {
            start = Clamp01(start);
            end = Clamp01(end);
            if (end - start < MinimumRangeSpan)
            {
                if (dragHandle == DragHandle.Start)
                {
                    start = Math.Max(0.0, end - MinimumRangeSpan);
                }
                else
                {
                    end = Math.Min(1.0, start + MinimumRangeSpan);
                }
            }

            isSettingRange = true;
            try
            {
                SetCurrentValue(RangeStartProperty, start);
                SetCurrentValue(RangeEndProperty, end);
            }
            finally
            {
                isSettingRange = false;
            }

            Render();
            if (userInitiated)
            {
                RangeChanged?.Invoke(this, new HistogramRangeChangedEventArgs(start, end));
            }
        }

        private void EndDrag()
        {
            dragHandle = DragHandle.None;
            if (HistogramCanvas.IsMouseCaptured)
            {
                HistogramCanvas.ReleaseMouseCapture();
            }
        }
    }
}
