using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Threading;
using ImageProcTest;
using ImageProcTest.Controls;
using ImageProcTest.ViewModels;

RunSelfCheck();
RunWpfE2EOnStaThread();
Console.WriteLine("GUI-S0 E2E passed.");

static void RunSelfCheck()
{
    var repoRoot = FindRepoRoot(AppContext.BaseDirectory);
    var selfCheckExe = Path.Combine(repoRoot, "gui", "ImageProcTest.SelfCheck", "bin", "Debug", "net8.0-windows", "ImageProcTest.SelfCheck.exe");
    Assert(File.Exists(selfCheckExe), $"Self-check exe not found: {selfCheckExe}");

    using var process = Process.Start(new ProcessStartInfo
    {
        FileName = selfCheckExe,
        WorkingDirectory = Path.GetDirectoryName(selfCheckExe)!,
        RedirectStandardOutput = true,
        RedirectStandardError = true,
        UseShellExecute = false
    }) ?? throw new InvalidOperationException("Failed to launch self-check.");

    var stdout = process.StandardOutput.ReadToEnd();
    var stderr = process.StandardError.ReadToEnd();
    process.WaitForExit(30000);

    Assert(process.ExitCode == 0, $"Self-check failed.{Environment.NewLine}{stdout}{Environment.NewLine}{stderr}");
    Assert(stdout.Contains("GUI-S0 self-check passed.", StringComparison.Ordinal), "Self-check success text missing.");
}

static void RunWpfE2EOnStaThread()
{
    Exception? capturedException = null;
    var thread = new Thread(() =>
    {
        try
        {
            RunWpfE2E();
        }
        catch (Exception ex)
        {
            capturedException = ex;
        }
    });

    thread.SetApartmentState(ApartmentState.STA);
    thread.Start();
    thread.Join();

    if (capturedException is not null)
    {
        throw new InvalidOperationException("WPF E2E failed.", capturedException);
    }
}

static void RunWpfE2E()
{
    var application = new Application
    {
        ShutdownMode = ShutdownMode.OnExplicitShutdown
    };

    var window = new MainWindow();
    try
    {
        window.Show();
        DoEvents();

        Assert(window.Title == "ImageProcTest GUI-S0", "Main window title mismatch.");

        var initializeButton = GetControl<Button>(window, "InitializeBackendButton");
        var shutdownButton = GetControl<Button>(window, "ShutdownBackendButton");
        var loadButton = GetControl<Button>(window, "LoadRawImageButton");
        var saveButton = GetControl<Button>(window, "SaveSettingsButton");
        var clearLogsButton = GetControl<Button>(window, "ClearLogsButton");
        var clearAlertsButton = GetControl<Button>(window, "ClearAlertsButton");
        var fileMenu = GetControl<MenuItem>(window, "FileMenu");
        var backendMenu = GetControl<MenuItem>(window, "BackendMenu");
        var viewMenu = GetControl<MenuItem>(window, "ViewMenu");
        var pipelineMenu = GetControl<MenuItem>(window, "PipelineMenu");
        var toolsMenu = GetControl<MenuItem>(window, "ToolsMenu");
        var helpMenu = GetControl<MenuItem>(window, "HelpMenu");
        var mainMenu = GetControl<Menu>(window, "MainMenu");
        var openRawMenuItem = GetControl<MenuItem>(window, "OpenRawMenuItem");
        var openRecentMenuItem = GetControl<MenuItem>(window, "OpenRecentMenuItem");
        var saveSettingsMenuItem = GetControl<MenuItem>(window, "SaveSettingsMenuItem");
        var exportEvidenceBundleMenuItem = GetControl<MenuItem>(window, "ExportEvidenceBundleMenuItem");
        var openDicomMenuItem = GetControl<MenuItem>(window, "OpenDicomMenuItem");
        var nativeModeMenuItem = GetControl<MenuItem>(window, "NativeBackendModeMenuItem");
        var openRuntimeLogsMenuItem = GetControl<MenuItem>(window, "OpenRuntimeLogsMenuItem");
        var pInvokeSmokeMenuItem = GetControl<MenuItem>(window, "PInvokeSmokeTestMenuItem");
        var applyDisplayPipelineMenuItem = GetControl<MenuItem>(window, "ApplyDisplayPipelineMenuItem");
        var runPreprocessingMenuItem = GetControl<MenuItem>(window, "RunPreprocessingMenuItem");
        var runDeterministicBaselineMenuItem = GetControl<MenuItem>(window, "RunDeterministicBaselineMenuItem");
        var runFullPipelineMenuItem = GetControl<MenuItem>(window, "RunFullPipelineMenuItem");
        var openPipelineDiagnosticsMenuItem = GetControl<MenuItem>(window, "OpenPipelineDiagnosticsMenuItem");
        var openEvidenceFolderMenuItem = GetControl<MenuItem>(window, "OpenEvidenceFolderMenuItem");
        var showRuntimePanelMenuItem = GetControl<MenuItem>(window, "ShowRuntimePanelMenuItem");
        var showDisplaySettingsPanelMenuItem = GetControl<MenuItem>(window, "ShowDisplaySettingsPanelMenuItem");
        var showLogsPanelMenuItem = GetControl<MenuItem>(window, "ShowLogsPanelMenuItem");
        var clearLogsMenuItem = GetControl<MenuItem>(window, "ClearLogsMenuItem");
        var clearAlertsMenuItem = GetControl<MenuItem>(window, "ClearAlertsMenuItem");
        var helpHomeMenuItem = GetControl<MenuItem>(window, "OpenHelpIndexMenuItem");
        var quickStartMenuItem = GetControl<MenuItem>(window, "OpenQuickStartHelpMenuItem");
        var scopeMenuItem = GetControl<MenuItem>(window, "OpenScopeHelpMenuItem");
        var currentWorkflowHelpMenuItem = GetControl<MenuItem>(window, "OpenCurrentWorkflowHelpMenuItem");
        var versionText = GetControl<TextBlock>(window, "BackendVersionText");
        var logsList = GetControl<ListBox>(window, "LogsListBox");
        var alertsList = GetControl<ListBox>(window, "AlertsListBox");
        var diagnosticsColumnSplitter = GetControl<GridSplitter>(window, "DiagnosticsColumnSplitter");
        var logsAlertsSplitter = GetControl<GridSplitter>(window, "LogsAlertsSplitter");
        var displayVersionText = GetControl<TextBlock>(window, "DisplayVersionText");
        var calibrationEvaluationSummaryText = GetControl<TextBlock>(window, "CalibrationEvaluationSummaryText");
        var offsetCorrectionModeComboBox = GetControl<ComboBox>(window, "OffsetCorrectionModeComboBox");
        var gainCorrectionModeComboBox = GetControl<ComboBox>(window, "GainCorrectionModeComboBox");
        var defectCorrectionModeComboBox = GetControl<ComboBox>(window, "DefectCorrectionModeComboBox");
        var ghostCorrectionModeComboBox = GetControl<ComboBox>(window, "GhostCorrectionModeComboBox");
        var temperatureCompensationModeComboBox = GetControl<ComboBox>(window, "TemperatureCompensationModeComboBox");
        var nonlinearityCorrectionModeComboBox = GetControl<ComboBox>(window, "NonlinearityCorrectionModeComboBox");
        var binningCorrectionModeComboBox = GetControl<ComboBox>(window, "BinningCorrectionModeComboBox");
        var displaySettingsGroup = GetControl<GroupBox>(window, "DisplaySettingsGroup");
        var voiLutModeComboBox = GetControl<ComboBox>(window, "VoiLutModeComboBox");
        var bodyPartPresetComboBox = GetControl<ComboBox>(window, "BodyPartPresetComboBox");
        var applyBodyPartPresetButton = GetControl<Button>(window, "ApplyBodyPartPresetButton");
        var applyDisplayPipelineButton = GetControl<Button>(window, "ApplyDisplayPipelineButton");
        var displayPipelineSummaryText = GetControl<TextBlock>(window, "DisplayPipelineSummaryText");
        var comparisonModeComboBox = GetControl<ComboBox>(window, "ComparisonModeComboBox");
        var comparisonOverlayOpacitySlider = GetControl<Slider>(window, "ComparisonOverlayOpacitySlider");
        var comparisonViewport = GetControl<ImageComparisonViewport>(window, "ImageComparisonViewport");
        var zoomFitMenuItem = GetControl<MenuItem>(window, "ZoomFitMenuItem");
        var zoomActualMenuItem = GetControl<MenuItem>(window, "ZoomActualMenuItem");
        var zoomInButton = GetControl<Button>(window, "ZoomInButton");
        var zoomOutButton = GetControl<Button>(window, "ZoomOutButton");
        var detachComparisonViewerButton = GetControl<Button>(window, "DetachComparisonViewerButton");
        var comparisonStatusText = GetControl<TextBlock>(window, "ComparisonStatusText");

        Assert(initializeButton.Command is not null, "Initialize button command missing.");
        Assert(shutdownButton.Command is not null, "Shutdown button command missing.");
        Assert(loadButton.Command is not null, "Load button command missing.");
        Assert(saveButton.Command is not null, "Save button command missing.");
        Assert(mainMenu.Items.Count == 6, "Main menu should expose 6 canonical top-level groups.");
        Assert(fileMenu.Header.ToString()?.Contains("File", StringComparison.OrdinalIgnoreCase) == true, "File menu missing.");
        Assert(backendMenu.Header.ToString()?.Contains("Backend", StringComparison.OrdinalIgnoreCase) == true, "Backend menu missing.");
        Assert(viewMenu.Header.ToString()?.Contains("View", StringComparison.OrdinalIgnoreCase) == true, "View menu missing.");
        Assert(pipelineMenu.Header.ToString()?.Contains("Pipeline", StringComparison.OrdinalIgnoreCase) == true, "Pipeline menu missing.");
        Assert(toolsMenu.Header.ToString()?.Contains("Tools", StringComparison.OrdinalIgnoreCase) == true, "Tools menu missing.");
        Assert(helpMenu.Items.Count >= 7, "Help menu should expose active and planned help entries.");
        Assert(openRawMenuItem.Command is not null, "Open Raw menu command missing.");
        Assert(saveSettingsMenuItem.Command is not null, "Save Settings menu command missing.");
        Assert(applyDisplayPipelineMenuItem.Command is not null, "Apply Display Pipeline menu command missing.");
        Assert(clearLogsMenuItem.Command is not null, "Clear Logs menu command missing.");
        Assert(clearAlertsMenuItem.Command is not null, "Clear Alerts menu command missing.");
        Assert(ReferenceEquals(initializeButton.Command, GetControl<MenuItem>(window, "InitializeBackendMenuItem").Command), "Initialize toolbar/menu command mismatch.");
        Assert(ReferenceEquals(shutdownButton.Command, GetControl<MenuItem>(window, "ShutdownBackendMenuItem").Command), "Shutdown toolbar/menu command mismatch.");
        Assert(ReferenceEquals(loadButton.Command, openRawMenuItem.Command), "Load Raw toolbar/menu command mismatch.");
        Assert(ReferenceEquals(saveButton.Command, saveSettingsMenuItem.Command), "Save Settings toolbar/menu command mismatch.");
        Assert(ReferenceEquals(clearLogsButton.Command, clearLogsMenuItem.Command), "Clear Logs toolbar/menu command mismatch.");
        Assert(ReferenceEquals(clearAlertsButton.Command, clearAlertsMenuItem.Command), "Clear Alerts toolbar/menu command mismatch.");
        Assert(showRuntimePanelMenuItem.IsCheckable, "Runtime view menu item should be checkable.");
        Assert(showDisplaySettingsPanelMenuItem.IsCheckable, "Display settings view menu item should be checkable.");
        Assert(showLogsPanelMenuItem.IsCheckable, "Logs view menu item should be checkable.");
        Assert(!openRecentMenuItem.IsEnabled, "Open Recent must be disabled until recent-file history is implemented.");
        Assert(!openDicomMenuItem.IsEnabled, "Open DICOM must be disabled in GUI-S0.");
        Assert(!exportEvidenceBundleMenuItem.IsEnabled, "Evidence bundle export must be disabled until deterministic artifacts exist.");
        Assert(!nativeModeMenuItem.IsEnabled, "Native backend mode must be disabled in GUI-S0.");
        Assert(!openRuntimeLogsMenuItem.IsEnabled, "Runtime logs menu must be disabled until persistent runtime log export exists.");
        Assert(!pInvokeSmokeMenuItem.IsEnabled, "P/Invoke smoke menu must be disabled until SPRINT-P0-07.");
        Assert(!runPreprocessingMenuItem.IsEnabled, "Preprocessing menu must be disabled until Phase 1a.");
        Assert(!runDeterministicBaselineMenuItem.IsEnabled, "Deterministic baseline menu must be disabled until Phase 1b.");
        Assert(!runFullPipelineMenuItem.IsEnabled, "Full pipeline menu must be disabled until Phase 2/3.");
        Assert(!openPipelineDiagnosticsMenuItem.IsEnabled, "Pipeline diagnostics must be disabled until pipeline traces exist.");
        Assert(!openEvidenceFolderMenuItem.IsEnabled, "Open Evidence Folder must be disabled until evidence folder management exists.");
        Assert(diagnosticsColumnSplitter.ResizeDirection == GridResizeDirection.Columns, "Diagnostics splitter should resize columns.");
        Assert(Grid.GetColumn(diagnosticsColumnSplitter) == 2, "Diagnostics splitter should sit between preview and diagnostics columns.");
        Assert(logsAlertsSplitter.ResizeDirection == GridResizeDirection.Rows, "Logs/Alerts splitter should resize rows.");
        Assert(Grid.GetRow(logsAlertsSplitter) == 1, "Logs/Alerts splitter should sit between Logs and Alerts rows.");
        Assert(displaySettingsGroup.Visibility == Visibility.Visible, "Display settings panel should be visible by default.");
        Assert(offsetCorrectionModeComboBox.Items.Count == 3, "Offset mode selector should expose Auto, On, and Off.");
        Assert(gainCorrectionModeComboBox.Items.Count == 3, "Gain mode selector should expose Auto, On, and Off.");
        Assert(defectCorrectionModeComboBox.Items.Count == 3, "Defect mode selector should expose Auto, On, and Off.");
        Assert(ghostCorrectionModeComboBox.Items.Count == 3, "Ghost mode selector should expose Auto, On, and Off.");
        Assert(temperatureCompensationModeComboBox.Items.Count == 3, "Temperature mode selector should expose Auto, On, and Off.");
        Assert(nonlinearityCorrectionModeComboBox.Items.Count == 3, "Nonlinearity mode selector should expose Auto, On, and Off.");
        Assert(binningCorrectionModeComboBox.Items.Count == 3, "Binning mode selector should expose Auto, On, and Off.");
        Assert(calibrationEvaluationSummaryText.Text.Contains("Offset=Auto", StringComparison.Ordinal), "Calibration summary should include Offset Auto mode.");
        Assert(voiLutModeComboBox.Items.Count >= 3, "VOI mode selector should expose Linear, LinearExact, and Sigmoid.");
        Assert(bodyPartPresetComboBox.Items.Count >= 4, "Body part selector should expose four native presets.");
        Assert(applyBodyPartPresetButton.Command is not null, "Apply Body Part Preset command missing.");
        Assert(applyDisplayPipelineButton.Command is not null, "Apply Display Pipeline button command missing.");
        Assert(comparisonModeComboBox.Items.Count == 7, "Comparison mode selector should expose all approved modes.");
        Assert(Math.Abs(comparisonOverlayOpacitySlider.Value - 0.5) < 0.001, "Overlay opacity should default to 50%.");
        Assert(comparisonViewport.CompareMode == "SwipeVertical", "Comparison viewport should default to vertical swipe.");
        Assert(zoomFitMenuItem.Command is not null, "Zoom Fit menu command missing.");
        Assert(zoomActualMenuItem.Command is not null, "Zoom 100% menu command missing.");
        Assert(zoomInButton.Command is not null, "Zoom In button command missing.");
        Assert(zoomOutButton.Command is not null, "Zoom Out button command missing.");
        Assert(detachComparisonViewerButton.Command is not null, "Detach Comparison Viewer command missing.");
        Assert(comparisonStatusText.Text.Contains("SwipeVertical", StringComparison.Ordinal), "Comparison status should include the current mode.");

        Assert(versionText.Text == "v0.0.0-mock", "Mock version text mismatch.");
        Assert(displayVersionText.Text == "v0.0.0-mock-display", "Mock display version text mismatch.");
        Assert(displayPipelineSummaryText.Text.Contains("not run", StringComparison.OrdinalIgnoreCase), "Display summary should start in not-run state.");
        Assert(logsList.Items.Count >= 5, "Logs list should have at least 5 items.");
        Assert(alertsList.Items.Count == 3, "Alerts list should have 3 items.");

        offsetCorrectionModeComboBox.SelectedItem = "Off";
        defectCorrectionModeComboBox.SelectedItem = "On";
        DoEvents();
        Assert(calibrationEvaluationSummaryText.Text.Contains("Offset=Off", StringComparison.Ordinal), "Calibration summary should update when Offset is bypassed.");
        Assert(calibrationEvaluationSummaryText.Text.Contains("Defect=On", StringComparison.Ordinal), "Calibration summary should update when Defect is forced on.");

        ExecuteMenuItem(quickStartMenuItem);
        DoEvents();

        var helpWindow = application.Windows.OfType<HelpWindow>().FirstOrDefault();
        if (helpWindow is null)
        {
            throw new InvalidOperationException("Quick Start help window should open.");
        }

        Assert(helpWindow.HelpTitle.Contains("Quick Start", StringComparison.Ordinal), "Quick Start help window title mismatch.");
        Assert(helpWindow.DocumentLoaded, "Quick Start help document should load.");
        Assert(helpWindow.CurrentDocumentPath.EndsWith("quick-start.html", StringComparison.OrdinalIgnoreCase), "Quick Start help path mismatch.");
        helpWindow.Close();
        DoEvents();

        ExecuteMenuItem(scopeMenuItem);
        DoEvents();

        helpWindow = application.Windows.OfType<HelpWindow>().FirstOrDefault();
        if (helpWindow is null)
        {
            throw new InvalidOperationException("Scope help window should open.");
        }

        Assert(helpWindow.HelpTitle.Contains("Scope", StringComparison.Ordinal), "Scope help window title mismatch.");
        Assert(helpWindow.DocumentLoaded, "Scope help document should load.");
        Assert(helpWindow.CurrentDocumentPath.EndsWith("scope.html", StringComparison.OrdinalIgnoreCase), "Scope help path mismatch.");
        helpWindow.Close();
        DoEvents();

        ExecuteMenuItem(currentWorkflowHelpMenuItem);
        DoEvents();

        helpWindow = application.Windows.OfType<HelpWindow>().FirstOrDefault();
        if (helpWindow is null)
        {
            throw new InvalidOperationException("Current Workflow help window should open.");
        }

        Assert(helpWindow.HelpTitle.Contains("Quick Start", StringComparison.Ordinal), "Current Workflow help should route to Quick Start in GUI-S0.");
        Assert(helpWindow.DocumentLoaded, "Current Workflow help document should load.");
        helpWindow.Close();
        DoEvents();

        ExecuteCommand(clearLogsMenuItem.Command, "Clear logs menu command missing.");
        ExecuteCommand(clearAlertsMenuItem.Command, "Clear alerts menu command missing.");
        DoEvents();

        Assert(logsList.Items.Count == 0, "Logs list should be cleared.");
        Assert(alertsList.Items.Count == 0, "Alerts list should be cleared.");

        if (window.DataContext is not MainWindowViewModel viewModel)
        {
            throw new InvalidOperationException("Main window DataContext should be MainWindowViewModel.");
        }

        ExecuteCommand(zoomActualMenuItem.Command, "Zoom 100% command missing.");
        DoEvents();
        Assert(Math.Abs(viewModel.Settings.ComparisonZoomScale - 1.0) < 0.001, "Zoom 100% command should set absolute scale.");
        ExecuteCommand(zoomFitMenuItem.Command, "Zoom Fit command missing.");
        DoEvents();
        Assert(Math.Abs(viewModel.Settings.ComparisonZoomScale) < 0.001, "Zoom Fit command should restore fit mode.");

        ExecuteCommand(shutdownButton.Command, "Shutdown button command missing before shutdown invoke.");
        DoEvents();
        Assert(viewModel.RuntimeInfo.State == "Shutdown", "Runtime state should switch to Shutdown.");
    }
    finally
    {
        window.Close();
        application.Shutdown();
    }
}

static T GetControl<T>(FrameworkElement root, string name) where T : class
{
    return root.FindName(name) as T
           ?? throw new InvalidOperationException($"Control '{name}' was not found.");
}

static void DoEvents()
{
    var frame = new DispatcherFrame();
    Dispatcher.CurrentDispatcher.BeginInvoke(DispatcherPriority.Background, new DispatcherOperationCallback(_ =>
    {
        frame.Continue = false;
        return null;
    }), null);
    Dispatcher.PushFrame(frame);
}

static void ExecuteCommand(ICommand? command, string missingCommandMessage)
{
    if (command is null)
    {
        throw new InvalidOperationException(missingCommandMessage);
    }

    command.Execute(null);
}

static void ExecuteMenuItem(MenuItem menuItem)
{
    menuItem.RaiseEvent(new RoutedEventArgs(MenuItem.ClickEvent, menuItem));
}

static string FindRepoRoot(string startDirectory)
{
    var current = new DirectoryInfo(startDirectory);
    while (current is not null)
    {
        if (File.Exists(Path.Combine(current.FullName, "docs", "project", "sprint-plan.md")))
        {
            return current.FullName;
        }

        current = current.Parent;
    }

    throw new InvalidOperationException("Repository root not found.");
}

static void Assert(bool condition, string message)
{
    if (!condition)
    {
        throw new InvalidOperationException(message);
    }
}
