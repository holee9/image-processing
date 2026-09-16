using System.Globalization;
using System.Windows;
using System.Windows.Data;

namespace ImageProcTest.Converters;

/// <summary>
/// Visible when the named tab is selected AND a boolean toggle is on.
///
/// <para>Added for #165 (GUI-C-65) to wire <c>View → Logs Panel</c>. The log region's visibility was
/// already owned by <c>AnalysisTab</c>, so the menu toggle needed a second condition rather than a
/// second writer: <b>the toggle does not change the tab</b>. Letting it do so would give the region
/// two controllers that overwrite each other — the shape GUI-C-47 removed from the comparison mode
/// and GUI-C-60 removed from its readouts.</para>
///
/// <para>Values, in order: the current tab (string), then the toggle (bool). The tab to match comes
/// from the converter parameter.</para>
/// </summary>
public sealed class TabAndToggleVisibilityConverter : IMultiValueConverter
{
    public object Convert(object[] values, Type targetType, object? parameter, CultureInfo culture)
    {
        if (values.Length < 2) return Visibility.Collapsed;

        var tabMatches = string.Equals(values[0] as string, parameter as string, StringComparison.Ordinal);
        var toggleOn = values[1] is true;

        return tabMatches && toggleOn ? Visibility.Visible : Visibility.Collapsed;
    }

    /// <summary>
    /// One-way. A visibility cannot say which of the two inputs produced it, and guessing would make
    /// the converter a writer.
    /// </summary>
    public object[] ConvertBack(object value, Type[] targetTypes, object? parameter, CultureInfo culture) =>
        throw new NotSupportedException("TabAndToggleVisibilityConverter is one-way.");
}
