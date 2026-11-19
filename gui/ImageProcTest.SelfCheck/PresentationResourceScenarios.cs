using System.IO;
using System.Windows;
using System.Windows.Media;
using System.Windows.Threading;
using ImageProcTest.Models;
using ImageProcTest.Services;
using ImageProcTest.Views;

namespace ImageProcTest.SelfCheck;

internal static class PresentationResourceScenarios
{
    public static void Run()
    {
        Exception? failure = null;
        var thread = new Thread(() =>
        {
            var dispatcher = Dispatcher.CurrentDispatcher;
            try { CheckConsumers(); }
            catch (Exception exception) { failure = exception; }
            finally { dispatcher.InvokeShutdown(); }
        });
        thread.SetApartmentState(ApartmentState.STA);
        thread.Start();
        thread.Join();
        if (failure is not null) throw new InvalidOperationException("App-independent presentation resources failed.", failure);
    }

    private static void CheckConsumers()
    {
        if (Application.Current is not null) throw new InvalidOperationException("This check must run without an Application.");
        var directory = Path.Combine(Path.GetTempPath(), "xpe-presentation-resources-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(directory);
        var settings = new AppSettingsService(Path.Combine(directory, "settings.json"));
        settings.Save(new AppSettings { BackendMode = "Mock" });
        var failures = new List<string>();
        (string Name, Func<FrameworkElement> Create, string Key)[] consumers =
        [
            ("AnalysisPanel", () => new AnalysisPanel(), "UiMetadataText"),
            ("AlgorithmBar", () => new AlgorithmBar(), "UiControlText"),
            ("StudyQueue", () => new StudyQueue(), "UiBodyText"),
            ("TopBar", () => new TopBar(), "UiMetadataText"),
            ("VerdictBar", () => new VerdictBar(), "UiBodyText"),
            ("ViewportShell", () => new ViewportShell(), "UiImageBackground"),
            ("MainWindow", () => new MainWindow(settings), "UiMetadataText"),
            ("ComparisonImportWindow", () => new ComparisonImportWindow((_, _, _) => throw new InvalidOperationException("No engine belongs to this resource check.")), "UiProvenanceText"),
        ];
        try
        {
            foreach (var consumer in consumers)
            {
                FrameworkElement? element = null;
                try
                {
                    element = consumer.Create();
                    var shared = new ResourceDictionary { Source = new Uri("/ImageProcTest;component/Resources/Presentation.xaml", UriKind.Relative) };
                    if (!element.Resources.MergedDictionaries.Any(dictionary => dictionary.Source?.OriginalString.EndsWith("Resources/Presentation.xaml", StringComparison.Ordinal) == true))
                        throw new InvalidOperationException("The consumer does not merge the shared presentation dictionary locally.");
                    if (element is MainWindow main && !ReferenceEquals(main.Style, main.FindResource("UiWindowStyle")))
                        throw new InvalidOperationException("MainWindow does not apply the shared window style.");
                    var actual = element.FindResource(consumer.Key) as SolidColorBrush;
                    var expected = shared[consumer.Key] as SolidColorBrush;
                    if (actual is null || expected is null || actual.Color != expected.Color)
                        throw new InvalidOperationException($"{consumer.Key} does not match the shared definition.");
                    if (Application.Current is not null) throw new InvalidOperationException("A consumer created an Application.");
                    Console.WriteLine($"GUI-C-274 standalone {consumer.Name}: constructor and {consumer.Key}={actual.Color} passed; Application.Current=null.");
                }
                catch (Exception exception)
                {
                    failures.Add($"{consumer.Name}: {exception}");
                }
                finally
                {
                    if (element is Window window) window.Close();
                }
            }
            if (failures.Count != 0) throw new InvalidOperationException(string.Join(Environment.NewLine, failures));
            Console.WriteLine($"GUI-C-274 standalone presentation resources passed: {consumers.Length}/{consumers.Length}.");
        }
        finally { Directory.Delete(directory, recursive: true); }
    }
}
