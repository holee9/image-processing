using System.IO;
using System.Text.Json;
using ImageProcTest.Models;
using ImageProcTest.Services;

namespace ImageProcTest;

public partial class App : System.Windows.Application
{
    /// <summary>Process exit code for a command line the automation refused to run.</summary>
    public const int InvalidAutomationArgsExitCode = 2;

    public static string? AutomationRawPath { get; private set; }

    public static string? AutomationReportPath { get; private set; }

    /// <summary>
    /// #136: the backend an automation run should exercise, supplied as an argument rather than read
    /// from the shipped settings file. Null when the argument was absent — the file still decides then.
    /// </summary>
    public static string? AutomationBackendMode { get; private set; }

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
        AutomationRawWidth = parsed.RawWidth;
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
