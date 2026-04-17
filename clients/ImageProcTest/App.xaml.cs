using System.Windows;

namespace ImageProcTest
{
    public partial class App : Application
    {
        private void Application_Startup(object sender, StartupEventArgs e)
        {
            if (e.Args.Contains("--probe-native-readiness", StringComparer.OrdinalIgnoreCase))
            {
                RunNativeReadinessProbe();
                return;
            }

            var window = new MainWindow();
            MainWindow = window;
            window.Show();
        }

        private void RunNativeReadinessProbe()
        {
            IXpeBackend backend = new CompositeXpeBackend(new RealXpeCommonBackend(), new MockXpeBackend());
            try
            {
                var health = backend.CheckHealth();
                var report = NativeReadinessProbe.WriteReport(health);
                var exitCode = report.PreprocessHealth.IsSyntheticOracleReady ? 0 : 2;
                Shutdown(exitCode);
            }
            catch
            {
                Shutdown(1);
            }
            finally
            {
                backend.Shutdown();
            }
        }
    }
}
