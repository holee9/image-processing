// GUI-C-255 / Refs #278. Observation harness. UI Automation patterns only.
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text.Json;
using FlaUI.Core;
using FlaUI.Core.AutomationElements;
using FlaUI.Core.Definitions;
using FlaUI.UIA3;
using IO = System.IO;

internal static class Program
{
    [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr hwnd, IntPtr dc, uint flags);
    [DllImport("user32.dll")] static extern uint GetDpiForWindow(IntPtr hwnd);
    [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll")] static extern bool IsWindow(IntPtr hwnd);
    static readonly JsonSerializerOptions Json = new() { WriteIndented = true };
    static string evidence = "";
    static Process? process;
    static UIA3Automation automation = null!;
    static Window window = null!;
    static int expectedDpi = 96;
    static readonly System.Collections.Concurrent.ConcurrentQueue<Exception> InvocationErrors = new();
    static readonly List<Task> Invocations = new();
    static readonly object LogLock = new();
    static void Note(string s) { lock (LogLock) { var line = $"{DateTimeOffset.Now:O} {s}"; Console.WriteLine(line); IO.File.AppendAllText(IO.Path.Combine(evidence, "flow.log"), line + "\n"); } }
    static AutomationElement Id(AutomationElement parent, string id) => parent.FindFirstDescendant(cf => cf.ByAutomationId(id)) ?? throw new InvalidOperationException($"Missing UIA id {id}");
    static void Invoke(AutomationElement e)
    {
        if (!e.IsEnabled) throw new InvalidOperationException($"Disabled UIA element {e.AutomationId}");
        var pattern = e.Patterns.Invoke.Pattern;
        Invocations.Add(Task.Run(() => { try { pattern.Invoke(); } catch (Exception ex) { InvocationErrors.Enqueue(ex); Note("INVOKE_ERROR=" + ex.Message); } }));
        Thread.Sleep(400);
    }
    static void Menu(string menu, string item)
    {
        Id(window, menu).Patterns.ExpandCollapse.Pattern.Expand(); Thread.Sleep(200);
        var target = Id(window, item);
        if (item == "ShowCalibrationPanelMenuItem" && target.Patterns.Toggle.IsSupported)
        {
            if (target.Patterns.Toggle.Pattern.ToggleState.Value != ToggleState.On) target.Patterns.Toggle.Pattern.Toggle();
            Id(window, menu).Patterns.ExpandCollapse.Pattern.Collapse();
            for (int n = 0; n < 50 && window.FindFirstDescendant(cf => cf.ByAutomationId("BrowseOffsetCalibrationButton")) == null; n++) Thread.Sleep(100);
        }
        else Invoke(target);
    }
    static object Dump(AutomationElement e)
    {
        var r = e.BoundingRectangle;
        string font = "unsupported";
        try { if (e.Patterns.Text.IsSupported) font = e.Patterns.Text.Pattern.DocumentRange.GetAttributeValue(FlaUI.UIA3.Identifiers.TextAttributes.FontSize).ToString() ?? "null"; } catch (Exception ex) { font = ex.GetType().Name; }
        return new { id = e.AutomationId, name = e.Name, type = e.ControlType.ToString(), enabled = e.IsEnabled, offscreen = e.IsOffscreen,
            x = r.X, y = r.Y, width = r.Width, height = r.Height, fontSizePt = font,
            invoke = e.Patterns.Invoke.IsSupported, value = e.Patterns.Value.IsSupported, text = e.Patterns.Text.IsSupported };
    }
    static void Snapshot(string stage)
    {
        foreach (var w in automation.GetDesktop().FindAllChildren(cf => cf.ByProcessId(process!.Id)))
        {
            var r = w.BoundingRectangle; if (r.Width < 1 || r.Height < 1) continue;
            var handle = w.Properties.NativeWindowHandle.Value;
            string stem = IO.Path.Combine(evidence, stage + "-" + handle);
            using (var image = new Bitmap(r.Width, r.Height))
            using (var g = Graphics.FromImage(image)) { var dc = g.GetHdc(); var printed = PrintWindow(handle, dc, 2); g.ReleaseHdc(dc); image.Save(stem + ".print.png", ImageFormat.Png); Note("PRINTWINDOW=" + printed); if (!printed) throw new InvalidOperationException("PrintWindow failed; saved image is not valid visual evidence"); }
            GetWindowThreadProcessId(GetForegroundWindow(), out uint front);
            if (front == process!.Id)
            { using var image = new Bitmap(r.Width, r.Height); using var g = Graphics.FromImage(image); g.CopyFromScreen(r.X, r.Y, 0, 0, image.Size); image.Save(stem + ".screen.png", ImageFormat.Png); }
            else Note("SCREEN_SKIPPED=another process is foreground");
            var rows = new List<object>();
            foreach (var e in new[] { w }.Concat(w.FindAllDescendants())) { try { rows.Add(Dump(e)); } catch (Exception ex) { rows.Add(new { dumpError = ex.Message }); } }
            IO.File.WriteAllText(stem + ".uia.json", JsonSerializer.Serialize(rows, Json));
            Note($"STAGE={stage} window={w.Name} dpi={GetDpiForWindow(handle)} elements={rows.Count}");
            if (handle == window.Properties.NativeWindowHandle.Value && GetDpiForWindow(handle) != expectedDpi)
                throw new InvalidOperationException($"Requested DPI {expectedDpi} differs from actual {GetDpiForWindow(handle)}");
        }
    }
    static Window Dialog()
    {
        static bool IsFileDialog(AutomationElement e) => e.Name == "Select calibration directory" || e.Name == "Load Raw Image" || e.Name.StartsWith("Save Corrected Image", StringComparison.Ordinal);
        for (int i = 0; i < 100; i++)
        {
            var d = window.ModalWindows.FirstOrDefault(IsFileDialog);
            if (d != null) return d;
            d = window.FindAllDescendants(cf => cf.ByControlType(ControlType.Window)).FirstOrDefault(IsFileDialog)?.AsWindow();
            if (d != null) return d;
            d = automation.GetDesktop().FindAllChildren(cf => cf.ByProcessId(process!.Id)).FirstOrDefault(IsFileDialog)?.AsWindow();
            if (d != null) return d;
            Thread.Sleep(100);
        }
        throw new InvalidOperationException("No file/folder dialog appeared");
    }
    static void DialogPath(string path)
    {
        var d = Dialog(); Note("DIALOG=" + d.Name); Snapshot("dialog-" + DateTime.Now.ToString("HHmmss"));
        IntPtr dialogHandle = d.Properties.NativeWindowHandle.Value;
        if (d.Name.StartsWith("Save Corrected Image", StringComparison.Ordinal))
        {
            // Preserve the app's suggested unique name. Its source is a copy in this run directory.
            Invoke(Id(d, "1")); Thread.Sleep(1500);
            for (int n = 0; n < 50 && IsWindow(dialogHandle); n++) Thread.Sleep(100);
            if (IsWindow(dialogHandle)) { Snapshot("save-dialog-remained"); throw new InvalidOperationException("Save dialog remained open; no overwrite confirmation was accepted"); }
            return;
        }
        AutomationElement? edit = null;
        for (int n = 0; n < 50 && edit == null; n++)
        {
            edit = d.FindAllDescendants(cf => cf.ByControlType(ControlType.Edit)).FirstOrDefault(e => new[] { "1001", "1152", "1148" }.Contains(e.AutomationId) && e.Patterns.Value.IsSupported);
            if (edit == null) Thread.Sleep(100);
        }
        if (edit == null) throw new InvalidOperationException("File/folder name Value pattern unavailable");
        edit.Patterns.Value.Pattern.SetValue(path.Replace('/', '\\'));
        Invoke(Id(d, "1")); Thread.Sleep(1500);
        for (int n = 0; n < 50 && IsWindow(dialogHandle); n++) Thread.Sleep(100);
        if (IsWindow(dialogHandle)) { Snapshot("dialog-not-closed"); throw new InvalidOperationException("Dialog remained open after Value/Invoke"); }
    }
    [STAThread]
    static int Main(string[] args)
    {
        if (args.Length >= 2 && args[0] == "--display-settings")
        {
            Process.Start(new ProcessStartInfo("ms-settings:display") { UseShellExecute = true });
            Thread.Sleep(2500);
            using var displayUia = new UIA3Automation();
            var settingsProcess = Process.GetProcessesByName("SystemSettings").First();
            AutomationElement? settingsWindow = null;
            for (int attempt = 0; attempt < 50 && settingsWindow == null; attempt++)
            {
                settingsWindow = displayUia.GetDesktop().FindFirstChild(cf => cf.ByProcessId(settingsProcess.Id))
                    ?? displayUia.GetDesktop().FindFirstChild(cf => cf.ByName("설정"))
                    ?? displayUia.GetDesktop().FindFirstChild(cf => cf.ByName("Settings"));
                if (settingsWindow == null) Thread.Sleep(200);
            }
            if (settingsWindow == null) { Console.WriteLine("DISPLAY_SETTINGS_BLOCKED=No Settings UIA window after 10 seconds"); return 1; }
            if (args.Length == 3)
            {
                var combo = Id(settingsWindow, "SystemSettings_Display_Scaling_ItemSizeOverride_ComboBox");
                Console.WriteLine("SCALE_BEFORE=" + string.Join("|", combo.FindAllDescendants().Select(e => e.Name)));
                combo.Patterns.ExpandCollapse.Pattern.Expand(); Thread.Sleep(300);
                var option = settingsWindow.FindAllDescendants().FirstOrDefault(e => e.Patterns.SelectionItem.IsSupported && e.Name.StartsWith(args[2] + "%", StringComparison.Ordinal));
                if (option == null) { Console.WriteLine("SCALE_BLOCKED=Requested option not exposed by UIA"); return 1; }
                option.Patterns.SelectionItem.Pattern.Select(); Thread.Sleep(2000);
                Console.WriteLine("SCALE_REQUESTED=" + args[2]);
            }
            var elements = settingsWindow.FindAllDescendants();
            IO.File.WriteAllText(args[1], JsonSerializer.Serialize(elements.Select(e => {
                try { return new { id = e.AutomationId, name = e.Name, type = e.ControlType.ToString(), value = e.Patterns.Value.IsSupported ? e.Patterns.Value.Pattern.Value.Value : "", selection = e.Patterns.SelectionItem.IsSupported, expand = e.Patterns.ExpandCollapse.IsSupported }; }
                catch { return new { id = "error", name = "", type = "", value = "", selection = false, expand = false }; }
            }), Json));
            return 0;
        }
        if (args.Length == 6 && args[0] == "--compare")
        { try { PreCli.Compare(args[1], args[2], args[3], args[4], args[5]); return 0; } catch (Exception ex) { Console.Error.WriteLine("CLI_COMPARISON_ERROR=" + ex.Message); return 1; } }
        string root = IO.Path.GetFullPath(args[0]), run = IO.Path.GetFullPath(args[1]), set = args[2];
        expectedDpi = args.Length >= 4 ? int.Parse(args[3], System.Globalization.CultureInfo.InvariantCulture) : 96;
        evidence = IO.Path.Combine(run, set + "-uia3-" + DateTime.Now.ToString("HHmmss")); IO.Directory.CreateDirectory(evidence);
        string exe = IO.Path.Combine(root, "gui/ImageProcTest/bin/x64/RelWithDebInfo/net8.0-windows/ImageProcTest.exe");
        string cal = $"D:/workspace-github/xpe-2g-offline/pre-w3072-20261022-1240/{set}/xcal";
        string rawSource = $"D:/workspace-github/xpe-2g-offline/batch-20261022-1230/in/{set}/img/001.raw";
        string raw = IO.Path.Combine(evidence, "input-001.raw"); IO.File.Copy(rawSource, raw, overwrite: false);
        string settings = IO.Path.Combine(evidence, "settings.json"); IO.File.WriteAllText(settings, "{}");
        var info = new ProcessStartInfo(exe) { UseShellExecute = false, WorkingDirectory = IO.Path.GetDirectoryName(exe)! };
        foreach (var a in new[] { "--automation-backend", "Native", "--automation-settings", settings, "--automation-width", "3072", "--automation-height", "2560" }) info.ArgumentList.Add(a);
        info.Environment["XPE_NATIVE_DIR"] = IO.Path.Combine(root, "build/gui-c255-native/bin"); info.Environment["XPE_NATIVE_DIR_EXCLUSIVE"] = "1"; info.Environment.Remove("XPE_E2E_ALLOW_GLOBAL_INPUT");
        using var uia = new UIA3Automation(); automation = uia;
        try
        {
            process = Process.Start(info)!; Note($"LAUNCH pid={process.Id} raw={raw} cal={cal}");
            using var app = FlaUI.Core.Application.Attach(process.Id);
            window = app.GetMainWindow(automation, TimeSpan.FromSeconds(30)); if (window == null) throw new InvalidOperationException("No main window");
            Thread.Sleep(1500); Snapshot("01-launch");
            Menu("ViewMenu", "ShowCalibrationPanelMenuItem"); Snapshot("02-calibration-panel");
            foreach (string kind in new[] { "Offset", "Gain", "Defect" }) { Invoke(Id(window, "Browse" + kind + "CalibrationButton")); DialogPath(cal); Snapshot("03-calibration-" + kind); }
            Menu("FileMenu", "OpenRawMenuItem"); DialogPath(raw); Thread.Sleep(3000); Snapshot("04-raw-open");
            Menu("PipelineMenu", "RunPreprocessingMenuItem"); Thread.Sleep(5000); Snapshot("05-pre-result");
            var metrics = window.FindAllDescendants(cf => cf.ByName("Metrics")).First(e => e.Patterns.Invoke.IsSupported); Invoke(metrics); Snapshot("06-metrics");
            string saved = IO.Path.Combine(evidence, "input-001_3072x2560_corrected_f32le.raw");
            Menu("FileMenu", "SaveCorrectedFloatMenuItem"); DialogPath(saved); Snapshot("07-saved");
            Note("SAVED_EXISTS=" + IO.File.Exists(saved));
            Menu("FileMenu", "ExitMenuItem"); if (!process.WaitForExit(10000)) throw new InvalidOperationException("App did not exit after UIA Exit"); Note("EXIT=" + process.ExitCode);
            if (!Task.WaitAll(Invocations.ToArray(), TimeSpan.FromSeconds(3))) throw new InvalidOperationException("UIA invocation did not return after app exit");
            if (!InvocationErrors.IsEmpty) throw new AggregateException("UIA invocation failed", InvocationErrors);
            IO.File.WriteAllText(IO.Path.Combine(evidence, "08-exit.uia.json"), "[]");
            PreCli.Compare(root, cal, raw, saved, evidence); Note("CLI_COMPARISON_WRITTEN"); return 0;
        }
        catch (Exception ex) { Note("BLOCKED=" + ex); if (window != null) try { Snapshot("99-blocked"); } catch (Exception se) { Note("SNAPSHOT_ERROR=" + se.Message); } return 1; }
        finally { if (process != null) { if (!process.HasExited) { process.Kill(); process.WaitForExit(5000); } Note("OWNED_PROCESS_REMAINING=" + (process.HasExited ? 0 : 1)); process.Dispose(); } }
    }
}
