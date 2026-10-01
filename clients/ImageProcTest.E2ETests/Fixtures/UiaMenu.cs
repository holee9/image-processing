// GUI-C-171: opening and closing a menu without sending the desktop a single mouse or keyboard event.
using FlaUI.Core.AutomationElements;

namespace ImageProcTest.E2ETests.Fixtures;

/// <summary>
/// Drives a top-level menu through the UI Automation ExpandCollapse pattern.
///
/// <para><b>Why this exists.</b> A mouse click and a key press are not addressed to the application under
/// test: they go to whatever window is in front at that moment. GUI-C-171 measured the consequence — on a
/// desktop where another window (a terminal) covered the app, the click that was meant for the View menu
/// landed on the terminal and the menu never opened, and the ESC key that was meant to close it went to the
/// terminal too. The failure read as "the menu did not open", nothing was wrong with the app, and an unrelated
/// window received input it should never have seen.</para>
///
/// <para>A UI Automation pattern call is addressed to the element itself, through the application's own
/// automation peer. It does not depend on which window is in front, where the app sits on screen, or who
/// owns the foreground, and it sends nothing to any other window.</para>
///
/// <para>There is deliberately no retry here: if the menu does not open, the caller's own check says so.</para>
/// </summary>
internal static class UiaMenu
{
    private static MenuItem? s_open;

    /// <summary>Expands the top-level menu with this automation id (first closing the one this helper opened before).</summary>
    public static MenuItem Open(Window window, string menuAutomationId)
    {
        Close();

        var menu = window.FindFirstDescendant(cf => cf.ByAutomationId(menuAutomationId))?.AsMenuItem()
                   ?? throw new InvalidOperationException(
                       $"The menu '{menuAutomationId}' was not found, so it cannot be opened.");
        menu.Expand();
        s_open = menu;
        return menu;
    }

    /// <summary>
    /// Collapses every top-level menu of the window — what an ESC key press used to be for. Needs no focus and
    /// sends nothing; a menu that is already closed is left alone.
    /// </summary>
    public static void CollapseAll(Window window)
    {
        s_open = null;
        foreach (var menu in window.FindAllDescendants(cf => cf.ByControlType(FlaUI.Core.Definitions.ControlType.MenuItem)))
        {
            try
            {
                var pattern = menu.Patterns.ExpandCollapse.PatternOrDefault;
                if (pattern is not null && pattern.ExpandCollapseState.ValueOrDefault == FlaUI.Core.Definitions.ExpandCollapseState.Expanded)
                {
                    pattern.Collapse();
                }
            }
            catch (Exception)
            {
                // An item that cannot say, or has gone away, is not an open menu.
            }
        }
    }

    /// <summary>
    /// Collapses every menu item AND every combo box that is currently expanded, and returns what it found open
    /// (control type and automation id). Menus alone are not enough: a drop-down left open by an earlier
    /// scenario takes the next click outside it as "close me" and the click never reaches what it was aimed at
    /// (GUI-C-173). The list is the evidence — an empty list says nothing was open.
    /// </summary>
    public static IReadOnlyList<string> CollapseEverythingOpen(Window window)
    {
        s_open = null;
        var found = new List<string>();
        foreach (var type in new[] { FlaUI.Core.Definitions.ControlType.MenuItem, FlaUI.Core.Definitions.ControlType.ComboBox })
        {
            foreach (var element in window.FindAllDescendants(cf => cf.ByControlType(type)))
            {
                try
                {
                    var pattern = element.Patterns.ExpandCollapse.PatternOrDefault;
                    if (pattern is not null && pattern.ExpandCollapseState.ValueOrDefault == FlaUI.Core.Definitions.ExpandCollapseState.Expanded)
                    {
                        found.Add($"{type}:{element.AutomationId}");
                        pattern.Collapse();
                    }
                }
                catch (Exception)
                {
                    // An element that cannot say, or has gone away, is not an open drop-down.
                }
            }
        }

        return found;
    }

    /// <summary>Collapses the menu this helper opened last. Safe to call when nothing is open.</summary>
    public static void Close()
    {
        var open = s_open;
        s_open = null;
        if (open is null)
        {
            return;
        }

        try
        {
            open.Collapse();
        }
        catch (Exception)
        {
            // Already closed (an Invoke on one of its items closes it), or the window is gone: either way
            // there is nothing left to collapse.
        }
    }
}
