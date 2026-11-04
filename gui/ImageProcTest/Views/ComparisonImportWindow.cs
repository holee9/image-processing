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
    private readonly Button _openButton = new() { Content = "Open and validate" };
    private readonly Button _computeButton = new() { Content = "Compute and save A−B statistics", IsEnabled = false };
    private readonly Button _cancelButton = new() { Content = "Cancel", IsEnabled = false };
    private readonly Button _applyWl = new() { Content = "Apply W/L", IsEnabled = false };
    private readonly Button _fit = new() { Content = "Fit", IsEnabled = false };
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
        root.Children.Add(_verdict); root.Children.Add(_reasons); root.Children.Add(_engines);

        // display controls: one window/level, one zoom, the same ROI
        var controls = new WrapPanel { Margin = new Thickness(0, 8, 0, 4) };
        controls.Children.Add(new TextBlock { Text = "Display-only W/L (shared by both images):  Window ", VerticalAlignment = VerticalAlignment.Center });
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
        viewers.Children.Add(Viewer("A — official Samsung 2G X64W (imported output)", _scrollA, _hostA, _imageA, _overlayA, _scaleA, "ComparisonViewerA"));
        viewers.Children.Add(Viewer("B — new PRE (imported output)", _scrollB, _hostB, _imageB, _overlayB, _scaleB, "ComparisonViewerB"));
        root.Children.Add(viewers);

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
        _scrollA.ScrollChanged += (_, _) => Sync(_scrollA, _scrollB);
        _scrollB.ScrollChanged += (_, _) => Sync(_scrollB, _scrollA);
        _manifestPath.TextChanged += (_, _) => Invalidate("Manifest path changed. Open and validate again.");
        _scriptPath.TextChanged += (_, _) => Invalidate("Contract script path changed. Open and validate again.");
        _pythonPath.TextChanged += (_, _) => Invalidate("Python path changed. Open and validate again.");
        _outputRoot.TextChanged += (_, _) => Invalidate("Output directory changed. Open and validate again.");
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
        _generation++;
        _snapshot = null; _manifest = null; _validation = null; _backend = null; _pixelsA = _pixelsB = null;
        _imageA.Source = _imageB.Source = null; _overlayA.Children.Clear(); _overlayB.Children.Clear();
        if (!_busy) _computeButton.IsEnabled = false;
        foreach (var b in new[] { _applyWl, _fit, _actual, _zoomIn, _zoomOut }) b.IsEnabled = false;
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
            if (response.Ok && response.Validation?["comparable"]?.GetValue<bool>() == true) await LoadDisplayAsync(gen, cts.Token);
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
        _verdict.Text = (comparable ? $"Script response: comparable — kind {kind}" : "Script response: not comparable — no numbers until the reasons below are resolved.")
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
        if (a is null || b is null) { _viewHint.Text = "Display layout could not be read or exceeds the preview limit (2^24 pixels per dimension). No preview; comparability is a separate result."; return; }
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
        if (mismatch is not null) { _viewHint.Text = $"{mismatch} output SHA-256 differs from the validated manifest. No preview drawn. Open again."; return; }
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
        _roiText.Text = $"ROI (manifest integer coordinates, shared by A and B): x={roi["x"]}, y={roi["y"]}, width={roi["width"]}, height={roi["height"]}";
        _viewHint.Text = $"Display-only copy: source {a.Width}×{a.Height}" + (step > 1 ? $", sampled every {step} pixels ({dw}×{dh}) — display coordinate = source coordinate ÷ {step}" : ", no downsampling")
            + ". The contract script computes statistics from source-file ROI rows, not display arrays. W/L, zoom and downsampling do not affect the numbers. Initial W/L spans all finite values in both images; no threshold is selected.";
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
                _viewHint.Text = "Invalid W/L: numeric values are required and Window must be positive. Display unchanged.";
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
