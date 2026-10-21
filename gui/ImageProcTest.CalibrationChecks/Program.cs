using System.IO;
using System.Security.Cryptography;
using System.Text.Json;
using System.Diagnostics;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Automation;
using System.Windows.Media;
using ImageProcTest.Models;
using ImageProcTest.Services;
using ImageProcTest.ViewModels;

internal static class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        var failures = 0;
        Check("Reload rejects bytes changed after child validation and incomplete load evidence", () =>
        {
            var hashes = new Dictionary<string, string> { ["o"] = "checked-o", ["g"] = "checked-g", ["d"] = "checked-d" };
            using var valid = JsonDocument.Parse(JsonSerializer.Serialize(new { passed = true, loads = hashes.Select(p => new { path = p.Key, sha256 = p.Value, code = 0 }) }));
            CalibrationWorkflowService.ValidateReloadReport(valid.RootElement, hashes);
            hashes["o"] = "unvalidated-new-bytes";
            try { CalibrationWorkflowService.ValidateReloadReport(valid.RootElement, hashes); throw new Exception("Post-child change accepted."); }
            catch (IOException e) when (e.Message.Contains("SHA")) { }
            using var partial = JsonDocument.Parse("{\"passed\":true,\"loads\":[{\"path\":\"o\",\"sha256\":\"checked-o\",\"code\":0}]}");
            try { CalibrationWorkflowService.ValidateReloadReport(partial.RootElement, hashes); throw new Exception("Incomplete report accepted."); }
            catch (IOException e) when (e.Message.Contains("3개")) { }
        });
        Check("Stage selection does not confuse requested modes and executed evidence", () =>
        {
            if (CalibrationStageSelection.ShouldLoad("Off", true) || CalibrationStageSelection.ShouldLoad("Auto", false)
                || !CalibrationStageSelection.ShouldLoad("On", false) || !CalibrationStageSelection.ShouldLoad("Auto", true))
                throw new Exception("Mode selection violates Auto/On/Off.");
            if (!CalibrationStageSelection.ExecutedFromReason("requested=[offset:Auto]; executed=[gain,defect]; flags=12").SequenceEqual(new[] { "gain", "defect" }))
                throw new Exception("HUD does not use actual executed stages.");
            if (CalibrationStageSelection.ExecutedFromReason("Mock applied").Length != 0)
                throw new Exception("Mock counted as native calibration.");
        });
        if (args.Length == 2 && args[1] == "real")
        {
            Check("Existing real RAW workflow (quality gates remain authoritative)", () =>
            {
                var inputs = "D:/workspace-github/image-processing/tests/test_data/cyan_test/calfiles";
                string FileName(string kind, int n) => Path.Combine(inputs, $"{kind}_{n:00}.raw");
                var dark = Enumerable.Range(7, 8).Select(i => FileName("Dark", i)).ToArray();
                var bright = Enumerable.Range(23, 6).Select(i => FileName("Bright", i)).ToArray();
                var bpm = Enumerable.Range(17, 12).Concat(Enumerable.Range(31, 6)).Select(i => FileName("Bright", i)).ToArray();
                var service = new CalibrationWorkflowService(Path.GetDirectoryName(args[0])!, args[0], new CalibrationInputs(3072, 3072, dark, bright, bpm, false, 0));
                Console.WriteLine("REAL_RUN " + service.RunDirectory);
                service.ValidateAsync().GetAwaiter().GetResult();
                service.GenerateAsync().GetAwaiter().GetResult();
                service.ReloadAsync().GetAwaiter().GetResult();
                service.VerifyReadyToApply();
                foreach (var entry in new[] { ("Bright", 29, "flat"), ("Dark", 15, "dark") })
                {
                    var output = service.EvaluateAsync(FileName(entry.Item1, entry.Item2), entry.Item3, "", "", "normalized_gain").GetAwaiter().GetResult();
                    Console.WriteLine("REAL_EVALUATION " + output + "\n" + CalibrationWorkflowService.QualitySummary(output));
                }
            });
        }
        if (args.Length == 1)
        {
            Check("Synthetic workflow: validate/no output, generate, reload, independent evaluation, reject reused input", () =>
            {
                var root = Path.Combine(Path.GetDirectoryName(args[0])!, "synthetic-" + Guid.NewGuid().ToString("N")[..8]);
                Directory.CreateDirectory(root);
                string Raw(string name, int level, int frame)
                {
                    var path = Path.Combine(root, name + ".raw");
                    using var writer = new BinaryWriter(new FileStream(path, FileMode.CreateNew));
                    for (var i = 0; i < 64 * 64; i++) writer.Write((ushort)(level + i % 13 + (i * 17 + frame * 7) % 11));
                    return path;
                }
                var dark = Enumerable.Range(0, 6).Select(i => Raw("dark" + i, 100, i)).ToArray();
                var bright = Enumerable.Range(0, 10).Select(i => Raw("bright" + i, 6000, i)).ToArray();
                var workflow = new CalibrationWorkflowService(root, args[0], new CalibrationInputs(64, 64, dark, bright, [], false, 0));
                workflow.ValidateAsync().GetAwaiter().GetResult();
                if (Directory.Exists(workflow.SetDirectory)) throw new Exception("Validate generated files.");
                workflow.GenerateAsync().GetAwaiter().GetResult();
                workflow.ReloadAsync().GetAwaiter().GetResult();
                workflow.VerifyReadyToApply();
                var tool = Path.Combine(args[0], "xpe_calib_fixture_gen.exe");
                void Refused(string label, string[] darks, string[] flats)
                {
                    var output = Path.Combine(root, label);
                    var info = new ProcessStartInfo(tool) { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true };
                    foreach (var arg in new[] { "--validate-only", "--width", "64", "--height", "64", "--out", output }) info.ArgumentList.Add(arg);
                    foreach (var p in darks) { info.ArgumentList.Add("--dark"); info.ArgumentList.Add(p); }
                    foreach (var p in flats) { info.ArgumentList.Add("--bright"); info.ArgumentList.Add(p); }
                    using var process = Process.Start(info)!;
                    var stdout = process.StandardOutput.ReadToEndAsync(); var stderr = process.StandardError.ReadToEndAsync();
                    process.WaitForExit(); Task.WaitAll(stdout, stderr);
                    if (process.ExitCode == 0 || Directory.Exists(output)) throw new Exception("Invalid input accepted/wrote output: " + label);
                    Console.WriteLine($"NATIVE_PREFLIGHT_REFUSAL {label} exit={process.ExitCode}");
                }
                Refused("too-few", dark[..4], bright);
                Refused("duplicate", dark, bright.Select((p, i) => i == 1 ? bright[0] : p).ToArray());
                var saturated = Raw("saturated", 65500, 0);
                using (var file = new BinaryWriter(new FileStream(saturated, FileMode.Open))) for (var i = 0; i < 20; i++) file.Write(ushort.MaxValue);
                Refused("saturation", dark, bright.Take(9).Append(saturated).ToArray());
                var mixed = Raw("mixed-dose", 18000, 0);
                Refused("mixed-dose", dark, bright.Take(9).Append(mixed).ToArray());
                var shortRaw = Path.Combine(root, "short.raw"); File.WriteAllBytes(shortRaw, new byte[10]);
                Refused("wrong-size", dark, bright.Take(9).Append(shortRaw).ToArray());
                var assembly = typeof(CalibrationWorkflowService).Assembly;
                assembly.GetType("ImageProcTest.Services.Native.GuiNativeLibraryResolver")!.GetMethod("Install")!.Invoke(null, null);
                Environment.SetEnvironmentVariable("XPE_NATIVE_DIR", args[0]);
                Environment.SetEnvironmentVariable("XPE_NATIVE_DIR_EXCLUSIVE", "1");
                var run = assembly.GetType("ImageProcTest.Services.Native.GuiPreprocessRunner")!.GetMethod("Run")!;
                var pixels = Enumerable.Repeat((ushort)6005, 64 * 64).ToArray();
                var preserved = (ushort[])pixels.Clone();
                PreprocessRunResult Apply(string offset, string gainMode, string defectMode) => (PreprocessRunResult)run.Invoke(null,
                    [pixels, 64, 64, workflow.SetDirectory, workflow.SetDirectory, workflow.SetDirectory, "CHEST", 0f, 0.14f, false, offset, gainMode, defectMode])!;
                var all = Apply("Auto", "Auto", "Auto");
                if (!all.Ran || !CalibrationStageSelection.ExecutedFromReason(all.Summary).SequenceEqual(new[] { "offset", "gain", "defect" }))
                    throw new Exception("Native all-stage Auto failed: " + all.Summary);
                var bypass = Apply("Off", "On", "On");
                if (!bypass.Ran || !CalibrationStageSelection.ExecutedFromReason(bypass.Summary).SequenceEqual(new[] { "gain", "defect" }))
                    throw new Exception("Native Offset Off failed: " + bypass.Summary);
                var off = Apply("Off", "Off", "Off");
                if (off.Ran || !off.Skipped || !pixels.SequenceEqual(preserved)) throw new Exception("All Off changed input or claimed application.");
                Console.WriteLine("NATIVE_MODES " + all.Summary + "\n" + bypass.Summary + "\n" + off.Summary);
                NativeRuntimeEvidence.VerifyPreCombination(args[0]);
                var wrongNative = Path.Combine(root, "wrong-native"); Directory.CreateDirectory(wrongNative);
                File.WriteAllBytes(Path.Combine(wrongNative, "xpe_common.dll"), new byte[] { 1, 2, 3 });
                try { NativeRuntimeEvidence.VerifyPreCombination(wrongNative); throw new Exception("Different GUI/tool native bytes accepted."); }
                catch (InvalidOperationException e) when (e.Message.Contains("SHA")) { }
                Console.WriteLine("NATIVE_COMBINATION matching accepted; mismatched SHA refused");
                try { workflow.EvaluateAsync(dark[0], "dark", "", "").GetAwaiter().GetResult(); throw new Exception("Generation input accepted as holdout."); }
                catch (InvalidOperationException e) when (e.Message.Contains("holdout")) { }
                var holdout = Raw("independent", 6010, 14);
                workflow.EvaluateAsync(holdout, "flat", "", "").GetAwaiter().GetResult();
                File.AppendAllText(bright[0], "mutation");
                try { workflow.ValidateAsync().GetAwaiter().GetResult(); throw new Exception("Changed input accepted."); }
                catch (InvalidOperationException e) when (e.Message.Contains("입력 변경")) { }
                var manifestPath = Path.Combine(workflow.SetDirectory, "manifest.json");
                var manifestNode = System.Text.Json.Nodes.JsonNode.Parse(File.ReadAllText(manifestPath))!;
                manifestNode["inputs"]![0]!["sha256"] = new string('0', 64);
                File.WriteAllText(manifestPath, manifestNode.ToJsonString());
                try { workflow.VerifyReadyToApply(); throw new Exception("Consumed-input SHA mismatch accepted."); }
                catch (IOException e) when (e.Message.Contains("baseline")) { }
                Console.WriteLine("MANIFEST_INPUT_MISMATCH refused; altered synthetic evidence intentionally retained");
                Console.WriteLine("SYNTHETIC_ONLY " + workflow.RunDirectory);
                var app = new Application { ShutdownMode = ShutdownMode.OnExplicitShutdown };
                RenderOptions.ProcessRenderMode = System.Windows.Interop.RenderMode.SoftwareOnly;
                var model = new MainWindowViewModel(new AppSettings { BackendMode = "Mock" }, new AppSettingsService(Path.Combine(root, "ui-settings.json")),
                    _ => new MockXpeBackend(new RawImageLoader(), "", false, "", false));
                var window = new ImageProcTest.Views.CalibrationWorkflowWindow(model);
                try
                {
                    window.Show(); window.UpdateLayout();
                    if (window.ActualWidth < 1 || window.ActualHeight < 1 || !window.Title.Contains("PRE")) throw new Exception("Workflow window did not display.");
                    Console.WriteLine("WPF_WORKFLOW_VISIBLE title=" + window.Title);
                    var main = new ImageProcTest.MainWindow { Width = 1280, Height = 800 };
                    try
                    {
                        main.Show();
                        var mainModel = (MainWindowViewModel)main.DataContext;
                        mainModel.Settings.ShowCalibrationPanel = true;
                        mainModel.Settings.ShowDisplayPanel = true;
                        mainModel.OpenPipelineDiagnosticsCommand.Execute(null);
                        main.UpdateLayout();
                        main.Dispatcher.Invoke(() => { }, System.Windows.Threading.DispatcherPriority.ApplicationIdle);
                        IEnumerable<DependencyObject> Walk(DependencyObject parent)
                        {
                            yield return parent;
                            for (var i = 0; i < VisualTreeHelper.GetChildrenCount(parent); i++)
                                foreach (var child in Walk(VisualTreeHelper.GetChild(parent, i))) yield return child;
                        }
                        var controls = Walk(main).OfType<FrameworkElement>().ToArray();
                        var offRadio = controls.OfType<RadioButton>().Single(c => AutomationProperties.GetAutomationId(c) == "CalibOffsetModeOff");
                        var ghostRadio = controls.OfType<RadioButton>().Single(c => AutomationProperties.GetAutomationId(c) == "CalibGhostModeOff");
                        if (!offRadio.IsEnabled || ghostRadio.IsEnabled) throw new Exception("Supported/unsupported stage controls not distinguished.");
                        offRadio.IsChecked = true;
                        if (mainModel.Settings.OffsetCorrectionMode != "Off") throw new Exception("Offset Off did not update actual bound settings.");
                        var viewport = controls.Single(c => AutomationProperties.GetAutomationId(c) == "ViewportShell");
                        var optional = controls.Single(c => AutomationProperties.GetAutomationId(c) == "OptionalSettingsPanels");
                        var status = controls.Single(c => AutomationProperties.GetAutomationId(c) == "StatusBarText");
                        if (viewport.ActualWidth <= 0 || optional.ActualWidth != 360 || optional.TranslatePoint(new Point(0, optional.ActualHeight), main).Y > status.TranslatePoint(new Point(0, 0), main).Y + 1)
                            throw new Exception("Optional panels consume viewport or overlap status.");
                        Console.WriteLine($"WPF_LAYOUT 1280x800 viewportWidth={viewport.ActualWidth}; optionalWidth={optional.ActualWidth}; OffsetOff bound; Ghost disabled");
                    }
                    finally { main.Close(); }
                }
                finally { window.Close(); model.ShutdownBackendBlocking(); app.Shutdown(); }
            });
        }
        if (args.Length != 2) Check("Evidence export preserves an already existing fixed report", () =>
        {
            // This is the isolated check output, never the user's running GUI output.
            if (!AppContext.BaseDirectory.Contains("PRE-GUI-20261019T063546Z-7f92a1", StringComparison.Ordinal))
                throw new InvalidOperationException("Run checks only from this task's unique build output.");
            var fixedPath = Path.Combine(AppContext.BaseDirectory, "menu-command-report.json");
            if (File.Exists(fixedPath))
                throw new InvalidOperationException("Refuse test output reuse.");
            var marker = "previous evidence must remain byte-identical";
            File.WriteAllText(fixedPath, marker);
            var settings = new AppSettings { BackendMode = "Mock" };
            var vm = new MainWindowViewModel(settings,
                new AppSettingsService(Path.Combine(AppContext.BaseDirectory, "settings-test.json")),
                _ => new MockXpeBackend(new RawImageLoader(), "", false, "", false));
            try
            {
                vm.ExportAutomationReportCommand.Execute(null);
                if (File.ReadAllText(fixedPath) != marker)
                    throw new InvalidOperationException("Existing fixed evidence was overwritten.");
                if (!vm.StatusText.Contains("exported", StringComparison.OrdinalIgnoreCase))
                    throw new InvalidOperationException("A successful unique export was not reported.");
                var first = vm.LastEvidenceSnapshotPath;
                if (first is null || !File.Exists(first))
                    throw new InvalidOperationException("Export path does not identify the actual new evidence.");
                vm.ExportAutomationReportCommand.Execute(null);
                if (File.ReadAllText(fixedPath) != marker)
                    throw new InvalidOperationException("Repeated export touched old evidence.");
                if (vm.LastEvidenceSnapshotPath == first)
                    throw new InvalidOperationException("Repeated export reused a path.");
            }
            finally { vm.ShutdownBackendBlocking(); }
        });
        // F260-1 (Reviewer #260): LastEvidenceSnapshotPath keeps the last SUCCESSFUL snapshot, so a consumer that read it after a failed export would take the previous file for this export's result.
        // The judgment the consumers use (EvidenceExportResult) must refuse, on the same view model that exported successfully a moment before. The failure is a deny rule on the snapshot folder
        // (new files cannot be created, the earlier snapshot stays in place and readable), which is exactly the case in which an old file looks like a fresh success.
        if (args.Length != 2) Check("A failed export is judged as a failure, not as the previous export's success", () =>
        {
            if (!AppContext.BaseDirectory.Contains("PRE-GUI-20261019T063546Z-7f92a1", StringComparison.Ordinal))
                throw new InvalidOperationException("Run checks only from this task's unique build output.");
            var vm = new MainWindowViewModel(new AppSettings { BackendMode = "Mock" },
                new AppSettingsService(Path.Combine(AppContext.BaseDirectory, "settings-test-failed-export.json")),
                _ => new MockXpeBackend(new RawImageLoader(), "", false, "", false));
            var folder = new DirectoryInfo(Path.Combine(GuiDataPaths.Root, "evidence", "snapshots"));
            var deny = new System.Security.AccessControl.FileSystemAccessRule(System.Security.Principal.WindowsIdentity.GetCurrent().User!,
                System.Security.AccessControl.FileSystemRights.CreateFiles, System.Security.AccessControl.InheritanceFlags.None,
                System.Security.AccessControl.PropagationFlags.None, System.Security.AccessControl.AccessControlType.Deny);
            var denied = false;
            try
            {
                var before = vm.LastEvidenceExport;
                vm.ExportAutomationReportCommand.Execute(null);
                var first = EvidenceExportResult.RequireNewSuccess(before, vm.LastEvidenceExport);
                var firstBytes = File.ReadAllBytes(first);
                // a result no new export produced is not a success, whatever it says
                if (EvidenceExportResult.NewSuccessPath(vm.LastEvidenceExport, vm.LastEvidenceExport) is not null)
                    throw new InvalidOperationException("A result that no new export produced was judged a success.");
                try { EvidenceExportResult.RequireNewSuccess(vm.LastEvidenceExport, vm.LastEvidenceExport); throw new InvalidOperationException("RequireNewSuccess accepted a result that no new export produced."); }
                catch (InvalidOperationException error) when (error.Message.Contains("No new export ran", StringComparison.Ordinal)) { }

                var security = folder.GetAccessControl();
                security.AddAccessRule(deny);
                folder.SetAccessControl(security);
                denied = true;
                before = vm.LastEvidenceExport;
                vm.ExportAutomationReportCommand.Execute(null);
                var after = vm.LastEvidenceExport;
                if (after is null || after.ExportId == before?.ExportId || after.SnapshotPath is not null || string.IsNullOrEmpty(after.SnapshotError))
                    throw new InvalidOperationException("The failed export did not leave its own failed result.");
                if (vm.LastEvidenceSnapshotPath != first || !File.ReadAllBytes(first).SequenceEqual(firstBytes))
                    throw new InvalidOperationException("The previous successful evidence was not preserved.");
                if (EvidenceExportResult.NewSuccessPath(before, after) is not null)
                    throw new InvalidOperationException("A failed export was judged a success (the previous file still exists).");
                string? judged = null, refusal = null;
                try { judged = EvidenceExportResult.RequireNewSuccess(before, after); }
                catch (InvalidOperationException error) { refusal = error.Message; }
                if (judged is not null || refusal is null || !refusal.Contains("failed", StringComparison.Ordinal))
                    throw new InvalidOperationException($"A failed export was judged a success ({judged}) or refused without saying why ({refusal}).");
            }
            finally
            {
                if (denied) { var security = folder.GetAccessControl(); security.RemoveAccessRule(deny); folder.SetAccessControl(security); }
                vm.ShutdownBackendBlocking();
            }
        });
        Console.WriteLine($"CALIBRATION_CHECKS failures={failures}");
        return failures == 0 ? 0 : 1;

        void Check(string name, Action test)
        {
            try { test(); Console.WriteLine("PASS " + name); }
            catch (Exception error) { failures++; Console.WriteLine("FAIL " + name + ": " + error.Message); }
        }
    }
}
