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
public class ApplicationFixture : IDisposable
{
    private readonly Application? _application;

    public ApplicationFixture()
        : this(rawImageRelativePath: null)
    {
    }

    /// <summary>
    /// #136 (GUI-C-34): a workflow run needs an image on screen. The app already loads one when
    /// given <c>--automation-raw</c>, and that path is reused rather than driving the file dialog
    /// through UIA — a modal Win32 dialog is the most brittle thing a suite can automate, and the
    /// app offers a supported way in.
    /// </summary>
    protected ApplicationFixture(string? rawImageRelativePath)
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

        BackendMode = ResolveBackendMode();
        if (BackendMode is null)
        {
            SkipReason =
                $"XPE_E2E_BACKEND='{Environment.GetEnvironmentVariable(BackendVariable)}' is not one of " +
                $"{string.Join(" | ", AcceptedBackendModes)}.";
            return;
        }

        var startInfo = new ProcessStartInfo(exePath)
        {
            WorkingDirectory = Path.GetDirectoryName(exePath)!,
            UseShellExecute = false,
        };
        startInfo.ArgumentList.Add("--automation-backend");
        startInfo.ArgumentList.Add(BackendMode);

        if (rawImageRelativePath is not null)
        {
            var raw = Path.Combine(Path.GetDirectoryName(exePath)!, rawImageRelativePath);
            if (!File.Exists(raw))
            {
                SkipReason = $"Fixture image not found: {raw}";
                return;
            }

            // Width/height must accompany the path — the loader reads raw bytes and cannot infer them.
            startInfo.ArgumentList.Add("--automation-raw");
            startInfo.ArgumentList.Add(raw);
            startInfo.ArgumentList.Add("--automation-width");
            startInfo.ArgumentList.Add("1024");
            startInfo.ArgumentList.Add("--automation-height");
            startInfo.ArgumentList.Add("1024");
            RawImagePath = raw;
        }

        if (BackendMode == "Native")
        {
            // The app resolves native DLLs through XPE_NATIVE_DIR; pinning it EXCLUSIVE keeps the
            // search from wandering into build directories or sibling checkouts (GUI-C-16/#129), so
            // a Native run names exactly which binaries it exercised.
            var nativeDir = Environment.GetEnvironmentVariable(NativeDirVariable);
            if (!string.IsNullOrWhiteSpace(nativeDir))
            {
                startInfo.Environment[NativeDirVariable] = nativeDir;
                startInfo.Environment["XPE_NATIVE_DIR_EXCLUSIVE"] = "1";
                NativeDirectory = nativeDir;
            }
        }

        _application = Application.Launch(startInfo);
        MainWindow = _application.GetMainWindow(Automation, TimeSpan.FromSeconds(30));
        ExecutablePath = exePath;
    }

    /// <summary>Environment variable selecting which backend the suite exercises.</summary>
    public const string BackendVariable = "XPE_E2E_BACKEND";

    /// <summary>Environment variable naming the directory the app loads native DLLs from.</summary>
    public const string NativeDirVariable = "XPE_NATIVE_DIR";

    /// <summary>Accepted values, matching the app's own rule (GUI-C-27).</summary>
    public static readonly string[] AcceptedBackendModes = ["Mock", "Native"];

    /// <summary>The backend this run drives, or null when the variable held an unusable value.</summary>
    public string? BackendMode { get; }

    /// <summary>The pinned native directory, when one was supplied for a Native run.</summary>
    public string? NativeDirectory { get; }

    /// <summary>The raw image this run was launched with, when the fixture asked for one.</summary>
    public string? RawImagePath { get; }

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
    /// Reads XPE_E2E_BACKEND, defaulting to Mock. An unrecognised value is REFUSED rather than
    /// silently treated as Mock: the whole point of the Native variant is to know which backend was
    /// measured, and a typo that quietly downgrades makes the run report the wrong thing
    /// (the same hazard GUI-C-27 removed from --automation-backend).
    /// </summary>
    private static string? ResolveBackendMode()
    {
        var requested = Environment.GetEnvironmentVariable(BackendVariable);
        if (string.IsNullOrWhiteSpace(requested))
        {
            return "Mock";
        }

        return AcceptedBackendModes.FirstOrDefault(
            m => string.Equals(m, requested, StringComparison.OrdinalIgnoreCase));
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
