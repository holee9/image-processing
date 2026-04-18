using System.Globalization;
using System.Windows;
using System.Windows.Data;

namespace ImageProcTest.ViewModels;

/// <summary>
/// Maps a string setting to a radio-button checked state.
/// </summary>
public sealed class StringEqualsConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        return string.Equals(value as string, parameter as string, StringComparison.OrdinalIgnoreCase);
    }

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
    {
        return value is true ? parameter?.ToString() ?? string.Empty : DependencyProperty.UnsetValue;
    }
}
