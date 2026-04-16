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

        public MainWindow()
        {
            InitializeComponent();
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            RefreshNativeHealth();
            LoadFixtureCases();
        }

        private void RefreshButton_Click(object sender, RoutedEventArgs e)
        {
            RefreshNativeHealth();
        }

        private void RefreshFixturesButton_Click(object sender, RoutedEventArgs e)
        {
            LoadFixtureCases();
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

        private void RefreshNativeHealth()
        {
            SetStatus("Checking native common backend...", Brushes.Goldenrod);
            var result = backend.CheckHealth();
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
                DisplayHealthText.Text = $"Display health: {report.DisplaySummary}";
                ReportText.Text = $"Readiness report: {report.ReportPath}";
            }
            catch (Exception ex)
            {
                DisplayHealthText.Text = "Display health: Report generation skipped";
                ReportText.Text = $"Readiness report: failed ({ex.Message})";
            }
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
                RawPreviewImage.Source = null;
                RawZoomSlider.Value = 1;

                var preview = RawPreviewService.LoadUInt16Preview(path);
                RawPreviewImage.Source = preview.Bitmap;
                RawPreviewInfoText.Text =
                    $"Source={preview.Width}x{preview.Height} uint16, file={RawFileDescriptor.FormatBytes(preview.FileSizeBytes)}, " +
                    $"preview={preview.PreviewWidth}x{preview.PreviewHeight}, stride={preview.SampleStride}, " +
                    $"min={preview.MinValue}, max={preview.MaxValue}";
                RawPreviewHashText.Text = $"SHA-256: {preview.Sha256}";
                ProcessingScaffoldText.Text = "Processing scaffold: loaded original buffer preview only. Native correction is still disabled until preprocess readiness gates pass.";
            }
            catch (Exception ex)
            {
                RawPreviewImage.Source = null;
                RawPreviewInfoText.Text = $"Raw preview failed: {ex.Message}";
                RawPreviewHashText.Text = "SHA-256: unavailable";
            }
        }

        private void SetStatus(string message, Brush brush)
        {
            StatusText.Text = $"Status: {message}";
            StatusText.Foreground = brush;
        }
    }
}
