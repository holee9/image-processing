// #225 (GUI-C-191b): the window's smallest width as the automation tree reports it, and a check that a run really got there.
using System.Runtime.InteropServices;
using System.Xml.Linq;
using FlaUI.Core.AutomationElements;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>What <see cref="WindowMinimumWidth.ResizeToMinimum"/> found. <see cref="Problem"/> is null only when the window really is at the app's minimum.</summary>
internal sealed record MinimumWidthResult(bool CanResize, int Effective, int Expected, uint Dpi, double MinWidthDip, int Final, string? Problem)
{
    public string Describe() =>
        $"MinWidth {MinWidthDip} DIP at {Dpi} dpi = {Expected} px expected; the window shrank to {Effective} px; width after the check {Final} px; " +
        $"CanResize={CanResize}" + (Problem is null ? string.Empty : $". PROBLEM: {Problem}");
}

/// <summary>
/// Puts the window at its smallest width and says whether it got there. The mark's reachability at the minimum is the property
/// (GUI-C-191); a run that resized to "1280" with no Transform pattern, or on a screen whose DPI scales the app's 1280 DIP into more
/// pixels, would pass at whatever width it happened to have and prove nothing. The expected pixel width is the app's own
/// <c>MinWidth</c> (read from MainWindow.xaml) converted with the window's DPI; the effective one is what the window does when asked for
/// far less than it allows.
/// </summary>
internal static class WindowMinimumWidth
{
    [DllImport("user32.dll")]
    private static extern uint GetDpiForWindow(IntPtr hwnd);

    /// <summary>The app's MinWidth in device-independent pixels, read from MainWindow.xaml above the test output directory.</summary>
    public static double ReadMinWidthDip()
    {
        for (var dir = new DirectoryInfo(AppContext.BaseDirectory); dir is not null; dir = dir.Parent)
        {
            var candidate = Path.Combine(dir.FullName, "gui", "ImageProcTest", "MainWindow.xaml");
            if (File.Exists(candidate))
            {
                var root = XDocument.Load(candidate).Root!;
                return double.Parse((string?)root.Attribute("MinWidth") ?? throw new InvalidOperationException("MainWindow.xaml has no MinWidth."),
                    System.Globalization.CultureInfo.InvariantCulture);
            }
        }

        throw new InvalidOperationException("gui/ImageProcTest/MainWindow.xaml was not found above the test output directory.");
    }

    /// <summary>Shrinks the window to the smallest width it accepts and compares that with what the app's MinWidth implies.</summary>
    public static MinimumWidthResult ResizeToMinimum(Window window)
    {
        var minDip = ReadMinWidthDip();
        var dpi = GetDpiForWindow(window.Properties.NativeWindowHandle.Value);
        var expected = ExpectedPixels(minDip, dpi);

        var transform = window.Patterns.Transform.PatternOrDefault;
        if (transform is null || !transform.CanResize)
        {
            return new MinimumWidthResult(false, -1, expected, dpi, minDip, (int)window.BoundingRectangle.Width,
                "the window offers no resize through UI Automation (no Transform pattern, or CanResize is false), so it cannot be put at its minimum width");
        }

        transform.Resize(200, window.BoundingRectangle.Height);   // far below the minimum: the window stops at its own limit
        var effective = WaitUntilStable(window);
        var final = (int)window.BoundingRectangle.Width;

        return new MinimumWidthResult(true, effective, expected, dpi, minDip, final, ProblemWith(effective, expected, final));
    }

    /// <summary>The app's MinWidth (device-independent pixels, 1/96 inch) in the physical pixels UI Automation reports at <paramref name="dpi"/>.</summary>
    public static int ExpectedPixels(double minWidthDip, uint dpi) => (int)Math.Round(minWidthDip * dpi / 96.0);

    /// <summary>
    /// What is wrong with a check, or null: the smallest width the window accepted must be the one the app's MinWidth implies (within 2 px),
    /// and the window must still be there when the check ends.
    /// </summary>
    public static string? ProblemWith(int effective, int expected, int final)
    {
        if (Math.Abs(effective - expected) > 2)
        {
            return $"the smallest width the window accepts ({effective} px) is not the {expected} px that the app's MinWidth implies";
        }

        if (Math.Abs(final - effective) > 2)
        {
            return $"the window did not stay at its minimum ({final} px after the check, {effective} px when it stopped shrinking)";
        }

        return null;
    }

    /// <summary>The window's width once it has stopped changing for 400 ms (at most 4 s).</summary>
    private static int WaitUntilStable(Window window)
    {
        var deadline = DateTime.UtcNow.AddSeconds(4);
        var last = -1;
        var since = DateTime.UtcNow;
        while (DateTime.UtcNow < deadline)
        {
            var now = (int)window.BoundingRectangle.Width;
            if (now != last)
            {
                last = now;
                since = DateTime.UtcNow;
            }
            else if (DateTime.UtcNow - since >= TimeSpan.FromMilliseconds(400))
            {
                break;
            }

            Thread.Sleep(50);
        }

        return last;
    }
}
