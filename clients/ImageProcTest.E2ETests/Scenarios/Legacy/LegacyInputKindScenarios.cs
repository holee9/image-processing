// GUI-C-253c (#273, Codex #241): the real connection of the input-kind choice, in the legacy diagnostic app (clients/ImageProcTest): the combo box reaches the preview service, and choosing another kind
// invalidates only the measured rows. UI Automation only (no key, no mouse); the native calls of the app go to the real xpe_preprocess.dll.
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Legacy;

/// <summary>
/// Needs <c>XPE_NATIVE_DIR</c> (a folder with the module DLLs, xpe_preprocess.dll of dev/preprocess 026ea384 or later: it must export xpe_verify_offset_signed_file). Otherwise it is skipped with a reason
/// (XPE-SKIP-ALLOWED:273). The scenario builds its own 1024x1024 target frame and calibration folder (a file named cdark.raw is the offset source) in a temp folder.
/// </summary>
public sealed class LegacyInputKindScenarios(ITestOutputHelper output)
{
    private const int Side = 1024;

    private static void WriteRaw(string path, Func<int, ushort> value)
    {
        var pixels = new ushort[Side * Side];
        for (var i = 0; i < pixels.Length; i++) pixels[i] = value(i);
        var bytes = new byte[pixels.Length * sizeof(ushort)];
        Buffer.BlockCopy(pixels, 0, bytes, 0, bytes.Length);
        File.WriteAllBytes(path, bytes);
    }

    private static int ArtifactFolders(string appDirectory)
    {
        var root = Path.Combine(appDirectory, "fixture-preview-artifacts");
        return Directory.Exists(root) ? Directory.GetDirectories(root).Length : 0;
    }

    [SkippableFact]
    public void K1_TheInputKindChoice_ReachesThePreview_AndChoosingAnotherKind_InvalidatesOnlyTheMeasuredRows()
    {
        var nativeDir = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
        Skip.If(string.IsNullOrEmpty(nativeDir) || !File.Exists(Path.Combine(nativeDir, "xpe_preprocess.dll")),
            "XPE_NATIVE_DIR does not name a folder containing xpe_preprocess.dll. XPE-SKIP-ALLOWED:273");
        Skip.If(!System.Text.Encoding.ASCII.GetString(File.ReadAllBytes(Path.Combine(nativeDir!, "xpe_preprocess.dll"))).Contains("xpe_verify_offset_signed_file", StringComparison.Ordinal),
            "this xpe_preprocess.dll does not export xpe_verify_offset_signed_file (older than dev/preprocess 026ea384). XPE-SKIP-ALLOWED:273");

        var exe = LegacyPreprocessReadinessScenarios.LegacyApp.FindExecutable();
        Skip.If(exe is null, "clients/ImageProcTest/bin/Debug/net8.0-windows/ImageProcTest.exe was not found: build clients/ImageProcTest first. XPE-SKIP-ALLOWED:273");
        var appDirectory = Path.GetDirectoryName(exe)!;

        var work = Path.Combine(Path.GetTempPath(), $"xpe_c253c_{Guid.NewGuid():N}");
        var calibration = Path.Combine(work, "calibration");
        Directory.CreateDirectory(calibration);
        var target = Path.Combine(work, "target.raw");
        WriteRaw(target, i => (ushort)(1000 + i % 97));
        WriteRaw(Path.Combine(calibration, "cdark.raw"), i => (ushort)(100 + i % 7));   // the offset source (role by name)

        try
        {
            using var app = LegacyPreprocessReadinessScenarios.LegacyApp.LaunchOrSkip(breakTemp: false);

            // 1. a raw frame and the calibration folder, the offset stage on: the preview runs with the DEFAULT kind (미확인)
            app.LoadRawThroughDialog(target);
            app.LoadCalibrationFolderThroughDialog(calibration);
            if (!app.IsChecked("Evaluation", "OffsetEnabledCheckBox")) app.ToggleCheckBox("Evaluation", "OffsetEnabledCheckBox");
            app.ClickButtonByIdWhenEnabled("Evaluation", "ApplyNativePreviewButton", TimeSpan.FromSeconds(60));   // "Run Selected": a stage switch only says "run selected to apply"
            var ran = app.WaitForTextOnTab("Evaluation", "NativePreviewText", t => t.Contains("latency=", StringComparison.Ordinal), TimeSpan.FromSeconds(90));
            output.WriteLine("1. preview text: " + ran);
            Assert.Equal("미확인", app.SelectedInputKind());
            var rows = app.ReadGridRows("DarkMetricsGrid");
            foreach (var row in rows) output.WriteLine("   1 | " + row);
            var four = rows.Where(r => !IsMeasured(r)).ToList();
            Assert.Equal(4, four.Count);                                           // the existing four rows
            Assert.Contains(rows, r => r.Contains("Metric = 측정값 기준", StringComparison.Ordinal) && r.Contains("미측정 — 다크 프레임 미확인", StringComparison.Ordinal));
            Assert.DoesNotContain(rows, r => r.Contains("Status = REPORTED", StringComparison.Ordinal));
            var folders = ArtifactFolders(appDirectory);
            var previewText = app.ReadText("Evaluation", "NativePreviewText");

            // 2. choose "다크 프레임" WITHOUT running: the measured rows say a new run is needed; nothing else moves and nothing runs
            app.SelectInputKind(1);
            Assert.Equal("다크 프레임", app.SelectedInputKind());
            rows = app.ReadGridRows("DarkMetricsGrid");
            foreach (var row in rows) output.WriteLine("   2 | " + row);
            Assert.Equal(four, rows.Where(r => !IsMeasured(r)).ToList());          // the four existing rows and their verdicts are exactly as they were
            Assert.Contains(rows, r => r.Contains("Metric = 측정값 기준", StringComparison.Ordinal) && r.Contains("미측정 — 입력 종류 변경 — 다시 실행 필요", StringComparison.Ordinal));
            Assert.DoesNotContain(rows, r => r.Contains("Status = REPORTED", StringComparison.Ordinal));
            Assert.Equal(folders, ArtifactFolders(appDirectory));                  // no preview ran: each run writes a new artifact folder
            Assert.Equal(previewText, app.ReadText("Evaluation", "NativePreviewText"));

            // 3. run again ("Run Selected"): the service receives "다크 프레임" and the module measures
            app.ClickButtonByIdWhenEnabled("Evaluation", "ApplyNativePreviewButton", TimeSpan.FromSeconds(60));
            app.WaitForTextOnTab("Evaluation", "NativePreviewText", t => t.Contains("latency=", StringComparison.Ordinal) && t != previewText, TimeSpan.FromSeconds(90));
            rows = app.ReadGridRows("DarkMetricsGrid");
            foreach (var row in rows) output.WriteLine("   3 | " + row);
            Assert.Equal(folders + 1, ArtifactFolders(appDirectory));
            Assert.Contains(rows, r => r.Contains("Metric = 측정값 기준", StringComparison.Ordinal) && r.Contains("입력 종류: 다크 프레임(사용자 선택, 취득 메타데이터로 확인하지 않음)", StringComparison.Ordinal));
            var reported = rows.Where(r => r.Contains("Status = REPORTED", StringComparison.Ordinal)).ToList();
            Assert.True(reported.Count >= 7, $"expected the measured rows of a confirmed dark, found {reported.Count}");
            Assert.Contains(reported, r => r.Contains("Metric = DarkBias 부호 있음", StringComparison.Ordinal));
            var fourAfterRun = rows.Where(r => !IsMeasured(r)).ToList();
            Assert.Equal(4, fourAfterRun.Count);
            folders = ArtifactFolders(appDirectory);
            previewText = app.ReadText("Evaluation", "NativePreviewText");

            // 4. dark -> clinical: the numbers go at once, with the clinical reason; then clinical -> dark does NOT bring them back
            app.SelectInputKind(3);
            rows = app.ReadGridRows("DarkMetricsGrid");
            foreach (var row in rows) output.WriteLine("   4 | " + row);
            Assert.Equal(fourAfterRun, rows.Where(r => !IsMeasured(r)).ToList());
            Assert.DoesNotContain(rows, r => r.Contains("Status = REPORTED", StringComparison.Ordinal));
            Assert.Contains(rows, r => r.Contains("다크 프레임이 아님(입력 종류: 임상·기타 영상)", StringComparison.Ordinal));
            app.SelectInputKind(1);
            rows = app.ReadGridRows("DarkMetricsGrid");
            foreach (var row in rows) output.WriteLine("   5 | " + row);
            Assert.DoesNotContain(rows, r => r.Contains("Status = REPORTED", StringComparison.Ordinal));
            Assert.Contains(rows, r => r.Contains("입력 종류 변경 — 다시 실행 필요", StringComparison.Ordinal));
            Assert.Equal(folders, ArtifactFolders(appDirectory));
            Assert.Equal(previewText, app.ReadText("Evaluation", "NativePreviewText"));

            // 6. a new file: the choice goes back to 미확인
            WriteRaw(Path.Combine(work, "second.raw"), i => (ushort)(2000 + i % 89));
            app.LoadRawThroughDialog(Path.Combine(work, "second.raw"));
            Assert.Equal("미확인", app.SelectedInputKind());
            output.WriteLine("6. after opening another file the kind reads: " + app.SelectedInputKind());
        }
        finally
        {
            try { Directory.Delete(work, recursive: true); } catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { /* a file the app still holds */ }
        }
    }

    /// <summary>A row of the measured set (the base row and the signed rows), recognised by the metric names the app gives them.</summary>
    private static bool IsMeasured(string row) =>
        row.Contains("Metric = 측정값 기준", StringComparison.Ordinal) || row.Contains("Metric = DarkBias 부호", StringComparison.Ordinal) || row.Contains("Metric = DSNU 부호", StringComparison.Ordinal)
        || row.Contains("Metric = DarkReduction 부호", StringComparison.Ordinal) || row.Contains("Metric = DarkBias 출력", StringComparison.Ordinal) || row.Contains("Metric = DSNU 출력", StringComparison.Ordinal)
        || row.Contains("Metric = ClampRate x<0", StringComparison.Ordinal) || row.Contains("Metric = 출력 0 비율", StringComparison.Ordinal) || row.Contains("Metric = Offset 맵 SHA-256", StringComparison.Ordinal);
}
