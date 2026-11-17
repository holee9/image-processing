using System.Globalization;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Windows;
using System.Windows.Automation;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Media;
using System.Windows.Input;
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
    private readonly Button _openButton = new() { Content = "Open and validate" };
    private readonly Button _computeButton = new() { Content = "Compute and save A−B statistics", IsEnabled = false };
    private readonly Button _cancelButton = new() { Content = "Cancel", IsEnabled = false };
    private readonly Button _applyWl = new() { Content = "Apply W/L", IsEnabled = false };
    private readonly Button _fit = new() { Content = "Fit", IsEnabled = false };
    private readonly Button _actual = new() { Content = "100%", IsEnabled = false };
    private readonly Button _zoomIn = new() { Content = "+", IsEnabled = false };
    private readonly Button _zoomOut = new() { Content = "−", IsEnabled = false };
    private readonly Button _sideBySide = new() { Content = "Side by side", IsEnabled = false };
    private readonly Button _toggleAb = new() { Content = "Show B (Ctrl+Tab)", IsEnabled = false };
    private readonly Button _difference = new() { Content = "Difference A−B (Ctrl+D)", IsEnabled = false };
    private readonly CheckBox _differenceExtrema = new() { Content = "Difference: extrema range", IsEnabled = false };
    private double _differenceK = 5;
    private readonly TextBlock _modeText = new() { TextWrapping = TextWrapping.Wrap };
    private readonly UniformGrid _viewers = new() { Columns = 2, Margin = new Thickness(0, 4, 0, 4) };
    private UIElement _panelB = null!;
    private TextBlock _captionA = null!;
    private BitmapSource? _bitmapA, _bitmapB, _differenceBitmap;
    private bool _single, _showB, _showDifference, _beforeDifferenceSingle;
    private string _differenceLegend = "";
    private int _displayWidthB, _displayHeightB;
    private readonly Image _imageA = new() { Stretch = Stretch.None, SnapsToDevicePixels = true };
    private readonly Image _imageB = new() { Stretch = Stretch.None, SnapsToDevicePixels = true };
    private readonly Canvas _overlayA = new(), _overlayB = new();
    private readonly Grid _hostA = new(), _hostB = new();
    private readonly ScrollViewer _scrollA = new() { HorizontalScrollBarVisibility = ScrollBarVisibility.Auto, VerticalScrollBarVisibility = ScrollBarVisibility.Auto, MinHeight = 240, MaxHeight = 400 };
    private readonly ScrollViewer _scrollB = new() { HorizontalScrollBarVisibility = ScrollBarVisibility.Auto, VerticalScrollBarVisibility = ScrollBarVisibility.Auto, MinHeight = 240, MaxHeight = 400 };
    private readonly ScaleTransform _scaleA = new(1, 1), _scaleB = new(1, 1);
    private readonly TextBox _markerK = new() { Width = 65, Text = "5" };
    private readonly TextBox _markerN = new() { Width = 65, Text = "16" };
    private readonly Button _markerApply = new() { Content = "Find difference regions", IsEnabled = false };
    private readonly CheckBox _showDefectMarks = new() { Content = "Defect-map disagreement (cyan)", IsEnabled = false };
    private readonly TextBlock _markerStatus = new() { TextWrapping = TextWrapping.Wrap };
    private readonly TextBlock _markerWarning = new() { FontWeight = FontWeights.Bold, Foreground = Brushes.DarkOrange, TextWrapping = TextWrapping.Wrap };
    private int _markerCandidateCount;
    private readonly ListBox _markerList = new() { MinWidth = 290, MaxHeight = 250 };
    private ComparisonMarkers.Prepared? _markerPrepared;
    private ComparisonDefectMasks.Masks? _defectMasks;
    private CancellationTokenSource? _markerCancel;
    private int _markerOperation;
    private long _sourceWidth, _sourceHeight;
    private const string MarkerParameterNote = "N and area are sample pixels. Region marks describe these outputs, not a pass/fail criterion.";


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
        Title = "Compare Imported 2G and New PRE Outputs";
        Width = 1180; Height = 900; MinWidth = 820; MinHeight = 560;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        MaxHeight = SystemParameters.WorkArea.Height;
        Auto(this, "ComparisonImportWindow");

        var root = new StackPanel { Margin = new Thickness(14) };
        root.Children.Add(Label("This window imports two existing outputs (A = official Samsung 2G X64W, B = new PRE), displays them under the same conditions and computes A−B statistics. It does not load vendor DLLs, run an engine, control hardware or judge image quality. Differences describe these two files; they do not establish improvement or a pass. The contract script, run as a child process, performs validation and calculation."));
        var provenance = new TextBlock { Text = "Synthetic wiring verification / no vendor measurement — this window does not verify whether inputs are real acquisitions or vendor production outputs. The contract has no provenance-verification field. No result establishes verified production history.", TextWrapping = TextWrapping.Wrap, FontWeight = FontWeights.Bold, Foreground = Brushes.White, Background = Brushes.DarkOrange, Padding = new Thickness(8, 4, 8, 4), Margin = new Thickness(0, 2, 0, 4) };
        Auto(provenance, "ComparisonProvenanceNotice");
        root.Children.Add(provenance);
        Auto(_state, "ComparisonStateText");
        _state.Text = "Status: no manifest opened. This compares imported files; it does not run 2G.";
        root.Children.Add(_state);
        Auto(_markerWarning, "ComparisonMarkerWarning"); root.Children.Add(_markerWarning);
        Auto(_verdict, "ComparisonValidationStatus"); Auto(_reasons, "ComparisonReasonsList");
        root.Children.Add(_verdict); root.Children.Add(_reasons);

        root.Children.Add(Label("Contract script path (tools/calibration/comparison_manifest.py — no default; last used value is remembered)"));
        root.Children.Add(_scriptPath); Auto(_scriptPath, "ComparisonScriptPathBox");
        root.Children.Add(Label("Python executable (empty uses python on PATH)"));
        root.Children.Add(_pythonPath); Auto(_pythonPath, "ComparisonPythonPathBox");
        Auto(_review, "ComparisonContractReviewState");
        root.Children.Add(_review);

        root.Children.Add(Label("Manifest (JSON, schema xpe-2g-pre-comparison/4). The script only reads this directory."));
        var pathRow = new DockPanel { Margin = new Thickness(0, 2, 0, 4) };
        var browse = new Button { Content = "Browse…", Margin = new Thickness(6, 0, 0, 0), Padding = new Thickness(10, 2, 10, 2) };
        _openButton.Margin = new Thickness(6, 0, 0, 0); _openButton.Padding = new Thickness(10, 2, 10, 2);
        DockPanel.SetDock(_openButton, Dock.Right); DockPanel.SetDock(browse, Dock.Right);
        pathRow.Children.Add(_openButton); pathRow.Children.Add(browse); pathRow.Children.Add(_manifestPath);
        root.Children.Add(pathRow);
        Auto(_manifestPath, "ComparisonManifestPathBox"); Auto(browse, "ComparisonBrowseButton"); Auto(_openButton, "ComparisonOpenButton");

        root.Children.Add(Label("Development output directory (outside the source-data directory; each run gets a new subdirectory and preserves existing files)"));
        _outputRoot.Text = System.IO.Path.Combine(ComparisonPaths.DataRoot, "comparison-runs");
        Auto(_outputRoot, "ComparisonOutputRootBox");
        root.Children.Add(_outputRoot);

        Auto(_verdict, "ComparisonValidationStatus"); Auto(_reasons, "ComparisonReasonsList"); Auto(_engines, "ComparisonEngineSummary");
        root.Children.Add(_engines);

        // display controls: one window/level, one zoom, the same ROI
        var controls = new WrapPanel { Margin = new Thickness(0, 8, 0, 4) };
        controls.Children.Add(new TextBlock { Text = "Display-only W/L (shared by both images):  Window ", VerticalAlignment = VerticalAlignment.Center });
        controls.Children.Add(_window);
        controls.Children.Add(new TextBlock { Text = "  Level ", VerticalAlignment = VerticalAlignment.Center });
        controls.Children.Add(_level);
        foreach (var b in new[] { _applyWl, _fit, _actual, _zoomIn, _zoomOut, _sideBySide, _toggleAb, _difference }) { b.Margin = new Thickness(6, 0, 0, 0); b.Padding = new Thickness(10, 2, 10, 2); controls.Children.Add(b); }
        Auto(_window, "ComparisonWindowBox"); Auto(_level, "ComparisonLevelBox"); Auto(_applyWl, "ComparisonApplyWlButton"); Auto(_fit, "ComparisonZoomFitButton");
        Auto(_actual, "ComparisonZoom100Button"); Auto(_zoomIn, "ComparisonZoomInButton"); Auto(_zoomOut, "ComparisonZoomOutButton");
        Auto(_sideBySide, "ComparisonSideBySideButton"); Auto(_toggleAb, "ComparisonToggleAbButton"); Auto(_difference, "ComparisonDifferenceButton");
        Auto(_modeText, "ComparisonDisplayMode"); Auto(_imageA, "ComparisonImageA"); Auto(_imageB, "ComparisonImageB");
        Auto(_differenceExtrema, "ComparisonDifferenceExtremaToggle");
        var differenceMode = new DockPanel();
        DockPanel.SetDock(_differenceExtrema, Dock.Right);
        differenceMode.Children.Add(_differenceExtrema); differenceMode.Children.Add(_modeText);
        _differenceExtrema.Checked += (_, _) => RefreshDifference();
        _differenceExtrema.Unchecked += (_, _) => RefreshDifference();
        root.Children.Add(controls); root.Children.Add(differenceMode);
        Auto(_roiText, "ComparisonRoiText"); Auto(_viewHint, "ComparisonViewHint");
        root.Children.Add(_roiText); root.Children.Add(_viewHint);

        var panelA = (StackPanel)Viewer("A — official Samsung 2G X64W (imported output)", _scrollA, _hostA, _imageA, _overlayA, _scaleA, "ComparisonViewerA");
        _captionA = (TextBlock)panelA.Children[0];
        _panelB = Viewer("B — new PRE (imported output)", _scrollB, _hostB, _imageB, _overlayB, _scaleB, "ComparisonViewerB");
        _viewers.Children.Add(panelA); _viewers.Children.Add(_panelB);
        var markerControls = new WrapPanel { Margin = new Thickness(0, 6, 0, 4) };
        markerControls.Children.Add(new TextBlock { Text = "Region k ", VerticalAlignment = VerticalAlignment.Center }); markerControls.Children.Add(_markerK);
        markerControls.Children.Add(new TextBlock { Text = "  minimum N (sample px) ", VerticalAlignment = VerticalAlignment.Center }); markerControls.Children.Add(_markerN);
        markerControls.Children.Add(_markerApply); markerControls.Children.Add(_showDefectMarks);
        Auto(_markerK, "ComparisonMarkerK"); Auto(_markerN, "ComparisonMarkerN"); Auto(_markerApply, "ComparisonFindMarkersButton");
        Auto(_showDefectMarks, "ComparisonDefectDisagreementToggle"); Auto(_markerStatus, "ComparisonMarkerStatus"); Auto(_markerList, "ComparisonMarkerList");
        _markerStatus.Text = "Candidate total M: not computed. " + MarkerParameterNote;
        var previewAndList = new DockPanel(); DockPanel.SetDock(_markerList, Dock.Right); previewAndList.Children.Add(_markerList); previewAndList.Children.Add(_viewers);
        root.Children.Add(markerControls); root.Children.Add(_markerStatus); root.Children.Add(previewAndList);

        var actions = new WrapPanel { Margin = new Thickness(0, 6, 0, 2) };
        _computeButton.Padding = new Thickness(12, 6, 12, 6);
        _cancelButton.Padding = new Thickness(12, 6, 12, 6); _cancelButton.Margin = new Thickness(8, 0, 0, 0);
        var live = new Button { Content = "Run real 2G", IsEnabled = false, Padding = new Thickness(12, 6, 12, 6), Margin = new Thickness(8, 0, 0, 0) };
        actions.Children.Add(_computeButton); actions.Children.Add(_cancelButton); actions.Children.Add(live);
        Auto(_computeButton, "ComparisonComputeButton"); Auto(_cancelButton, "ComparisonCancelButton"); Auto(live, "ComparisonLive2gButton");
        root.Children.Add(actions);
        var liveReason = Label("Live 2G execution is blocked (G-2G-READY): offline handle creation/initialization, buffer ownership, exclusive access, applicable usage conditions and source RAW/map provenance are not defined and authorized. This window does not load vendor DLLs.");
        Auto(liveReason, "ComparisonLive2gReason");
        root.Children.Add(liveReason);
        Auto(_result, "ComparisonResultBox");
        root.Children.Add(_result);

        Content = new ScrollViewer { Content = root, VerticalScrollBarVisibility = ScrollBarVisibility.Auto };
        LoadRemembered();
        UpdateReviewLine(null);

        browse.Click += (_, _) =>
        {
            var dlg = new Microsoft.Win32.OpenFileDialog { Filter = "Comparison manifest (*.json)|*.json|All files|*.*", CheckFileExists = true };
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
        _sideBySide.Click += (_, _) => { _single = _showB = _showDifference = false; ApplyView(); };
        _toggleAb.Click += (_, _) => { _single = true; _showDifference = false; _showB = !_showB; ApplyView(); };
        _difference.Click += (_, _) =>
        {
            if (!_showDifference) { _beforeDifferenceSingle = _single; _single = true; }
            else _single = _beforeDifferenceSingle;
            _showDifference = !_showDifference; ApplyView();
        };
        PreviewKeyDown += (_, e) =>
        {
            if (Keyboard.Modifiers != ModifierKeys.Control) return;
            var button = e.Key == Key.Tab ? _toggleAb : e.Key == Key.D ? _difference : null;
            if (button?.IsEnabled == true) { button.RaiseEvent(new RoutedEventArgs(Button.ClickEvent)); e.Handled = true; }
        };
        _markerApply.Click += async (_, _) => await UpdateMarkersAsync();
        _showDefectMarks.Checked += (_, _) => DrawMarkerOverlays(); _showDefectMarks.Unchecked += (_, _) => DrawMarkerOverlays();
        _markerList.SelectionChanged += (_, _) => CenterMarkerSelection();
        void ParametersChanged()
        {
            _markerOperation++; try { _markerCancel?.Cancel(); } catch (ObjectDisposedException) { }
            _markerCandidateCount = 0; _markerWarning.Text = "";
            _markerList.Items.Clear(); DrawMarkerOverlays(); _markerApply.IsEnabled = _markerPrepared?.ValidPixels > 0 && !_closed;
            _markerStatus.Text = "Candidate total M: not computed for the edited parameters. " + MarkerParameterNote + " " + (_defectMasks?.Status ?? "");
        }
        _markerK.TextChanged += (_, _) => ParametersChanged(); _markerN.TextChanged += (_, _) => ParametersChanged();
        _scrollA.ScrollChanged +=  (_, _) => Sync(_scrollA, _scrollB);
        _scrollB.ScrollChanged += (_, _) => Sync(_scrollB, _scrollA);
        _manifestPath.TextChanged += (_, _) => Invalidate("Manifest path changed. Open and validate again.");
        _scriptPath.TextChanged += (_, _) => Invalidate("Contract script path changed. Open and validate again.");
        _pythonPath.TextChanged += (_, _) => Invalidate("Python path changed. Open and validate again.");
        _outputRoot.TextChanged += (_, _) => Invalidate("Output directory changed. Open and validate again.");
        Closing += (_, _) => { _closed = true; _generation++; try { _cancel?.Cancel(); _markerCancel?.Cancel(); } catch (ObjectDisposedException) { } };   // closing is never blocked: the child process is stopped by the token and no late result is accepted
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
            _review.Text = "Contract review: the script has not run yet.";
            return;
        }
        _review.Text = $"Executed script (pinned copy per request): {id.PinnedPath}\nOriginal {id.OriginalPath}\nPinned SHA-256 before {id.PinnedShaBefore} / after {id.PinnedShaAfter ?? "unreadable"}\nOriginal SHA-256 after {id.OriginalShaAfter ?? "unreadable"}\nPython {id.Python.Version} · NumPy {id.Python.NumpyVersion} · {id.Python.ExePath}\nContract review: "
            + (id.Reviewed ? "SHA-256 matches the reviewed script both before and after execution."
               : "Unreviewed contract — the executed copy differs from the reviewed SHA-256 or changed during execution. Do not treat its response as a comparability pass.");
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
        _generation++; _markerOperation++; try { _markerCancel?.Cancel(); } catch (ObjectDisposedException) { }
        _markerPrepared = null; _defectMasks = null; _markerCandidateCount = 0; _markerWarning.Text = ""; _markerList.Items.Clear(); _markerApply.IsEnabled = false;
        _showDefectMarks.IsEnabled = false; _showDefectMarks.IsChecked = false; _markerStatus.Text = "Candidate total M: not computed. " + MarkerParameterNote;
        _snapshot = null; _manifest = null; _validation = null; _backend = null; _pixelsA = _pixelsB = null;
        _imageA.Source = _imageB.Source = null;
        _differenceExtrema.IsEnabled = false; _differenceK = 5;
        _bitmapA = _bitmapB = _differenceBitmap = null; _single = _showB = _showDifference = false;
        _modeText.Text = ""; _panelB.Visibility = Visibility.Visible; _viewers.Columns = 2; _overlayA.Children.Clear(); _overlayB.Children.Clear();
        if (!_busy) _computeButton.IsEnabled = false;
        foreach (var b in new[] { _applyWl, _fit, _actual, _zoomIn, _zoomOut, _sideBySide, _toggleAb, _difference }) b.IsEnabled = false;
        _verdict.Text = ""; _reasons.Text = ""; _engines.Text = ""; _roiText.Text = ""; _viewHint.Text = "";
        _result.Text = "";
        _state.Text = "Status: " + why + " Imported-file comparison; no 2G execution.";
        UpdateReviewLine(null);
    }

    private async Task OpenAsync()
    {
        if (_busy) return;
        var path = _manifestPath.Text.Trim();
        Invalidate("Validating with the contract script in a child process.");
        var gen = _generation;
        SetBusy(true);
        var cts = new CancellationTokenSource();
        _cancel = cts;
        try
        {
            if (!File.Exists(path)) throw new IOException("Manifest file not found: " + path);
            Remember();
            var snapshot = ManifestSnapshot.Create(path);   // one read: these bytes are what is validated, previewed and computed
            var backend = _backendFactory(_scriptPath.Text.Trim(), _pythonPath.Text.Trim(), _outputRoot.Text.Trim());
            var response = await backend.ValidateAsync(snapshot, cts.Token);
            if (gen != _generation || _closed) return;   // an input changed while the script ran: this answer is stale
            _snapshot = snapshot; _manifest = snapshot.Json; _baseDir = snapshot.BaseDir; _backend = backend; _validation = response;
            UpdateReviewLine(response.Script);
            ShowValidation();
            if (response.Ok) await LoadDisplayAsync(gen, cts.Token);
        }
        catch (OperationCanceledException)
        {
            if (gen != _generation || _closed) return;
            Invalidate("Cancelled. Pending arrays, display and success state were not accepted.");
            _verdict.Text = "Cancelled — not a pass.";
        }
        catch (Exception e)
        {
            if (gen != _generation || _closed) return;
            _verdict.Text = "Not comparable — could not open.";
            _reasons.Text = e.Message;
            _state.Text = "Status: open failed. Imported-file comparison, no 2G execution. Failure is not a pass.";
        }
        finally { if (ReferenceEquals(_cancel, cts)) _cancel = null; cts.Dispose(); if (!_closed) SetBusy(false); }
    }

    private void ShowValidation()
    {
        var r = _validation!;
        if (!r.Ok)
        {
            _verdict.Text = "Failed — no response from the contract script. Not a pass.";
            _reasons.Text = r.Failure + (string.IsNullOrWhiteSpace(r.StderrTail) ? "" : Environment.NewLine + "stderr: " + r.StderrTail);
            _state.Text = "Status: validation failed. Imported-file comparison, no 2G execution.";
            return;
        }
        var v = r.Validation!;
        var comparable = v["comparable"]!.GetValue<bool>();
        var kind = v["comparison_kind"]?.GetValue<string>() ?? "?";
        var attribution = v["algorithm_attribution"]?.GetValue<bool>() == true;
        var reviewed = r.Script?.Reviewed == true;
        _verdict.Text = (comparable ? $"Script response: comparable — kind {kind}" : "Script response: not comparable — preview only; reasons below. Statistics remain disabled.")
            + (comparable ? $" · Contract attribution conditions: {(attribution ? "satisfied (production history and quality are not verified)" : "not applicable (non-attributable label)")}" : "")
            + (reviewed ? "" : "  [unreviewed contract]");
        var lines = ((JsonArray)v["reasons"]!).Select(x => x!.GetValue<string>()).ToList();
        var blockers = (v["blockers"] as JsonArray)?.Select(x => x!.GetValue<string>()) ?? [];
        _reasons.Text = string.Join(Environment.NewLine, lines.Concat(blockers).Select(c => $"{c}: {ComparisonReasonText.For(c)}"));
        string Eng(string key)
        {
            var e = _manifest?["engines"]?[key];
            var o = e?["output"];
            string S(JsonNode? n) => n is JsonValue jv && jv.GetValueKind() == JsonValueKind.String ? jv.GetValue<string>() : "?";
            return $"{key}: stage {S(o?["stage"])} · dtype {S(o?["dtype"])} · units {S(o?["units"])} · execution {S(e?["execution"])}";
        }
        _engines.Text = Eng("A") + Environment.NewLine + Eng("B");
        _state.Text = comparable
            ? (reviewed ? "Status: script reports comparable. No statistics computed yet. No 2G execution."
                        : "Status: script reports comparable under an unreviewed contract. Displayed, but not accepted as a pass. No 2G execution.")
            : "Status: script reports not comparable. Imported-file comparison, no 2G execution.";
    }

    private async Task LoadDisplayAsync(int gen, CancellationToken cancel)
    {
        var a = ComparisonDisplayReader.ReadLayout(_manifest?["engines"]?["A"]?["output"], _baseDir);
        var b = ComparisonDisplayReader.ReadLayout(_manifest?["engines"]?["B"]?["output"], _baseDir);
        if (a is null || b is null) { _viewHint.Text = "Cannot safely preview: unsupported dtype/endian/packed stride, missing or wrong-length file, invalid path, or dimensions above 2^24. Comparability is a separate result."; return; }
        var step = Math.Max(ComparisonDisplayReader.ChooseStep(a.Width, a.Height), ComparisonDisplayReader.ChooseStep(b.Width, b.Height));
        var (pa, pb, dw, dh, bw, bh, mismatch) = await Task.Run(async () =>
        {
            // the preview is bound to the confirmed manifest: a file whose bytes differ from the recorded SHA-256 is not drawn
            foreach (var (name, l) in new[] { ("A", a), ("B", b) })
            {
                var actual = await ComparisonDisplayReader.Sha256Async(l.Path, cancel);
                if (!string.Equals(actual, l.Sha256, StringComparison.OrdinalIgnoreCase)) return (Array.Empty<double>(), Array.Empty<double>(), 0, 0, 0, 0, (string?)name);
            }
            var (x, w, h) = ComparisonDisplayReader.ReadDecimated(a, step, cancel);
            var (y, wb, hb) = ComparisonDisplayReader.ReadDecimated(b, step, cancel);
            return (x, y, w, h, wb, hb, (string?)null);
        }, cancel);
        cancel.ThrowIfCancellationRequested();
        if (gen != _generation || _closed) return;
        if (mismatch is not null) { _viewHint.Text = $"{mismatch} output SHA-256 differs from the validated manifest. No preview drawn. Open again."; return; }
        _sourceWidth = a.Width; _sourceHeight = a.Height;
        _pixelsA = pa; _pixelsB = pb; _displayWidth = dw; _displayHeight = dh; _displayWidthB = bw; _displayHeightB = bh; _step = step;
        double lo = double.PositiveInfinity, hi = double.NegativeInfinity;
        foreach (var arr in new[] { pa, pb })
            foreach (var v in arr)
                if (double.IsFinite(v)) { if (v < lo) lo = v; if (v > hi) hi = v; }
        if (!double.IsFinite(lo)) { lo = 0; hi = 1; }
        if (hi <= lo) hi = lo + 1;
        _dataMin = lo; _dataMax = hi;
        _window.Text = (hi - lo).ToString("R", CultureInfo.InvariantCulture);
        _level.Text = ((hi + lo) / 2).ToString("R", CultureInfo.InvariantCulture);
        var roi = _manifest!["roi"] as JsonObject;
        _roiText.Text = roi is null ? "No ROI recorded." : $"ROI (manifest coordinates; no alignment applied): x={roi["x"]}, y={roi["y"]}, width={roi["width"]}, height={roi["height"]}";
        _viewHint.Text = $"Display-only copy: source {a.Width}×{a.Height}" + (step > 1 ? $", sampled every {step} pixels ({dw}×{dh}) — display coordinate = source coordinate ÷ {step}" : ", no downsampling")
            + ". The contract script computes statistics from source-file ROI rows, not display arrays. W/L, zoom and downsampling do not affect the numbers. Initial W/L spans all finite values in both images; no threshold is selected.";
        foreach (var btn in new[] { _applyWl, _fit, _actual, _zoomIn, _zoomOut, _sideBySide, _toggleAb }) btn.IsEnabled = true;
        _difference.IsEnabled = a.Width == b.Width && a.Height == b.Height;
        _differenceExtrema.IsEnabled = _difference.IsEnabled;
        _difference.ToolTip = _difference.IsEnabled ? "Signed original-value A−B; blue negative, white zero, red positive." : "Difference unavailable: output dimensions differ; no alignment or resizing is performed.";
        if (!_difference.IsEnabled) _viewHint.Text += " Difference unavailable: dimensions differ; both outputs retain their own dimensions.";
        _differenceBitmap = null;
        RenderBoth(useTextWl: false);
        if (roi is not null) DrawRoi(roi);
        ZoomFit();
        SetBusy(_busy);   // enables the compute button once the display is ready
        await PrepareMarkersAsync(gen, cancel);
    }

    private async Task PrepareMarkersAsync(int gen, CancellationToken cancel)
    {
        if (!_difference.IsEnabled) { _markerStatus.Text = "Region/defect marks unavailable: output dimensions differ; no alignment is performed."; return; }
        int? R(string key) => _manifest?["roi"]?[key] is JsonValue v && v.TryGetValue<long>(out var n) && n >= 0 && n <= int.MaxValue ? (int)n : null;
        var x = R("x"); var y = R("y"); var w = R("width"); var h = R("height");
        if (x is null || y is null || w is null or <= 0 || h is null or <= 0 || (long)x + w > _sourceWidth || (long)y + h > _sourceHeight)
        { _markerStatus.Text = "Regions unavailable: an in-frame integer ROI is required."; return; }
        var manifest = _manifest; var baseDir = _baseDir; var a = _pixelsA!; var b = _pixelsB!; var sw = _displayWidth; var sh = _displayHeight; var step = _step; var sourceW = _sourceWidth; var sourceH = _sourceHeight;
        var result = await Task.Run(() =>
        {
            var masks = ComparisonDefectMasks.Read(manifest, baseDir, sourceW, sourceH, step, cancel);
            var prepared = ComparisonMarkers.Prepare(a, b, sw, sh, step, x.Value, y.Value, w.Value, h.Value, masks.Excluded, cancel);
            return (masks, prepared);
        }, cancel);
        cancel.ThrowIfCancellationRequested(); if (_closed || gen != _generation) return;
        _markerPrepared = result.prepared; _defectMasks = result.masks;
        _showDefectMarks.IsEnabled = result.masks.DisagreementAvailable; _showDefectMarks.IsChecked = result.masks.DisagreementAvailable;
        _markerApply.IsEnabled = result.prepared.ValidPixels > 0;
        _markerStatus.Text = MarkerParameterNote + $" Preview step {_step}; finite non-defect ROI samples {result.prepared.ValidPixels}; s={result.prepared.Scale.ToString("G8", CultureInfo.InvariantCulture)}. " + result.masks.Status;
        if (!string.IsNullOrWhiteSpace(_markerK.Text) && !string.IsNullOrWhiteSpace(_markerN.Text)) await UpdateMarkersAsync();
    }

    private async Task UpdateMarkersAsync()
    {
        if (_markerPrepared is null || _closed) return;
        if (!double.TryParse(_markerK.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out var k) || !double.IsFinite(k) || k <= 0 ||
            !int.TryParse(_markerN.Text, NumberStyles.None, CultureInfo.InvariantCulture, out var n) || n <= 0)
        { _markerStatus.Text = "Enter finite positive k and positive integer N. " + MarkerParameterNote; return; }
        try { _markerCancel?.Cancel(); } catch (ObjectDisposedException) { }
        using var cts = new CancellationTokenSource(); _markerCancel = cts;
        var operation = ++_markerOperation; var gen = _generation; var prepared = _markerPrepared;
        _markerCandidateCount = 0; _markerWarning.Text = ""; _markerList.Items.Clear(); DrawMarkerOverlays(); _markerApply.IsEnabled = false;
        _markerStatus.Text = "Finding connected regions on the display sample grid…";
        try
        {
            var result = await Task.Run(() => ComparisonMarkers.Analyze(prepared, k, n, cts.Token), cts.Token);
            if (_closed || gen != _generation || operation != _markerOperation) return;
            _differenceK = k; RefreshDifference();
            _markerCandidateCount = result.CandidateCount;
            _markerWarning.Text = result.CandidateCount > 20 ? $"차이가 넓게 퍼짐 — 후보 {result.CandidateCount}개 중 상위 20개 표시" : "";
            for (var i = 0; i < result.Regions.Count; i++) _markerList.Items.Add(new MarkerRow(i + 1, result.Regions[i]));
            _markerList.SelectedIndex = -1;
            _markerStatus.Text = $"{result.CandidateCount} uncapped candidates; {result.Regions.Count}/20 shown by sum |A−B|. k={k.ToString("G8", CultureInfo.InvariantCulture)}, N={n} sample px, s={result.Scale.ToString("G8", CultureInfo.InvariantCulture)}, step {_step}. Lime boxes: difference regions. " + (_defectMasks?.Status ?? "");
            DrawMarkerOverlays();
        }
        catch (OperationCanceledException) { }
        catch (Exception e) { if (!_closed && gen == _generation && operation == _markerOperation) _markerStatus.Text = "Region analysis failed: " + e.Message; }
        finally { if (!_closed && gen == _generation && operation == _markerOperation) _markerApply.IsEnabled = true; if (ReferenceEquals(_markerCancel, cts)) _markerCancel = null; }
    }

    private sealed record MarkerRow(int Number, ComparisonMarkers.Region Region)
    {
        public override string ToString() => $"{Number}: ({Region.SourceX},{Region.SourceY}) · area {Region.Area} sample px · mean {Region.MeanDifference.ToString("G6", CultureInfo.InvariantCulture)}";
    }

    private void DrawMarkerOverlays()
    {
        BitmapSource? defectBitmap = null;
        if (_markerPrepared is not null && _showDefectMarks.IsChecked == true && _defectMasks?.Disagreement is bool[] mask && mask.Any(x => x))
        {
            var colors = new byte[checked(mask.Length * 4)];
            for (var i = 0; i < mask.Length; i++) if (mask[i]) { colors[i * 4] = 255; colors[i * 4 + 1] = 255; colors[i * 4 + 3] = 180; }
            defectBitmap = BitmapSource.Create(_displayWidth, _displayHeight, 96, 96, PixelFormats.Bgra32, null, colors, _displayWidth * 4); defectBitmap.Freeze();
        }
        foreach (var canvas in new[] { _overlayA, _overlayB })
        {
            foreach (var item in canvas.Children.OfType<FrameworkElement>().Where(x => Equals(x.Tag, "marker")).ToArray()) canvas.Children.Remove(item);
            if (_markerPrepared is null) continue;
            if (defectBitmap is not null) canvas.Children.Add(new Image { Source = defectBitmap, Width = _displayWidth, Height = _displayHeight, Tag = "marker", IsHitTestVisible = false });
            foreach (MarkerRow row in _markerList.Items)
            {
                // Dense fields retain the ranked list but draw no automatic difference boxes. Selection may highlight one region.
                if (_markerCandidateCount > 20 && !ReferenceEquals(_markerList.SelectedItem, row)) continue;
                var r = row.Region;
                var box = new System.Windows.Shapes.Rectangle { Width = r.Width, Height = r.Height, Stroke = Brushes.Lime, StrokeThickness = 1 / Math.Max(_zoom, 1e-6), Tag = "marker", IsHitTestVisible = false };
                Canvas.SetLeft(box, r.X); Canvas.SetTop(box, r.Y); canvas.Children.Add(box);
                var label = new TextBlock { Text = row.Number.ToString(CultureInfo.InvariantCulture), Foreground = Brushes.Lime, Background = Brushes.Black, FontSize = 12 / Math.Max(_zoom, 1e-6), Tag = "marker", IsHitTestVisible = false };
                Canvas.SetLeft(label, r.X); Canvas.SetTop(label, r.Y); canvas.Children.Add(label);
            }
        }
    }

    private void CenterMarkerSelection()
    {
        if (_markerList.SelectedItem is not MarkerRow row) return;
        var r = row.Region;
        var width = Math.Max(1, Math.Min(_scrollA.ViewportWidth, _scrollB.ViewportWidth));
        var height = Math.Max(1, Math.Min(_scrollA.ViewportHeight, _scrollB.ViewportHeight));
        SetZoom(Math.Max(_zoom, Math.Min(width / Math.Max(1, r.Width * 2), height / Math.Max(1, r.Height * 2)))); UpdateLayout();
        foreach (var viewer in new[] { _scrollA, _scrollB })
        { viewer.ScrollToHorizontalOffset((r.X + r.Width / 2.0) * _zoom - viewer.ViewportWidth / 2); viewer.ScrollToVerticalOffset((r.Y + r.Height / 2.0) * _zoom - viewer.ViewportHeight / 2); }
        DrawMarkerOverlays();
    }

    private void RenderBoth(bool useTextWl)
    {
        if (_pixelsA is null || _pixelsB is null) return;
        double lo = _dataMin, hi = _dataMax;
        if (useTextWl)
        {
            if (!double.TryParse(_window.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out var ww) || !double.TryParse(_level.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out var ll) || !(ww > 0) || !double.IsFinite(ll))
            {
                _viewHint.Text = "Invalid W/L: numeric values are required and Window must be positive. Display unchanged.";
                return;
            }
            lo = ll - ww / 2; hi = ll + ww / 2;
        }
        _bitmapA = ToBitmap(_pixelsA, _displayWidth, _displayHeight, lo, hi);
        _bitmapB = ToBitmap(_pixelsB, _displayWidthB, _displayHeightB, lo, hi); // SAME lo/hi
        ApplyView();
    }

    private static BitmapSource ToBitmap(double[] pixels, int width, int height, double lo, double hi)
    {
        var bytes = new byte[pixels.Length];
        var scale = 255.0 / (hi - lo);
        for (var i = 0; i < pixels.Length; i++)
        {
            var v = pixels[i];
            bytes[i] = !double.IsFinite(v) ? (byte)0 : (byte)Math.Clamp(Math.Round((v - lo) * scale), 0, 255);
        }
        var bmp = BitmapSource.Create(width, height, 96, 96, PixelFormats.Gray8, null, bytes, width);
        bmp.Freeze();
        return bmp;
    }

    private void ApplyView()
    {
        if (_bitmapA is null || _bitmapB is null) return;
        if (_showDifference && _differenceBitmap is null) _differenceBitmap = DifferenceBitmap();
        _imageA.Source = _showDifference ? _differenceBitmap : _single && _showB ? _bitmapB : _bitmapA;
        _imageB.Source = _bitmapB;
        _viewers.Columns = _single ? 1 : 2;
        _panelB.Visibility = _single ? Visibility.Collapsed : Visibility.Visible;
        var name = _showDifference ? "A−B difference" : _single && _showB ? "B — new PRE" : "A — official Samsung 2G X64W";
        _captionA.Text = name;
        _toggleAb.Content = _showB ? "Show A (Ctrl+Tab)" : "Show B (Ctrl+Tab)";
        _difference.Content = _showDifference ? "Hide difference (Ctrl+D)" : "Difference A−B (Ctrl+D)";
        _modeText.Text = _showDifference ? _differenceLegend : _single ? name + " · one-position comparison" : "Side by side · shared W/L, zoom and scroll";
        foreach (var (image, canvas, label) in new[] { (_imageA, _overlayA, name), (_imageB, _overlayB, "B — new PRE") })
        {
            if (image.Source is not BitmapSource bitmap) continue;
            canvas.Width = bitmap.PixelWidth; canvas.Height = bitmap.PixelHeight;
            AutomationProperties.SetName(image, label);
            AutomationProperties.SetHelpText(image, $"Displayed {bitmap.PixelWidth}×{bitmap.PixelHeight}; source sampling step {_step}. No alignment applied.");
        }
    }

    private void RefreshDifference()
    {
        _differenceBitmap = null;
        ApplyView();
    }

    private BitmapSource DifferenceBitmap()
    {
        var a = _pixelsA!; var b = _pixelsB!;
        double peak = 0;
        for (var i = 0; i < a.Length; i++)
            if (double.IsFinite(a[i] - b[i])) peak = Math.Max(peak, Math.Abs(a[i] - b[i]));
        var extrema = _differenceExtrema.IsChecked == true;
        var range = extrema ? peak : _differenceK * (_markerPrepared?.Scale ?? 0);
        var fallback = !double.IsFinite(range);
        if (fallback) range = peak;
        var saturated = 0; var finite = 0;
        var bytes = new byte[a.Length * 4];
        for (var i = 0; i < a.Length; i++)
        {
            var difference = a[i] - b[i]; var offset = i * 4;
            if (!double.IsFinite(difference)) { bytes[offset] = 255; bytes[offset + 2] = 255; }
            else
            {
                finite++;
                if (Math.Abs(difference) > range) saturated++;
                // Zero MAD gives a zero-width range: zero is white, every nonzero value saturates.
                var magnitude = range > 0 ? Math.Min(1, Math.Abs(difference) / range) : difference == 0 ? 0 : 1;
                var pale = (byte)Math.Round(255 * (1 - magnitude));
                bytes[offset] = difference <= 0 ? (byte)255 : pale;
                bytes[offset + 1] = pale;
                bytes[offset + 2] = difference >= 0 ? (byte)255 : pale;
            }
            bytes[offset + 3] = 255;
        }
        _differenceLegend = $"Signed original-value A−B ({(extrema ? "extrema" : fallback ? "extrema fallback: k·s is non-finite" : "scale k·s")}): blue −{range.ToString("G6", CultureInfo.InvariantCulture)} · white 0 · red +{range.ToString("G6", CultureInfo.InvariantCulture)}; saturated {saturated}/{finite} display pixels (포화 화소 수). Magenta = non-finite. W/L does not change differences.";
        var bitmap = BitmapSource.Create(_displayWidth, _displayHeight, 96, 96, PixelFormats.Bgra32, null, bytes, _displayWidth * 4);
        bitmap.Freeze(); return bitmap;
    }

    private void DrawRoi(JsonNode roi)
    {
        long L(string k) => roi[k] is JsonValue v && v.TryGetValue<long>(out var l) ? l : 0;
        foreach (var canvas in new[] { _overlayA, _overlayB })
        {
            canvas.Children.Clear();
            canvas.ClipToBounds = true;
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
        foreach (var canvas in new[] { _overlayA, _overlayB }) foreach (var t in canvas.Children.OfType<TextBlock>().Where(x => Equals(x.Tag, "marker"))) t.FontSize = 12 / _zoom;
    }

    private void ZoomFit()
    {
        if (_displayWidth <= 0 || _displayHeight <= 0) return;
        var vw = Math.Max(50.0, Math.Min(_scrollA.ViewportWidth > 0 ? _scrollA.ViewportWidth : 500, _scrollB.ViewportWidth > 0 ? _scrollB.ViewportWidth : 500) - 4);
        var vh = Math.Max(50.0, Math.Min(_scrollA.ViewportHeight > 0 ? _scrollA.ViewportHeight : 300, _scrollB.ViewportHeight > 0 ? _scrollB.ViewportHeight : 300) - 4);
        SetZoom(Math.Min(vw / Math.Max(_displayWidth, _displayWidthB), vh / Math.Max(_displayHeight, _displayHeightB)));
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
            Invalidate("Manifest file changed since opening.");
            _verdict.Text = "Rejected — current file differs from the validated manifest. Not a pass.";
            _result.Text = $"Rejected: SHA-256 at open {snapshot.Sha256}\nCurrent SHA-256 {currentSha ?? "unreadable"}\nPrevious preview and validation cleared. Open and validate again.";
            return;
        }
        SetBusy(true);
        var cts = new CancellationTokenSource();
        _cancel = cts;
        _result.Text = "Computing in the contract-script child process. Source files are read only…";
        try
        {
            var response = await backend.CompareAsync(snapshot, cts.Token);
            if (gen != _generation || _closed) { if (!_closed) _result.Text = ""; return; }   // other inputs were opened meanwhile: this answer describes the old ones and is not shown as current
            UpdateReviewLine(response.Script);
            var reviewed = response.Script?.Reviewed == true;
            var text = new System.Text.StringBuilder();
            if (!response.Ok)
            {
                text.AppendLine("Failed — results cannot be trusted. Not a pass.");
                text.AppendLine(response.Failure);
                if (!string.IsNullOrWhiteSpace(response.StderrTail)) text.AppendLine("stderr: " + response.StderrTail);
                if (response.RunDirectory is not null) text.AppendLine("Script output directory (if created): " + response.RunDirectory);
                _state.Text = "Status: calculation failed. Not a pass. No 2G execution.";
            }
            else
            {
                var res = response.Result!;
                var status = res["status"]!.GetValue<string>();
                var reasons = (res["reasons"] as JsonArray)?.Select(x => x!.GetValue<string>()).ToList() ?? [];
                text.AppendLine($"Result: {status}" + (reasons.Count > 0 ? "  Reasons: " + string.Join(", ", reasons) : "") + (reviewed ? "" : "  [unreviewed contract]"));
                if (status == "computed")
                {
                    var kind = res["validation"]?["comparison_kind"]?.GetValue<string>() ?? "?";
                    var attribution = res["validation"]?["algorithm_attribution"]?.GetValue<bool>() == true;
                    text.AppendLine($"Valid ROI pixels {res["valid_pixels"]}  ·  Units {res["units"]}  ·  Difference = A − B  ·  Kind {kind}  ·  Contract attribution conditions: {(attribution ? "satisfied (production history and quality are not verified)" : "not applicable (non-attributable label)")}");
                    text.AppendLine($"Mean {F(res["mean_a_minus_b"])}  Population SD {F(res["std_a_minus_b"])}  RMSE {F(res["rmse"])}  Mean absolute difference {F(res["mean_abs"])}  Min {F(res["min_a_minus_b"])}  Max {F(res["max_a_minus_b"])}");
                    _state.Text = "Status: computed numeric differences between imported files." + (reviewed ? "" : " Response from an unreviewed contract.") + " This is not 2G execution or an image-quality verdict.";
                }
                else
                {
                    text.AppendLine("No numbers produced. " + string.Join(" ", reasons.Select(ComparisonReasonText.For)));
                    _state.Text = $"Status: comparison ended with {status}. Not a pass. No 2G execution.";
                }
                text.AppendLine($"Sources unchanged: {(res["sources_unchanged"]?.GetValue<bool>() == true ? "yes" : "no")}  ·  2G executed: {(res["actual_2g_executed"]?.GetValue<bool>() == true ? "yes" : "no")}  ·  Image-quality verdict: none");
                text.AppendLine("Saved to: " + response.RunDirectory);
                text.AppendLine("Validated manifest SHA-256: " + snapshot.Sha256);
            }
            _result.Text = text.ToString();
        }
        catch (Exception e) { if (gen == _generation) { _result.Text = "Failed: " + e.Message; _state.Text = "Status: calculation failed. Not a pass."; } }
        finally { if (ReferenceEquals(_cancel, cts)) _cancel = null; cts.Dispose(); if (!_closed) SetBusy(false); }
    }

    private static string F(JsonNode? n) => n is JsonValue v && v.TryGetValue<double>(out var d) ? d.ToString("G10", CultureInfo.InvariantCulture) : "—";
}
