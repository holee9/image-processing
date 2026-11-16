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
        Title = "PRE Calibration — Validate, Generate, Reload and Evaluate";
        Width = 940; Height = 820; MinWidth = 720; MinHeight = 500;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        MaxHeight = SystemParameters.WorkArea.Height;
        Content = new ScrollViewer { Content = _form, VerticalScrollBarVisibility = ScrollBarVisibility.Auto };
        Label("This window does not control acquisition hardware. It reads original UINT16LE files and saves each run in a new folder. Generation or application is not an image-quality pass.");
        Label("1. Prepare inputs: one full file path per line. The first frame in the complete Dark list is excluded once by default. Gain uses the same dose; Defect may use a separate list with multiple doses.");
        var native = Field("Local tools/DLL directory (xpe_calib_fixture_gen.exe, xpe_pre_e2e.exe, v5 PRE/common)", Environment.GetEnvironmentVariable("XPE_NATIVE_DIR") ?? "");
        var output = Field("Parent output directory for new runs (existing files are not overwritten)", "");
        var width = Field("Width (pixels)", model.Settings.RawWidth.ToString());
        var height = Field("Height (pixels)", model.Settings.RawHeight.ToString());
        var dark = Field("Dark RAW files (acquisition order)", "", true);
        var gain = Field("Gain Bright RAW files (same conditions/dose)", "", true);
        var defect = Field("Defect Bright RAW files (empty uses Gain list)", "", true);
        var keep = new CheckBox { Content = "Include the first Dark frame (explicit --keep-first-dark)", Margin = new Thickness(0, 4, 0, 8) };
        _form.Children.Add(keep); _inputs.Add(keep);
        var expiry = Field("Explicit XCAL expiry: Unix epoch milliseconds (no inferred policy; 0 leaves expiry unspecified in the existing generator)", "0");
        Label("Unspecified expiry and generator session=generated do not establish detector UUID or approval for clinical use. The existing offset generator's 100 ms / 25°C are template values, not acquisition measurements.");
        var rawDeclaration = new CheckBox { Content = "I checked the acquisition records: inputs are raw, before offset subtraction (no corrected CalData)", Margin = new Thickness(0, 6, 0, 6) };
        _form.Children.Add(rawDeclaration); _inputs.Add(rawDeclaration);
        var creationInputs = _inputs.ToArray();
        AddButton("2. Validate inputs (no generation)", async () =>
        {
            if (_workflow is null)
            {
                if (rawDeclaration.IsChecked != true) throw new InvalidOperationException("Confirm the input correction state. Acquisition conditions and correction state are not inferred from pixel values.");
                if (string.IsNullOrWhiteSpace(native.Text) || string.IsNullOrWhiteSpace(output.Text)) throw new InvalidOperationException("Specify the tools and output directories.");
                _workflow = new CalibrationWorkflowService(output.Text, native.Text,
                    new CalibrationInputs(int.Parse(width.Text), int.Parse(height.Text), Lines(dark.Text), Lines(gain.Text), Lines(defect.Text), keep.IsChecked == true, ulong.Parse(expiry.Text)));
                foreach (var control in creationInputs) control.IsEnabled = false;
            }
            Append("Validation: " + await _workflow.ValidateAsync());
        });
        AddButton("3. Generate Offset / Gain / Defect and save a new set", async () =>
            Append("Generation: " + await Required().GenerateAsync() + "\nSet: " + Required().SetDirectory));
        AddButton("4. Reload saved set through the PRE public API in a separate process", async () =>
            Append("Reload: " + await Required().ReloadAsync() + "\nImage quality is still unverified."));
        AddButton("5. Apply paths to this GUI (does not run processing)", () =>
        {
            Required().VerifyReadyToApply();
            if (model.IsBackendTransitioning) throw new InvalidOperationException("Wait for the backend transition to finish before applying paths.");
            if (model.IsMockBackend) throw new InvalidOperationException("Mock is not real PRE application. Confirm the Native backend.");
            NativeRuntimeEvidence.VerifyPreCombination(Required().NativeDirectory);
            CalibrationWorkflowService.WriteNew(Path.Combine(Required().RunDirectory, "gui-apply-" + Guid.NewGuid().ToString("N") + ".json"),
                new { native = NativeRuntimeEvidence.Capture(), set = Required().SetDirectory, quality = "경로 적용 준비 검증; 실제 Run/품질 합격 아님" });
            model.Settings.OffsetCalibrationDirectory = Required().SetDirectory;
            model.Settings.GainCalibrationDirectory = Required().SetDirectory;
            model.Settings.DefectCalibrationDirectory = Required().SetDirectory;
            Append("Paths applied. Close this window, open the evaluation RAW and run Run Preprocessing. Check actual Native/Applied status and warnings. This is not a quality pass.");
            return Task.CompletedTask;
        });
        Label("6. Independent evaluation: a SHA shared with any generation input, including the excluded first Dark, is rejected. Evaluation uses detector-domain PRE output before enhancement/VOI. Check acquisition conditions and ground-truth provenance against separate records.");
        var holdout = Field("Full path to evaluation RAW", "");
        var kind = new ComboBox { ItemsSource = new[] { "image", "dark", "flat" }, SelectedIndex = 0, Margin = new Thickness(0, 4, 0, 8) };
        _form.Children.Add(kind);
        Label("Gain semantics (choose from user evidence; keep unknown if unclear; conditional FPN evaluation remains unverified)");
        var semantics = new ComboBox { ItemsSource = new[] { "unknown", "normalized_gain", "reciprocal_gain" }, SelectedIndex = 0 };
        _form.Children.Add(semantics);
        var oracle = Field("Optional independent external defect oracle UINT8 (0 good / nonzero bad), not the source RAW", "");
        var reference = Field("Optional independent reference FLOAT32LE (same stage, alignment, size and ADU), not the source RAW", "");
        AddButton("6. Evaluate independently and save reports", async () =>
        {
            var result = await Required().EvaluateAsync(holdout.Text, (string)kind.SelectedItem, oracle.Text, reference.Text, (string)semantics.SelectedItem);
            Append("Evaluation output: " + result + "\n" + CalibrationWorkflowService.QualitySummary(result)
                + "\nExit code 0 means execution succeeded. Items without an oracle/reference remain unverified.");
        });
        Label("Close and reopen this window to regenerate or change inputs. Keep the output folder's inputs/manifest/reload/evaluate reports with the original acquisition records. Distinct SHA values alone do not establish external oracle/reference independence.");
        _form.Children.Add(_log);
        Closing += (_, e) => { if (_busy) { e.Cancel = true; Append("This window stays open until its local task finishes. Other processes are not interrupted."); } };
    }

    private static string[] Lines(string text) => text.Split(['\r', '\n'], StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
    private CalibrationWorkflowService Required() => _workflow ?? throw new InvalidOperationException("Validate inputs first.");
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
            Append("Running: " + title + " — wait for completion. Source files and other processes are preserved.");
            try { await action(); }
            catch (Exception error) { Append("Failed/incomplete: " + error.Message); }
            finally { _busy = false; }
        };
        _form.Children.Add(button);
    }
    private void Append(string message) { _log.AppendText(message + Environment.NewLine); _log.ScrollToEnd(); }
}
