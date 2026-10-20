using System.Globalization;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Windows;
using System.Windows.Automation;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using ImageProcTest.Services;
using ImageProcTest.Services.Comparison;

namespace ImageProcTest.Views;

/// <summary>
/// QA-A-273 (Refs #278): the first slice of the 2G / new PRE comparison workbench. It OPENS a manifest naming two EXISTING output files (A = official Samsung 2G X64W, B = new PRE), lets the contract
/// SCRIPT (a child process, not a C# copy) judge it, shows the script's verdict with its reason codes, shows A and B with ONE window/level, ONE zoom and the SAME ROI, has the script compute the signed
/// A-minus-B statistics from the original arrays (never from what is drawn) and shows where the result was saved. It never loads a vendor DLL, never runs an engine, never judges image quality.
/// The live 2G action is disabled and says why. A script that cannot be run, or whose answer is malformed, is a FAILURE and is never shown as a pass.
/// </summary>
public sealed class ComparisonImportWindow : Window
{
    private readonly Func<string, string, string, IComparisonBackend> _backendFactory;
    private readonly string _rememberFile = System.IO.Path.Combine(ComparisonPaths.DataRoot, "comparison-import.json");

    private readonly TextBox _scriptPath = new() { MinWidth = 420 };
    private readonly TextBox _pythonPath = new() { MinWidth = 420 };
    private readonly TextBox _manifestPath = new() { MinWidth = 420 };
    private readonly TextBox _outputRoot = new() { MinWidth = 420 };
    private readonly TextBlock _state = new() { FontWeight = FontWeights.SemiBold, TextWrapping = TextWrapping.Wrap };
    private readonly TextBlock _review = new() { TextWrapping = TextWrapping.Wrap };
    private readonly TextBlock _verdict = new() { FontWeight = FontWeights.SemiBold, TextWrapping = TextWrapping.Wrap };
    private readonly TextBox _reasons = new() { IsReadOnly = true, TextWrapping = TextWrapping.Wrap, MinHeight = 56, MaxHeight = 140, VerticalScrollBarVisibility = ScrollBarVisibility.Auto };
    private readonly TextBlock _engines = new() { TextWrapping = TextWrapping.Wrap };
    private readonly TextBox _window = new() { Width = 100 };
    private readonly TextBox _level = new() { Width = 100 };
    private readonly TextBlock _roiText = new() { TextWrapping = TextWrapping.Wrap };
    private readonly TextBlock _viewHint = new() { TextWrapping = TextWrapping.Wrap };
    private readonly TextBox _result = new() { IsReadOnly = true, TextWrapping = TextWrapping.Wrap, MinHeight = 96, VerticalScrollBarVisibility = ScrollBarVisibility.Auto };
    private readonly Button _openButton = new() { Content = "열기·검증" };
    private readonly Button _computeButton = new() { Content = "A−B 통계 계산·저장", IsEnabled = false };
    private readonly Button _cancelButton = new() { Content = "취소", IsEnabled = false };
    private readonly Button _applyWl = new() { Content = "W/L 적용", IsEnabled = false };
    private readonly Button _fit = new() { Content = "맞춤", IsEnabled = false };
    private readonly Button _actual = new() { Content = "100%", IsEnabled = false };
    private readonly Button _zoomIn = new() { Content = "+", IsEnabled = false };
    private readonly Button _zoomOut = new() { Content = "−", IsEnabled = false };
    private readonly Image _imageA = new() { Stretch = Stretch.None, SnapsToDevicePixels = true };
    private readonly Image _imageB = new() { Stretch = Stretch.None, SnapsToDevicePixels = true };
    private readonly Canvas _overlayA = new(), _overlayB = new();
    private readonly Grid _hostA = new(), _hostB = new();
    private readonly ScrollViewer _scrollA = new() { HorizontalScrollBarVisibility = ScrollBarVisibility.Auto, VerticalScrollBarVisibility = ScrollBarVisibility.Auto, MinHeight = 240 };
    private readonly ScrollViewer _scrollB = new() { HorizontalScrollBarVisibility = ScrollBarVisibility.Auto, VerticalScrollBarVisibility = ScrollBarVisibility.Auto, MinHeight = 240 };
    private readonly ScaleTransform _scaleA = new(1, 1), _scaleB = new(1, 1);

    private ManifestSnapshot? _snapshot;   // what the user confirmed: validation, preview and computation are all bound to it
    private JsonNode? _manifest;
    private string _baseDir = "";
    private bool _closed;
    private IComparisonBackend? _backend;
    private ValidateResponse? _validation;
    private double[]? _pixelsA, _pixelsB;
    private int _displayWidth, _displayHeight, _step = 1;
    private double _dataMin, _dataMax, _zoom = 1.0;
    private int _generation;
    private bool _busy, _syncing;
    private CancellationTokenSource? _cancel;

    public ComparisonImportWindow() : this((script, python, outRoot) => new PythonScriptComparisonBackend(script, python, outRoot)) { }

    /// <summary>The factory lets a check substitute the backend; the shipped path always uses the child-process client.</summary>
    public ComparisonImportWindow(Func<string, string, string, IComparisonBackend> backendFactory)
    {
        _backendFactory = backendFactory;
        Title = "2G · 신규 PRE 출력 가져오기 비교";
        Width = 1180; Height = 900; MinWidth = 820; MinHeight = 560;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        MaxHeight = SystemParameters.WorkArea.Height;
        Auto(this, "ComparisonImportWindow");

        var root = new StackPanel { Margin = new Thickness(14) };
        root.Children.Add(Label("이 창은 이미 존재하는 출력 파일 두 개(A = 공식 Samsung 2G X64W, B = 신규 PRE)를 가져와 같은 조건으로 보여 주고 A−B 수치를 계산합니다. 제조사 DLL을 불러오거나 실행하지 않고, 장비를 제어하지 않으며, 화질을 판정하지 않습니다. 계산된 차이는 두 파일에 대한 수치 사실일 뿐 개선·합격의 근거가 아닙니다. 검증과 계산은 이 창이 다시 구현하지 않고 계약 스크립트(자식 프로세스)가 합니다."));
        var provenance = new TextBlock { Text = "합성 배선 검증 / 제조사 미실측 — 이 창은 입력의 출처(실제 영상인지, 제조사 생산 결과인지)를 확인하지 않습니다. 계약에는 출처 확인 필드가 없으므로 어떤 결과도 '생산 이력 확인됨'으로 읽으면 안 됩니다.", TextWrapping = TextWrapping.Wrap, FontWeight = FontWeights.Bold, Foreground = Brushes.White, Background = Brushes.DarkOrange, Padding = new Thickness(8, 4, 8, 4), Margin = new Thickness(0, 2, 0, 4) };
        Auto(provenance, "ComparisonProvenanceNotice");
        root.Children.Add(provenance);
        Auto(_state, "ComparisonStateText");
        _state.Text = "상태: 매니페스트를 아직 열지 않았습니다. 가져온 파일 비교이며 2G 실행이 아닙니다.";
        root.Children.Add(_state);

        root.Children.Add(Label("계약 스크립트 경로 (tools/calibration/comparison_manifest.py — 기본값 없음, 마지막에 쓴 값을 기억합니다)"));
        root.Children.Add(_scriptPath); Auto(_scriptPath, "ComparisonScriptPathBox");
        root.Children.Add(Label("python 실행 파일 (비우면 PATH 의 python)"));
        root.Children.Add(_pythonPath); Auto(_pythonPath, "ComparisonPythonPathBox");
        Auto(_review, "ComparisonContractReviewState");
        root.Children.Add(_review);

        root.Children.Add(Label("매니페스트(JSON, 스키마 xpe-2g-pre-comparison/4). 스크립트는 이 폴더를 읽기만 합니다."));
        var pathRow = new DockPanel { Margin = new Thickness(0, 2, 0, 4) };
        var browse = new Button { Content = "찾아보기…", Margin = new Thickness(6, 0, 0, 0), Padding = new Thickness(10, 2, 10, 2) };
        _openButton.Margin = new Thickness(6, 0, 0, 0); _openButton.Padding = new Thickness(10, 2, 10, 2);
        DockPanel.SetDock(_openButton, Dock.Right); DockPanel.SetDock(browse, Dock.Right);
        pathRow.Children.Add(_openButton); pathRow.Children.Add(browse); pathRow.Children.Add(_manifestPath);
        root.Children.Add(pathRow);
        Auto(_manifestPath, "ComparisonManifestPathBox"); Auto(browse, "ComparisonBrowseButton"); Auto(_openButton, "ComparisonOpenButton");

        root.Children.Add(Label("결과를 저장할 개발 산출물 폴더(원데이터 폴더 밖이어야 합니다. 실행마다 새 하위 폴더가 만들어지고 기존 파일은 덮어쓰지 않습니다)"));
        _outputRoot.Text = System.IO.Path.Combine(ComparisonPaths.DataRoot, "comparison-runs");
        Auto(_outputRoot, "ComparisonOutputRootBox");
        root.Children.Add(_outputRoot);

        Auto(_verdict, "ComparisonValidationStatus"); Auto(_reasons, "ComparisonReasonsList"); Auto(_engines, "ComparisonEngineSummary");
        root.Children.Add(_verdict); root.Children.Add(_reasons); root.Children.Add(_engines);

        // display controls: one window/level, one zoom, the same ROI
        var controls = new WrapPanel { Margin = new Thickness(0, 8, 0, 4) };
        controls.Children.Add(new TextBlock { Text = "표시 W/L(두 영상 공통, 표시 전용):  Window ", VerticalAlignment = VerticalAlignment.Center });
        controls.Children.Add(_window);
        controls.Children.Add(new TextBlock { Text = "  Level ", VerticalAlignment = VerticalAlignment.Center });
        controls.Children.Add(_level);
        foreach (var b in new[] { _applyWl, _fit, _actual, _zoomIn, _zoomOut }) { b.Margin = new Thickness(6, 0, 0, 0); b.Padding = new Thickness(10, 2, 10, 2); controls.Children.Add(b); }
        Auto(_window, "ComparisonWindowBox"); Auto(_level, "ComparisonLevelBox"); Auto(_applyWl, "ComparisonApplyWlButton"); Auto(_fit, "ComparisonZoomFitButton");
        Auto(_actual, "ComparisonZoom100Button"); Auto(_zoomIn, "ComparisonZoomInButton"); Auto(_zoomOut, "ComparisonZoomOutButton");
        root.Children.Add(controls);
        Auto(_roiText, "ComparisonRoiText"); Auto(_viewHint, "ComparisonViewHint");
        root.Children.Add(_roiText); root.Children.Add(_viewHint);

        var viewers = new UniformGrid { Columns = 2, Margin = new Thickness(0, 4, 0, 4) };
        viewers.Children.Add(Viewer("A — 공식 Samsung 2G X64W (가져온 출력)", _scrollA, _hostA, _imageA, _overlayA, _scaleA, "ComparisonViewerA"));
        viewers.Children.Add(Viewer("B — 신규 PRE (가져온 출력)", _scrollB, _hostB, _imageB, _overlayB, _scaleB, "ComparisonViewerB"));
        root.Children.Add(viewers);

        var actions = new WrapPanel { Margin = new Thickness(0, 6, 0, 2) };
        _computeButton.Padding = new Thickness(12, 6, 12, 6);
        _cancelButton.Padding = new Thickness(12, 6, 12, 6); _cancelButton.Margin = new Thickness(8, 0, 0, 0);
        var live = new Button { Content = "실제 2G 실행", IsEnabled = false, Padding = new Thickness(12, 6, 12, 6), Margin = new Thickness(8, 0, 0, 0) };
        actions.Children.Add(_computeButton); actions.Children.Add(_cancelButton); actions.Children.Add(live);
        Auto(_computeButton, "ComparisonComputeButton"); Auto(_cancelButton, "ComparisonCancelButton"); Auto(live, "ComparisonLive2gButton");
        root.Children.Add(actions);
        var liveReason = Label("실제 2G 실행은 막혀 있습니다 (G-2G-READY blocked): 오프라인 핸들 생성·초기화, 입출력 버퍼와 소유권, 배타 접근, 적용 가능한 사용 조건, 원본 RAW·맵의 출처가 아직 정의·허가되지 않았습니다. 이 창은 제조사 DLL을 불러오지 않습니다.");
        Auto(liveReason, "ComparisonLive2gReason");
        root.Children.Add(liveReason);
        Auto(_result, "ComparisonResultBox");
        root.Children.Add(_result);

        Content = new ScrollViewer { Content = root, VerticalScrollBarVisibility = ScrollBarVisibility.Auto };
        LoadRemembered();
        UpdateReviewLine(null);

        browse.Click += (_, _) =>
        {
            var dlg = new Microsoft.Win32.OpenFileDialog { Filter = "비교 매니페스트 (*.json)|*.json|모든 파일|*.*", CheckFileExists = true };
            if (dlg.ShowDialog(this) == true) _manifestPath.Text = dlg.FileName;
        };
        _openButton.Click += async (_, _) => await OpenAsync();
        _computeButton.Click += async (_, _) => await ComputeAsync();
        _cancelButton.Click += (_, _) => _cancel?.Cancel();
        _applyWl.Click += (_, _) => RenderBoth(useTextWl: true);
        _fit.Click += (_, _) => ZoomFit();
        _actual.Click += (_, _) => SetZoom(1.0 / _step);
        _zoomIn.Click += (_, _) => SetZoom(_zoom * 1.25);
        _zoomOut.Click += (_, _) => SetZoom(_zoom / 1.25);
        _scrollA.ScrollChanged += (_, _) => Sync(_scrollA, _scrollB);
        _scrollB.ScrollChanged += (_, _) => Sync(_scrollB, _scrollA);
        _manifestPath.TextChanged += (_, _) => Invalidate("매니페스트 경로가 바뀌었습니다. 다시 열기·검증을 누르십시오.");
        _scriptPath.TextChanged += (_, _) => Invalidate("계약 스크립트 경로가 바뀌었습니다. 다시 열기·검증을 누르십시오.");
        _pythonPath.TextChanged += (_, _) => Invalidate("python 경로가 바뀌었습니다. 다시 열기·검증을 누르십시오.");
        _outputRoot.TextChanged += (_, _) => Invalidate("결과 폴더가 바뀌었습니다. 다시 열기·검증을 누르십시오.");
        Closing += (_, _) => { _closed = true; _generation++; try { _cancel?.Cancel(); } catch (ObjectDisposedException) { } };   // closing is never blocked: the child process is stopped by the token and no late result is accepted
    }

    private static TextBlock Label(string text) => new() { Text = text, TextWrapping = TextWrapping.Wrap, Margin = new Thickness(0, 4, 0, 4) };
    private static void Auto(DependencyObject d, string id) => AutomationProperties.SetAutomationId(d, id);

    private static UIElement Viewer(string title, ScrollViewer scroll, Grid host, Image image, Canvas overlay, ScaleTransform scale, string id)
    {
        host.Children.Add(image); host.Children.Add(overlay);
        host.LayoutTransform = scale;
        scroll.Content = host;
        scroll.Background = Brushes.Black;
        Auto(scroll, id);
        var panel = new StackPanel { Margin = new Thickness(0, 0, 6, 0) };
        panel.Children.Add(new TextBlock { Text = title, FontWeight = FontWeights.SemiBold, Margin = new Thickness(0, 0, 0, 2) });
        panel.Children.Add(scroll);
        return panel;
    }

    private void LoadRemembered()
    {
        try
        {
            if (!File.Exists(_rememberFile)) return;
            var o = JsonNode.Parse(File.ReadAllText(_rememberFile)) as JsonObject;
            _scriptPath.Text = o?["script_path"]?.GetValue<string>() ?? "";
            _pythonPath.Text = o?["python_path"]?.GetValue<string>() ?? "";
        }
        catch (Exception e) when (e is IOException or JsonException or InvalidOperationException) { /* a missing or damaged memory file is just an empty field */ }
    }

    private void Remember()
    {
        try
        {
            Directory.CreateDirectory(ComparisonPaths.DataRoot);
            var o = new JsonObject { ["script_path"] = _scriptPath.Text.Trim(), ["python_path"] = _pythonPath.Text.Trim() };
            AtomicFile.WriteAllBytes(_rememberFile, System.Text.Encoding.UTF8.GetBytes(o.ToJsonString()));
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException) { /* remembering is a convenience, never a reason to fail */ }
    }

    /// <summary>The contract review state is shown on every answer: an unreviewed script's answer is never presented as a plain pass.</summary>
    private void UpdateReviewLine(ScriptIdentity? id)
    {
        if (id is null)
        {
            _review.Text = "계약 검토 상태: 스크립트를 아직 실행하지 않았습니다.";
            return;
        }
        _review.Text = $"실행한 스크립트(요청마다 고정 사본): {id.PinnedPath}\n원본 {id.OriginalPath}\n고정 사본 SHA-256 실행 전 {id.PinnedShaBefore} / 실행 후 {id.PinnedShaAfter ?? "읽지 못함"}\n원본 SHA-256 실행 후 {id.OriginalShaAfter ?? "읽지 못함"}\npython {id.Python.Version} · NumPy {id.Python.NumpyVersion} · {id.Python.ExePath}\n계약 검토 상태: "
            + (id.Reviewed ? "검토를 통과한 스크립트와 SHA-256 이 같습니다(실행 전·후 모두)."
               : "검토 안 된 계약 — 실행한 사본의 SHA-256 이 검토를 통과한 값과 다르거나 실행 전후가 다릅니다. 아래 답은 비교 가능(통과)으로 보지 마십시오.");
    }

    private void SetBusy(bool busy)
    {
        _busy = busy;
        _openButton.IsEnabled = !busy;
        _cancelButton.IsEnabled = busy;
        _computeButton.IsEnabled = !busy && _validation?.Ok == true && _validation.Validation?["comparable"]?.GetValue<bool>() == true;
    }

    /// <summary>Drops everything derived from the previous inputs: a result must never outlive the inputs it describes.</summary>
    private void Invalidate(string why)
    {
        _generation++;
        _snapshot = null; _manifest = null; _validation = null; _backend = null; _pixelsA = _pixelsB = null;
        _imageA.Source = _imageB.Source = null; _overlayA.Children.Clear(); _overlayB.Children.Clear();
        if (!_busy) _computeButton.IsEnabled = false;
        foreach (var b in new[] { _applyWl, _fit, _actual, _zoomIn, _zoomOut }) b.IsEnabled = false;
        _verdict.Text = ""; _reasons.Text = ""; _engines.Text = ""; _roiText.Text = ""; _viewHint.Text = "";
        _result.Text = "";
        _state.Text = "상태: " + why + " 가져온 파일 비교이며 2G 실행이 아닙니다.";
        UpdateReviewLine(null);
    }

    private async Task OpenAsync()
    {
        if (_busy) return;
        var path = _manifestPath.Text.Trim();
        Invalidate("검증 중입니다(계약 스크립트를 자식 프로세스로 실행).");
        var gen = _generation;
        SetBusy(true);
        var cts = new CancellationTokenSource();
        _cancel = cts;
        try
        {
            if (!File.Exists(path)) throw new IOException("매니페스트 파일이 없습니다: " + path);
            Remember();
            var snapshot = ManifestSnapshot.Create(path);   // one read: these bytes are what is validated, previewed and computed
            var backend = _backendFactory(_scriptPath.Text.Trim(), _pythonPath.Text.Trim(), _outputRoot.Text.Trim());
            var response = await backend.ValidateAsync(snapshot, cts.Token);
            if (gen != _generation || _closed) return;   // an input changed while the script ran: this answer is stale
            _snapshot = snapshot; _manifest = snapshot.Json; _baseDir = snapshot.BaseDir; _backend = backend; _validation = response;
            UpdateReviewLine(response.Script);
            ShowValidation();
            if (response.Ok && response.Validation?["comparable"]?.GetValue<bool>() == true) await LoadDisplayAsync(gen, cts.Token);
        }
        catch (OperationCanceledException)
        {
            if (gen != _generation || _closed) return;
            Invalidate("취소했습니다. 읽던 배열·화면·성공 상태를 받아들이지 않았습니다.");
            _verdict.Text = "취소 — 통과가 아닙니다.";
        }
        catch (Exception e)
        {
            if (gen != _generation || _closed) return;
            _verdict.Text = "비교 불가 — 열지 못했습니다.";
            _reasons.Text = e.Message;
            _state.Text = "상태: 열기 실패. 가져온 파일 비교이며 2G 실행이 아닙니다. 실패는 통과가 아닙니다.";
        }
        finally { if (ReferenceEquals(_cancel, cts)) _cancel = null; cts.Dispose(); if (!_closed) SetBusy(false); }
    }

    private void ShowValidation()
    {
        var r = _validation!;
        if (!r.Ok)
        {
            _verdict.Text = "실패 — 계약 스크립트의 답을 얻지 못했습니다. 통과가 아닙니다.";
            _reasons.Text = r.Failure + (string.IsNullOrWhiteSpace(r.StderrTail) ? "" : Environment.NewLine + "stderr: " + r.StderrTail);
            _state.Text = "상태: 검증 실패. 가져온 파일 비교이며 2G 실행이 아닙니다.";
            return;
        }
        var v = r.Validation!;
        var comparable = v["comparable"]!.GetValue<bool>();
        var kind = v["comparison_kind"]?.GetValue<string>() ?? "?";
        var attribution = v["algorithm_attribution"]?.GetValue<bool>() == true;
        var reviewed = r.Script?.Reviewed == true;
        _verdict.Text = (comparable ? $"스크립트의 답: 비교 가능 — 종류 {kind}" : "스크립트의 답: 비교 불가 — 아래 사유가 해소되기 전에는 숫자를 만들지 않습니다.")
            + (comparable ? $" · 계약상 귀속 조건: {(attribution ? "충족(생산 이력·품질 확인 아님)" : "해당 없음(비귀속 라벨)")}" : "")
            + (reviewed ? "" : "  [검토 안 된 계약]");
        var lines = ((JsonArray)v["reasons"]!).Select(x => x!.GetValue<string>()).ToList();
        var blockers = (v["blockers"] as JsonArray)?.Select(x => x!.GetValue<string>()) ?? [];
        _reasons.Text = string.Join(Environment.NewLine, lines.Concat(blockers).Select(c => $"{c}: {ComparisonReasonText.For(c)}"));
        string Eng(string key)
        {
            var e = _manifest?["engines"]?[key];
            var o = e?["output"];
            string S(JsonNode? n) => n is JsonValue jv && jv.GetValueKind() == JsonValueKind.String ? jv.GetValue<string>() : "?";
            return $"{key}: 단계 {S(o?["stage"])} · 자료형 {S(o?["dtype"])} · 단위 {S(o?["units"])} · 실행 방식 {S(e?["execution"])}";
        }
        _engines.Text = Eng("A") + Environment.NewLine + Eng("B");
        _state.Text = comparable
            ? (reviewed ? "상태: 스크립트가 비교 가능이라고 답했습니다. 아직 수치를 계산하지 않았습니다. 2G 실행이 아닙니다."
                        : "상태: 스크립트는 비교 가능이라고 답했지만 검토 안 된 계약입니다. 표시는 하되 통과로 취급하지 않습니다. 2G 실행이 아닙니다.")
            : "상태: 스크립트가 비교 불가라고 답했습니다. 가져온 파일 비교이며 2G 실행이 아닙니다.";
    }

    private async Task LoadDisplayAsync(int gen, CancellationToken cancel)
    {
        var a = ComparisonDisplayReader.ReadLayout(_manifest?["engines"]?["A"]?["output"], _baseDir);
        var b = ComparisonDisplayReader.ReadLayout(_manifest?["engines"]?["B"]?["output"], _baseDir);
        if (a is null || b is null) { _viewHint.Text = "표시용 배치를 읽지 못했거나 미리보기 한도(가로·세로 각 2^24 화소)를 넘습니다. 표시하지 않았을 뿐 비교 판정과는 별개입니다."; return; }
        var step = ComparisonDisplayReader.ChooseStep(a.Width, a.Height);
        var (pa, pb, dw, dh, mismatch) = await Task.Run(async () =>
        {
            // the preview is bound to the confirmed manifest: a file whose bytes differ from the recorded SHA-256 is not drawn
            foreach (var (name, l) in new[] { ("A", a), ("B", b) })
            {
                var actual = await ComparisonDisplayReader.Sha256Async(l.Path, cancel);
                if (!string.Equals(actual, l.Sha256, StringComparison.OrdinalIgnoreCase)) return (Array.Empty<double>(), Array.Empty<double>(), 0, 0, (string?)name);
            }
            var (x, w, h) = ComparisonDisplayReader.ReadDecimated(a, step, cancel);
            var (y, _, _) = ComparisonDisplayReader.ReadDecimated(b, step, cancel);
            return (x, y, w, h, (string?)null);
        }, cancel);
        cancel.ThrowIfCancellationRequested();
        if (gen != _generation || _closed) return;
        if (mismatch is not null) { _viewHint.Text = $"{mismatch} 출력 파일이 확인한 매니페스트의 SHA-256 과 다릅니다. 미리보기를 그리지 않았습니다. 다시 열기를 누르십시오."; return; }
        _pixelsA = pa; _pixelsB = pb; _displayWidth = dw; _displayHeight = dh; _step = step;
        double lo = double.PositiveInfinity, hi = double.NegativeInfinity;
        foreach (var arr in new[] { pa, pb })
            foreach (var v in arr)
                if (double.IsFinite(v)) { if (v < lo) lo = v; if (v > hi) hi = v; }
        if (!double.IsFinite(lo)) { lo = 0; hi = 1; }
        if (hi <= lo) hi = lo + 1;
        _dataMin = lo; _dataMax = hi;
        _window.Text = (hi - lo).ToString("R", CultureInfo.InvariantCulture);
        _level.Text = ((hi + lo) / 2).ToString("R", CultureInfo.InvariantCulture);
        var roi = _manifest!["roi"]!;
        _roiText.Text = $"ROI(매니페스트의 정수 좌표, A·B 양쪽에 같은 좌표로 표시): x={roi["x"]}, y={roi["y"]}, 가로={roi["width"]}, 세로={roi["height"]}";
        _viewHint.Text = $"표시 전용 복사본: 원본 {a.Width}×{a.Height}" + (step > 1 ? $", 표시는 {step}화소마다 한 점({dw}×{dh})로 줄였습니다 — 표시 좌표 = 원본 좌표 ÷ {step}" : ", 줄이지 않음")
            + ". 통계는 표시 배열이 아니라 계약 스크립트가 원본 파일의 ROI 행을 직접 읽어 계산합니다. W/L·확대·줄임은 숫자에 영향을 주지 않습니다. 초기 W/L = 두 영상의 유한한 값 전체의 최솟값~최댓값(임계값을 정하지 않음).";
        foreach (var btn in new[] { _applyWl, _fit, _actual, _zoomIn, _zoomOut }) btn.IsEnabled = true;
        RenderBoth(useTextWl: false);
        DrawRoi(roi);
        ZoomFit();
        SetBusy(_busy);   // enables the compute button once the display is ready
    }

    private void RenderBoth(bool useTextWl)
    {
        if (_pixelsA is null || _pixelsB is null) return;
        double lo = _dataMin, hi = _dataMax;
        if (useTextWl)
        {
            if (!double.TryParse(_window.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out var ww) || !double.TryParse(_level.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out var ll) || !(ww > 0) || !double.IsFinite(ll))
            {
                _viewHint.Text = "W/L 값이 올바르지 않습니다(Window > 0 인 숫자). 표시를 바꾸지 않았습니다.";
                return;
            }
            lo = ll - ww / 2; hi = ll + ww / 2;
        }
        _imageA.Source = ToBitmap(_pixelsA, lo, hi);
        _imageB.Source = ToBitmap(_pixelsB, lo, hi);   // the SAME lo/hi for both: one window/level
    }

    private BitmapSource ToBitmap(double[] pixels, double lo, double hi)
    {
        var bytes = new byte[pixels.Length];
        var scale = 255.0 / (hi - lo);
        for (var i = 0; i < pixels.Length; i++)
        {
            var v = pixels[i];
            bytes[i] = !double.IsFinite(v) ? (byte)0 : (byte)Math.Clamp(Math.Round((v - lo) * scale), 0, 255);
        }
        var bmp = BitmapSource.Create(_displayWidth, _displayHeight, 96, 96, PixelFormats.Gray8, null, bytes, _displayWidth);
        bmp.Freeze();
        return bmp;
    }

    private void DrawRoi(JsonNode roi)
    {
        long L(string k) => roi[k] is JsonValue v && v.TryGetValue<long>(out var l) ? l : 0;
        foreach (var canvas in new[] { _overlayA, _overlayB })
        {
            canvas.Children.Clear();
            canvas.Width = _displayWidth; canvas.Height = _displayHeight;
            var rect = new System.Windows.Shapes.Rectangle { Width = Math.Max(1.0, (double)L("width") / _step), Height = Math.Max(1.0, (double)L("height") / _step), Stroke = Brushes.Yellow, StrokeThickness = 1.0 / Math.Max(_zoom, 1e-6), IsHitTestVisible = false };
            Canvas.SetLeft(rect, (double)L("x") / _step); Canvas.SetTop(rect, (double)L("y") / _step);
            canvas.Children.Add(rect);
        }
    }

    private void SetZoom(double zoom)
    {
        _zoom = Math.Clamp(zoom, 0.02, 64.0);
        _scaleA.ScaleX = _scaleA.ScaleY = _scaleB.ScaleX = _scaleB.ScaleY = _zoom;   // one scale for both
        foreach (var canvas in new[] { _overlayA, _overlayB })
            foreach (var r in canvas.Children.OfType<System.Windows.Shapes.Rectangle>()) r.StrokeThickness = 1.0 / _zoom;
    }

    private void ZoomFit()
    {
        if (_displayWidth <= 0 || _displayHeight <= 0) return;
        var vw = Math.Max(50.0, Math.Min(_scrollA.ViewportWidth > 0 ? _scrollA.ViewportWidth : 500, _scrollB.ViewportWidth > 0 ? _scrollB.ViewportWidth : 500) - 4);
        var vh = Math.Max(50.0, Math.Min(_scrollA.ViewportHeight > 0 ? _scrollA.ViewportHeight : 300, _scrollB.ViewportHeight > 0 ? _scrollB.ViewportHeight : 300) - 4);
        SetZoom(Math.Min(vw / _displayWidth, vh / _displayHeight));
    }

    private void Sync(ScrollViewer from, ScrollViewer to)
    {
        if (_syncing) return;
        _syncing = true;
        try { to.ScrollToHorizontalOffset(from.HorizontalOffset); to.ScrollToVerticalOffset(from.VerticalOffset); }
        finally { _syncing = false; }
    }

    private async Task ComputeAsync()
    {
        if (_busy || _snapshot is null || _backend is null || _validation?.Ok != true) return;
        var gen = _generation;
        var backend = _backend;
        var snapshot = _snapshot;
        if (!snapshot.StillMatchesDisk(out var currentSha))
        {
            // same path, different bytes: a generation number cannot see this (Reviewer #252 F252-2)
            Invalidate("매니페스트 파일이 열 때와 다릅니다.");
            _verdict.Text = "거부 — 확인한 매니페스트와 지금 파일이 다릅니다. 통과가 아닙니다.";
            _result.Text = $"거부: 열 때 SHA-256 {snapshot.Sha256}\n지금 SHA-256 {currentSha ?? "읽지 못함"}\n이전 미리보기·검증 결과를 지웠습니다. 다시 열기·검증을 누르십시오.";
            return;
        }
        SetBusy(true);
        var cts = new CancellationTokenSource();
        _cancel = cts;
        _result.Text = "계산 중입니다(계약 스크립트 자식 프로세스). 원본 파일은 읽기만 합니다…";
        try
        {
            var response = await backend.CompareAsync(snapshot, cts.Token);
            if (gen != _generation || _closed) { if (!_closed) _result.Text = ""; return; }   // other inputs were opened meanwhile: this answer describes the old ones and is not shown as current
            UpdateReviewLine(response.Script);
            var reviewed = response.Script?.Reviewed == true;
            var text = new System.Text.StringBuilder();
            if (!response.Ok)
            {
                text.AppendLine("실패 — 결과를 믿을 수 없습니다. 통과가 아닙니다.");
                text.AppendLine(response.Failure);
                if (!string.IsNullOrWhiteSpace(response.StderrTail)) text.AppendLine("stderr: " + response.StderrTail);
                if (response.RunDirectory is not null) text.AppendLine("스크립트가 만든 폴더(있다면): " + response.RunDirectory);
                _state.Text = "상태: 계산 실패. 통과가 아닙니다. 2G 실행이 아닙니다.";
            }
            else
            {
                var res = response.Result!;
                var status = res["status"]!.GetValue<string>();
                var reasons = (res["reasons"] as JsonArray)?.Select(x => x!.GetValue<string>()).ToList() ?? [];
                text.AppendLine($"결과: {status}" + (reasons.Count > 0 ? "  사유: " + string.Join(", ", reasons) : "") + (reviewed ? "" : "  [검토 안 된 계약]"));
                if (status == "computed")
                {
                    var kind = res["validation"]?["comparison_kind"]?.GetValue<string>() ?? "?";
                    var attribution = res["validation"]?["algorithm_attribution"]?.GetValue<bool>() == true;
                    text.AppendLine($"ROI 유효 화소 {res["valid_pixels"]}  ·  단위 {res["units"]}  ·  차이 = A − B  ·  종류 {kind}  ·  계약상 귀속 조건: {(attribution ? "충족(생산 이력·품질 확인 아님)" : "해당 없음(비귀속 라벨)")}");
                    text.AppendLine($"평균 {F(res["mean_a_minus_b"])}  표준편차(모집단) {F(res["std_a_minus_b"])}  RMSE {F(res["rmse"])}  평균 절댓값 {F(res["mean_abs"])}  최솟값 {F(res["min_a_minus_b"])}  최댓값 {F(res["max_a_minus_b"])}");
                    _state.Text = "상태: 가져온 파일의 수치 차이를 계산했습니다." + (reviewed ? "" : " 검토 안 된 계약의 답입니다.") + " 이것은 2G 실행도 화질 판정도 아닙니다.";
                }
                else
                {
                    text.AppendLine("숫자를 만들지 않았습니다. " + string.Join(" ", reasons.Select(ComparisonReasonText.For)));
                    _state.Text = $"상태: 비교가 {status} 로 끝났습니다. 통과가 아닙니다. 2G 실행이 아닙니다.";
                }
                text.AppendLine($"원본 파일 변경 없음: {(res["sources_unchanged"]?.GetValue<bool>() == true ? "예" : "아니오")}  ·  2G 실행됨: {(res["actual_2g_executed"]?.GetValue<bool>() == true ? "예" : "아니오")}  ·  화질 판정: 하지 않음");
                text.AppendLine("저장 위치: " + response.RunDirectory);
                text.AppendLine("확인한 매니페스트 SHA-256: " + snapshot.Sha256);
            }
            _result.Text = text.ToString();
        }
        catch (Exception e) { if (gen == _generation) { _result.Text = "실패: " + e.Message; _state.Text = "상태: 계산 실패. 통과가 아닙니다."; } }
        finally { if (ReferenceEquals(_cancel, cts)) _cancel = null; cts.Dispose(); if (!_closed) SetBusy(false); }
    }

    private static string F(JsonNode? n) => n is JsonValue v && v.TryGetValue<double>(out var d) ? d.ToString("G10", CultureInfo.InvariantCulture) : "—";
}
