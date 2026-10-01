// GUI-C-171: the ONLY place in the E2E project that sends real mouse or keyboard input to the desktop.
using FlaUI.Core.AutomationElements;
using FlaUI.Core.Input;
using FlaUI.Core.WindowsAPI;
using Xunit;

namespace ImageProcTest.E2ETests.Fixtures;

/// <summary>
/// Real, desktop-wide mouse and keyboard input — quarantined, and OFF unless asked for.
///
/// <para><b>What goes wrong without this.</b> A key press or a mouse event is not addressed to the application
/// under test; it goes to whichever window is in front. GUI-C-171 measured it on a desktop shared with other
/// sessions: the click aimed at the View menu landed on a terminal window that covered the app, and the ESC
/// that was meant to close the menu was delivered to that terminal as well — interrupting a session that
/// had nothing to do with the test. Input sent this way is also invisible in the test result: the failure
/// reads "the menu did not open".</para>
///
/// <para><b>What is allowed here.</b> Only the scenarios whose subject IS the input: a key gesture bound in
/// the window ("does F8 change the mode"), a mouse wheel or drag on the viewport, a click on a button that
/// must absorb it while disabled. A UI Automation pattern call cannot stand in for those — it would test the
/// command, not the gesture. Everything else (menus, text commits, buttons) goes through UI Automation
/// instead: <see cref="UiaMenu"/>, <see cref="UiaInput"/>.</para>
///
/// <para><b>How it is gated.</b> Each method first calls <see cref="Require"/>, which SKIPS the scenario unless
/// <c>XPE_E2E_ALLOW_GLOBAL_INPUT=1</c> is set in the environment — before anything has been sent. Set it only
/// on a desktop that nothing else is using.</para>
/// </summary>
internal static class GlobalInput
{
    public const string OptInVariable = "XPE_E2E_ALLOW_GLOBAL_INPUT";

    public static bool Allowed => Environment.GetEnvironmentVariable(OptInVariable) == "1";

    /// <summary>Skips the calling scenario unless real desktop input was explicitly allowed.</summary>
    public static void Require(string what) =>
        Skip.IfNot(
            Allowed,
            $"{what} sends real mouse/keyboard input to whichever window is in front, which on a shared desktop " +
            $"is not the application under test (GUI-C-171). Skipped; set {OptInVariable}=1 on a desktop nothing " +
            "else is using to run it.");

    public static void Press(VirtualKeyShort key)
    {
        Require($"Pressing {key}");
        Keyboard.Press(key);
    }

    /// <summary>Ctrl+A — selects everything in the control that holds the keyboard focus.</summary>
    public static void SelectAll()
    {
        Require("Pressing Ctrl+A");
        Keyboard.TypeSimultaneously(VirtualKeyShort.CONTROL, VirtualKeyShort.KEY_A);
    }

    /// <summary>Types the text as real key presses into whatever holds the keyboard focus.</summary>
    public static void Type(string text)
    {
        Require("Typing");
        Keyboard.Type(text);
    }

    public static void MoveTo(System.Drawing.Point point, double pixelsPerMillisecond = 100)
    {
        Require("Moving the mouse");
        Mouse.MovePixelsPerMillisecond = pixelsPerMillisecond;
        Mouse.Position = point;
    }

    public static void Scroll(double notches)
    {
        Require("Turning the mouse wheel");
        Mouse.Scroll(notches);
    }

    public static void Down(MouseButton button)
    {
        Require("Pressing a mouse button");
        Mouse.Down(button);
    }

    public static void Up(MouseButton button)
    {
        Require("Releasing a mouse button");
        Mouse.Up(button);
    }

    public static void Click(AutomationElement element)
    {
        Require("Clicking with the mouse");
        element.Click();
    }
}
