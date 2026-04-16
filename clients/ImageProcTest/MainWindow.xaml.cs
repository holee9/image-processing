using System.Windows;
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
        }

        private void RefreshButton_Click(object sender, RoutedEventArgs e)
        {
            RefreshNativeHealth();
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

        private void SetStatus(string message, Brush brush)
        {
            StatusText.Text = $"Status: {message}";
            StatusText.Foreground = brush;
        }
    }
}
