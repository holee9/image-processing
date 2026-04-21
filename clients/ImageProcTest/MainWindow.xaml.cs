using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using ImageProcTest.PInvokeWrappers;
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
        private NativeEnhanceBasicPreviewResult? lastEnhanceBasicPreviewResult;
        private IReadOnlyList<ModuleReadinessSnapshot> currentModuleReadiness = [];
        private IReadOnlyList<AlgorithmValidationItem> currentAlgorithmValidation = [];
        private IReadOnlyList<AlgorithmNode> currentAlgorithmNodes = [];
        private readonly ObservableCollection<AlgorithmChainStep> selectedAlgorithmChain = [];
        private AlgorithmChainPlan currentAlgorithmChainPlan =
            AlgorithmChainCatalogService.BuildPlan([]);
        private readonly List<ReportArtifactRow> reportArtifacts = [];
        private AlgorithmValidationRunSnapshot? lastAlgorithmValidationRun;
        private UserEvaluationSnapshot? lastUserEvaluation;
        private FixtureCaseInfo? currentCalibrationContext;
        private string? currentCalibrationFolderPath;
        private readonly EvaluationContextService evaluationContextService = new();
        private ActiveEvaluationContext? activeEvaluationContext;
        private bool isDraggingComparisonSwipe;
        private bool isUpdatingViewerControls;
        private ViewportRenderParams originalViewportParams = ViewportRenderParams.Default;
        private ViewportRenderParams processedViewportParams = ViewportRenderParams.Default;

        public MainWindow()
        {
            InitializeComponent();
            SelectedAlgorithmChainListBox.ItemsSource = selectedAlgorithmChain;
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
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
            EvaluationViewer.FitComparisonToViewport();
        }

        private void MenuZoomActual_Click(object sender, RoutedEventArgs e)
        {
            EvaluationViewer.SetComparisonZoom(1.0);
        }

        private void MenuZoomIn_Click(object sender, RoutedEventArgs e)
        {
            EvaluationViewer.AdjustComparisonZoom(1.25);
        }

        private void MenuZoomOut_Click(object sender, RoutedEventArgs e)
        {
            EvaluationViewer.AdjustComparisonZoom(0.8);
        }

        private void MenuResetLayout_Click(object sender, RoutedEventArgs e)
        {
            SelectTab("Evaluation");
            EvaluationViewer.ResetComparisonLayout();
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

        private void OpenCalibrationSetupButton_Click(object sender, RoutedEventArgs e)
        {
            SelectTab("Calibration");
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
            if (currentAlgorithmChainPlan.Steps.Count == 0)
            {
                WorkflowBeforeAfterText.Text = "Select one or more algorithm stages before running evaluation.";
                return;
            }

            WorkflowRunButton.IsEnabled = false;
            WorkflowCancelButton.IsEnabled = true;
            try
            {
                var run = RunAlgorithmChainValidation();
                WorkflowBeforeAfterText.Text =
                    $"Algorithm chain: {currentAlgorithmChainPlan.DisplayName}{Environment.NewLine}" +
                    $"Result: {run.Status}{Environment.NewLine}" +
                    $"Latency: {(run.LatencyMs.HasValue ? $"{run.LatencyMs.Value:0.###} ms" : "n/a")}{Environment.NewLine}" +
                    $"Artifacts: {run.ArtifactDirectory ?? "none"}{Environment.NewLine}" +
                    $"Rules: {currentAlgorithmChainPlan.Summary}{Environment.NewLine}" +
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

        private void AddAlgorithmToChain_Click(object sender, RoutedEventArgs e)
        {
            if (AvailableAlgorithmListBox.SelectedItem is not AlgorithmNode node)
            {
                return;
            }

            var nodes = selectedAlgorithmChain.Select(step => step.Node).ToList();
            nodes.Add(node);
            SetAlgorithmChain(nodes, node.StageKey);
        }

        private void RemoveAlgorithmFromChain_Click(object sender, RoutedEventArgs e)
        {
            if (SelectedAlgorithmChainListBox.SelectedItem is not AlgorithmChainStep step)
            {
                return;
            }

            var nodes = selectedAlgorithmChain
                .Where(item => !ReferenceEquals(item, step))
                .Select(item => item.Node)
                .ToList();
            SetAlgorithmChain(nodes);
        }

        private void MoveAlgorithmUp_Click(object sender, RoutedEventArgs e)
        {
            MoveSelectedAlgorithmStep(-1);
        }

        private void MoveAlgorithmDown_Click(object sender, RoutedEventArgs e)
        {
            MoveSelectedAlgorithmStep(1);
        }

        private void UseRunnablePreprocessChain_Click(object sender, RoutedEventArgs e)
        {
            ApplyAlgorithmPreset(AlgorithmChainPreset.RunnablePreprocess);
        }

        private void UseRunnablePostBasicChain_Click(object sender, RoutedEventArgs e)
        {
            ApplyAlgorithmPreset(AlgorithmChainPreset.RunnablePostBasic);
        }

        private void UseRunnablePrePostBasicChain_Click(object sender, RoutedEventArgs e)
        {
            ApplyAlgorithmPreset(AlgorithmChainPreset.RunnablePrePostBasic);
        }

        private void UseProductCanonicalChain_Click(object sender, RoutedEventArgs e)
        {
            ApplyAlgorithmPreset(AlgorithmChainPreset.ProductCanonical);
        }

        private void UsePreE2eProofChain_Click(object sender, RoutedEventArgs e)
        {
            ApplyAlgorithmPreset(AlgorithmChainPreset.PreE2eProof);
        }

        private void ClearAlgorithmChain_Click(object sender, RoutedEventArgs e)
        {
            SetAlgorithmChain([]);
        }

        private void SelectedAlgorithmChainListBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            UpdateAlgorithmChainPlan();
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

            if (IsEnhanceBasicStageKey(item.StageKey))
            {
                try
                {
                    var inputPixels = lastNativePreviewResult?.OutputPixels;
                    var inputSource = inputPixels is null ? "raw-float-bypass" : "preprocess-output";
                    var result = RunNativeEnhanceBasicPreview(
                        CreateEnhanceBasicValidationSelection(item.StageKey),
                        GetEnhanceBasicParameters(),
                        inputPixels,
                        inputSource,
                        $"{item.SwuId} {item.AlgorithmName}",
                        [item.StageKey!]);
                    lastAlgorithmValidationRun = new AlgorithmValidationRunSnapshot(
                        item.SwuId,
                        item.AlgorithmName,
                        "Pass",
                        $"input={result.InputSource}; {FormatMetricSummary(result.Metrics)}; EI={FormatNullable(result.ExposureIndex)}, DI={FormatNullable(result.DeviationIndex)}",
                        ArtifactDirectory: null,
                        result.TotalLatencyMs);
                    AlgorithmValidationResultText.Text =
                        $"Post validation: PASS {item.SwuId}; latency={result.TotalLatencyMs:0.###}ms; input={result.InputSource}; {FormatMetricSummary(result.Metrics)}";
                    UpdateEvaluationDashboards();
                }
                catch (Exception ex)
                {
                    lastEnhanceBasicPreviewResult = null;
                    EvaluationViewer.ClearNativePreview();
                    lastAlgorithmValidationRun = new AlgorithmValidationRunSnapshot(
                        item.SwuId,
                        item.AlgorithmName,
                        "Fail",
                        ex.Message,
                        ArtifactDirectory: null,
                        LatencyMs: null);
                    AlgorithmValidationResultText.Text = $"Post validation: FAIL {item.SwuId}; {ex.Message}";
                    SetStatus("Post validation failed", Brushes.OrangeRed);
                    UpdateEvaluationDashboards();
                }

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
                lastNativePreviewResult = null;
                lastEnhanceBasicPreviewResult = null;
                EvaluationViewer.ClearNativePreview();
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

            if (IsEnhanceBasicStageKey(item.StageKey))
            {
                try
                {
                    var inputPixels = lastNativePreviewResult?.OutputPixels;
                    var inputSource = inputPixels is null ? "raw-float-bypass" : "preprocess-output";
                    var result = RunNativeEnhanceBasicPreview(
                        CreateEnhanceBasicValidationSelection(item.StageKey),
                        GetEnhanceBasicParameters(),
                        inputPixels,
                        inputSource,
                        $"{item.SwuId} {item.AlgorithmName}",
                        [item.StageKey!]);
                    var pass = new AlgorithmValidationRunSnapshot(
                        item.SwuId,
                        item.AlgorithmName,
                        "Pass",
                        $"input={result.InputSource}; {FormatMetricSummary(result.Metrics)}; EI={FormatNullable(result.ExposureIndex)}, DI={FormatNullable(result.DeviationIndex)}",
                        ArtifactDirectory: null,
                        result.TotalLatencyMs);
                    lastAlgorithmValidationRun = pass;
                    UpdateEvaluationDashboards();
                    return pass;
                }
                catch (Exception ex)
                {
                    lastEnhanceBasicPreviewResult = null;
                    EvaluationViewer.ClearNativePreview();
                    var fail = new AlgorithmValidationRunSnapshot(
                        item.SwuId,
                        item.AlgorithmName,
                        "Fail",
                        ex.Message,
                        ArtifactDirectory: null,
                        LatencyMs: null);
                    lastAlgorithmValidationRun = fail;
                    SetStatus("Post validation failed", Brushes.OrangeRed);
                    UpdateEvaluationDashboards();
                    return fail;
                }
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
                lastNativePreviewResult = null;
                EvaluationViewer.ClearNativePreview();
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

        private AlgorithmValidationRunSnapshot RunAlgorithmChainValidation()
        {
            if (!currentAlgorithmChainPlan.CanExecute || currentAlgorithmChainPlan.HasHardBlocks)
            {
                var hardFindings = currentAlgorithmChainPlan.Findings
                    .Where(finding => finding.Severity == AlgorithmRuleSeverity.Hard)
                    .Select(finding => $"{finding.RuleId}: {finding.Message}")
                    .ToArray();
                var details = hardFindings.Length == 0
                    ? currentAlgorithmChainPlan.Summary
                    : string.Join(" ", hardFindings);
                var blocked = new AlgorithmValidationRunSnapshot(
                    "CHAIN",
                    currentAlgorithmChainPlan.DisplayName,
                    "Blocked",
                    details,
                    ArtifactDirectory: null,
                    LatencyMs: null);
                lastAlgorithmValidationRun = blocked;
                AlgorithmValidationResultText.Text = $"Algorithm chain: blocked; {details}";
                SetStatus("Algorithm chain blocked", Brushes.OrangeRed);
                UpdateEvaluationDashboards();
                return blocked;
            }

            if (currentAlgorithmChainPlan.IsFolderAuditOnly)
            {
                var folderAuditItem = currentAlgorithmValidation.FirstOrDefault(item =>
                    string.Equals(item.StageKey, "calib-folder", StringComparison.OrdinalIgnoreCase));
                if (folderAuditItem is null)
                {
                    var blocked = new AlgorithmValidationRunSnapshot(
                        "CHAIN",
                        currentAlgorithmChainPlan.DisplayName,
                        "Blocked",
                        "Calibration folder audit row is not available in the validation catalog.",
                        ArtifactDirectory: null,
                        LatencyMs: null);
                    lastAlgorithmValidationRun = blocked;
                    AlgorithmValidationResultText.Text = $"Algorithm chain: blocked; {blocked.Details}";
                    UpdateEvaluationDashboards();
                    return blocked;
                }

                var folderAudit = RunCalibrationFolderAudit(folderAuditItem);
                AlgorithmValidationResultText.Text =
                    $"Algorithm chain: {folderAudit.Status}; {folderAudit.Details}";
                return folderAudit;
            }

            if (currentPreview is null)
            {
                var blocked = new AlgorithmValidationRunSnapshot(
                    "CHAIN",
                    currentAlgorithmChainPlan.DisplayName,
                    "Blocked",
                    "Load the target raw image first.",
                    ArtifactDirectory: null,
                    LatencyMs: null);
                lastAlgorithmValidationRun = blocked;
                AlgorithmValidationResultText.Text = $"Algorithm chain: blocked; {blocked.Details}";
                UpdateEvaluationDashboards();
                return blocked;
            }

            var preprocessSelection = currentAlgorithmChainPlan.ToPreprocessSelection();
            var enhanceSelection = currentAlgorithmChainPlan.ToEnhanceBasicSelection();

            if (preprocessSelection.HasAnyStage && currentCalibrationContext is not FixtureCaseInfo)
            {
                var blocked = new AlgorithmValidationRunSnapshot(
                    "CHAIN",
                    currentAlgorithmChainPlan.DisplayName,
                    "Blocked",
                    "Select the acquired calibration folder first.",
                    ArtifactDirectory: null,
                    LatencyMs: null);
                lastAlgorithmValidationRun = blocked;
                AlgorithmValidationResultText.Text = $"Algorithm chain: blocked; {blocked.Details}";
                UpdateEvaluationDashboards();
                return blocked;
            }

            try
            {
                var pass = RunSelectedNativePreview(
                    preprocessSelection,
                    enhanceSelection,
                    $"algorithm chain {currentAlgorithmChainPlan.DisplayName}",
                    currentAlgorithmChainPlan.NativeStageOrder,
                    currentAlgorithmChainPlan.EnhanceBasicStageOrder);
                lastAlgorithmValidationRun = pass;
                AlgorithmValidationResultText.Text =
                    $"Algorithm chain: PASS; latency={FormatNullableLatency(pass.LatencyMs)}; " +
                    $"artifacts={pass.ArtifactDirectory ?? "none"}; {pass.Details}";
                UpdateEvaluationDashboards();
                return pass;
            }
            catch (Exception ex)
            {
                lastNativePreviewResult = null;
                EvaluationViewer.ClearNativePreview();
                var fail = new AlgorithmValidationRunSnapshot(
                    "CHAIN",
                    currentAlgorithmChainPlan.DisplayName,
                    "Fail",
                    ex.Message,
                    ArtifactDirectory: null,
                    LatencyMs: null);
                lastAlgorithmValidationRun = fail;
                AlgorithmValidationResultText.Text = $"Algorithm chain: FAIL; {ex.Message}";
                SetStatus("Algorithm chain failed", Brushes.OrangeRed);
                UpdateEvaluationDashboards();
                return fail;
            }
        }

        private void RecordUserEvaluationButton_Click(object sender, RoutedEventArgs e)
        {
            var selected = AlgorithmValidationGrid.SelectedItem as AlgorithmValidationItem;
            var algorithmKey = currentAlgorithmChainPlan.Steps.Count > 0
                ? currentAlgorithmChainPlan.DisplayName
                : selected is null
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
            EvaluationViewer.SetPreviewSelectionMessage(
                "Raw preview: no target raw loaded",
                $"Calibration case selected: {selectedCase.RootPath}");

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

            EvaluationViewer.SetPreviewSelectionMessage(
                $"Raw preview: {raw.Name}",
                $"Selected: {raw.Path} ({RawFileDescriptor.FormatBytes(raw.Length)})");
            WorkflowFilePathText.Text = raw.Path;
            UpdateWorkflowRunState();
            UpdateEvaluationDashboards();
        }

        private void LoadSelectedRawButton_Click(object sender, RoutedEventArgs e)
        {
            if (ImageFilesListBox.SelectedItem is not RawFileDescriptor raw)
            {
                EvaluationViewer.SetPreviewSelectionMessage(
                    "Raw preview: no file loaded",
                    "Select a raw image before loading.",
                    "SHA-256: not calculated");
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
                    lastEnhanceBasicPreviewResult,
                    GetStageModes(),
                    currentModuleReadiness,
                    currentAlgorithmValidation,
                    currentAlgorithmChainPlan,
                    lastAlgorithmValidationRun,
                    lastUserEvaluation,
                    activeEvaluationContext,
                    EvaluationViewer.CreateSnapshot());

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
                EvaluationViewer.PrepareForRawLoad(path);
                currentPreview = null;
                lastNativePreviewResult = null;
                lastEnhanceBasicPreviewResult = null;

                var preview = RawPreviewService.LoadUInt16Preview(path);
                currentPreview = preview;
                EvaluationViewer.LoadRawPreview(preview, IsNativePreviewReady());
                UpdateNativePreviewControls();
                UpdateWorkflowRunState();
                UpdateEvaluationDashboards();
            }
            catch (Exception ex)
            {
                currentPreview = null;
                lastNativePreviewResult = null;
                lastEnhanceBasicPreviewResult = null;
                EvaluationViewer.ClearRawPreview($"Raw preview failed: {ex.Message}");
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

            var preprocessSelection = GetPreprocessSelection();
            var enhanceSelection = GetEnhanceBasicSelection();
            if (!preprocessSelection.HasAnyStage && !enhanceSelection.HasAnyStage)
            {
                ApplyBypassPreview("All pre/post stages are unchecked.");
                UpdateEvaluationDashboards();
                return;
            }

            if (preprocessSelection.HasAnyStage && !IsNativePreviewReady())
            {
                NativePreviewText.Text = "Native preview: xpe_preprocess.dll export readiness is not available.";
                return;
            }

            if (enhanceSelection.HasAnyStage && !IsEnhanceBasicPreviewReady())
            {
                NativePreviewText.Text = "Native preview: xpe_enhance_basic.dll ABI smoke readiness is not available.";
                return;
            }

            if (preprocessSelection.HasAnyStage && currentCalibrationContext is not FixtureCaseInfo)
            {
                NativePreviewText.Text = "Native preview: select the acquired calibration folder before running preprocess stages.";
                return;
            }

            try
            {
                RunSelectedNativePreview(
                    preprocessSelection,
                    enhanceSelection,
                    "native pre/post preview");
            }
            catch (Exception ex)
            {
                lastNativePreviewResult = null;
                lastEnhanceBasicPreviewResult = null;
                EvaluationViewer.ClearNativePreview();
                NativePreviewText.Text = $"Native preview failed: {ex.Message}";
                SetStatus("Native pre/post preview failed", Brushes.OrangeRed);
                UpdateEvaluationDashboards();
            }
        }

        private void StageSelection_Changed(object sender, RoutedEventArgs e)
        {
            if (OffsetEnabledCheckBox is null || GainEnabledCheckBox is null || DefectEnabledCheckBox is null ||
                EiEnabledCheckBox is null || LogEnabledCheckBox is null || NoiseEnabledCheckBox is null ||
                ContrastEnabledCheckBox is null || EdgeEnabledCheckBox is null)
            {
                return;
            }

            lastNativePreviewResult = null;
            lastEnhanceBasicPreviewResult = null;
            EvaluationViewer.ClearNativePreview();
            var preprocessSelection = GetPreprocessSelection();
            var enhanceSelection = GetEnhanceBasicSelection();
            if (preprocessSelection.HasAnyStage || enhanceSelection.HasAnyStage)
            {
                NativePreviewText.Text = "Native preview: stage selection changed. Run Selected to apply the checked pre/post stages.";
                WorkflowBeforeAfterText.Text = "Stage switches changed. Viewer is reset to bypass output until the selected stages are run.";
            }
            else
            {
                ApplyBypassPreview("All pre/post stages are unchecked.");
                return;
            }

            UpdateWorkflowRunState();
            UpdateEvaluationDashboards();
        }

        private void ApplyBypassPreview(string reason)
        {
            if (currentPreview is null)
            {
                lastNativePreviewResult = null;
                lastEnhanceBasicPreviewResult = null;
                EvaluationViewer.ClearNativePreview();
                NativePreviewText.Text = $"Bypass preview: {reason} Load a target raw image to view the bypass output.";
                WorkflowBeforeAfterText.Text = "Bypass preview: no target raw image is loaded.";
                return;
            }

            var bypass = NativePreprocessPreviewService.CreateBypass(currentPreview, reason);
            lastNativePreviewResult = bypass;
            lastEnhanceBasicPreviewResult = null;
            EvaluationViewer.SetBypassPreview(bypass, reason);
            NativePreviewText.Text =
                $"Bypass preview: {reason} output buffer is copied from input; changed={bypass.Metrics.ChangedPixels}/{bypass.Metrics.PixelCount}; " +
                $"nanInf={bypass.Metrics.NaNInfCount}.";
            WorkflowBeforeAfterText.Text =
                "Stage switches: all Off. No correction stage executed; viewer shows Original vs bypass output.";
        }

        private NativePreprocessPreviewResult RunNativePreprocessPreview(
            PreprocessStageSelection selection,
            FixtureCaseInfo selectedCase,
            string statusLabel,
            IReadOnlyList<string>? stageOrder = null)
        {
            if (currentPreview is null)
            {
                throw new InvalidOperationException("Load a raw preview before running native preprocessing.");
            }

            SetStatus($"Running {statusLabel}...", Brushes.Goldenrod);
            var result = NativePreprocessPreviewService.Run(
                currentPreview,
                selection,
                selectedCase,
                lastPreprocessHealth?.DllPath,
                stageOrder);
            lastNativePreviewResult = result;
            lastEnhanceBasicPreviewResult = null;
            EvaluationViewer.SetNativePreview(result, selectedCase.CalibrationDirectoryPath);
            NativePreviewText.Text =
                $"Native preview: loads={FormatCalibrationSummary(result.CalibrationLoads)}; " +
                $"stages={FormatStageSummary(result.Stages)}; " +
                $"metrics={FormatMetricSummary(result.Metrics)}; " +
                $"latency={result.TotalLatencyMs:0.###}ms; output={result.OutputMin:0.###}..{result.OutputMax:0.###}";
            SetStatus($"{statusLabel} complete", Brushes.ForestGreen);
            UpdateEvaluationDashboards();
            return result;
        }

        private AlgorithmValidationRunSnapshot RunSelectedNativePreview(
            PreprocessStageSelection preprocessSelection,
            EnhanceBasicStageSelection enhanceSelection,
            string statusLabel,
            IReadOnlyList<string>? preprocessStageOrder = null,
            IReadOnlyList<string>? enhanceBasicStageOrder = null)
        {
            if (currentPreview is null)
            {
                throw new InvalidOperationException("Load a raw preview before running native pre/post processing.");
            }

            NativePreprocessPreviewResult? preprocessResult = null;
            if (preprocessSelection.HasAnyStage)
            {
                if (currentCalibrationContext is not FixtureCaseInfo selectedCase)
                {
                    throw new InvalidOperationException("Select the acquired calibration folder before running preprocess stages.");
                }

                preprocessResult = RunNativePreprocessPreview(
                    preprocessSelection,
                    selectedCase,
                    statusLabel,
                    preprocessStageOrder);
            }
            else
            {
                lastNativePreviewResult = null;
            }

            if (enhanceSelection.HasAnyStage)
            {
                var inputPixels = preprocessResult?.OutputPixels;
                var inputSource = preprocessResult is null
                    ? "raw-float-bypass"
                    : "preprocess-output";
                var enhanceResult = RunNativeEnhanceBasicPreview(
                    enhanceSelection,
                    GetEnhanceBasicParameters(),
                    inputPixels,
                    inputSource,
                    statusLabel,
                    enhanceBasicStageOrder);
                var run = new AlgorithmValidationRunSnapshot(
                    "CHAIN",
                    currentAlgorithmChainPlan.DisplayName,
                    "Pass",
                    $"{currentAlgorithmChainPlan.Summary} postMetrics={FormatMetricSummary(enhanceResult.Metrics)}; input={enhanceResult.InputSource}",
                    ArtifactDirectory: null,
                    enhanceResult.TotalLatencyMs + (preprocessResult?.TotalLatencyMs ?? 0));
                lastAlgorithmValidationRun = run;
                UpdateEvaluationDashboards();
                return run;
            }

            if (preprocessResult is null)
            {
                ApplyBypassPreview("No executable pre/post stage is selected.");
                return new AlgorithmValidationRunSnapshot(
                    "CHAIN",
                    currentAlgorithmChainPlan.DisplayName,
                    "Bypassed",
                    "No executable pre/post stage selected.",
                    ArtifactDirectory: null,
                    LatencyMs: 0);
            }

            var preprocessRun = new AlgorithmValidationRunSnapshot(
                "CHAIN",
                currentAlgorithmChainPlan.DisplayName,
                "Pass",
                $"{currentAlgorithmChainPlan.Summary} {FormatMetricSummary(preprocessResult.Metrics)}",
                preprocessResult.ArtifactDirectory,
                preprocessResult.TotalLatencyMs);
            lastAlgorithmValidationRun = preprocessRun;
            UpdateEvaluationDashboards();
            return preprocessRun;
        }

        private NativeEnhanceBasicPreviewResult RunNativeEnhanceBasicPreview(
            EnhanceBasicStageSelection selection,
            EnhanceBasicStageParameters parameters,
            IReadOnlyList<float>? inputPixels,
            string inputSource,
            string statusLabel,
            IReadOnlyList<string>? stageOrder = null)
        {
            if (currentPreview is null)
            {
                throw new InvalidOperationException("Load a raw preview before running native post-processing.");
            }

            SetStatus($"Running {statusLabel} post stages...", Brushes.Goldenrod);
            var result = NativeEnhanceBasicPreviewService.Run(
                currentPreview,
                inputPixels,
                inputSource,
                selection,
                parameters,
                preferredDllPath: null,
                stageOrder);
            lastEnhanceBasicPreviewResult = result;
            EvaluationViewer.SetAlgorithmPreview(
                result.OutputPixels,
                "Post after",
                $"Post basic applied from {Path.GetFileName(result.DllPath)}; input={result.InputSource}; " +
                $"latency={result.TotalLatencyMs:0.###}ms; EI={FormatNullable(result.ExposureIndex)}, DI={FormatNullable(result.DeviationIndex)}.");
            NativePreviewText.Text =
                $"Native preview: post stages={FormatStageSummary(result.Stages)}; " +
                $"metrics={FormatMetricSummary(result.Metrics)}; latency={result.TotalLatencyMs:0.###}ms; " +
                $"output={result.OutputMin:0.###}..{result.OutputMax:0.###}; input={result.InputSource}; " +
                $"EI={FormatNullable(result.ExposureIndex)}, DI={FormatNullable(result.DeviationIndex)}; " +
                $"sigma={FormatNullable(result.SigmaBefore)}->{FormatNullable(result.SigmaAfter)}";
            SetStatus($"{statusLabel} complete", Brushes.ForestGreen);
            UpdateEvaluationDashboards();
            return result;
        }

        private IReadOnlyList<StageModeSnapshot> GetStageModes()
        {
            var preprocessReason = IsNativePreviewReady()
                ? "Native preprocess export readiness is available. Checked stages execute; unchecked stages bypass."
                : "Native preprocess execution is disabled until xpe_preprocess.dll export readiness passes.";
            var postReason = IsEnhanceBasicPreviewReady()
                ? "Native enhance_basic ABI smoke is available. Checked post stages execute on pre output or raw-float bypass input."
                : "Native enhance_basic execution is disabled until xpe_enhance_basic.dll ABI smoke passes.";

            return
            [
                new StageModeSnapshot("Offset", GetMode(OffsetEnabledCheckBox), preprocessReason),
                new StageModeSnapshot("Gain", GetMode(GainEnabledCheckBox), preprocessReason),
                new StageModeSnapshot("Defect", GetMode(DefectEnabledCheckBox), preprocessReason),
                new StageModeSnapshot("EI", GetMode(EiEnabledCheckBox), postReason),
                new StageModeSnapshot("Log", GetMode(LogEnabledCheckBox), postReason),
                new StageModeSnapshot("Noise", GetMode(NoiseEnabledCheckBox), postReason),
                new StageModeSnapshot("Contrast", GetMode(ContrastEnabledCheckBox), postReason),
                new StageModeSnapshot("Edge", GetMode(EdgeEnabledCheckBox), postReason)
            ];
        }

        private void UpdateWorkflowRunState()
        {
            if (WorkflowRunButton is null)
            {
                return;
            }

            activeEvaluationContext = evaluationContextService.Build(
                currentCalibrationContext,
                currentCalibrationFolderPath,
                currentPreview,
                currentAlgorithmChainPlan,
                GetPreprocessSelection());

            WorkflowRunButton.IsEnabled = activeEvaluationContext.IsReady;
            ActiveContextSummaryText.Text = activeEvaluationContext.Summary;
            ActiveContextDetailsText.Text = activeEvaluationContext.BlockingReason + " " + activeEvaluationContext.Details;
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
            var previousChainKeys = selectedAlgorithmChain.Select(step => step.StageKey).ToArray();
            currentAlgorithmValidation = AlgorithmValidationCatalogService.Build(currentModuleReadiness);
            AlgorithmValidationGrid.ItemsSource = currentAlgorithmValidation;
            currentAlgorithmNodes = AlgorithmChainCatalogService.BuildNodes(currentModuleReadiness, currentAlgorithmValidation);
            AvailableAlgorithmListBox.ItemsSource = currentAlgorithmNodes;

            if (previousChainKeys.Length == 0)
            {
                ApplyAlgorithmPreset(AlgorithmChainPreset.RunnablePreprocess);
            }
            else
            {
                var refreshed = previousChainKeys
                    .Select(stageKey => currentAlgorithmNodes.FirstOrDefault(node =>
                        string.Equals(node.StageKey, stageKey, StringComparison.OrdinalIgnoreCase)))
                    .Where(node => node is not null)
                    .Select(node => node!)
                    .ToList();
                SetAlgorithmChain(refreshed);
            }

            var runnableCount = currentAlgorithmValidation.Count(item => item.CanRun);
            var moduleCount = currentAlgorithmValidation
                .Select(item => item.ModuleName)
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .Count();

            AlgorithmValidationResultText.Text =
                $"Algorithm validation: {currentAlgorithmValidation.Count} SWUs across {moduleCount} modules; runnable={runnableCount}.";
            UpdateAlgorithmChainPlan();
        }

        private void ApplyAlgorithmPreset(AlgorithmChainPreset preset)
        {
            if (currentAlgorithmNodes.Count == 0)
            {
                SetAlgorithmChain([]);
                return;
            }

            SetAlgorithmChain(AlgorithmChainCatalogService.BuildPreset(currentAlgorithmNodes, preset));
        }

        private void SetAlgorithmChain(IReadOnlyList<AlgorithmNode> nodes, string? selectedStageKey = null)
        {
            selectedAlgorithmChain.Clear();
            for (var i = 0; i < nodes.Count; i++)
            {
                selectedAlgorithmChain.Add(new AlgorithmChainStep(i + 1, nodes[i]));
            }

            if (!string.IsNullOrWhiteSpace(selectedStageKey))
            {
                SelectedAlgorithmChainListBox.SelectedItem = selectedAlgorithmChain.FirstOrDefault(step =>
                    string.Equals(step.StageKey, selectedStageKey, StringComparison.OrdinalIgnoreCase));
            }

            UpdateAlgorithmChainPlan();
        }

        private void MoveSelectedAlgorithmStep(int delta)
        {
            if (SelectedAlgorithmChainListBox.SelectedItem is not AlgorithmChainStep selected)
            {
                return;
            }

            var oldIndex = selectedAlgorithmChain.IndexOf(selected);
            var newIndex = oldIndex + delta;
            if (oldIndex < 0 || newIndex < 0 || newIndex >= selectedAlgorithmChain.Count)
            {
                return;
            }

            var nodes = selectedAlgorithmChain.Select(step => step.Node).ToList();
            (nodes[oldIndex], nodes[newIndex]) = (nodes[newIndex], nodes[oldIndex]);
            SetAlgorithmChain(nodes, selected.StageKey);
        }

        private void UpdateAlgorithmChainPlan()
        {
            currentAlgorithmChainPlan = AlgorithmChainCatalogService.BuildPlan(
                selectedAlgorithmChain.Select(step => step.Node).ToArray());
            WorkflowRuleFindingsGrid.ItemsSource = currentAlgorithmChainPlan.Findings;

            var inputState = currentCalibrationContext is null
                ? "select calibration folder"
                : currentPreview is null && !currentAlgorithmChainPlan.IsFolderAuditOnly
                    ? "load target raw"
                    : currentAlgorithmChainPlan.CanExecute
                        ? "ready"
                        : "blocked";
            WorkflowAlgorithmStatusText.Text = $"{currentAlgorithmChainPlan.Summary} Input: {inputState}.";
            UpdateWorkflowRunState();
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
                "Algorithm Chain",
                "selected order",
                currentAlgorithmChainPlan.DisplayName,
                "PIPE-SPEC-001"));
            rows.Add(new EvaluationMetricRow(
                "Algorithm Chain",
                "rule status",
                currentAlgorithmChainPlan.Summary,
                currentAlgorithmChainPlan.HasHardBlocks ? "Blocked" : "Runnable"));

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

            if (lastEnhanceBasicPreviewResult is null)
            {
                rows.Add(new EvaluationMetricRow("Post Basic", "native preview", "not run", "ENH-BASIC"));
            }
            else
            {
                var metrics = lastEnhanceBasicPreviewResult.Metrics;
                rows.Add(new EvaluationMetricRow("Post Basic", "input source", lastEnhanceBasicPreviewResult.InputSource, "ENH-BASIC"));
                rows.Add(new EvaluationMetricRow("Post Basic", "total latency", $"{lastEnhanceBasicPreviewResult.TotalLatencyMs:0.###} ms", "Performance"));
                rows.Add(new EvaluationMetricRow("Post Basic", "throughput", CalculateThroughput(metrics.PixelCount, lastEnhanceBasicPreviewResult.TotalLatencyMs), "Performance"));
                rows.Add(new EvaluationMetricRow("Post Basic", "mean abs delta", metrics.MeanAbsoluteDelta.ToString("0.###"), "Functional"));
                rows.Add(new EvaluationMetricRow("Post Basic", "rmse", metrics.Rmse.ToString("0.###"), "Functional"));
                rows.Add(new EvaluationMetricRow("Post Basic", "changed pixels", $"{metrics.ChangedPixels}/{metrics.PixelCount} ({metrics.ChangedPixelRatio:P2})", "Functional"));
                rows.Add(new EvaluationMetricRow("Post Basic", "EI / DI", $"{FormatNullable(lastEnhanceBasicPreviewResult.ExposureIndex)} / {FormatNullable(lastEnhanceBasicPreviewResult.DeviationIndex)}", "EI"));
                rows.Add(new EvaluationMetricRow("Post Basic", "sigma before/after", $"{FormatNullable(lastEnhanceBasicPreviewResult.SigmaBefore)} -> {FormatNullable(lastEnhanceBasicPreviewResult.SigmaAfter)}", "Noise"));
                rows.Add(new EvaluationMetricRow("Post Basic", "output range", $"{lastEnhanceBasicPreviewResult.OutputMin:0.###}..{lastEnhanceBasicPreviewResult.OutputMax:0.###}", "Functional"));
            }

            EvaluationMetricsGrid.ItemsSource = rows;
            StageLatencyGrid.ItemsSource = (lastNativePreviewResult?.Stages ?? [])
                .Concat(lastEnhanceBasicPreviewResult?.Stages ?? [])
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
            var postState = lastEnhanceBasicPreviewResult is null
                ? "post preview not run"
                : $"post preview {lastEnhanceBasicPreviewResult.TotalLatencyMs:0.###} ms";
            MetricsSummaryText.Text = $"Metrics: {previewState}; {nativeState}; {postState}.";
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
            var enhanceReady = IsEnhanceBasicPreviewReady();
            EvaluationViewer.UpdateNativeReadiness(preprocessReady);
            OffsetEnabledCheckBox.IsEnabled = preprocessReady;
            GainEnabledCheckBox.IsEnabled = preprocessReady;
            DefectEnabledCheckBox.IsEnabled = preprocessReady;
            EiEnabledCheckBox.IsEnabled = enhanceReady;
            LogEnabledCheckBox.IsEnabled = enhanceReady;
            NoiseEnabledCheckBox.IsEnabled = enhanceReady;
            ContrastEnabledCheckBox.IsEnabled = enhanceReady;
            EdgeEnabledCheckBox.IsEnabled = enhanceReady;

            ApplyNativePreviewButton.IsEnabled = currentPreview is not null &&
                ((preprocessReady && currentCalibrationContext is not null) || enhanceReady);

            if (!preprocessReady && !enhanceReady)
            {
                StageModesInfoText.Text = "Native pre/post exports are not ready, so algorithm execution switches stay disabled.";
                NativePreviewText.Text = lastPreprocessHealth is null
                    ? "Native preview: readiness has not been checked."
                    : $"Native preview: unavailable (pre={lastPreprocessHealth.Status}; exportsReady={lastPreprocessHealth.IsExportReady}; synthetic={lastPreprocessHealth.SyntheticOracle.Status}).";
                return;
            }

            StageModesInfoText.Text =
                $"Preprocess={(preprocessReady ? "ready" : "blocked")}; Post basic={(enhanceReady ? "ready" : "blocked")}. " +
                "Checked stages execute in the selected order; unchecked stages bypass. Post uses preprocess output when available, otherwise raw-to-float input.";
            if (lastNativePreviewResult is null && lastEnhanceBasicPreviewResult is null)
            {
                NativePreviewText.Text = currentPreview is null
                    ? "Native preview: load a target raw image to run pre/post algorithms."
                    : "Native preview: ready. Check pre/post stages to apply, or leave all unchecked for bypass output.";
            }
        }

        private bool IsNativePreviewReady()
        {
            return lastPreprocessHealth?.IsExportReady == true;
        }

        private bool IsEnhanceBasicPreviewReady()
        {
            return currentModuleReadiness.Any(module =>
                string.Equals(module.ModuleName, "xpe_enhance_basic", StringComparison.OrdinalIgnoreCase) &&
                module.ProcessingEnabled);
        }

        private PreprocessStageSelection GetPreprocessSelection()
        {
            return new PreprocessStageSelection(
                GetPreprocessMode(OffsetEnabledCheckBox),
                GetPreprocessMode(GainEnabledCheckBox),
                GetPreprocessMode(DefectEnabledCheckBox));
        }

        private EnhanceBasicStageSelection GetEnhanceBasicSelection()
        {
            return new EnhanceBasicStageSelection(
                EiEnabledCheckBox?.IsChecked == true,
                LogEnabledCheckBox?.IsChecked == true,
                NoiseEnabledCheckBox?.IsChecked == true,
                ContrastEnabledCheckBox?.IsChecked == true,
                EdgeEnabledCheckBox?.IsChecked == true);
        }

        private EnhanceBasicStageParameters GetEnhanceBasicParameters()
        {
            var defaults = EnhanceBasicStageParameters.Default;
            var noise = defaults.Noise;
            noise.SigmaSpace = ReadFloat(NoiseSigmaSpaceTextBox, defaults.Noise.SigmaSpace, min: 0.1f, max: 100f);
            noise.SigmaRange = ReadFloat(NoiseSigmaRangeTextBox, defaults.Noise.SigmaRange, min: 0.1f, max: 100_000f);

            var contrast = defaults.Contrast;
            contrast.ClipLimit = ReadFloat(ClaheClipLimitTextBox, defaults.Contrast.ClipLimit, min: 1.0f, max: 100f);

            var edge = defaults.Edge;
            edge.Amount = ReadFloat(UsmAmountTextBox, defaults.Edge.Amount, min: 0.0f, max: 5.0f);
            edge.Radius = ReadFloat(UsmRadiusTextBox, defaults.Edge.Radius, min: 0.5f, max: 10.0f);
            edge.Threshold = ReadFloat(UsmThresholdTextBox, defaults.Edge.Threshold, min: 0.0f, max: 1_000_000f);

            return new EnhanceBasicStageParameters(
                ReadFloat(LogNormFactorTextBox, defaults.LogNormFactor, min: 0.001f, max: 1_000_000f),
                noise,
                contrast,
                edge);
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

        private static EnhanceBasicStageSelection CreateEnhanceBasicValidationSelection(string? stageKey)
        {
            return stageKey?.ToLowerInvariant() switch
            {
                "ei-whole" => new EnhanceBasicStageSelection(true, false, false, false, false),
                "log" => new EnhanceBasicStageSelection(false, true, false, false, false),
                "basic-noise" => new EnhanceBasicStageSelection(false, false, true, false, false),
                "contrast" => new EnhanceBasicStageSelection(false, false, false, true, false),
                "edge" => new EnhanceBasicStageSelection(false, false, false, false, true),
                _ => throw new InvalidOperationException($"No post-basic validation adapter exists for '{stageKey}'.")
            };
        }

        private static bool IsEnhanceBasicStageKey(string? stageKey)
        {
            return string.Equals(stageKey, "ei-whole", StringComparison.OrdinalIgnoreCase) ||
                string.Equals(stageKey, "log", StringComparison.OrdinalIgnoreCase) ||
                string.Equals(stageKey, "basic-noise", StringComparison.OrdinalIgnoreCase) ||
                string.Equals(stageKey, "contrast", StringComparison.OrdinalIgnoreCase) ||
                string.Equals(stageKey, "edge", StringComparison.OrdinalIgnoreCase);
        }

        private static string GetMode(CheckBox enabled)
        {
            return enabled.IsChecked == true ? "On" : "Off";
        }

        private static PreprocessStageMode GetPreprocessMode(CheckBox enabled)
        {
            return enabled.IsChecked == true ? PreprocessStageMode.On : PreprocessStageMode.Off;
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

        private static string FormatNullable(float? value)
        {
            return value.HasValue ? value.Value.ToString("0.###") : "n/a";
        }

        private static string FormatNullableLatency(double? latencyMs)
        {
            return latencyMs.HasValue ? $"{latencyMs.Value:0.###} ms" : "n/a";
        }

        private static float ReadFloat(TextBox? textBox, float fallback, float min, float max)
        {
            if (textBox is null ||
                !float.TryParse(textBox.Text, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var value))
            {
                return fallback;
            }

            return Math.Clamp(value, min, max);
        }

        private void ViewerWindowSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (isUpdatingViewerControls || ViewerWindowCenterText is null || ViewerWindowWidthText is null)
            {
                return;
            }

            ApplyViewerControlsToParams();
            RenderComparisonViewports();
        }

        private void ViewerControl_Changed(object sender, RoutedEventArgs e)
        {
            if (isUpdatingViewerControls || ViewerWindowCenterSlider is null || ViewerWindowWidthSlider is null)
            {
                return;
            }

            ApplyViewerControlsToParams();
            RenderComparisonViewports();
        }

        private void ViewerTargetComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (isUpdatingViewerControls)
            {
                return;
            }

            SyncViewerControlsFromActive();
        }

        private void ViewerPresetComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (isUpdatingViewerControls || currentPreview is null)
            {
                return;
            }

            var selectedPreset = GetSelectedComboText(ViewerPresetComboBox);
            if (string.IsNullOrWhiteSpace(selectedPreset))
            {
                return;
            }

            var current = GetActiveViewportParams();
            var next = string.Equals(selectedPreset, "Auto Fit", StringComparison.OrdinalIgnoreCase)
                ? CreateAutoFitForActiveTarget(current)
                : ViewportRenderService.Presets.TryGetValue(selectedPreset, out var preset)
                    ? PreserveRenderFlags(preset, current)
                    : current;

            if (IsViewerLinked())
            {
                originalViewportParams = next;
                processedViewportParams = next;
            }
            else
            {
                SetActiveViewportParams(next);
            }

            ConfigureViewerSliderBounds();
            SyncViewerControlsFromActive();
            RenderComparisonViewports();
        }

        private void ResetViewerParamsForPreview(RawPreviewResult preview)
        {
            originalViewportParams = ViewportRenderService.AutoFit(preview.SampledPixels);
            processedViewportParams = originalViewportParams;
            ConfigureViewerSliderBounds();
        }

        private void ConfigureViewerSliderBounds()
        {
            if (ViewerWindowCenterSlider is null || ViewerWindowWidthSlider is null || currentPreview is null)
            {
                return;
            }

            var min = (double)currentPreview.MinValue;
            var max = (double)currentPreview.MaxValue;
            if (lastNativePreviewResult is not null)
            {
                min = Math.Min(min, lastNativePreviewResult.OutputMin);
                max = Math.Max(max, lastNativePreviewResult.OutputMax);
            }
            if (lastEnhanceBasicPreviewResult is not null)
            {
                min = Math.Min(min, lastEnhanceBasicPreviewResult.OutputMin);
                max = Math.Max(max, lastEnhanceBasicPreviewResult.OutputMax);
            }

            var range = Math.Max(1.0, max - min);
            isUpdatingViewerControls = true;
            try
            {
                ViewerWindowCenterSlider.Minimum = Math.Floor(min - range);
                ViewerWindowCenterSlider.Maximum = Math.Ceiling(max + range);
                ViewerWindowWidthSlider.Minimum = 1;
                ViewerWindowWidthSlider.Maximum = Math.Ceiling(Math.Max(4096.0, range * 4.0));
            }
            finally
            {
                isUpdatingViewerControls = false;
            }
        }

        private void ApplyViewerControlsToParams()
        {
            if (ViewerWindowCenterSlider is null || ViewerWindowWidthSlider is null)
            {
                return;
            }

            var next = new ViewportRenderParams(
                (float)ViewerWindowCenterSlider.Value,
                (float)Math.Max(1.0, ViewerWindowWidthSlider.Value),
                ViewerInvertCheckBox?.IsChecked == true,
                ViewerLutComboBox?.SelectedIndex == 1 ? ViewportLutType.Sigmoid : ViewportLutType.Linear).WithSafeWidth();

            if (IsViewerLinked())
            {
                originalViewportParams = next;
                processedViewportParams = next;
            }
            else
            {
                SetActiveViewportParams(next);
            }

            if (ViewerWindowCenterText is not null)
            {
                ViewerWindowCenterText.Text = $"{next.WindowCenter:0.###}";
            }

            if (ViewerWindowWidthText is not null)
            {
                ViewerWindowWidthText.Text = $"{next.WindowWidth:0.###}";
            }
        }

        private void SyncViewerControlsFromActive()
        {
            if (ViewerWindowCenterSlider is null ||
                ViewerWindowWidthSlider is null ||
                ViewerWindowCenterText is null ||
                ViewerWindowWidthText is null)
            {
                return;
            }

            var parameters = GetActiveViewportParams().WithSafeWidth();
            var center = ClampSliderValue(ViewerWindowCenterSlider, parameters.WindowCenter);
            var width = ClampSliderValue(ViewerWindowWidthSlider, parameters.WindowWidth);

            isUpdatingViewerControls = true;
            try
            {
                ViewerWindowCenterSlider.Value = center;
                ViewerWindowWidthSlider.Value = width;
                ViewerWindowCenterText.Text = $"{center:0.###}";
                ViewerWindowWidthText.Text = $"{width:0.###}";
                if (ViewerInvertCheckBox is not null)
                {
                    ViewerInvertCheckBox.IsChecked = parameters.Invert;
                }

                if (ViewerLutComboBox is not null)
                {
                    ViewerLutComboBox.SelectedIndex = parameters.Lut == ViewportLutType.Sigmoid ? 1 : 0;
                }
            }
            finally
            {
                isUpdatingViewerControls = false;
            }
        }

        private ViewportRenderParams GetActiveViewportParams()
        {
            return IsAfterViewportTarget() ? processedViewportParams : originalViewportParams;
        }

        private void SetActiveViewportParams(ViewportRenderParams parameters)
        {
            if (IsAfterViewportTarget())
            {
                processedViewportParams = parameters;
            }
            else
            {
                originalViewportParams = parameters;
            }
        }

        private bool IsViewerLinked()
        {
            return ViewerLinkCheckBox?.IsChecked != false;
        }

        private bool IsAfterViewportTarget()
        {
            return ViewerTargetComboBox?.SelectedIndex == 1;
        }

        private void RenderComparisonViewports()
        {
            if (currentPreview is null)
            {
                ClearViewerRenderState();
                return;
            }

            var before = ViewportRenderService.Render(
                currentPreview.SampledPixels,
                currentPreview.PreviewWidth,
                currentPreview.PreviewHeight,
                originalViewportParams);
            originalViewportParams = before.Params;
            BeforePreviewImage.Source = before.Bitmap;
            OriginalHistogramControl.Data = before.Histogram;
            OriginalHistogramText.Text = $"Original histogram: {before.Histogram.Summary}";

            if (IsViewerLinked())
            {
                processedViewportParams = originalViewportParams;
            }

            ViewportRenderResult after;
            if (TryGetProcessedOutputPixels(out var outputPixels))
            {
                after = ViewportRenderService.Render(
                    outputPixels,
                    currentPreview.PreviewWidth,
                    currentPreview.PreviewHeight,
                    processedViewportParams);
            }
            else
            {
                after = ViewportRenderService.Render(
                    currentPreview.SampledPixels,
                    currentPreview.PreviewWidth,
                    currentPreview.PreviewHeight,
                    processedViewportParams);
            }

            processedViewportParams = after.Params;
            AfterPreviewImage.Source = after.Bitmap;
            AfterHistogramControl.Data = after.Histogram;
            AfterHistogramText.Text = $"After histogram: {after.Histogram.Summary}";
            UpdateComparisonClip();
        }

        private bool TryGetProcessedOutputPixels(out float[] pixels)
        {
            if (lastEnhanceBasicPreviewResult?.OutputPixels is { Count: > 0 } postValues)
            {
                pixels = postValues as float[] ?? postValues.ToArray();
                return true;
            }

            if (lastNativePreviewResult?.OutputPixels is { Count: > 0 } values)
            {
                pixels = values as float[] ?? values.ToArray();
                return true;
            }

            pixels = [];
            return false;
        }

        private ViewportRenderParams CreateAutoFitForActiveTarget(ViewportRenderParams current)
        {
            if (IsAfterViewportTarget() && TryGetProcessedOutputPixels(out var outputPixels))
            {
                return PreserveRenderFlags(ViewportRenderService.AutoFit(outputPixels), current);
            }

            return currentPreview is null
                ? current
                : PreserveRenderFlags(ViewportRenderService.AutoFit(currentPreview.SampledPixels), current);
        }

        private static ViewportRenderParams PreserveRenderFlags(
            ViewportRenderParams next,
            ViewportRenderParams current)
        {
            return next with
            {
                Invert = current.Invert,
                Lut = current.Lut
            };
        }

        private static float ClampSliderValue(Slider slider, float value)
        {
            return (float)Math.Clamp(value, slider.Minimum, slider.Maximum);
        }

        private static string GetSelectedComboText(ComboBox comboBox)
        {
            return (comboBox.SelectedItem as ComboBoxItem)?.Content?.ToString() ?? string.Empty;
        }

        private void ClearViewerRenderState()
        {
            if (BeforePreviewImage is not null)
            {
                BeforePreviewImage.Source = null;
            }

            if (AfterPreviewImage is not null)
            {
                AfterPreviewImage.Source = null;
            }

            if (OriginalHistogramControl is not null)
            {
                OriginalHistogramControl.Data = HistogramData.Empty;
            }

            if (AfterHistogramControl is not null)
            {
                AfterHistogramControl.Data = HistogramData.Empty;
            }

            if (OriginalHistogramText is not null)
            {
                OriginalHistogramText.Text = "Original histogram: empty";
            }

            if (AfterHistogramText is not null)
            {
                AfterHistogramText.Text = "After histogram: empty";
            }
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
