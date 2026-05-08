using System.Globalization;
using System.Windows;
using System.Windows.Data;
using ImageProcTest.Models;
using ImageProcTest.ViewModels;

namespace ImageProcTest.Views;

public partial class ViewportShell : System.Windows.Controls.UserControl
{
    public ViewportShell()
    {
        InitializeComponent();
        Loaded += OnLoaded;
    }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
        if (DataContext is MainWindowViewModel vm)
        {
            vm.Settings.PropertyChanged += OnSettingsPropertyChanged;
            UpdateOpacitySliderVisibility(vm.Settings.ComparisonMode);
        }
    }

    private void CompareModeButton_Click(object sender, RoutedEventArgs e)
    {
        if (sender is FrameworkElement { Tag: string mode } && DataContext is MainWindowViewModel vm)
        {
            vm.Settings.ComparisonMode = mode;
            UpdateOpacitySliderVisibility(mode);
        }
    }

    private void OnSettingsPropertyChanged(object? sender, System.ComponentModel.PropertyChangedEventArgs e)
    {
        if (e.PropertyName == nameof(AppSettings.ComparisonMode) && DataContext is MainWindowViewModel vm)
        {
            UpdateOpacitySliderVisibility(vm.Settings.ComparisonMode);
        }
    }

    private void UpdateOpacitySliderVisibility(string mode)
    {
        OpacitySliderPanel.Visibility = mode == "OverlayOpacity"
            ? Visibility.Visible
            : Visibility.Collapsed;
    }
}

public sealed class ZoomPercentConverter : IValueConverter
{
    public object Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        if (value is double zoom)
        {
            return zoom <= 0.0 ? "fit" : $"{zoom * 100.0:0}%";
        }
        return "fit";
    }

    public object ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        return System.Windows.Data.Binding.DoNothing;
    }
}
