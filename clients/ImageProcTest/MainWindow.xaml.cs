using System;
using System.Diagnostics;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Threading;
using Microsoft.Win32;

namespace ImageProcTest
{
    internal sealed record EvaluationMetricRow(string Area, string Metric, string Value, string Gate);

    internal sealed record StageLatencyRow(string Stage, string Status, bool Executed, string LatencyMs, string Details);

    internal sealed record CalibrationLatencyRow(string Stage, string Status, bool Loaded, string LatencyMs, string Expiry, string Source);

    internal sealed record ReportArtifactRow(string Kind, string Path);

    public partial class MainWindow : Window
    {
        private readonly IXpeBackend backend = new CompositeXpeBackend(
            new RealXpeCommonBackend(),
            new MockXpeBackend());

        private RawPreviewResult? currentPreview;
        private BackendHealthResult? lastBackendHealth;
        private string? lastReadinessReportPath;
        private PreprocessHealthResult? lastPreprocessHealth;
        private NativePreprocessPreviewResult? lastNativePreviewResult;
        private IReadOnlyList<ModuleReadinessSnapshot> currentModuleReadiness = [];
        private IReadOnlyList<AlgorithmValidationItem> currentAlgorithmValidation = [];
        private readonly List<ReportArtifactRow> reportArtifacts = [];
        private AlgorithmValidationRunSnapshot? lastAlgorithmValidationRun;
        private UserEvaluationSnapshot? lastUserEvaluation;
        private FixtureCaseInfo? currentCalibrationContext;
        private string? currentCalibrationFolderPath;
        private bool hasInitializedNativeStageDefaults;
        private bool isDraggingComparisonSwipe;

        public MainWindow()
        {
            InitializeComponent();
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            MoveComparisonViewerToEvaluation();
            RefreshNativeHealth();
            LoadFixtureCases();
            RefreshModuleReadiness();
            UpdateEvaluationDashboards();
        }

        private void Window_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.F1)
            {
                SelectTab("Help");
                e.Handled = true;
                return;
            }

            if ((Keyboard.Modifiers & ModifierKeys.Control) == ModifierKeys.Control)
            {
                if (e.Key == Key.O)
                {
                    WorkflowBrowseButton_Click(sender, e);
                    e.Handled = true;
                }
                else if (e.Key == Key.S)
                {
                    SaveE2eReportButton_Click(sender, e);
                    e.Handled = true;
                }
            }
        }

        private void MenuOpenRaw_Click(object sender, RoutedEventArgs e)
        {
            WorkflowBrowseButton_Click(sender, e);
        }

        private void MenuSaveEvidenceReport_Click(object sender, RoutedEventArgs e)
        {
            SaveE2eReportButton_Click(sender, e);
        }

        private void MenuExit_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        private void MenuRefreshNativeDiagnostics_Click(object sender, RoutedEventArgs e)
        {
            RefreshNativeHealth();
        }

        private void MenuRunSmokeTest_Click(object sender, RoutedEventArgs e)
        {
            SelectTab("Diagnostics");
            RefreshNativeHealth();
        }

        private void MenuShowEvaluation_Click(object sender, RoutedEventArgs e)
        {
            SelectTab("Evaluation");
        }

        private void MenuShowCalibration_Click(object sender, RoutedEventArgs e)
        {
            SelectTab("Calibration");
        }

        private void MenuShowMetrics_Click(object sender, RoutedEventArgs e)
        {
            SelectTab("Metrics");
        }

        private void MenuShowReports_Click(object sender, RoutedEventArgs e)
        {
            SelectTab("Reports");
        }

        private void MenuShowDiagnostics_Click(object sender, RoutedEventArgs e)
        {
            SelectTab("Diagnostics");
        }

        private void MenuZoomFit_Click(object sender, RoutedEventArgs e)
        {
            FitComparisonToViewport();
        }

        private void MenuZoomActual_Click(object sender, RoutedEventArgs e)
        {
            SetComparisonZoom(1.0);
        }

        private void MenuZoomIn_Click(object sender, RoutedEventArgs e)
        {
            AdjustComparisonZoom(1.25);
        }

        private void MenuZoomOut_Click(object sender, RoutedEventArgs e)
        {
            AdjustComparisonZoom(0.8);
        }

        private void MenuResetLayout_Click(object sender, RoutedEventArgs e)
        {
            SelectTab("Evaluation");
            CompareSwipeSlider.Value = 0.5;
            FitComparisonToViewport();
        }

        private void MenuOpenEvidenceFolder_Click(object sender, RoutedEventArgs e)
        {
            var directory = Path.Combine(AppContext.BaseDirectory, "preprocess-gui-reports");
            Directory.CreateDirectory(directory);
            Process.Start(new ProcessStartInfo
            {
                FileName = directory,
                UseShellExecute = true
            });
        }

        private void MenuHelpHome_Click(object sender, RoutedEventArgs e)
        {
            SelectTab("Help");
        }

        private void SelectTab(string header)
        {
            foreach (var item in MainTabControl.Items.OfType<TabItem>())
            {
                if (string.Equals(item.Header?.ToString(), header, StringComparison.OrdinalIgnoreCase))
                {
                    MainTabControl.SelectedItem = item;
                    return;
                }
            }
        }

        private void MoveComparisonViewerToEvaluation()
        {
            if (EvaluationComparisonHost.Content == ComparisonViewerPanel)
            {
                return;
            }

            if (ComparisonViewerPanel.Parent is Panel panel)
            {
                panel.Children.Remove(ComparisonViewerPanel);
            }

            EvaluationComparisonHost.Content = ComparisonViewerPanel;
        }

        private void WorkflowBrowseButton_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new OpenFileDialog
            {
                Title = "Select Target Raw Image",
                Filter = "Raw image files (*.raw)|*.raw",
                Multiselect = false,
            };

            if (dialog.ShowDialog() == true)
            {
                WorkflowFilePathText.Text = dialog.FileName;
                LoadRawPreview(dialog.FileName);
                UpdateWorkflowRunState();
            }
        }

        private void WorkflowBrowseCalibrationFolderButton_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new OpenFolderDialog
            {
                Title = "Select Calibration Folder",
                Multiselect = false
            };

            if (dialog.ShowDialog() == true)
            {
                LoadCalibrationWorkspace(dialog.FolderName);
            }
        }

        private void WorkflowRefreshCalibrationFolderButton_Click(object sender, RoutedEventArgs e)
        {
            if (string.IsNullOrWhiteSpace(currentCalibrationFolderPath))
            {
                WorkflowCalibrationContextText.Text = "Calibration context: select the acquired calibration folder first.";
                UpdateWorkflowRunState();
                return;
            }

            LoadCalibrationWorkspace(currentCalibrationFolderPath);
        }

        private void LoadCalibrationWorkspace(string selectedPath)
        {
            try
            {
                var context = FixtureCatalogService.LoadCalibrationFolder(selectedPath);
                SetActiveCalibrationContext(context, selectedPath);
            }
            catch (Exception ex)
            {
                currentCalibrationContext = null;
                currentCalibrationFolderPath = null;
                WorkflowCalibrationFolderText.Text = selectedPath;
                WorkflowCalibrationFilesListBox.ItemsSource = null;
                WorkflowCalibrationContextText.Text = $"Calibration context: folder scan failed: {ex.Message}";
                SetStatus("Calibration folder scan failed", Brushes.OrangeRed);
                UpdateWorkflowRunState();
                UpdateEvaluationDashboards();
            }
        }

        private void WorkflowRunButton_Click(object sender, RoutedEventArgs e)
        {
            if (WorkflowAlgorithmComboBox.SelectedItem is not AlgorithmValidationItem item)
            {
                WorkflowBeforeAfterText.Text = "Select a calibration SWU before running evaluation.";
                return;
            }

            WorkflowRunButton.IsEnabled = false;
            WorkflowCancelButton.IsEnabled = true;
            try
            {
                var run = RunAlgorithmValidation(item);
                WorkflowBeforeAfterText.Text =
                    $"Calibration SWU: {run.SwuId} {run.AlgorithmName}{Environment.NewLine}" +
                    $"Result: {run.Status}{Environment.NewLine}" +
                    $"Latency: {(run.LatencyMs.HasValue ? $"{run.LatencyMs.Value:0.###} ms" : "n/a")}{Environment.NewLine}" +
                    $"Artifacts: {run.ArtifactDirectory ?? "none"}{Environment.NewLine}" +
                    $"Details: {run.Details}";
            }
            finally
            {
                UpdateWorkflowRunState();
                WorkflowCancelButton.IsEnabled = false;
            }

        }
        private void WorkflowCancelButton_Click(object sender, RoutedEventArgs e)
        {
            WorkflowCancelButton.IsEnabled = false;
            UpdateWorkflowRunState();
            SetStatus("Cancelled", System.Windows.Media.Brushes.Gray);
        }

        private void RefreshButton_Click(object sender, RoutedEventArgs e)
        {
            RefreshNativeHealth();
        }

        private void RefreshFixturesButton_Click(object sender, RoutedEventArgs e)
        {
            LoadFixtureCases();
        }

        private void RefreshModulesButton_Click(object sender, RoutedEventArgs e)
        {
            RefreshModuleReadiness();
        }

        private void RefreshAlgorithmsButton_Click(object sender, RoutedEventArgs e)
        {
            RefreshAlgorithmValidation();
        }

        private void WorkflowAlgorithmComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (WorkflowAlgorithmComboBox.SelectedItem is not AlgorithmValidationItem item)
            {
                WorkflowAlgorithmStatusText.Text = "Calibration evaluation: select a SWU.";
                return;
            }

            WorkflowAlgorithmStatusText.Text =
                $"{item.SwuId} {item.AlgorithmName}; module={item.ModuleName}; status={item.Status}; " +
                $"requirements={item.RequirementIds}; tests={item.TestIds}; run={item.CanRun}";
            UpdateWorkflowRunState();
        }

        private void AlgorithmValidationGrid_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (AlgorithmValidationGrid.SelectedItem is not AlgorithmValidationItem item)
            {
                AlgorithmValidationResultText.Text = "Calibration validation: no row selected.";
                return;
            }

            AlgorithmValidationResultText.Text =
                $"Selected {item.SwuId} {item.AlgorithmName}; status={item.Status}; run={item.CanRun}; next={item.NextAction}";
        }

        private void RunSelectedAlgorithmValidationButton_Click(object sender, RoutedEventArgs e)
        {
            if (AlgorithmValidationGrid.SelectedItem is not AlgorithmValidationItem item)
            {
                AlgorithmValidationResultText.Text = "Calibration validation: select a calibration SWU row first.";
                return;
            }

            if (!item.CanRun || string.IsNullOrWhiteSpace(item.StageKey))
            {
                lastAlgorithmValidationRun = new AlgorithmValidationRunSnapshot(
                    item.SwuId,
                    item.AlgorithmName,
                    "Blocked",
                    item.NextAction,
                    ArtifactDirectory: null,
                    LatencyMs: null);
                AlgorithmValidationResultText.Text =
                    $"Calibration validation: blocked for {item.SwuId}; {item.NextAction}";
                UpdateEvaluationDashboards();
                return;
            }

            if (string.Equals(item.StageKey, "calib-folder", StringComparison.OrdinalIgnoreCase))
            {
                var folderAudit = RunCalibrationFolderAudit(item);
                AlgorithmValidationResultText.Text =
                    $"Calibration validation: {folderAudit.Status} {item.SwuId}; {folderAudit.Details}";
                return;
            }

            if (currentPreview is null)
            {
                AlgorithmValidationResultText.Text = "Calibration validation: load the target raw image first.";
                return;
            }

            if (currentCalibrationContext is not FixtureCaseInfo selectedCase)
            {
                AlgorithmValidationResultText.Text = "Calibration validation: select the acquired calibration folder first.";
                return;
            }

            try
            {
                var selection = CreateCalibrationValidationSelection(item.StageKey);
                var result = RunNativePreprocessPreview(selection, selectedCase, $"{item.SwuId} {item.AlgorithmName}");
                lastAlgorithmValidationRun = new AlgorithmValidationRunSnapshot(
                    item.SwuId,
                    item.AlgorithmName,
                    "Pass",
                    FormatMetricSummary(result.Metrics),
                    result.ArtifactDirectory,
                    result.TotalLatencyMs);
                AlgorithmValidationResultText.Text =
                    $"Calibration validation: PASS {item.SwuId}; latency={result.TotalLatencyMs:0.###}ms; " +
                    $"artifacts={result.ArtifactDirectory}; {FormatMetricSummary(result.Metrics)}";
            }
            catch (Exception ex)
            {
                lastAlgorithmValidationRun = new AlgorithmValidationRunSnapshot(
                    item.SwuId,
                    item.AlgorithmName,
                    "Fail",
                    ex.Message,
                    ArtifactDirectory: null,
                    LatencyMs: null);
                AlgorithmValidationResultText.Text = $"Calibration validation: FAIL {item.SwuId}; {ex.Message}";
                SetStatus("Calibration validation failed", Brushes.OrangeRed);
                UpdateEvaluationDashboards();
            }
        }

        private AlgorithmValidationRunSnapshot RunAlgorithmValidation(AlgorithmValidationItem item)
        {
            if (!item.CanRun || string.IsNullOrWhiteSpace(item.StageKey))
            {
                var blocked = new AlgorithmValidationRunSnapshot(
                    item.SwuId,
                    item.AlgorithmName,
                    "Blocked",
                    item.NextAction,
                    ArtifactDirectory: null,
                    LatencyMs: null);
                lastAlgorithmValidationRun = blocked;
                SetStatus("Calibration validation blocked", Brushes.OrangeRed);
                UpdateEvaluationDashboards();
                return blocked;
            }

            if (string.Equals(item.StageKey, "calib-folder", StringComparison.OrdinalIgnoreCase))
            {
                return RunCalibrationFolderAudit(item);
            }

            if (currentPreview is null)
            {
                var blocked = new AlgorithmValidationRunSnapshot(
                    item.SwuId,
                    item.AlgorithmName,
                "Blocked",
                    "Load the target raw image first.",
                    ArtifactDirectory: null,
                    LatencyMs: null);
                lastAlgorithmValidationRun = blocked;
                UpdateEvaluationDashboards();
                return blocked;
            }

            if (currentCalibrationContext is not FixtureCaseInfo selectedCase)
            {
                var blocked = new AlgorithmValidationRunSnapshot(
                    item.SwuId,
                    item.AlgorithmName,
                "Blocked",
                    "Select the acquired calibration folder first.",
                    ArtifactDirectory: null,
                    LatencyMs: null);
                lastAlgorithmValidationRun = blocked;
                UpdateEvaluationDashboards();
                return blocked;
            }

            try
            {
                var selection = CreateCalibrationValidationSelection(item.StageKey);
                var result = RunNativePreprocessPreview(selection, selectedCase, $"{item.SwuId} {item.AlgorithmName}");
                var pass = new AlgorithmValidationRunSnapshot(
                    item.SwuId,
                    item.AlgorithmName,
                    "Pass",
                    FormatMetricSummary(result.Metrics),
                    result.ArtifactDirectory,
                    result.TotalLatencyMs);
                lastAlgorithmValidationRun = pass;
                return pass;
            }
            catch (Exception ex)
            {
                var fail = new AlgorithmValidationRunSnapshot(
                    item.SwuId,
                    item.AlgorithmName,
                    "Fail",
                    ex.Message,
                    ArtifactDirectory: null,
                    LatencyMs: null);
                lastAlgorithmValidationRun = fail;
                SetStatus("Calibration validation failed", Brushes.OrangeRed);
                UpdateEvaluationDashboards();
                return fail;
            }
        }

        private void RecordUserEvaluationButton_Click(object sender, RoutedEventArgs e)
        {
            var selected = WorkflowAlgorithmComboBox.SelectedItem as AlgorithmValidationItem ??
                AlgorithmValidationGrid.SelectedItem as AlgorithmValidationItem;
            var algorithmKey = selected is null
                ? "none"
                : $"{selected.SwuId} {selected.AlgorithmName}";
            var verdict = UserFailRadio.IsChecked == true
                ? "Fail"
                : UserReviewRadio.IsChecked == true
                    ? "Needs review"
                    : "Pass";
            var evaluator = string.IsNullOrWhiteSpace(EvaluatorTextBox.Text)
                ? Environment.UserName
                : EvaluatorTextBox.Text.Trim();
            var notes = UserEvaluationNotesTextBox.Text.Trim();
            var evidence = lastAlgorithmValidationRun is null
                ? "No calibration run recorded in this session."
                : $"{lastAlgorithmValidationRun.Status}; {lastAlgorithmValidationRun.Details}";

            lastUserEvaluation = new UserEvaluationSnapshot(
                algorithmKey,
                evaluator,
                verdict,
                notes,
                evidence);
            UserEvaluationStatusText.Text =
                $"User evaluation: {verdict} by {evaluator}; calibration={algorithmKey}; evidence={evidence}";
            UpdateEvaluationDashboards();
        }

        private void FixtureCaseComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (FixtureCaseComboBox.SelectedItem is not FixtureCaseInfo selectedCase)
            {
                ImageFilesListBox.ItemsSource = null;
                CalibrationFilesListBox.ItemsSource = null;
                SelectedCaseText.Text = "Selected case: none";
                WorkflowCalibrationContextText.Text = currentCalibrationContext is null
                    ? "Calibration context: no folder selected."
                    : BuildCalibrationContextStatus(currentCalibrationContext);
                UpdateEvaluationDashboards();
                return;
            }

            ImageFilesListBox.ItemsSource = selectedCase.Images;
            CalibrationFilesListBox.ItemsSource = selectedCase.CalibrationFiles;
            SelectedCaseText.Text =
                $"Selected case: {selectedCase.Name}; calibration roles: {selectedCase.CalibrationSummary}; root={selectedCase.RootPath}";
            RawPreviewInfoText.Text = $"Case: {selectedCase.RootPath}";
            ProcessingScaffoldText.Text = "Preprocessing test: select an image in this case, load it, then run the native fixture-calibrated chain.";

            SetActiveCalibrationContext(selectedCase, selectedCase.CalibrationDirectoryPath);
            UpdateNativePreviewControls();
            UpdateEvaluationDashboards();
        }

        private void ImageFilesListBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (ImageFilesListBox.SelectedItem is not RawFileDescriptor raw)
            {
                return;
            }

            RawPreviewTitleText.Text = $"Raw preview: {raw.Name}";
            RawPreviewInfoText.Text = $"Selected: {raw.Path} ({RawFileDescriptor.FormatBytes(raw.Length)})";
            RawPreviewHashText.Text = "SHA-256: not calculated until preview load";
            WorkflowFilePathText.Text = raw.Path;
            UpdateWorkflowRunState();
            UpdateEvaluationDashboards();
        }

        private void LoadSelectedRawButton_Click(object sender, RoutedEventArgs e)
        {
            if (ImageFilesListBox.SelectedItem is not RawFileDescriptor raw)
            {
                RawPreviewInfoText.Text = "Select a raw image before loading.";
                return;
            }

            WorkflowFilePathText.Text = raw.Path;
            LoadRawPreview(raw.Path);
            UpdateWorkflowRunState();
        }

        private void SetActiveCalibrationContext(FixtureCaseInfo selectedCase, string selectedPath)
        {
            currentCalibrationContext = selectedCase;
            currentCalibrationFolderPath = selectedPath;
            WorkflowCalibrationFolderText.Text = selectedCase.CalibrationDirectoryPath;
            WorkflowCalibrationFilesListBox.ItemsSource = selectedCase.CalibrationFiles;
            WorkflowCalibrationContextText.Text = BuildCalibrationContextStatus(selectedCase);
            UpdateWorkflowRunState();
        }

        private void RawZoomSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (RawImageScaleTransform is null || ZoomValueText is null)
            {
                return;
            }

            var scale = Math.Max(0.1, e.NewValue);
            RawImageScaleTransform.ScaleX = scale;
            RawImageScaleTransform.ScaleY = scale;
            ZoomValueText.Text = $"{scale * 100:0}%";
        }

        private void ZoomFitButton_Click(object sender, RoutedEventArgs e)
        {
            FitComparisonToViewport();
        }

        private void ZoomActualButton_Click(object sender, RoutedEventArgs e)
        {
            SetComparisonZoom(1.0);
        }

        private void ZoomInButton_Click(object sender, RoutedEventArgs e)
        {
            AdjustComparisonZoom(1.25);
        }

        private void ZoomOutButton_Click(object sender, RoutedEventArgs e)
        {
            AdjustComparisonZoom(0.8);
        }

        private void CompareSwipeSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            UpdateComparisonClip();
        }

        private void ComparisonCanvas_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            UpdateComparisonClip();
        }

        private void ComparisonCanvas_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            isDraggingComparisonSwipe = true;
            ComparisonCanvas.CaptureMouse();
            UpdateComparisonSwipeFromPoint(e.GetPosition(ComparisonCanvas).X);
            e.Handled = true;
        }

        private void ComparisonCanvas_MouseMove(object sender, MouseEventArgs e)
        {
            if (!isDraggingComparisonSwipe || e.LeftButton != MouseButtonState.Pressed)
            {
                return;
            }

            UpdateComparisonSwipeFromPoint(e.GetPosition(ComparisonCanvas).X);
            e.Handled = true;
        }

        private void ComparisonCanvas_MouseLeftButtonUp(object sender, MouseButtonEventArgs e)
        {
            if (!isDraggingComparisonSwipe)
            {
                return;
            }

            UpdateComparisonSwipeFromPoint(e.GetPosition(ComparisonCanvas).X);
            EndComparisonSwipeDrag();
            e.Handled = true;
        }

        private void ComparisonCanvas_MouseLeave(object sender, MouseEventArgs e)
        {
            if (isDraggingComparisonSwipe && e.LeftButton != MouseButtonState.Pressed)
            {
                EndComparisonSwipeDrag();
            }
        }

        private void ComparisonCanvas_MouseWheel(object sender, MouseWheelEventArgs e)
        {
            AdjustComparisonZoom(e.Delta > 0 ? 1.1 : 1.0 / 1.1);
            e.Handled = true;
        }

        private void SaveE2eReportButton_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                var selectedCase = currentCalibrationContext ?? FixtureCaseComboBox.SelectedItem as FixtureCaseInfo;
                var selectedRaw = ImageFilesListBox.SelectedItem as RawFileDescriptor;
                if (selectedRaw is null && currentPreview is not null)
                {
                    selectedRaw = new RawFileDescriptor(currentPreview.FilePath);
                }

                var report = GuiE2eReportService.WriteReport(
                    selectedCase,
                    selectedRaw,
                    currentPreview,
                    lastBackendHealth,
                    lastReadinessReportPath,
                    lastPreprocessHealth,
                    lastNativePreviewResult,
                    GetStageModes(),
                    currentModuleReadiness,
                    currentAlgorithmValidation,
                    lastAlgorithmValidationRun,
                    lastUserEvaluation);

                E2eReportText.Text = $"E2E report: {report.JsonPath}";
                AddReportArtifact("GUI report JSON", report.JsonPath);
                AddReportArtifact("GUI report Markdown", report.MarkdownPath);
                ReportsSummaryText.Text = $"Reports: current GUI report saved at {report.MarkdownPath}";
                UpdateEvaluationDashboards();
            }
            catch (Exception ex)
            {
                E2eReportText.Text = $"E2E report failed: {ex.Message}";
                ReportsSummaryText.Text = $"Reports: current GUI report failed: {ex.Message}";
            }
        }

        private void RefreshNativeHealth()
        {
            SetStatus("Checking native common backend...", Brushes.Goldenrod);
            var result = backend.CheckHealth();
            lastBackendHealth = result;
            VersionText.Text = $"Version: {result.Version}";
            NativePathText.Text = $"DLL: {result.DllPath}";
            InitText.Text = $"Init: {result.Init}";
            VersionCheckText.Text = $"Version: {result.Version}";
            ParamRangeText.Text = result.ParamRange;
            MemoryAbiText.Text = result.MemoryAbi;
            AlertText.Text = result.Alerts;
            DetailsText.Text = result.Details;

            var readinessBrush = result.IsNativeReady ? Brushes.ForestGreen : Brushes.OrangeRed;
            SetStatus(result.Status, readinessBrush);

            try
            {
                var report = NativeReadinessProbe.WriteReport(result);
                lastReadinessReportPath = report.ReportPath;
                lastPreprocessHealth = report.PreprocessHealth;
                DisplayHealthText.Text = $"Display health: {report.DisplaySummary}";
                PreprocessHealthText.Text = $"Preprocess health: {report.PreprocessSummary}";
                PreprocessSmokeText.Text =
                    $"Preprocess smoke: {report.PreprocessHealth.SyntheticOracle.Status}; " +
                    $"pass={report.PreprocessHealth.SyntheticOracle.Passed}; " +
                    $"latency={report.PreprocessHealth.SyntheticOracle.TotalLatencyMs:0.###}ms";
                PreprocessParamRangeText.Text =
                    $"Preprocess parameter ranges: {NativeReadinessProbe.FormatPreprocessParameterRanges(report.PreprocessHealth.ParameterRanges)}";
                PreprocessParamRangeGrid.ItemsSource = report.PreprocessHealth.ParameterRanges;
                ReportText.Text = $"Readiness report: {report.ReportPath}";
            }
            catch (Exception ex)
            {
                DisplayHealthText.Text = "Display health: Report generation skipped";
                PreprocessHealthText.Text = "Preprocess health: Report generation skipped";
                PreprocessSmokeText.Text = "Preprocess smoke: Report generation skipped";
                PreprocessParamRangeText.Text = "Preprocess parameter ranges: Report generation skipped";
                PreprocessParamRangeGrid.ItemsSource = null;
                ReportText.Text = $"Readiness report: failed ({ex.Message})";
                lastReadinessReportPath = null;
                lastPreprocessHealth = null;
            }

            UpdateNativePreviewControls();
            RefreshModuleReadiness();
        }

        private void Window_Closed(object? sender, System.EventArgs e)
        {
            backend.Shutdown();
        }

        private void LoadFixtureCases()
        {
            try
            {
                var cases = FixtureCatalogService.LoadCases();
                FixtureCaseComboBox.ItemsSource = cases;

                if (cases.Count == 0)
                {
                    ImageFilesListBox.ItemsSource = null;
                    CalibrationFilesListBox.ItemsSource = null;
                    SelectedCaseText.Text = "Sample data: none";
                    UpdateEvaluationDashboards();
                    return;
                }

                FixtureCaseComboBox.SelectedIndex = -1;
                SelectedCaseText.Text = $"Sample data available for internal regression: {cases.Count} folder(s).";
                UpdateEvaluationDashboards();
            }
            catch (Exception ex)
            {
                SelectedCaseText.Text = $"Sample data scan failed: {ex.Message}";
                UpdateEvaluationDashboards();
            }
        }

        private void LoadRawPreview(string path)
        {
            try
            {
                RawPreviewTitleText.Text = $"Raw preview: {Path.GetFileName(path)}";
                RawPreviewInfoText.Text = "Loading preview and SHA-256...";
                RawPreviewHashText.Text = "SHA-256: calculating";
                BeforePreviewImage.Source = null;
                AfterPreviewImage.Source = null;
                currentPreview = null;
                lastNativePreviewResult = null;
                RawZoomSlider.Value = 1;
                CompareSwipeSlider.Value = 0.5;
                AfterPreviewLabelText.Text = "After preview";

                var preview = RawPreviewService.LoadUInt16Preview(path);
                currentPreview = preview;
                ComparisonCanvas.Width = preview.PreviewWidth;
                ComparisonCanvas.Height = preview.PreviewHeight;
                BeforePreviewImage.Source = preview.Bitmap;
                AfterPreviewImage.Source = preview.Bitmap;
                RawPreviewInfoText.Text =
                    $"Source={preview.Width}x{preview.Height} uint16, file={RawFileDescriptor.FormatBytes(preview.FileSizeBytes)}, " +
                    $"preview={preview.PreviewWidth}x{preview.PreviewHeight}, stride={preview.SampleStride}, " +
                    $"min={preview.MinValue}, max={preview.MaxValue}";
                RawPreviewHashText.Text = $"SHA-256: {preview.Sha256}";
                ProcessingScaffoldText.Text = IsNativePreviewReady()
                    ? "Preprocessing test: Before=original. Select Offset/Gain/Defect modes and run the active calibration folder against the target raw to update After."
                    : "Preprocessing test: Before=original, After=identity placeholder. Native correction is disabled until xpe_preprocess.dll exports are available.";
                UpdateNativePreviewControls();
                UpdateComparisonClip();
                Dispatcher.BeginInvoke(new Action(FitComparisonToViewport), DispatcherPriority.Loaded);
                UpdateWorkflowRunState();
                UpdateEvaluationDashboards();
            }
            catch (Exception ex)
            {
                BeforePreviewImage.Source = null;
                AfterPreviewImage.Source = null;
                currentPreview = null;
                lastNativePreviewResult = null;
                RawPreviewInfoText.Text = $"Raw preview failed: {ex.Message}";
                RawPreviewHashText.Text = "SHA-256: unavailable";
                NativePreviewText.Text = "Native preview: unavailable";
                UpdateNativePreviewControls();
                UpdateWorkflowRunState();
                UpdateEvaluationDashboards();
            }
        }

        private void ApplyNativePreviewButton_Click(object sender, RoutedEventArgs e)
        {
            if (currentPreview is null)
            {
                NativePreviewText.Text = "Native preview: load a raw file first.";
                return;
            }

            if (!IsNativePreviewReady())
            {
                NativePreviewText.Text = "Native preview: xpe_preprocess.dll export readiness is not available.";
                return;
            }

            if (currentCalibrationContext is not FixtureCaseInfo selectedCase)
            {
                NativePreviewText.Text = "Native preview: select the acquired calibration folder first.";
                return;
            }

            var selection = GetPreprocessSelection();
            if (!selection.HasAnyStage)
            {
                NativePreviewText.Text = "Native preview: select at least one Offset/Gain/Defect stage.";
                return;
            }

            try
            {
                RunNativePreprocessPreview(selection, selectedCase, "native preprocess preview");
            }
            catch (Exception ex)
            {
                lastNativePreviewResult = null;
                NativePreviewText.Text = $"Native preview failed: {ex.Message}";
                SetStatus("Native preprocess preview failed", Brushes.OrangeRed);
                UpdateEvaluationDashboards();
            }
        }

        private NativePreprocessPreviewResult RunNativePreprocessPreview(
            PreprocessStageSelection selection,
            FixtureCaseInfo selectedCase,
            string statusLabel)
        {
            if (currentPreview is null)
            {
                throw new InvalidOperationException("Load a raw preview before running native preprocessing.");
            }

            SetStatus($"Running {statusLabel}...", Brushes.Goldenrod);
            var result = NativePreprocessPreviewService.Run(currentPreview, selection, selectedCase, lastPreprocessHealth?.DllPath);
            lastNativePreviewResult = result;
            AfterPreviewImage.Source = result.Bitmap;
            AfterPreviewLabelText.Text = "Native after";
            NativePreviewText.Text =
                $"Native preview: loads={FormatCalibrationSummary(result.CalibrationLoads)}; " +
                $"stages={FormatStageSummary(result.Stages)}; " +
                $"metrics={FormatMetricSummary(result.Metrics)}; " +
                $"latency={result.TotalLatencyMs:0.###}ms; output={result.OutputMin:0.###}..{result.OutputMax:0.###}";
            ProcessingScaffoldText.Text =
                $"Preprocessing test: calibration was loaded from {selectedCase.CalibrationDirectoryPath}; " +
                $"artifacts={result.ArtifactDirectory}. Before/After uses the target raw display window.";
            SetStatus($"{statusLabel} complete", Brushes.ForestGreen);
            UpdateComparisonClip();
            UpdateEvaluationDashboards();
            return result;
        }

        private IReadOnlyList<StageModeSnapshot> GetStageModes()
        {
            var preprocessReason = IsNativePreviewReady()
                ? "Native preprocess export readiness is available. Auto executes only when the active calibration folder contains that role."
                : "Native preprocess execution is disabled until xpe_preprocess.dll export readiness passes.";

            return
            [
                new StageModeSnapshot("Offset", GetMode(OffsetOffRadio, OffsetOnRadio, OffsetAutoRadio), preprocessReason),
                new StageModeSnapshot("Gain", GetMode(GainOffRadio, GainOnRadio, GainAutoRadio), preprocessReason),
                new StageModeSnapshot("Defect", GetMode(DefectOffRadio, DefectOnRadio, DefectAutoRadio), preprocessReason)
            ];
        }

        private void UpdateWorkflowRunState()
        {
            if (WorkflowRunButton is null)
            {
                return;
            }

            var selected = WorkflowAlgorithmComboBox.SelectedItem as AlgorithmValidationItem;
            var hasCalibrationFolder = currentCalibrationContext is not null;
            var hasTargetRaw = currentPreview is not null;
            var isFolderAudit = string.Equals(selected?.StageKey, "calib-folder", StringComparison.OrdinalIgnoreCase);
            WorkflowRunButton.IsEnabled = selected?.CanRun == true &&
                hasCalibrationFolder &&
                (isFolderAudit || hasTargetRaw);
        }

        private AlgorithmValidationRunSnapshot RunCalibrationFolderAudit(AlgorithmValidationItem item)
        {
            if (currentCalibrationContext is not FixtureCaseInfo context)
            {
                var blocked = new AlgorithmValidationRunSnapshot(
                    item.SwuId,
                    item.AlgorithmName,
                    "Blocked",
                    "Select the acquired calibration folder first.",
                    ArtifactDirectory: null,
                    LatencyMs: null);
                lastAlgorithmValidationRun = blocked;
                UpdateEvaluationDashboards();
                return blocked;
            }

            var hasOffset = HasCalibrationRole(context, CalibrationRole.Offset);
            var hasGain = HasCalibrationRole(context, CalibrationRole.Gain);
            var unknownCount = context.CalibrationFiles.Count(file => file.Role == CalibrationRole.Unknown);
            var status = hasOffset && hasGain ? "Pass" : "Blocked";
            var details = BuildCalibrationContextStatus(context);
            if (unknownCount > 0)
            {
                details += $" Unknown calibration file(s) require role confirmation: {unknownCount}.";
            }

            var run = new AlgorithmValidationRunSnapshot(
                item.SwuId,
                item.AlgorithmName,
                status,
                details,
                ArtifactDirectory: context.CalibrationDirectoryPath,
                LatencyMs: null);
            lastAlgorithmValidationRun = run;
            UpdateEvaluationDashboards();
            return run;
        }

        private static bool HasCalibrationRole(FixtureCaseInfo context, CalibrationRole role)
        {
            return context.CalibrationFiles.Any(file => file.Role == role);
        }

        private static string BuildCalibrationContextStatus(FixtureCaseInfo context)
        {
            var hasOffset = HasCalibrationRole(context, CalibrationRole.Offset);
            var hasGain = HasCalibrationRole(context, CalibrationRole.Gain);
            var hasDefect = HasCalibrationRole(context, CalibrationRole.Defect);
            var hasDefectOracle = HasCalibrationRole(context, CalibrationRole.DefectOracle);
            var unknownCount = context.CalibrationFiles.Count(file => file.Role == CalibrationRole.Unknown);
            var mandatory = hasOffset && hasGain
                ? "mandatory offset/gain present"
                : "blocked: offset/dark and gain/flat are mandatory";
            var roles = string.IsNullOrWhiteSpace(context.CalibrationSummary)
                ? "none"
                : context.CalibrationSummary;

            return $"Calibration context: folder={context.CalibrationDirectoryPath}; files={context.CalibrationFiles.Count}; " +
                $"roles={roles}; {mandatory}; offset={hasOffset}, gain={hasGain}, defect={hasDefect}, oracle={hasDefectOracle}, unknown={unknownCount}.";
        }

        private void RefreshModuleReadiness()
        {
            currentModuleReadiness = ModuleReadinessService.Evaluate(lastBackendHealth);
            ModuleReadinessGrid.ItemsSource = currentModuleReadiness;

            var enabledCount = currentModuleReadiness.Count(module => module.ProcessingEnabled);
            var nativeBlocked = string.Join(", ", currentModuleReadiness
                .Where(module => !module.ProcessingEnabled && module.ModuleName != "xpe_common")
                .Select(module => $"{module.ModuleName}:{module.Level}"));

            ModuleReadinessSummaryText.Text =
                $"Executable modules={enabledCount}; blocked modules={nativeBlocked}. " +
                "Only modules marked Exec can run preview adapters; clinical processing remains gated by formal verification evidence.";
            RefreshAlgorithmValidation();
            UpdateNativePreviewControls();
            UpdateEvaluationDashboards();
        }

        private void RefreshAlgorithmValidation()
        {
            var previousKey = WorkflowAlgorithmComboBox.SelectedItem is AlgorithmValidationItem previous
                ? $"{previous.SwuId}|{previous.AlgorithmName}"
                : null;
            currentAlgorithmValidation = AlgorithmValidationCatalogService.Build(currentModuleReadiness);
            AlgorithmValidationGrid.ItemsSource = currentAlgorithmValidation;
            WorkflowAlgorithmComboBox.ItemsSource = currentAlgorithmValidation;

            var selected = previousKey is null
                ? currentAlgorithmValidation.FirstOrDefault(item => item.CanRun) ?? currentAlgorithmValidation.FirstOrDefault()
                : currentAlgorithmValidation.FirstOrDefault(item => $"{item.SwuId}|{item.AlgorithmName}" == previousKey) ??
                  currentAlgorithmValidation.FirstOrDefault(item => item.CanRun) ??
                  currentAlgorithmValidation.FirstOrDefault();
            WorkflowAlgorithmComboBox.SelectedItem = selected;

            var runnableCount = currentAlgorithmValidation.Count(item => item.CanRun);
            var moduleCount = currentAlgorithmValidation
                .Select(item => item.ModuleName)
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .Count();

            AlgorithmValidationResultText.Text =
                $"Calibration validation: {currentAlgorithmValidation.Count} calibration SWUs across {moduleCount} modules; runnable={runnableCount}.";
        }

        private void UpdateEvaluationDashboards()
        {
            var rows = new List<EvaluationMetricRow>
            {
                new(
                    "Native",
                    "preprocess export readiness",
                    lastPreprocessHealth?.IsExportReady == true ? "ready" : lastPreprocessHealth?.Status ?? "not checked",
                    "R2"),
                new(
                    "Native",
                    "synthetic oracle",
                    lastPreprocessHealth is null
                        ? "not checked"
                        : $"{lastPreprocessHealth.SyntheticOracle.Status}; pass={lastPreprocessHealth.SyntheticOracle.Passed}; latency={lastPreprocessHealth.SyntheticOracle.TotalLatencyMs:0.###} ms",
                    "R3"),
                new(
                    "Readiness",
                    "executable module count",
                    currentModuleReadiness.Count(item => item.ProcessingEnabled).ToString(),
                    "GUI-GATE")
            };

            rows.Add(new EvaluationMetricRow(
                "Calibration",
                "SWU coverage",
                $"{currentAlgorithmValidation.Count} calibration SWUs; runnable={currentAlgorithmValidation.Count(item => item.CanRun)}",
                "SRS-CALIB-001"));

            rows.Add(new EvaluationMetricRow(
                "Calibration",
                "active folder",
                currentCalibrationContext is null
                    ? "not selected"
                    : $"{currentCalibrationContext.CalibrationFiles.Count} file(s); {currentCalibrationContext.CalibrationSummary}; {currentCalibrationContext.CalibrationDirectoryPath}",
                "PRE-E2E-0"));

            if (lastAlgorithmValidationRun is not null)
            {
                rows.Add(new EvaluationMetricRow(
                    "Calibration",
                    "latest SWU validation",
                    $"{lastAlgorithmValidationRun.Status} {lastAlgorithmValidationRun.SwuId}; {lastAlgorithmValidationRun.Details}",
                    "SWU"));
            }

            if (lastUserEvaluation is not null)
            {
                rows.Add(new EvaluationMetricRow(
                    "User Evaluation",
                    lastUserEvaluation.AlgorithmKey,
                    $"{lastUserEvaluation.Verdict} by {lastUserEvaluation.Evaluator}; {lastUserEvaluation.Notes}",
                    "User verdict"));
            }

            if (currentPreview is null)
            {
                rows.Add(new EvaluationMetricRow("Input", "raw preview", "not loaded", "PRE-E2E-0"));
            }
            else
            {
                rows.Add(new EvaluationMetricRow(
                    "Input",
                    "raw image",
                    $"{currentPreview.Width}x{currentPreview.Height}; preview={currentPreview.PreviewWidth}x{currentPreview.PreviewHeight}; stride={currentPreview.SampleStride}",
                    "PRE-E2E-0"));
                rows.Add(new EvaluationMetricRow("Input", "sha256", currentPreview.Sha256, "PRE-E2E-0"));
            }

            if (lastNativePreviewResult is null)
            {
                rows.Add(new EvaluationMetricRow("Preprocess", "native preview", "not run", "PRE-E2E-2"));
            }
            else
            {
                var metrics = lastNativePreviewResult.Metrics;
                rows.Add(new EvaluationMetricRow("Preprocess", "total latency", $"{lastNativePreviewResult.TotalLatencyMs:0.###} ms", "Performance"));
                rows.Add(new EvaluationMetricRow("Preprocess", "throughput", CalculateThroughput(metrics.PixelCount, lastNativePreviewResult.TotalLatencyMs), "Performance"));
                rows.Add(new EvaluationMetricRow("Preprocess", "mean abs delta", metrics.MeanAbsoluteDelta.ToString("0.###"), "Functional"));
                rows.Add(new EvaluationMetricRow("Preprocess", "rmse", metrics.Rmse.ToString("0.###"), "Functional"));
                rows.Add(new EvaluationMetricRow("Preprocess", "max abs delta", metrics.MaxAbsoluteDelta.ToString("0.###"), "Functional"));
                rows.Add(new EvaluationMetricRow(
                    "Preprocess",
                    "changed pixels",
                    $"{metrics.ChangedPixels}/{metrics.PixelCount} ({metrics.ChangedPixelRatio:P2})",
                    "Functional"));
                rows.Add(new EvaluationMetricRow("Preprocess", "NaN/Inf count", metrics.NaNInfCount.ToString(), "Safety"));
                rows.Add(new EvaluationMetricRow(
                    "Preprocess",
                    "output range",
                    $"{lastNativePreviewResult.OutputMin:0.###}..{lastNativePreviewResult.OutputMax:0.###}",
                    "Functional"));
            }

            EvaluationMetricsGrid.ItemsSource = rows;
            StageLatencyGrid.ItemsSource = lastNativePreviewResult?.Stages
                .Select(stage => new StageLatencyRow(
                    stage.Stage,
                    stage.ErrorCode,
                    stage.Executed,
                    $"{stage.LatencyMs:0.###} ms",
                    stage.Details))
                .ToList();
            CalibrationMetricsGrid.ItemsSource = lastNativePreviewResult?.CalibrationLoads
                .Select(load => new CalibrationLatencyRow(
                    load.Stage,
                    load.Status,
                    load.Loaded,
                    $"{load.LatencyMs:0.###} ms",
                    FormatCalibrationExpiry(load.Expiry),
                    load.SourceRawPath ?? "none"))
                .ToList();
            var detectorMetrics = lastNativePreviewResult?.DetectorMetrics ??
                MetricsComputationService.Empty("native calibration preview not run");
            DarkMetricsGrid.ItemsSource = detectorMetrics.DarkMetrics;
            FlatMetricsGrid.ItemsSource = detectorMetrics.FlatMetrics;
            DefectMetricsGrid.ItemsSource = detectorMetrics.DefectMetrics;
            ReportArtifactsGrid.ItemsSource = reportArtifacts.ToList();

            var previewState = currentPreview is null ? "no raw loaded" : $"{currentPreview.PreviewWidth}x{currentPreview.PreviewHeight} preview";
            var nativeState = lastNativePreviewResult is null
                ? "native preview not run"
                : $"native preview {lastNativePreviewResult.TotalLatencyMs:0.###} ms";
            MetricsSummaryText.Text = $"Metrics: {previewState}; {nativeState}.";
            if (reportArtifacts.Count > 0 && ReportsSummaryText.Text.Contains("no report", StringComparison.OrdinalIgnoreCase))
            {
                ReportsSummaryText.Text = $"Reports: {reportArtifacts.Count} artifact(s) generated in this session.";
            }
        }

        private void AddReportArtifact(string kind, string path)
        {
            if (string.IsNullOrWhiteSpace(path))
            {
                return;
            }

            if (reportArtifacts.Any(item =>
                    string.Equals(item.Kind, kind, StringComparison.OrdinalIgnoreCase) &&
                    string.Equals(item.Path, path, StringComparison.OrdinalIgnoreCase)))
            {
                return;
            }

            reportArtifacts.Insert(0, new ReportArtifactRow(kind, path));
        }

        private static string CalculateThroughput(int pixelCount, double latencyMs)
        {
            if (pixelCount <= 0 || latencyMs <= 0)
            {
                return "n/a";
            }

            var megapixelsPerSecond = pixelCount / 1_000_000.0 / (latencyMs / 1000.0);
            return $"{megapixelsPerSecond:0.###} MPix/s";
        }

        private static string FormatCalibrationExpiry(NativePreviewCalibrationExpiryResult? expiry)
        {
            if (expiry is null)
            {
                return "not checked";
            }

            var remaining = expiry.RemainingDays.HasValue
                ? $"{expiry.RemainingDays.Value:0.#} d"
                : "unknown";
            return $"{expiry.Status}; {(expiry.Expired ? "expired" : "valid")}; {remaining}";
        }

        private void UpdateNativePreviewControls()
        {
            var preprocessReady = IsNativePreviewReady();
            OffsetOnRadio.IsEnabled = preprocessReady;
            OffsetAutoRadio.IsEnabled = preprocessReady;
            GainOnRadio.IsEnabled = preprocessReady;
            GainAutoRadio.IsEnabled = preprocessReady;
            DefectOnRadio.IsEnabled = preprocessReady;
            DefectAutoRadio.IsEnabled = preprocessReady;

            ApplyNativePreviewButton.IsEnabled = preprocessReady && currentPreview is not null && currentCalibrationContext is not null;

            if (preprocessReady && !hasInitializedNativeStageDefaults)
            {
                OffsetAutoRadio.IsChecked = true;
                GainAutoRadio.IsChecked = true;
                DefectAutoRadio.IsChecked = true;
                hasInitializedNativeStageDefaults = true;
            }

            if (!preprocessReady)
            {
                StageModesInfoText.Text = "Native preprocess exports are not ready, so Offset/Gain/Defect execution stays disabled.";
                NativePreviewText.Text = lastPreprocessHealth is null
                    ? "Native preview: readiness has not been checked."
                    : $"Native preview: unavailable ({lastPreprocessHealth.Status}; exportsReady={lastPreprocessHealth.IsExportReady}; synthetic={lastPreprocessHealth.SyntheticOracle.Status}).";
                return;
            }

            StageModesInfoText.Text = lastPreprocessHealth?.IsSyntheticOracleReady == true
                ? "Native preprocess is ready. Auto runs Offset/Gain/Defect only when the active calibration folder has the matching role."
                : "Native preprocess exports are available, so calibration diagnostics can run. Synthetic oracle is not passed; review the metric/report output carefully.";
            if (lastNativePreviewResult is null)
            {
                NativePreviewText.Text = currentPreview is null
                    ? "Native preview: load a target raw image to run preprocessing."
                    : "Native preview: ready. Select stage modes and run preprocessing.";
            }
        }

        private bool IsNativePreviewReady()
        {
            return lastPreprocessHealth?.IsExportReady == true;
        }

        private PreprocessStageSelection GetPreprocessSelection()
        {
            return new PreprocessStageSelection(
                GetPreprocessMode(OffsetOffRadio, OffsetOnRadio, OffsetAutoRadio),
                GetPreprocessMode(GainOffRadio, GainOnRadio, GainAutoRadio),
                GetPreprocessMode(DefectOffRadio, DefectOnRadio, DefectAutoRadio));
        }

        private static PreprocessStageSelection CreateCalibrationValidationSelection(string stageKey)
        {
            return stageKey.ToLowerInvariant() switch
            {
                "offset" => new PreprocessStageSelection(PreprocessStageMode.On, PreprocessStageMode.Off, PreprocessStageMode.Off),
                "gain" => new PreprocessStageSelection(PreprocessStageMode.On, PreprocessStageMode.On, PreprocessStageMode.Off),
                "defect" => new PreprocessStageSelection(PreprocessStageMode.On, PreprocessStageMode.On, PreprocessStageMode.On),
                _ => throw new InvalidOperationException($"No calibration validation adapter exists for '{stageKey}'.")
            };
        }

        private static string GetMode(RadioButton off, RadioButton on, RadioButton auto)
        {
            if (on.IsChecked == true)
            {
                return "On";
            }

            if (auto.IsChecked == true)
            {
                return "Auto";
            }

            return off.IsChecked == true ? "Off" : "Unknown";
        }

        private static PreprocessStageMode GetPreprocessMode(RadioButton off, RadioButton on, RadioButton auto)
        {
            if (on.IsChecked == true)
            {
                return PreprocessStageMode.On;
            }

            if (auto.IsChecked == true)
            {
                return PreprocessStageMode.Auto;
            }

            return PreprocessStageMode.Off;
        }

        private static string FormatStageSummary(IReadOnlyList<NativePreviewStageResult> stages)
        {
            return string.Join(", ", stages.Select(stage =>
                stage.Executed
                    ? $"{stage.Stage}={stage.ErrorCode}/{stage.LatencyMs:0.###}ms"
                    : $"{stage.Stage}=skipped({stage.ErrorCode})"));
        }

        private static string FormatCalibrationSummary(IReadOnlyList<NativePreviewCalibrationResult> loads)
        {
            if (loads.Count == 0)
            {
                return "none";
            }

            return string.Join(", ", loads.Select(load =>
                load.Loaded
                    ? $"{load.Stage}={load.Status}/{load.LatencyMs:0.###}ms{FormatExpirySummary(load.Expiry)}"
                    : $"{load.Stage}={load.Status}{FormatExpirySummary(load.Expiry)}"));
        }

        private static string FormatExpirySummary(NativePreviewCalibrationExpiryResult? expiry)
        {
            if (expiry is null)
            {
                return "";
            }

            var remaining = expiry.RemainingDays.HasValue
                ? $"{expiry.RemainingDays.Value:0.#}d"
                : "unknown";
            var expired = expiry.Expired ? "expired" : "valid";
            return $", expiry={expiry.Status}/{expired}/{remaining}";
        }

        private static string FormatMetricSummary(NativePreviewMetrics metrics)
        {
            return $"meanAbsDelta={metrics.MeanAbsoluteDelta:0.###}, " +
                $"rmse={metrics.Rmse:0.###}, " +
                $"maxAbsDelta={metrics.MaxAbsoluteDelta:0.###}, " +
                $"changed={metrics.ChangedPixels}/{metrics.PixelCount} ({metrics.ChangedPixelRatio:P2}), " +
                $"inputPreserved={metrics.InputPreserved}, nanInf={metrics.NaNInfCount}";
        }

        private void UpdateComparisonClip()
        {
            if (AfterPreviewClip is null || SwipeLine is null || SwipeValueText is null || ComparisonCanvas is null)
            {
                return;
            }

            var width = GetComparisonCanvasWidth();
            var height = GetComparisonCanvasHeight();

            if (width <= 0 || height <= 0)
            {
                return;
            }

            var fraction = CompareSwipeSlider is null ? 0.5 : Math.Clamp(CompareSwipeSlider.Value, 0, 1);
            var x = width * fraction;
            AfterPreviewClip.Rect = new Rect(0, 0, x, height);
            SwipeLine.Height = height;
            SwipeLine.Margin = new Thickness(Math.Max(0, x - 1), 0, 0, 0);
            SwipeValueText.Text = $"{fraction * 100:0}%";
        }

        private void FitComparisonToViewport()
        {
            if (currentPreview is null || RawImageScrollViewer is null || RawZoomSlider is null)
            {
                return;
            }

            var viewportWidth = RawImageScrollViewer.ViewportWidth > 0
                ? RawImageScrollViewer.ViewportWidth
                : RawImageScrollViewer.ActualWidth;
            var viewportHeight = RawImageScrollViewer.ViewportHeight > 0
                ? RawImageScrollViewer.ViewportHeight
                : RawImageScrollViewer.ActualHeight;

            if (viewportWidth <= 0 || viewportHeight <= 0)
            {
                return;
            }

            var widthScale = Math.Max(0.01, (viewportWidth - 24) / currentPreview.PreviewWidth);
            var heightScale = Math.Max(0.01, (viewportHeight - 24) / currentPreview.PreviewHeight);
            var fitScale = Math.Min(1.0, Math.Min(widthScale, heightScale));
            RawZoomSlider.Value = Math.Clamp(fitScale, RawZoomSlider.Minimum, RawZoomSlider.Maximum);
        }

        private void AdjustComparisonZoom(double factor)
        {
            if (RawZoomSlider is null)
            {
                return;
            }

            SetComparisonZoom(RawZoomSlider.Value * factor);
        }

        private void SetComparisonZoom(double value)
        {
            if (RawZoomSlider is null)
            {
                return;
            }

            RawZoomSlider.Value = Math.Clamp(value, RawZoomSlider.Minimum, RawZoomSlider.Maximum);
        }

        private void UpdateComparisonSwipeFromPoint(double x)
        {
            var width = GetComparisonCanvasWidth();
            if (width <= 0 || CompareSwipeSlider is null)
            {
                return;
            }

            CompareSwipeSlider.Value = Math.Clamp(x / width, 0, 1);
        }

        private void EndComparisonSwipeDrag()
        {
            isDraggingComparisonSwipe = false;
            ComparisonCanvas.ReleaseMouseCapture();
        }

        private double GetComparisonCanvasWidth()
        {
            return !double.IsNaN(ComparisonCanvas.Width) && ComparisonCanvas.Width > 0
                ? ComparisonCanvas.Width
                : ComparisonCanvas.ActualWidth;
        }

        private double GetComparisonCanvasHeight()
        {
            return !double.IsNaN(ComparisonCanvas.Height) && ComparisonCanvas.Height > 0
                ? ComparisonCanvas.Height
                : ComparisonCanvas.ActualHeight;
        }

        private void SetStatus(string message, Brush brush)
        {
            StatusText.Text = $"Status: {message}";
            StatusText.Foreground = brush;
        }
    }
}
