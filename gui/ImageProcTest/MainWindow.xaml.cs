using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading.Tasks;
using System.Windows;
using ImageProcTest.Models;
using ImageProcTest.Services;
using ImageProcTest.ViewModels;

namespace ImageProcTest;

public partial class MainWindow : System.Windows.Window
{
    private readonly HelpBundleService _helpBundleService = new();

    public MainWindow()
    {
        InitializeComponent();

        var settingsService = new AppSettingsService();
        var settings = settingsService.Load();
        DataContext = new MainWindowViewModel(settings, settingsService, XpeBackendFactory.Create);
        Loaded += OnLoaded;
    }

    protected override void OnClosed(System.EventArgs e)
    {
        if (DataContext is MainWindowViewModel viewModel)
        {
            viewModel.ShutdownBackend();
        }

        base.OnClosed(e);
    }

    private async void OnLoaded(object sender, RoutedEventArgs e)
    {
        if (!App.IsAutomationMode)
        {
            return;
        }

        await RunAutomationScenarioAsync();
    }

    private async Task RunAutomationScenarioAsync()
    {
        var report = new GuiAutomationReport();

        try
        {
            if (DataContext is not MainWindowViewModel viewModel)
            {
                throw new InvalidOperationException("DataContext is not MainWindowViewModel.");
            }

            report.BackendVersion = viewModel.RuntimeInfo.Version;
            report.InitialLogCount = viewModel.Logs.Count;
            report.InitialAlertCount = viewModel.Alerts.Count;

            ClickButton(LoadRawImageButton);
            await Task.Delay(400);

            report.LogCountAfterLoad = viewModel.Logs.Count;
            report.AlertCountAfterLoad = viewModel.Alerts.Count;
            report.ActiveImageSummary = viewModel.ActiveImageSummary;
            report.StatusAfterLoad = viewModel.StatusText;
            report.LastRawDirectory = viewModel.Settings.LastRawDirectory;

            ClickButton(SaveSettingsButton);
            await Task.Delay(250);

            report.TopLevelMenuCount = MainMenu.Items.Count;
            report.CanonicalMenuGroupsDetected =
                FileMenu is not null &&
                BackendMenu is not null &&
                ViewMenu is not null &&
                PipelineMenu is not null &&
                ToolsMenu is not null &&
                HelpMenu is not null;
            report.DisabledFutureCommandCount = new[]
                {
                    OpenDicomMenuItem,
                    NativeBackendModeMenuItem,
                    PInvokeSmokeTestMenuItem,
                    ZoomFitMenuItem,
                    ZoomActualMenuItem,
                    RunPreprocessingMenuItem,
                    RunDeterministicBaselineMenuItem,
                    RunFullPipelineMenuItem,
                    StopProcessingMenuItem,
                    StageTimingMenuItem,
                    RunSelfCheckMenuItem,
                    RunGuiE2EMenuItem,
                    BenchmarkRunnerMenuItem,
                    QaConstancyMenuItem,
                    OpenApiReferenceMenuItem,
                    OpenTroubleshootingMenuItem
                }
                .Count(item => !item.IsEnabled);

            ClickMenuItem(ExportAutomationReportMenuItem);
            await Task.Delay(200);
            report.MenuCommandReportCreated = File.Exists(Path.Combine(AppContext.BaseDirectory, "menu-command-report.json"));

            var settingsFile = Path.Combine(AppContext.BaseDirectory, "appsettings.json");
            if (File.Exists(settingsFile))
            {
                using var document = JsonDocument.Parse(File.ReadAllText(settingsFile));
                if (document.RootElement.TryGetProperty("lastRawDir", out var lastRawDirElement))
                {
                    report.LastRawDirPersisted = string.Equals(
                        lastRawDirElement.GetString(),
                        report.LastRawDirectory,
                        StringComparison.OrdinalIgnoreCase);
                }
            }

            OpenHelpPage(HelpPageKind.QuickStart);
            await Task.Delay(350);

            if (OwnedWindows.OfType<HelpWindow>().FirstOrDefault() is { } helpWindow)
            {
                report.HelpWindowOpened = true;
                report.HelpWindowTitle = helpWindow.Title;
                report.HelpDocumentPath = helpWindow.CurrentDocumentPath;
                report.HelpDocumentLoaded = helpWindow.DocumentLoaded;
                helpWindow.Close();
            }

            ClickButton(ClearLogsButton);
            ClickButton(ClearAlertsButton);
            await Task.Delay(200);

            report.LogCountAfterClear = viewModel.Logs.Count;
            report.AlertCountAfterClear = viewModel.Alerts.Count;

            ClickButton(ShutdownBackendButton);
            await Task.Delay(200);

            report.RuntimeStateAfterShutdown = viewModel.RuntimeInfo.State;
            report.Passed =
                report.BackendVersion == "v0.0.0-mock" &&
                report.InitialLogCount >= 5 &&
                report.InitialAlertCount == 3 &&
                report.LogCountAfterLoad > report.InitialLogCount &&
                report.ActiveImageSummary.StartsWith("RAW ", StringComparison.Ordinal) &&
                report.LastRawDirPersisted &&
                report.HelpWindowOpened &&
                report.HelpDocumentLoaded &&
                !string.IsNullOrWhiteSpace(report.HelpDocumentPath) &&
                report.TopLevelMenuCount >= 6 &&
                report.CanonicalMenuGroupsDetected &&
                report.DisabledFutureCommandCount >= 10 &&
                report.MenuCommandReportCreated &&
                report.LogCountAfterClear == 0 &&
                report.AlertCountAfterClear == 0 &&
                report.RuntimeStateAfterShutdown == "Shutdown";
        }
        catch (Exception ex)
        {
            report.Passed = false;
            report.Error = ex.ToString();
        }
        finally
        {
            if (!string.IsNullOrWhiteSpace(App.AutomationReportPath))
            {
                Directory.CreateDirectory(Path.GetDirectoryName(App.AutomationReportPath)!);
                File.WriteAllText(
                    App.AutomationReportPath,
                    JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }));
            }

            Close();
            System.Windows.Application.Current.Shutdown();
        }
    }

    private static void ClickButton(System.Windows.Controls.Primitives.ButtonBase button)
    {
        if (button.Command is not null && button.Command.CanExecute(button.CommandParameter))
        {
            button.Command.Execute(button.CommandParameter);
            return;
        }

        button.RaiseEvent(new RoutedEventArgs(System.Windows.Controls.Primitives.ButtonBase.ClickEvent, button));
    }

    private static void ClickMenuItem(System.Windows.Controls.MenuItem menuItem)
    {
        if (menuItem.Command is not null && menuItem.Command.CanExecute(menuItem.CommandParameter))
        {
            menuItem.Command.Execute(menuItem.CommandParameter);
            return;
        }

        menuItem.RaiseEvent(new RoutedEventArgs(System.Windows.Controls.MenuItem.ClickEvent, menuItem));
    }

    private void ExitMenuItem_OnClick(object sender, RoutedEventArgs e)
    {
        Close();
    }

    private void OpenHelpIndexMenuItem_OnClick(object sender, RoutedEventArgs e)
    {
        OpenHelpPage(HelpPageKind.Index);
    }

    private void OpenQuickStartHelpMenuItem_OnClick(object sender, RoutedEventArgs e)
    {
        OpenHelpPage(HelpPageKind.QuickStart);
    }

    private void OpenScopeHelpMenuItem_OnClick(object sender, RoutedEventArgs e)
    {
        OpenHelpPage(HelpPageKind.Scope);
    }

    private void AboutBuildInfoMenuItem_OnClick(object sender, RoutedEventArgs e)
    {
        System.Windows.MessageBox.Show(
            $"ImageProcTest GUI-S0{Environment.NewLine}Backend: {(DataContext as MainWindowViewModel)?.RuntimeInfo.Version ?? "unknown"}{Environment.NewLine}Help bundle: packaged offline HTML",
            "About ImageProcTest",
            System.Windows.MessageBoxButton.OK,
            System.Windows.MessageBoxImage.Information);
    }

    private void OpenHelpPage(HelpPageKind pageKind)
    {
        var pagePath = _helpBundleService.GetHelpPagePath(pageKind);
        var helpTitle = pageKind switch
        {
            HelpPageKind.Index => "ImageProcTest Help",
            HelpPageKind.QuickStart => "ImageProcTest Help - Quick Start",
            HelpPageKind.Scope => "ImageProcTest Help - Scope and Limitations",
            _ => "ImageProcTest Help"
        };

        if (!_helpBundleService.HelpPageExists(pageKind))
        {
            System.Windows.MessageBox.Show(
                $"Help page not found: {pagePath}",
                "ImageProcTest Help",
                System.Windows.MessageBoxButton.OK,
                System.Windows.MessageBoxImage.Warning);
            return;
        }

        var helpWindow = new HelpWindow(helpTitle, pagePath)
        {
            Owner = this
        };
        helpWindow.Show();
        helpWindow.Activate();
    }
}
