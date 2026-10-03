// GUI-C-212 (#249): the legacy diagnostic app says the same thing about preprocess on every tab.
using System.Diagnostics;
using FlaUI.Core;
using FlaUI.Core.AutomationElements;
using FlaUI.Core.Definitions;
using FlaUI.UIA3;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Legacy;

/// <summary>
/// GUI-C-211 measured on screen that <c>clients/ImageProcTest</c> told three stories about one module: the Diagnostics tab and the Calibration tab turned preprocess off because the
/// synthetic oracle failed, while the Evaluation tab said "Preprocess=ready" and enabled its switches (it asked <c>IsExportReady</c>, which ignores the oracle). They now ask one question
/// (<c>ModuleReadinessGrading.IsProcessingEnabled</c>). These scenarios read all three tabs of the REAL app and require the same answer from each, in both states:
/// the oracle passes (preprocess is enabled everywhere) and the oracle cannot run (it is blocked everywhere). The second state is produced by an environment fault, not by editing anything:
/// the app's temp path points at a file, so the oracle cannot create its calibration folder — exports are still ready, which is exactly the state that used to split the tabs.
///
/// <para>UIA patterns only (tab <c>SelectionItem.Select</c>, reading names and enabled flags): no key, no mouse. This is the DIAGNOSTIC app (the one with the readiness matrix), not the
/// user app the rest of this project drives; it is launched from <c>clients/ImageProcTest/bin/Debug/net8.0-windows</c> and needs <c>XPE_NATIVE_DIR</c> to name a folder with the native DLLs
/// (it finds the other modules' DLLs through the build tree; only xpe_common and xpe_preprocess matter here).</para>
/// </summary>
public sealed class LegacyPreprocessReadinessScenarios(ITestOutputHelper output)
{
    [SkippableFact]
    public void R01_AllThreeTabsEnablePreprocess_WhenTheOraclePasses()
    {
        using var app = LegacyApp.LaunchOrSkip(breakTemp: false);
        var view = app.ReadPreprocessOnEveryTab();
        output.WriteLine(view.Describe());

        Assert.True(view.Diagnostics, $"Diagnostics tab: {view.DiagnosticsEvidence}");
        Assert.True(view.Calibration, $"Calibration tab: {view.CalibrationEvidence}");
        Assert.True(view.Evaluation, $"Evaluation tab: {view.EvaluationEvidence}");
    }

    [SkippableFact]
    public void R02_AllThreeTabsBlockPreprocess_WhenTheOracleCannotRun_NotJustTwoOfThem()
    {
        using var app = LegacyApp.LaunchOrSkip(breakTemp: true);
        var view = app.ReadPreprocessOnEveryTab();
        output.WriteLine(view.Describe());

        // Exports are still ready in this state, so a tab that asks IsExportReady (the old Evaluation tab) says "ready" here.
        Assert.False(view.Diagnostics, $"Diagnostics tab: {view.DiagnosticsEvidence}");
        Assert.False(view.Calibration, $"Calibration tab: {view.CalibrationEvidence}");
        Assert.False(view.Evaluation, $"Evaluation tab: {view.EvaluationEvidence}");
    }

    private sealed record TabView(bool Diagnostics, string DiagnosticsEvidence, bool Calibration, string CalibrationEvidence, bool Evaluation, string EvaluationEvidence)
    {
        public string Describe() => $"diagnostics={Diagnostics} [{DiagnosticsEvidence}] | calibration={Calibration} [{CalibrationEvidence}] | evaluation={Evaluation} [{EvaluationEvidence}]";
    }

    private sealed class LegacyApp : IDisposable
    {
        private readonly UIA3Automation _automation;
        private readonly Application _application;
        private readonly Window _window;
        private readonly string? _faultFile;

        private LegacyApp(Application application, Window window, UIA3Automation automation, string? faultFile)
        {
            _application = application;
            _window = window;
            _automation = automation;
            _faultFile = faultFile;
        }

        public static LegacyApp LaunchOrSkip(bool breakTemp)
        {
            var exe = FindExecutable();
            Skip.If(exe is null, "clients/ImageProcTest/bin/Debug/net8.0-windows/ImageProcTest.exe was not found: build clients/ImageProcTest first.");
            AssertFresh(exe!);

            var nativeDir = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
            Skip.If(string.IsNullOrEmpty(nativeDir) || !File.Exists(Path.Combine(nativeDir, "xpe_preprocess.dll")), "XPE_NATIVE_DIR does not name a folder containing xpe_preprocess.dll.");

            var info = new ProcessStartInfo(exe!) { WorkingDirectory = Path.GetDirectoryName(exe)!, UseShellExecute = false };
            info.Environment["XPE_NATIVE_DIR"] = nativeDir;
            string? faultFile = null;
            if (breakTemp)
            {
                // A FILE where the temp directory should be: Path.GetTempPath() returns it, and nothing can be created beneath it.
                faultFile = Path.Combine(Path.GetTempPath(), $"xpe_not_a_directory_{Guid.NewGuid():N}");
                File.WriteAllText(faultFile, "not a directory");
                info.Environment["TMP"] = faultFile;
                info.Environment["TEMP"] = faultFile;
            }

            var automation = new UIA3Automation();
            var application = Application.Launch(info);
            var window = application.GetMainWindow(automation, TimeSpan.FromSeconds(60));
            Assert.NotNull(window);
            return new LegacyApp(application, window, automation, faultFile);
        }

        public TabView ReadPreprocessOnEveryTab()
        {
            // Diagnostics: the smoke line, the matrix row and the summary of what is executable.
            SelectTab("Diagnostics");
            var summary = WaitForText("ModuleReadinessSummaryText", t => t.StartsWith("Executable modules=", StringComparison.Ordinal), TimeSpan.FromSeconds(60));
            var smoke = WaitForText("PreprocessSmokeText", t => t.Contains("pass=", StringComparison.Ordinal), TimeSpan.FromSeconds(30));
            var row = DataItems().FirstOrDefault(n => n.Contains("ModuleReadinessSnapshot { ModuleName = xpe_preprocess,", StringComparison.Ordinal)) ?? "(no xpe_preprocess row)";
            var level = row.Contains("ModuleName = xpe_preprocess, Level = R3,", StringComparison.Ordinal) ? "R3" : "below R3";
            var diagnostics = smoke.Contains("pass=True", StringComparison.Ordinal) && level == "R3" && !summary.Contains("xpe_preprocess:R", StringComparison.Ordinal);
            var diagnosticsEvidence = $"{smoke}; matrix level {level}; summary '{summary}'";

            // Calibration: the dependency findings that block the preprocess stages.
            SelectTab("Calibration");
            Thread.Sleep(500);
            var blocking = DataItems().Count(n => n.Contains("RuleId = NATIVE-NOT-READY", StringComparison.Ordinal) && n.Contains("native preprocess adapter is not ready", StringComparison.Ordinal));
            var calibration = blocking == 0;
            var calibrationEvidence = $"{blocking} NATIVE-NOT-READY finding(s) for the preprocess stages";

            // Evaluation: the stage-mode line and the first preprocess switch.
            SelectTab("Evaluation");
            var info = WaitForText("StageModesInfoText", t => t.StartsWith("Preprocess=", StringComparison.Ordinal) || t.StartsWith("Native pre/post exports are not ready", StringComparison.Ordinal), TimeSpan.FromSeconds(30));
            var offsetSwitch = _window.FindFirstDescendant(cf => cf.ByAutomationId("OffsetEnabledCheckBox"));
            var switchEnabled = offsetSwitch is not null && offsetSwitch.IsEnabled;
            var evaluation = info.StartsWith("Preprocess=ready", StringComparison.Ordinal) && switchEnabled;
            var evaluationEvidence = $"'{info.Split('.')[0]}'; Offset switch enabled={switchEnabled}";

            return new TabView(diagnostics, diagnosticsEvidence, calibration, calibrationEvidence, evaluation, evaluationEvidence);
        }

        private void SelectTab(string header)
        {
            var tab = _window.FindAllDescendants(cf => cf.ByControlType(ControlType.TabItem)).FirstOrDefault(t => t.Name == header)?.AsTabItem();
            Assert.True(tab is not null, $"tab '{header}' was not found");
            tab!.Select();
            Thread.Sleep(600);
        }

        private IEnumerable<string> DataItems() =>
            _window.FindAllDescendants(cf => cf.ByControlType(ControlType.DataItem)).Select(e => e.Name ?? string.Empty).ToList();

        private string WaitForText(string automationId, Func<string, bool> done, TimeSpan timeout)
        {
            var deadline = DateTime.UtcNow + timeout;
            var text = string.Empty;
            while (DateTime.UtcNow < deadline)
            {
                text = _window.FindFirstDescendant(cf => cf.ByAutomationId(automationId))?.Name ?? string.Empty;
                if (done(text)) return text;
                Thread.Sleep(500);
            }

            Assert.Fail($"'{automationId}' never reached the expected state within {timeout.TotalSeconds:0} s; last text: '{text}'");
            return text;
        }

        public void Dispose()
        {
            try
            {
                if (!_application.HasExited) _application.Close();
            }
            catch (Exception)
            {
                // The process may already be gone; the kill below is the guarantee.
            }

            try
            {
                if (!_application.HasExited) _application.Kill();
            }
            catch (Exception)
            {
            }

            _automation.Dispose();
            if (_faultFile is not null) { try { File.Delete(_faultFile); } catch (IOException) { } }
        }

        private static string? FindExecutable()
        {
            for (var dir = new DirectoryInfo(AppContext.BaseDirectory); dir is not null; dir = dir.Parent)
            {
                var candidate = Path.Combine(dir.FullName, "clients", "ImageProcTest", "bin", "Debug", "net8.0-windows", "ImageProcTest.exe");
                if (File.Exists(candidate)) return candidate;
            }

            return null;
        }

        /// <summary>A stale binary reports a green for code that never ran: the exe must be newer than every source it was built from (the user-app fixture does the same).</summary>
        private static void AssertFresh(string exe)
        {
            var projectDir = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(exe)!, "..", "..", ".."));
            // The .exe is a launcher stub that a rebuild does not rewrite; the code is in the .dll beside it.
            var built = File.GetLastWriteTimeUtc(Path.ChangeExtension(exe, ".dll"));
            var newer = Directory.EnumerateFiles(projectDir, "*.cs", SearchOption.AllDirectories)
                .Where(f => !f.Contains($"{Path.DirectorySeparatorChar}obj{Path.DirectorySeparatorChar}") && !f.Contains($"{Path.DirectorySeparatorChar}bin{Path.DirectorySeparatorChar}"))
                .FirstOrDefault(f => File.GetLastWriteTimeUtc(f) > built);
            Assert.True(newer is null, $"clients/ImageProcTest's ImageProcTest.dll is older than {newer}: rebuild it (dotnet build clients/ImageProcTest/ImageProcTest.csproj -c Debug) before this scenario.");
        }
    }
}
