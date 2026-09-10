// XPE-GUI-E2E-001 §4.1: the smoke suite. Gate: the five scenarios together under 30 s.
using System.Diagnostics;
using System.Text.RegularExpressions;
using FlaUI.Core.AutomationElements;
using FlaUI.Core.Definitions;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Smoke;

/// <summary>
/// S-01 … S-05 from the plan's §4.1 table, driven against the real WPF app in Mock backend mode.
///
/// AutomationIds deviate from the plan and that is deliberate: the app already carries 76 ids in an
/// element-name convention (<c>FileMenu</c>, <c>LoadRawImageButton</c>), while the plan invents an
/// <c>XPE_*</c> one. Renaming 76 ids to satisfy a table would be a UI-wide change this card excludes,
/// and would leave two conventions if done partially — so these scenarios target what the app has.
/// The full mapping is in the GUI-C-29 report.
///
/// Each scenario prints its own elapsed time so the gate is measured rather than assumed.
/// </summary>
[Collection(ApplicationCollection.Name)]
public sealed class SmokeScenarios
{
    private readonly ApplicationFixture _app;
    private readonly ITestOutputHelper _output;

    public SmokeScenarios(ApplicationFixture app, ITestOutputHelper output)
    {
        _app = app;
        _output = output;
    }

    /// <summary>S-01: the app starts and shows a main window whose title names the product.</summary>
    [SkippableFact]
    public void S01_Launch_HasMainWindow()
    {
        Measure("S-01", window =>
        {
            Assert.Contains("ImageProcTest", window.Title, StringComparison.Ordinal);
            Assert.Equal("MainWindow", window.AutomationId);
            Assert.True(window.IsAvailable, "The main window is not available to automation.");
        });
    }

    /// <summary>
    /// S-02: the six menu groups exist. The plan names them XPE_Menu_*; the app calls them
    /// <c>FileMenu</c> … <c>HelpMenu</c>, which is what is asserted.
    /// </summary>
    [SkippableFact]
    public void S02_MenuBar_HasSixGroups()
    {
        string[] expected = ["FileMenu", "BackendMenu", "ViewMenu", "PipelineMenu", "ToolsMenu", "HelpMenu"];

        Measure("S-02", window =>
        {
            foreach (var id in expected)
            {
                var item = window.FindFirstDescendant(cf => cf.ByAutomationId(id));
                Assert.True(item is not null, $"Menu group '{id}' was not found in the window tree.");
                Assert.Equal(ControlType.MenuItem, item!.ControlType);
            }
        });
    }

    /// <summary>
    /// S-03: the toolbar's primary buttons are enabled in Mock mode. The plan lists Open/Run/Reset;
    /// the app's toolbar carries Load Raw Image / Initialize Backend / Clear Logs, which are the
    /// same three roles (open input, start backend work, reset view state).
    /// </summary>
    [SkippableFact]
    public void S03_ToolbarButtons_AreEnabledInMockMode()
    {
        string[] buttons = ["LoadRawImageButton", "InitializeBackendButton", "ClearLogsButton"];

        Measure("S-03", window =>
        {
            foreach (var id in buttons)
            {
                var button = window.FindFirstDescendant(cf => cf.ByAutomationId(id));
                Assert.True(button is not null, $"Toolbar button '{id}' was not found.");
                Assert.True(button!.IsEnabled, $"Toolbar button '{id}' is disabled in Mock mode.");
            }
        });
    }

    /// <summary>
    /// S-04: the offline "Help Home" entry is reachable (the plan's XPE_Menu_Help_Home).
    ///
    /// The menu must be EXPANDED first. WPF realises a submenu's children lazily, so
    /// OpenHelpIndexMenuItem exists in MainWindow.xaml and is still absent from the UIA tree until
    /// the Help group opens — measured: the first version of this scenario failed with "not found"
    /// against an id that is plainly in the XAML.
    ///
    /// The item is asserted rather than clicked: clicking spawns a second window whose teardown
    /// would outlive this scenario and leak into the next one, and the gate is 30 s for all five.
    /// </summary>
    [SkippableFact]
    public void S04_HelpMenu_OffersOfflineHelpHome()
    {
        Measure("S-04", window =>
        {
            var helpMenu = window.FindFirstDescendant(cf => cf.ByAutomationId("HelpMenu"));
            Assert.True(helpMenu is not null, "HelpMenu was not found.");

            helpMenu!.AsMenuItem().Expand();
            try
            {
                var helpHome = WaitFor(() =>
                    window.FindFirstDescendant(cf => cf.ByAutomationId("OpenHelpIndexMenuItem")));

                Assert.True(helpHome is not null, "OpenHelpIndexMenuItem was not found after expanding HelpMenu.");
                Assert.True(helpHome!.IsEnabled, "The offline Help Home entry is disabled.");
            }
            finally
            {
                helpMenu.AsMenuItem().Collapse();
            }
        });
    }

    /// <summary>Polls briefly for an element the UI creates lazily. Returns null when it never appears.</summary>
    private static AutomationElement? WaitFor(Func<AutomationElement?> find)
    {
        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(5);
        while (DateTime.UtcNow < deadline)
        {
            var found = find();
            if (found is not null) return found;
            Thread.Sleep(100);
        }

        return null;
    }

    /// <summary>
    /// S-05 (plan §4.1, restored): the runtime version is visible on screen.
    ///
    /// GUI-C-29 measured that nothing rendered a version — RuntimeInfo was not bound to the view —
    /// so the scenario was narrowed and the absence reported. GUI-C-30 added the label, and this is
    /// the plan's assertion put back: the text names the backend mode and carries a version that is
    /// either a semver or the mock marker.
    /// </summary>
    [SkippableFact]
    public void S05_RuntimePanel_ShowsBackendVersion()
    {
        Measure("S-05", window =>
        {
            var label = window.FindFirstDescendant(cf => cf.ByAutomationId("RuntimeCommonVersionText"));
            Assert.True(label is not null, "RuntimeCommonVersionText was not found.");

            var text = label!.Name;
            Assert.False(string.IsNullOrWhiteSpace(text), "The runtime version label is empty.");
            Assert.Contains("mode=", text, StringComparison.Ordinal);

            // A version the backend actually reported: either dotted digits, or the mock marker.
            Assert.True(
                Regex.IsMatch(text, @"\d+\.\d+\.\d+") || text.Contains("mock", StringComparison.OrdinalIgnoreCase),
                $"The label shows no version — expected a semver or a mock marker, got '{text}'.");
        });
    }

    /// <summary>Runs one scenario against the window, skipping cleanly when the app is not built.</summary>
    private void Measure(string scenario, Action<Window> body)
    {
        Skip.If(!_app.IsAvailable, _app.SkipReason ?? "The application is not available.");

        var stopwatch = Stopwatch.StartNew();
        try
        {
            body(_app.MainWindow!);
        }
        finally
        {
            stopwatch.Stop();
            _output.WriteLine($"{scenario} elapsed {stopwatch.ElapsedMilliseconds} ms");
        }
    }
}
