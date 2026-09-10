// XPE-GUI-E2E-001 §3: launches the WPF app under test and guarantees it is gone afterwards.
using System.Diagnostics;
using FlaUI.Core;
using FlaUI.Core.AutomationElements;
using FlaUI.UIA3;
using Xunit;

namespace ImageProcTest.E2ETests.Fixtures;

/// <summary>
/// Starts one instance of the GUI for a test class and disposes it, whatever the tests did.
///
/// Two properties are load-bearing:
///
/// <list type="bullet">
/// <item>The app is launched in <b>Mock</b> backend mode through <c>--automation-backend</c>
/// (GUI-C-26), so the smoke suite never depends on native DLLs being staged.</item>
/// <item>It is launched WITHOUT <c>--automation-report</c>, so <c>App.IsAutomationMode</c> stays
/// false and the self-driving scenario does not run — that harness closes the window on its own,
/// which would race every assertion here.</item>
/// </list>
///
/// A leaked app process holds a window that the next test class would find and drive, so disposal
/// kills rather than asks: <c>Close()</c> on a WPF window can be refused or blocked by a dialog.
/// </summary>
public sealed class ApplicationFixture : IDisposable
{
    private readonly Application? _application;

    public ApplicationFixture()
    {
        Automation = new UIA3Automation();

        var exePath = ResolveApplicationExecutable();
        if (exePath is null)
        {
            SkipReason =
                "gui/ImageProcTest/bin/Debug/net8.0-windows/ImageProcTest.exe was not found. " +
                "Build the gui project (or the clients solution, which now includes it) before the E2E suite.";
            return;
        }

        var startInfo = new ProcessStartInfo(exePath)
        {
            WorkingDirectory = Path.GetDirectoryName(exePath)!,
            UseShellExecute = false,
        };
        startInfo.ArgumentList.Add("--automation-backend");
        startInfo.ArgumentList.Add("Mock");

        _application = Application.Launch(startInfo);
        MainWindow = _application.GetMainWindow(Automation, TimeSpan.FromSeconds(30));
        ExecutablePath = exePath;
    }

    /// <summary>The UIA layer, shared by every scenario in the class.</summary>
    public UIA3Automation Automation { get; }

    /// <summary>The app's main window, or null when the app could not be started.</summary>
    public Window? MainWindow { get; }

    /// <summary>Path the app was launched from — reported so a run names what it exercised.</summary>
    public string? ExecutablePath { get; }

    /// <summary>Non-null when the app was not started; scenarios skip with this text.</summary>
    public string? SkipReason { get; }

    /// <summary>True when a window is available to drive.</summary>
    public bool IsAvailable => MainWindow is not null;

    public void Dispose()
    {
        try
        {
            _application?.Kill();
        }
        catch (InvalidOperationException)
        {
            // Already exited — nothing to kill.
        }
        finally
        {
            _application?.Dispose();
            Automation.Dispose();
        }
    }

    /// <summary>
    /// Walks up from the test output directory to the repository, then to the gui build output.
    /// Returns null rather than throwing so the suite can report "not built" as a skip instead of
    /// a failure that looks like a defect in the app.
    /// </summary>
    private static string? ResolveApplicationExecutable()
    {
        var relative = Path.Combine(
            "gui", "ImageProcTest", "bin", "Debug", "net8.0-windows", "ImageProcTest.exe");

        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null)
        {
            var candidate = Path.Combine(dir.FullName, relative);
            if (File.Exists(candidate)) return candidate;
            dir = dir.Parent;
        }

        return null;
    }
}

/// <summary>One app instance per test class — launching is the expensive part of the gate.</summary>
[CollectionDefinition(Name)]
public sealed class ApplicationCollection : ICollectionFixture<ApplicationFixture>
{
    public const string Name = "gui-application";
}
