using System.Globalization;
using System.Windows;
using System.Windows.Data;

namespace ImageProcTest.Converters;

/// <summary>
/// Multi-value converter: (FocusMode, PanelOpen) => Visibility.
/// When FocusMode is false, panel is always Visible.
/// When FocusMode is true, panel visibility follows PanelOpen.
/// </summary>
public sealed class FocusPanelVisibilityConverter : IMultiValueConverter
{
    public object Convert(object[] values, Type targetType, object parameter, CultureInfo culture)
    {
        if (values.Length >= 2 && values[0] is bool focusMode && values[1] is bool panelOpen)
        {
            return focusMode ? (panelOpen ? Visibility.Visible : Visibility.Collapsed) : Visibility.Visible;
        }
        return Visibility.Visible;
    }

    public object[] ConvertBack(object value, Type[] targetTypes, object parameter, CultureInfo culture)
        => Array.Empty<object>();
}
