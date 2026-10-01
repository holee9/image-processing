using System.Windows.Controls;

namespace ImageProcTest.Views;

/// <summary>
/// #225 row 7 (GUI-C-170). View over the three calibration directory settings and their Browse
/// commands; it holds no state — see the XAML comment.
/// </summary>
public partial class CalibrationPathsPanel : UserControl
{
    public CalibrationPathsPanel()
    {
        InitializeComponent();
    }
}
