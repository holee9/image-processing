using System.Windows.Controls;

namespace ImageProcTest.Views;

/// <summary>
/// #225 row 8 (GUI-C-170). Read-only view: requested VOI settings next to what the last render used. It
/// holds no state — see the XAML comment.
/// </summary>
public partial class DisplaySettingsPanel : UserControl
{
    public DisplaySettingsPanel()
    {
        InitializeComponent();
    }
}
