using System;
using System.Linq;
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

            if (e.Args.Contains("--run-preprocess-fixture-e2e", StringComparer.OrdinalIgnoreCase))
            {
                RunPreprocessFixtureE2e(e.Args);
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
                Environment.ExitCode = exitCode;
                Shutdown(exitCode);
            }
            catch
            {
                Environment.ExitCode = 1;
                Shutdown(1);
            }
            finally
            {
                backend.Shutdown();
            }
        }

        private void RunPreprocessFixtureE2e(string[] args)
        {
            try
            {
                var options = PreprocessFixtureE2eOptions.Parse(args);
                var report = PreprocessFixtureE2eService.Run(options);
                Environment.ExitCode = report.ExitCode;
                Shutdown(report.ExitCode);
            }
            catch (Exception ex)
            {
                PreprocessFixtureE2eService.WriteUnhandledException(ex);
                Environment.ExitCode = 1;
                Shutdown(1);
            }
        }
    }
}
