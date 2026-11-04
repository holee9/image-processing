// GUI-C-250 (#273): one raw frame corrected in the app with a calibration set, the pictures and the corrected data saved so two sets can be compared outside the app.
// UI Automation only (menus by pattern calls, the file dialogs by their own edit and button). The pictures are screen copies of the window's rectangle; nothing is sent to whichever window is in front.
using System.Runtime.InteropServices;
using Xunit;
using Xunit.Abstractions;
using GuiApp = ImageProcTest.E2ETests.Scenarios.Operator.QuickStartWalkthroughScenarios.GuiApp;

namespace ImageProcTest.E2ETests.Scenarios.Operator;

/// <summary>
/// Needs <c>XPE_C231_NATIVE_DIR</c> (DLLs), <c>XPE_C250_CALIB</c> (the calibration folder), <c>XPE_C247_RAW</c> (the raw frame), <c>XPE_C250_OUT</c> (an output folder of this session's own) and
/// <c>XPE_C250_TAG</c> (a short label for the files). Skipped with a reason if one is unset; it fails if they are set and a path is missing. The set lives outside the repository, so CI cannot run it (XPE-SKIP-ALLOWED:273).
/// </summary>
public sealed class RealTestScenarios(ITestOutputHelper output)
{
    private static string? Env(string name) => Environment.GetEnvironmentVariable(name) is { Length: > 0 } v ? v : null;

    [DllImport("user32.dll")] private static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr hwnd, out Rect rect);
    [StructLayout(LayoutKind.Sequential)] private struct Rect { public int Left, Top, Right, Bottom; }

    /// <summary>The window's own picture through PrintWindow (PW_RENDERFULLCONTENT), which asks the window to paint itself; no input is sent and the screen is not read.</summary>
    private static string PrintPicture(IntPtr window, string path)
    {
        GetWindowRect(window, out var r);
        using var bitmap = new System.Drawing.Bitmap(r.Right - r.Left, r.Bottom - r.Top, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
        using (var g = System.Drawing.Graphics.FromImage(bitmap))
        {
            var dc = g.GetHdc();
            try { PrintWindow(window, dc, 2); } finally { g.ReleaseHdc(dc); }
        }
        var first = bitmap.GetPixel(40, 80);
        var varied = false;
        for (var y = 80; y < bitmap.Height - 20 && !varied; y += 17)
            for (var x = 40; x < bitmap.Width - 20; x += 17)
                if (bitmap.GetPixel(x, y) != first) { varied = true; break; }
        bitmap.Save(path, System.Drawing.Imaging.ImageFormat.Png);
        return $"{path} {bitmap.Width}x{bitmap.Height} {(varied ? "inside has variation" : "inside is UNIFORM")}";
    }

    [SkippableFact]
    public void R1_OneFrame_CorrectedWithASet_PicturesAndDataSaved()
    {
        var native = Env("XPE_C231_NATIVE_DIR");
        var calib = Env("XPE_C250_CALIB");
        var raw = Env("XPE_C247_RAW");
        var outRoot = Env("XPE_C250_OUT");
        var tag = Env("XPE_C250_TAG");
        Skip.If(string.IsNullOrEmpty(native) || string.IsNullOrEmpty(calib) || string.IsNullOrEmpty(raw) || string.IsNullOrEmpty(outRoot) || string.IsNullOrEmpty(tag),
            "XPE_C231_NATIVE_DIR/XPE_C250_CALIB/XPE_C247_RAW/XPE_C250_OUT/XPE_C250_TAG not set - the real-frame set is not in the repository. XPE-SKIP-ALLOWED:273");
        Assert.True(Directory.Exists(calib!), "the calibration folder does not exist: " + calib);
        Assert.True(File.Exists(raw!), "the raw frame does not exist: " + raw);

        // The app saves beside the image it opened, so the test opens its own copy inside the output folder.
        var dir = Path.Combine(outRoot!, tag!);
        if (Directory.Exists(dir)) Directory.Delete(dir, recursive: true);
        Directory.CreateDirectory(dir);
        var copy = Path.Combine(dir, "input.raw");
        File.Copy(raw!, copy);

        using var app = GuiApp.Launch(output, native!, extraArguments:
            ["--automation-backend", "Native", "--automation-calib", calib!, "--automation-width", "3072", "--automation-height", "3072"]);
        app.Menu("FileMenu");
        app.InvokeMenuItemThenDialog("OpenRawMenuItem", copy, confirmId: "1");
        app.WaitFor(() => app.AnyTextContains("RAW 3072x3072"), "the image summary appears", 90);
        app.WaitFor(() => app.Read("ChainStatusText").Contains("preprocess=Applied", StringComparison.Ordinal), "preprocess=Applied", 180);
        Thread.Sleep(3000);

        var chain = app.Read("ChainStatusText");
        var hud = app.Read("HudCalibStages");
        var status = app.Read("StatusBarText");
        output.WriteLine($"   chain: {chain}");
        output.WriteLine($"   HUD: '{hud}'");
        output.WriteLine($"   status: {status}");

        app.Menu("ViewMenu");
        if (!app.IsToggled("ShowLogsPanelMenuItem")) app.InvokeMenuItem("ShowLogsPanelMenuItem");
        app.ClickButtonByName("Log");
        Thread.Sleep(1000);
        var log = app.ListItems("LogListBox");
        output.WriteLine($"   log lines: {log.Count}; ALERT lines: {log.Count(l => l.Contains("ALERT ", StringComparison.Ordinal))}");
        foreach (var line in log) output.WriteLine("   LOG " + line);

        foreach (var (item, label) in new[] { ("CompareSourceOnlyMenuItem", "source_only"), ("CompareProcessedOnlyMenuItem", "processed_only"), ("CompareDifferenceMenuItem", "difference") })
        {
            app.Menu("ViewMenu", "CompareModeMenuItem");
            app.InvokeMenuItem(item);
            Thread.Sleep(2000);
            output.WriteLine("   picture " + label + ": " + PrintPicture(app.WindowHandle, Path.Combine(dir, $"screen_{label}.png")) + " | screen copy: " + app.SaveWindowPicture(Path.Combine(dir, $"screencopy_{label}.png")));
        }

        app.Menu("FileMenu");
        app.InvokeMenuItemThenDialog("SaveCorrectedFloatMenuItem", null, confirmId: "1");
        string floatPath = "";
        app.WaitFor(() => (floatPath = Directory.GetFiles(dir, "*_corrected_f32le.raw").FirstOrDefault() ?? "") != "" && app.Read("StatusBarText").StartsWith("Saved corrected image (float32 raw)", StringComparison.Ordinal), "the float file is saved", 60);
        output.WriteLine("   status: " + app.Read("StatusBarText"));
        app.Menu("FileMenu");
        app.InvokeMenuItemThenDialog("SaveCorrectedPngMenuItem", null, confirmId: "1");
        string pngPath = "";
        app.WaitFor(() => (pngPath = Directory.GetFiles(dir, "*_corrected_16bit.png").FirstOrDefault() ?? "") != "" && app.Read("StatusBarText").StartsWith("Saved corrected image (16-bit PNG)", StringComparison.Ordinal), "the PNG is saved", 120);
        output.WriteLine("   status: " + app.Read("StatusBarText"));
        output.WriteLine($"   saved: {floatPath} ({new FileInfo(floatPath).Length} bytes), {pngPath} ({new FileInfo(pngPath).Length} bytes)");

        Assert.Contains("preprocess=Applied", chain, StringComparison.Ordinal);
    }
}
