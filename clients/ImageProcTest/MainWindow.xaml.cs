using System;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace ImageProcTest
{
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
        private bool hasInitializedNativeStageDefaults;

        public MainWindow()
        {
            InitializeComponent();
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            RefreshNativeHealth();
            LoadFixtureCases();
            RefreshModuleReadiness();
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

        private void FixtureCaseComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (FixtureCaseComboBox.SelectedItem is not FixtureCaseInfo selectedCase)
            {
                ImageFilesListBox.ItemsSource = null;
                CalibrationFilesListBox.ItemsSource = null;
                SelectedCaseText.Text = "Selected case: none";
                return;
            }

            ImageFilesListBox.ItemsSource = selectedCase.Images;
            CalibrationFilesListBox.ItemsSource = selectedCase.CalibrationFiles;
            SelectedCaseText.Text =
                $"Selected case: {selectedCase.Name}; calibration roles: {selectedCase.CalibrationSummary}; root={selectedCase.RootPath}";
            RawPreviewInfoText.Text = $"Case: {selectedCase.RootPath}";
            ProcessingScaffoldText.Text = "Preprocessing test: select an image in this case, load it, then run the native fixture-calibrated chain.";
            UpdateNativePreviewControls();
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
        }

        private void LoadSelectedRawButton_Click(object sender, RoutedEventArgs e)
        {
            if (ImageFilesListBox.SelectedItem is not RawFileDescriptor raw)
            {
                RawPreviewInfoText.Text = "Select a raw image before loading.";
                return;
            }

            LoadRawPreview(raw.Path);
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

        private void CompareSwipeSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            UpdateComparisonClip();
        }

        private void ComparisonCanvas_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            UpdateComparisonClip();
        }

        private void SaveE2eReportButton_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                var selectedCase = FixtureCaseComboBox.SelectedItem as FixtureCaseInfo;
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
                    currentModuleReadiness);

                E2eReportText.Text = $"E2E report: {report.JsonPath}";
            }
            catch (Exception ex)
            {
                E2eReportText.Text = $"E2E report failed: {ex.Message}";
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
                ReportText.Text = $"Readiness report: {report.ReportPath}";
            }
            catch (Exception ex)
            {
                DisplayHealthText.Text = "Display health: Report generation skipped";
                PreprocessHealthText.Text = "Preprocess health: Report generation skipped";
                PreprocessSmokeText.Text = "Preprocess smoke: Report generation skipped";
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
                    RawPreviewInfoText.Text = "No calibration fixture cases found under tests/test_data/calibration_cases.";
                    ImageFilesListBox.ItemsSource = null;
                    CalibrationFilesListBox.ItemsSource = null;
                    SelectedCaseText.Text = "Selected case: none";
                    return;
                }

                FixtureCaseComboBox.SelectedIndex = 0;
                RawPreviewInfoText.Text = $"Loaded {cases.Count} fixture cases.";
            }
            catch (Exception ex)
            {
                RawPreviewInfoText.Text = $"Fixture scan failed: {ex.Message}";
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
                    ? "Preprocessing test: Before=original. Select Offset/Gain/Defect modes and run the fixture-calibrated native chain to update After."
                    : "Preprocessing test: Before=original, After=identity placeholder. Native correction is disabled until xpe_preprocess.dll exports are available.";
                UpdateNativePreviewControls();
                UpdateComparisonClip();
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

            if (FixtureCaseComboBox.SelectedItem is not FixtureCaseInfo selectedCase)
            {
                NativePreviewText.Text = "Native preview: select a prepared fixture case first.";
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
                SetStatus("Running native preprocess preview...", Brushes.Goldenrod);
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
                    $"Preprocessing test: fixture calibration was loaded from {selectedCase.Name}; " +
                    $"artifacts={result.ArtifactDirectory}. Before/After uses the same raw display window so brightness changes are visible.";
                SetStatus("Native preprocess preview complete", Brushes.ForestGreen);
                UpdateComparisonClip();
            }
            catch (Exception ex)
            {
                lastNativePreviewResult = null;
                NativePreviewText.Text = $"Native preview failed: {ex.Message}";
                SetStatus("Native preprocess preview failed", Brushes.OrangeRed);
            }
        }

        private IReadOnlyList<StageModeSnapshot> GetStageModes()
        {
            var preprocessReason = IsNativePreviewReady()
                ? "Native preprocess export readiness is available. Auto executes only when fixture calibration for that role is present."
                : "Native preprocess execution is disabled until xpe_preprocess.dll export readiness passes.";

            return
            [
                new StageModeSnapshot("Offset", GetMode(OffsetOffRadio, OffsetOnRadio, OffsetAutoRadio), preprocessReason),
                new StageModeSnapshot("Gain", GetMode(GainOffRadio, GainOnRadio, GainAutoRadio), preprocessReason),
                new StageModeSnapshot("Defect", GetMode(DefectOffRadio, DefectOnRadio, DefectAutoRadio), preprocessReason)
            ];
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
                "Only modules marked Exec can run preview adapters; clinical processing remains gated by fixture E2E.";
            UpdateNativePreviewControls();
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

            ApplyNativePreviewButton.IsEnabled = preprocessReady && currentPreview is not null && FixtureCaseComboBox.SelectedItem is FixtureCaseInfo;

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
                ? "Native preprocess is ready. Auto runs Offset/Gain/Defect only when the selected fixture has the matching calibration role."
                : "Native preprocess exports are available, so fixture diagnostics can run. Synthetic oracle is not passed; review the metric/report output carefully.";
            if (lastNativePreviewResult is null)
            {
                NativePreviewText.Text = currentPreview is null
                    ? "Native preview: load a fixture raw image to run preprocessing."
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
                    ? $"{load.Stage}={load.Status}/{load.LatencyMs:0.###}ms"
                    : $"{load.Stage}={load.Status}"));
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

            var width = !double.IsNaN(ComparisonCanvas.Width) && ComparisonCanvas.Width > 0
                ? ComparisonCanvas.Width
                : ComparisonCanvas.ActualWidth;
            var height = !double.IsNaN(ComparisonCanvas.Height) && ComparisonCanvas.Height > 0
                ? ComparisonCanvas.Height
                : ComparisonCanvas.ActualHeight;

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

        private void SetStatus(string message, Brush brush)
        {
            StatusText.Text = $"Status: {message}";
            StatusText.Foreground = brush;
        }
    }
}
