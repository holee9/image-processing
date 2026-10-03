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

            var oracleMode = Array.FindIndex(e.Args, a => a.Equals(XpePreprocessOracleProcess.ModeArgument, StringComparison.OrdinalIgnoreCase));
            if (oracleMode >= 0)
            {
                RunPreprocessOracleWorker(e.Args, oracleMode);
                return;
            }

            if (e.Args.Contains("--run-preprocess-fixture-e2e", StringComparer.OrdinalIgnoreCase))
            {
                RunPreprocessFixtureE2e(e.Args);
                return;
            }

            if (e.Args.Contains("--run-phase1b-fixture-e2e", StringComparer.OrdinalIgnoreCase))
            {
                RunPhase1bFixtureE2e(e.Args);
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

        // GUI-C-212b: the synthetic oracle runs here, in a child process of the app, so that its module shutdown and synthetic calibration never touch the app's own module state.
        private void RunPreprocessOracleWorker(string[] args, int modeIndex)
        {
            var dllPath = modeIndex + 1 < args.Length ? args[modeIndex + 1] : string.Empty;
            XpePreprocessOracleProcess.RunWorker(dllPath, Console.Out);
            Environment.ExitCode = 0;
            Shutdown(0);
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

        private void RunPhase1bFixtureE2e(string[] args)
        {
            try
            {
                var options = Phase1bFixtureE2eOptions.Parse(args);
                var report = Phase1bFixtureE2eService.Run(options);
                Environment.ExitCode = report.ExitCode;
                Shutdown(report.ExitCode);
            }
            catch (Exception ex)
            {
                Phase1bFixtureE2eService.WriteUnhandledException(ex);
                Environment.ExitCode = 1;
                Shutdown(1);
            }
        }
    }
}
