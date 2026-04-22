using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace ImageProcTest.ViewModels
{
    internal sealed class ViewportViewModel : INotifyPropertyChanged
    {
        private ViewportRenderParams originalParams = ViewportRenderParams.Default;
        private ViewportRenderParams processedParams = ViewportRenderParams.Default;
        private HistogramData originalHistogram = HistogramData.Empty;
        private HistogramData processedHistogram = HistogramData.Empty;
        private double zoom = 1.0;
        private double swipeFraction = 0.5;
        private bool linkedWindowLevel = true;
        private string activeTarget = "Original";

        public event PropertyChangedEventHandler? PropertyChanged;

        public ViewportRenderParams OriginalParams
        {
            get => originalParams;
            set => SetField(ref originalParams, value);
        }

        public ViewportRenderParams ProcessedParams
        {
            get => processedParams;
            set => SetField(ref processedParams, value);
        }

        public HistogramData OriginalHistogram
        {
            get => originalHistogram;
            set => SetField(ref originalHistogram, value);
        }

        public HistogramData ProcessedHistogram
        {
            get => processedHistogram;
            set => SetField(ref processedHistogram, value);
        }

        public double Zoom
        {
            get => zoom;
            set => SetField(ref zoom, value);
        }

        public double SwipeFraction
        {
            get => swipeFraction;
            set => SetField(ref swipeFraction, value);
        }

        public bool LinkedWindowLevel
        {
            get => linkedWindowLevel;
            set => SetField(ref linkedWindowLevel, value);
        }

        public string ActiveTarget
        {
            get => activeTarget;
            set => SetField(ref activeTarget, value);
        }

        private bool SetField<T>(ref T field, T value, [CallerMemberName] string? propertyName = null)
        {
            if (Equals(field, value))
            {
                return false;
            }

            field = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
            return true;
        }
    }
}
