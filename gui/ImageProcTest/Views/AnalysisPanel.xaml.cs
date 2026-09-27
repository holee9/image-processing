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

    /// <summary>
    /// Brings the selected log line into view (#214, GUI-C-145).
    ///
    /// <para><b>The defect.</b> Turning the "Alerts only" filter on hides the selected line, and WPF moves
    /// the selection to the first remaining row without scrolling there. While the filtered list fits one
    /// screen that is invisible — the first row IS the top of the view (GUI-C-142/143). With 27 alerts the
    /// filtered list reports <c>viewSize</c> 71.4%, and then, with the list scrolled to the BOTTOM before
    /// the filter, the moved-to row lands above the viewport: measured <c>offscreen=True</c>, clipped to
    /// nothing, on a row that had been visible a moment earlier. <c>Copy</c> reads that selection, so the
    /// user would copy a line they cannot see — the shape GUI-C-123 refused to create.</para>
    ///
    /// <para><b>Attributed by disabling it</b> (GUI-C-145 §3): with this handler returning early the case is
    /// red (<c>offscreen=True</c>); with the one line below it is green (<c>verticallyInside=True</c>).</para>
    ///
    /// <para><b>One line, after three were tried.</b> A deferred call (<c>Dispatcher.BeginInvoke</c>), a
    /// <c>BringIntoView</c> on the container, and <c>VirtualizingPanel.ScrollUnit="Item"</c> were all added
    /// while chasing a "12 pixel" residual — which turned out to be the measuring instrument, not the app:
    /// the test was comparing each row against a list rectangle captured earlier in the case, and the list
    /// moves 13 px between those moments. A geometry probe settled it (list <c>976,308</c>, selected row
    /// <c>977,309</c> — contiguous, fully inside). With the instrument reading the rectangle fresh, the
    /// plain call is enough and the other three were removed rather than left in as cargo.</para>
    ///
    /// <para><b>Scope, stated rather than glossed.</b> This fires on every selection change, not only the
    /// filter's. For a row the user just clicked the call is a no-op — it is already in view; what it adds
    /// is that any PROGRAMMATIC selection change scrolls into view too. That is broader than the measured
    /// defect, which is why the whole E2E suite was re-run rather than only the new case.</para>
    /// </summary>
    private void OnLogSelectionChanged(object sender, System.Windows.Controls.SelectionChangedEventArgs e)
    {
        if (LogList.SelectedItem is { } selected)
        {
            LogList.ScrollIntoView(selected);
        }
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
