using System.IO;
using System.Windows;
using System.Windows.Controls;
using ImageProcTest.Services;
using ImageProcTest.ViewModels;

namespace ImageProcTest.Views;

/// <summary>Korean operator steps. All work uses local tools and unique output directories.</summary>
public sealed class CalibrationWorkflowWindow : Window
{
    private readonly StackPanel _form = new() { Margin = new Thickness(18) };
    private readonly TextBox _log = new() { IsReadOnly = true, TextWrapping = TextWrapping.Wrap, MinHeight = 140, VerticalScrollBarVisibility = ScrollBarVisibility.Auto };
    private readonly List<Control> _inputs = [];
    private CalibrationWorkflowService? _workflow;
    private bool _busy;

    public CalibrationWorkflowWindow(MainWindowViewModel model)
    {
        Title = "PRE 캘리브레이션 — 입력·검사·생성·재로드·평가";
        Width = 940; Height = 820; MinWidth = 720; MinHeight = 500;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        MaxHeight = SystemParameters.WorkArea.Height;
        Content = new ScrollViewer { Content = _form, VerticalScrollBarVisibility = ScrollBarVisibility.Auto };
        Label("이 창은 장비를 제어하지 않습니다. 원본 UINT16LE만 읽고 새 고유 폴더에 저장합니다. 생성/적용 ≠ 실영상 품질 합격.");
        Label("1. 입력 준비: 각 목록은 파일 전체 경로 한 줄당 하나. Dark 전체 목록의 첫 장은 기본 1회 제외됩니다. Gain은 동일 선량, Defect는 별도 다선량 목록을 줄 수 있습니다.");
        var native = Field("로컬 도구/DLL 폴더 (xpe_calib_fixture_gen.exe, xpe_pre_e2e.exe, v5 PRE/common)", Environment.GetEnvironmentVariable("XPE_NATIVE_DIR") ?? "");
        var output = Field("새 실행들을 보관할 출력 상위 폴더 (기존 파일 덮어쓰기 없음)", "");
        var width = Field("가로 화소", model.Settings.RawWidth.ToString());
        var height = Field("세로 화소", model.Settings.RawHeight.ToString());
        var dark = Field("Dark RAW 목록 (취득 순서)", "", true);
        var gain = Field("Gain Bright RAW 목록 (동일 조건/선량)", "", true);
        var defect = Field("Defect Bright RAW 목록 (비우면 Gain 목록 사용)", "", true);
        var keep = new CheckBox { Content = "첫 Dark도 사용 (명시적으로 선택한 경우만 --keep-first-dark)", Margin = new Thickness(0, 4, 0, 8) };
        _form.Children.Add(keep); _inputs.Add(keep);
        var expiry = Field("명시적 XCAL 만료 Unix epoch 밀리초 (정책 자동 결정 안 함; 0=기존 생성기의 만료 미지정)", "0");
        Label("만료 미지정/생성기 session=generated는 detector UUID 또는 임상 사용 승인 증거가 아닙니다. 기존 offset 생성기의 100ms/25°C는 템플릿이며 촬영 실측값으로 쓰지 않습니다.");
        var rawDeclaration = new CheckBox { Content = "촬영기록을 대조했고 입력은 offset 미차감 원시 RAW입니다 (이미 보정된 CalData 등은 넣지 않음)", Margin = new Thickness(0, 6, 0, 6) };
        _form.Children.Add(rawDeclaration); _inputs.Add(rawDeclaration);
        var creationInputs = _inputs.ToArray();
        AddButton("2. 입력 검사 (생성하지 않음)", async () =>
        {
            if (_workflow is null)
            {
                if (rawDeclaration.IsChecked != true) throw new InvalidOperationException("입력 보정상태를 확인하십시오. 촬영조건/보정상태를 수치만으로 추정하지 않습니다.");
                if (string.IsNullOrWhiteSpace(native.Text) || string.IsNullOrWhiteSpace(output.Text)) throw new InvalidOperationException("도구와 출력 폴더를 지정하십시오.");
                _workflow = new CalibrationWorkflowService(output.Text, native.Text,
                    new CalibrationInputs(int.Parse(width.Text), int.Parse(height.Text), Lines(dark.Text), Lines(gain.Text), Lines(defect.Text), keep.IsChecked == true, ulong.Parse(expiry.Text)));
                foreach (var control in creationInputs) control.IsEnabled = false;
            }
            Append("검사: " + await _workflow.ValidateAsync());
        });
        AddButton("3. Offset / Gain / Defect 생성·고유 경로 저장", async () =>
            Append("생성: " + await Required().GenerateAsync() + "\n세트: " + Required().SetDirectory));
        AddButton("4. 저장된 세트 재로드 (별도 PRE 프로세스 공개 API)", async () =>
            Append("재로드: " + await Required().ReloadAsync() + "\n주의: 실영상 품질은 아직 미검증"));
        AddButton("5. 이 GUI의 경로로 적용 (자동 Run 아님)", () =>
        {
            Required().VerifyReadyToApply();
            if (model.IsBackendTransitioning) throw new InvalidOperationException("백엔드 전환 중입니다. 완료 후 적용하십시오.");
            if (model.IsMockBackend) throw new InvalidOperationException("Mock backend는 실제 PRE 적용이 아닙니다. Native backend를 확인하십시오.");
            NativeRuntimeEvidence.VerifyPreCombination(Required().NativeDirectory);
            CalibrationWorkflowService.WriteNew(Path.Combine(Required().RunDirectory, "gui-apply-" + Guid.NewGuid().ToString("N") + ".json"),
                new { native = NativeRuntimeEvidence.Capture(), set = Required().SetDirectory, quality = "경로 적용 준비 검증; 실제 Run/품질 합격 아님" });
            model.Settings.OffsetCalibrationDirectory = Required().SetDirectory;
            model.Settings.GainCalibrationDirectory = Required().SetDirectory;
            model.Settings.DefectCalibrationDirectory = Required().SetDirectory;
            Append("경로 적용 완료. 창을 닫고 평가 RAW를 열어 Run Preprocessing 실행. 실제 Native/Applied와 경고를 확인하십시오. 품질 합격 아님.");
            return Task.CompletedTask;
        });
        Label("6. 독립영상 평가: 생성 입력(첫 제외 Dark 포함)과 SHA가 겹치면 거부. detector-domain 기본 PRE 출력 기준, enhancement/VOI 전 평가. 촬영 조건·정답 출처는 별도 기록과 대조해야 합니다.");
        var holdout = Field("평가용 RAW 전체 경로", "");
        var kind = new ComboBox { ItemsSource = new[] { "image", "dark", "flat" }, SelectedIndex = 0, Margin = new Thickness(0, 4, 0, 8) };
        _form.Children.Add(kind);
        Label("Gain 의미 (사용자 근거로 선택; 모르면 unknown 유지, 조건부 FPN 평가는 미검증)");
        var semantics = new ComboBox { ItemsSource = new[] { "unknown", "normalized_gain", "reciprocal_gain" }, SelectedIndex = 0 };
        _form.Children.Add(semantics);
        var oracle = Field("선택: 독립 외부 defect oracle UINT8 (0 good / nonzero bad), 원본 RAW 아님", "");
        var reference = Field("선택: 독립 reference FLOAT32LE (동일 단계·정렬·크기·ADU), 원본 RAW 아님", "");
        AddButton("6. 독립 평가 및 보고서 저장", async () =>
        {
            var result = await Required().EvaluateAsync(holdout.Text, (string)kind.SelectedItem, oracle.Text, reference.Text, (string)semantics.SelectedItem);
            Append("평가 산출물: " + result + "\n" + CalibrationWorkflowService.QualitySummary(result)
                + "\n종료코드 0은 실행 성공입니다. oracle/reference 미제공 항목은 미검증입니다.");
        });
        Label("같은 입력의 재생성/수정은 이 창을 닫고 새로 여십시오. 결과 폴더의 inputs/manifest/reload/evaluate 보고서와 원본 촬영기록을 함께 보관하십시오. 외부 oracle/ref의 독립성은 SHA 비중복만으로 증명되지 않습니다.");
        _form.Children.Add(_log);
        Closing += (_, e) => { if (_busy) { e.Cancel = true; Append("현재 로컬 작업이 끝날 때까지 창을 유지합니다. 다른 프로세스를 중단하지 않습니다."); } };
    }

    private static string[] Lines(string text) => text.Split(['\r', '\n'], StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
    private CalibrationWorkflowService Required() => _workflow ?? throw new InvalidOperationException("먼저 입력 검사를 실행하십시오.");
    private void Label(string text) => _form.Children.Add(new TextBlock { Text = text, TextWrapping = TextWrapping.Wrap, Margin = new Thickness(0, 6, 0, 6) });
    private TextBox Field(string label, string value, bool multiple = false)
    {
        Label(label);
        var box = new TextBox { Text = value, AcceptsReturn = multiple, MinHeight = multiple ? 72 : 26, VerticalScrollBarVisibility = multiple ? ScrollBarVisibility.Auto : ScrollBarVisibility.Disabled };
        _form.Children.Add(box); _inputs.Add(box); return box;
    }
    private void AddButton(string title, Func<Task> action)
    {
        var button = new Button { Content = title, HorizontalAlignment = HorizontalAlignment.Left, Padding = new Thickness(12, 6, 12, 6), Margin = new Thickness(0, 8, 0, 4) };
        button.Click += async (_, _) =>
        {
            if (_busy) return;
            _busy = true;
            Append("실행 중: " + title + " — 완료될 때까지 기다려 주십시오. 원본/다른 프로세스는 유지합니다.");
            try { await action(); }
            catch (Exception error) { Append("실패/미완료: " + error.Message); }
            finally { _busy = false; }
        };
        _form.Children.Add(button);
    }
    private void Append(string message) { _log.AppendText(message + Environment.NewLine); _log.ScrollToEnd(); }
}
