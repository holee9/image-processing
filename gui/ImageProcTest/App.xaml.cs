using System.IO;

namespace ImageProcTest;

public partial class App : System.Windows.Application
{
    public static string? AutomationRawPath { get; private set; }

    public static string? AutomationReportPath { get; private set; }

    public static bool IsAutomationMode =>
        !string.IsNullOrWhiteSpace(AutomationRawPath) &&
        !string.IsNullOrWhiteSpace(AutomationReportPath);

    protected override void OnStartup(System.Windows.StartupEventArgs e)
    {
        ParseAutomationArgs(e.Args);

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

    private static void ParseAutomationArgs(string[] args)
    {
        for (var i = 0; i < args.Length; i++)
        {
            if (string.Equals(args[i], "--automation-raw", StringComparison.OrdinalIgnoreCase) &&
                i + 1 < args.Length)
            {
                AutomationRawPath = Path.GetFullPath(args[i + 1]);
                i++;
                continue;
            }

            if (string.Equals(args[i], "--automation-report", StringComparison.OrdinalIgnoreCase) &&
                i + 1 < args.Length)
            {
                AutomationReportPath = Path.GetFullPath(args[i + 1]);
                i++;
            }
        }
    }
}
