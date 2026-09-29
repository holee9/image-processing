using System.IO;
using System.Text.Json;
using ImageProcTest.Models;
using ImageProcTest.Services;

namespace ImageProcTest;

public partial class App : System.Windows.Application
{
    /// <summary>Process exit code for a command line the automation refused to run.</summary>
    public const int InvalidAutomationArgsExitCode = 2;

    /// <summary>
    /// GUI-C-84: process exit code for an automation run that completed with <c>Passed=false</c>. Until
    /// then such a run exited 0, so a caller reading only the exit code took a failed run for a pass.
    /// </summary>
    public const int AutomationFailedExitCode = 1;

    public static string? AutomationRawPath { get; private set; }

    public static string? AutomationReportPath { get; private set; }

    /// <summary>
    /// #136: the backend an automation run should exercise, supplied as an argument rather than read
    /// from the shipped settings file. Null when the argument was absent — the file still decides then.
    /// </summary>
    public static string? AutomationBackendMode { get; private set; }

    /// <summary>#141: directory holding the generated XCal set for this run, when one was supplied.</summary>
    public static string? AutomationCalibrationDirectory { get; private set; }

    /// <summary>#171 (GUI-C-79): armed only by <c>--automation-fault</c>; null means no fault injection.</summary>
    public static int? AutomationDisplayPipelineFailAfter { get; private set; }

    /// <summary>#173 (GUI-C-119): the settings file this run reads and writes, when one was named.</summary>
    public static string? AutomationSettingsPath { get; private set; }

    /// <summary>
    /// Runner path for the Run Self-Check command, when the command line overrode it
    /// (<c>--automation-selfcheck-exe</c>, #225/GUI-C-159). Null on a normal launch.
    /// </summary>
    public static string? AutomationSelfCheckExePath { get; private set; }

    public static int? AutomationRawWidth { get; private set; }

    public static int? AutomationRawHeight { get; private set; }

    public static bool IsAutomationMode =>
        !string.IsNullOrWhiteSpace(AutomationRawPath) &&
        !string.IsNullOrWhiteSpace(AutomationReportPath);

    protected override void OnStartup(System.Windows.StartupEventArgs e)
    {
        // #129: install the shared native search policy before anything can P/Invoke. Doing it here
        // rather than in a static constructor keeps the ordering explicit: the resolver must be in
        // place before the first DllImport, and only one may ever be registered per assembly.
        Services.Native.GuiNativeLibraryResolver.Install();

        // #136: parsing lives in AutomationArgs so it can be tested; this class only applies it.
        var parsed = AutomationArgs.Parse(e.Args);

        AutomationRawPath = parsed.RawPath;
        AutomationReportPath = parsed.ReportPath;
        AutomationBackendMode = parsed.BackendMode;
        AutomationCalibrationDirectory = parsed.CalibrationDirectory;
        AutomationSettingsPath = parsed.SettingsPath;
        AutomationSelfCheckExePath = parsed.SelfCheckExePath;
        // #200 (GUI-C-138): handed to the control directly. The viewport is created by XAML, so there is
        // no constructor to carry it, and routing it through settings would make a diagnostic switch
        // look like a user preference.
        Controls.ImageComparisonViewport.AutomationRenderDumpPath = parsed.RenderDumpPath;
        AutomationRawWidth = parsed.RawWidth;
        AutomationDisplayPipelineFailAfter = parsed.DisplayPipelineFailAfter;
        AutomationRawHeight = parsed.RawHeight;

        if (!parsed.IsValid)
        {
            // Refuse rather than start: an unusable switch used to be skipped, which produced a run
            // that looked requested but measured something else.
            WriteRejectionReport(parsed.ReportPath, parsed.Error!);
            Environment.Exit(InvalidAutomationArgsExitCode);
            return;
        }

        DispatcherUnhandledException += (_, args) =>
        {
            System.Windows.MessageBox.Show(
                $"Unhandled UI exception: {args.Exception.Message}",
                "ImageProcTest GUI-S0",
                System.Windows.MessageBoxButton.OK,
                System.Windows.MessageBoxImage.Error);
            args.Handled = true;
        };

        base.OnStartup(e);
    }

    /// <summary>
    /// Records the refusal where the caller asked for the report. Best effort: when the command line
    /// was rejected before naming a report path, or the path cannot be written, the exit code is the
    /// only signal.
    /// </summary>
    private static void WriteRejectionReport(string? reportPath, string reason)
    {
        if (string.IsNullOrWhiteSpace(reportPath))
        {
            return;
        }

        try
        {
            var report = new GuiAutomationReport { Passed = false, Error = reason };
            Directory.CreateDirectory(Path.GetDirectoryName(reportPath)!);
            File.WriteAllText(
                reportPath,
                JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }));
        }
        catch (IOException)
        {
            // Nothing better to do while shutting down; the exit code still carries the refusal.
        }
        catch (UnauthorizedAccessException)
        {
        }
    }
}
