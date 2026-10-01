// GUI-C-171: committing a text box without a key press.
using FlaUI.Core.AutomationElements;
using FlaUI.Core.Definitions;
using Xunit;

namespace ImageProcTest.E2ETests.Fixtures;

/// <summary>
/// UI Automation replacements for keystrokes that only ever served to move the focus.
///
/// <para>Typing into a text box here is <c>input.Text = value</c> followed by a Tab key press, and the Tab is
/// there for one reason: the binding takes the new value when the box LOSES focus. A Tab key press is addressed
/// to the foreground window, not to the app (see <see cref="GlobalInput"/>); moving the focus to another control
/// through UI Automation reaches the same state and is addressed to the app.</para>
/// </summary>
internal static class UiaInput
{
    /// <summary>
    /// Takes the focus away from <paramref name="input"/> by giving it to another text box in the window — what
    /// Tab did — so the edit is committed. Fails loudly when there is no other text box, rather than leaving the
    /// edit uncommitted and the next assertion to explain why.
    /// </summary>
    public static void CommitByMovingFocus(Window window, TextBox input)
    {
        var inputId = input.AutomationId;
        var other = window.FindAllDescendants(cf => cf.ByControlType(ControlType.Edit))
            .FirstOrDefault(e => e.AutomationId != inputId
                                 && e.Properties.IsKeyboardFocusable.ValueOrDefault
                                 && !e.Properties.IsOffscreen.ValueOrDefault);
        Assert.True(
            other is not null,
            $"There is no other text box to move the focus to, so the edit of '{inputId}' cannot be committed without a key press.");
        other!.Focus();
    }
}
