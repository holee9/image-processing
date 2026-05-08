using System.Globalization;
using System.Windows;
using System.Windows.Data;
using ImageProcTest.ViewModels;

namespace ImageProcTest.Views;

public partial class AnalysisPanel : System.Windows.Controls.UserControl
{
    public AnalysisPanel()
    {
        InitializeComponent();
    }
}

/// <summary>
/// Converts a string comparison to Visibility for tab content panels.
/// Visible when the bound value equals the ConverterParameter.
/// </summary>
public sealed class StringEqualsConverter : IValueConverter
{
    public object Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        var equals = string.Equals(value as string, parameter as string, StringComparison.OrdinalIgnoreCase);
        return equals ? Visibility.Visible : Visibility.Collapsed;
    }

    public object ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
        => Binding.DoNothing;
}
