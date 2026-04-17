using Microsoft.Win32;
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
        private IReadOnlyList<ModuleReadinessSnapshot> currentModuleReadiness = [];

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
                return;
            }

            ImageFilesListBox.ItemsSource = selectedCase.Images;
            CalibrationFilesListBox.ItemsSource = selectedCase.CalibrationFiles;
            RawPreviewInfoText.Text = $"Case: {selectedCase.RootPath}";
            ProcessingScaffoldText.Text = "Processing scaffold: native preprocess is disabled. Selected calibration files are visible for traceability only.";
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

        private void OpenRawFileButton_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new OpenFileDialog
            {
                Filter = "Raw uint16 image (*.raw)|*.raw",
                CheckFileExists = true,
                Multiselect = false,
                Title = "Open uint16 raw image"
            };

            if (dialog.ShowDialog(this) == true)
            {
                LoadRawPreview(dialog.FileName);
            }
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
                RawZoomSlider.Value = 1;
                CompareSwipeSlider.Value = 0.5;

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
                ProcessingScaffoldText.Text = "Processing scaffold: Before=original, After=identity mock. Native correction is still disabled until preprocess readiness gates pass.";
                UpdateComparisonClip();
            }
            catch (Exception ex)
            {
                BeforePreviewImage.Source = null;
                AfterPreviewImage.Source = null;
                currentPreview = null;
                RawPreviewInfoText.Text = $"Raw preview failed: {ex.Message}";
                RawPreviewHashText.Text = "SHA-256: unavailable";
            }
        }

        private IReadOnlyList<StageModeSnapshot> GetStageModes()
        {
            const string reason = "Native preprocess/display execution is disabled until readiness gates pass.";
            return
            [
                new StageModeSnapshot("Offset", OffsetOffRadio.IsChecked == true ? "Off" : "Unknown", reason),
                new StageModeSnapshot("Gain", GainOffRadio.IsChecked == true ? "Off" : "Unknown", reason),
                new StageModeSnapshot("Defect", DefectOffRadio.IsChecked == true ? "Off" : "Unknown", reason),
                new StageModeSnapshot("Display", DisplayOffRadio.IsChecked == true ? "Off" : "Unknown", reason)
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
                "Native image processing remains disabled until module-specific readiness gates pass.";
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
