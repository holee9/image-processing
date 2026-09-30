using System.Windows.Controls;

namespace ImageProcTest.Views;

/// <summary>
/// #225 row 13 (GUI-C-168). Read-only view over what the last render recorded; it holds no state and
/// performs no measurement — see the XAML comment for which existing view-model members answer which
/// of the three states the card requires kept apart.
/// </summary>
public partial class PipelineDiagnosticsPanel : UserControl
{
    public PipelineDiagnosticsPanel()
    {
        InitializeComponent();
    }
}
