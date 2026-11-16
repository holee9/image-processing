using System.Windows.Automation.Peers;
using System.Windows.Controls;

namespace ImageProcTest.Controls;

/// <summary>A layout border with a UI Automation container, so its actual bounds can be observed.</summary>
public sealed class AutomationBorder : Border
{
    protected override AutomationPeer OnCreateAutomationPeer() => new BorderPeer(this);

    private sealed class BorderPeer(AutomationBorder owner) : FrameworkElementAutomationPeer(owner)
    {
        protected override string GetClassNameCore() => nameof(AutomationBorder);
        protected override AutomationControlType GetAutomationControlTypeCore() => AutomationControlType.Pane;
        protected override bool IsControlElementCore() => true;
    }
}
