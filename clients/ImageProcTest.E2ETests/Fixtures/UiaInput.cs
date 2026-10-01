// GUI-C-171: committing a text box without a key press.
using FlaUI.Core.AutomationElements;
using FlaUI.Core.Definitions;
using Xunit;

namespace ImageProcTest.E2ETests.Fixtures;

/// <summary>
/// UI Automation replacement for the Tab key press that followed every text edit.
///
/// <para>Typing into a text box here is <c>input.Text = value</c> followed by a Tab key press. A Tab key press is
/// addressed to the foreground window, not to the app (see <see cref="GlobalInput"/>); moving the focus to another
/// control through UI Automation is addressed to the app.</para>
///
/// <para><b>What this does NOT do (corrected in GUI-C-172).</b> It is not what makes the edit take effect. Every
/// text box in this app binds its text with <c>UpdateSourceTrigger=PropertyChanged</c>, so the value reaches the
/// view model as it is set, with or without a focus change. An earlier version of this note said the binding
/// "takes the new value when the box loses focus"; that was wrong for these boxes. The focus move is kept because
/// the Tab it replaces moved focus and nothing measured that removing it is harmless; a real Tab is exercised by
/// <c>RealInputScenarios.R02</c>.</para>
/// </summary>
internal static class UiaInput
{
    /// <summary>
    /// Takes the focus away from <paramref name="input"/> by giving it to another text box in the window — what
    /// Tab did. Fails loudly when there is no other text box to give it to.
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
            $"There is no other text box to move the focus to after the edit of '{inputId}'.");
        other!.Focus();
    }
}
