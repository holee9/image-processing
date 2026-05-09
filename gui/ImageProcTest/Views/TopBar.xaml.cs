using System.Globalization;
using System.Windows;
using System.Windows.Data;
using ImageProcTest.Models;

namespace ImageProcTest.Views;

public partial class TopBar : System.Windows.Controls.UserControl
{
    public TopBar()
    {
        InitializeComponent();
    }
}

public sealed class RunSetBarWidthConverter : IMultiValueConverter
{
    public object Convert(object[] values, Type targetType, object? parameter, CultureInfo culture)
    {
        if (values.Length >= 2 && values[0] is int count && values[1] is int total)
            return total == 0 ? new GridLength(0) : new GridLength(count, GridUnitType.Star);
        return new GridLength(0);
    }

    public object[] ConvertBack(object value, Type[] targetTypes, object? parameter, CultureInfo culture)
        => [Binding.DoNothing];
}

public sealed class RunSetRemainderConverter : IMultiValueConverter
{
    public object Convert(object[] values, Type targetType, object? parameter, CultureInfo culture)
    {
        if (values.Length >= 4
            && values[0] is int passed
            && values[1] is int failed
            && values[2] is int deferred
            && values[3] is int total)
        {
            if (total == 0)
                return new GridLength(1, GridUnitType.Star);
            var used = passed + failed + deferred;
            var remainder = total - used;
            return remainder > 0 ? new GridLength(remainder, GridUnitType.Star) : new GridLength(0);
        }
        return new GridLength(1, GridUnitType.Star);
    }

    public object[] ConvertBack(object value, Type[] targetTypes, object? parameter, CultureInfo culture)
        => [Binding.DoNothing];
}
