using System.Globalization;
using System.Windows.Data;

namespace ImageProcTest.Converters;

/// <summary>Presentation aliases only; recorded stage names and elapsed values are not changed.</summary>
public sealed class MetricCellDisplayConverter : IValueConverter
{
    public object Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        var text = value as string ?? string.Empty;
        var separator = text.IndexOf(" · ", StringComparison.Ordinal);
        var state = separator < 0 ? text : text[..separator];
        var display = state switch
        {
            "RequestedNotApplied" => "Requested / not applied",
            "AppliedNoChange" => "Applied / no change",
            "NotRequested" => "Not requested",
            _ => state,
        };
        return display + (separator < 0 ? string.Empty : text[separator..]);
    }

    public object ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture) => Binding.DoNothing;
}
