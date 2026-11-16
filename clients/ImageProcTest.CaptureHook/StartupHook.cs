// GUI-C-255b / Refs #278. Loaded only by an observation launch using DOTNET_STARTUP_HOOKS.
using System.IO;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Imaging;

public static class StartupHook
{
    public static void Initialize()
    {
        string? configured = Environment.GetEnvironmentVariable("XPE_GUI_CAPTURE_DIR");
        if (string.IsNullOrWhiteSpace(configured)) return;
        string root = Path.GetFullPath(configured);
        if (!Directory.Exists(root)) return;
        new Thread(() => Poll(root)) { IsBackground = true, Name = "GUI-C-255b capture requests" }.Start();
    }

    static void Poll(string root)
    {
        string prefix = root.TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
        while (true)
        {
            try
            {
                var app = Application.Current;
                if (app != null && !app.Dispatcher.HasShutdownStarted)
                {
                    foreach (string file in Directory.EnumerateFiles(root, "*.request.json"))
                    {
                        string response = file.Replace(".request.json", ".response.json", StringComparison.Ordinal);
                        if (File.Exists(response)) continue;
                        try
                        {
                            using var request = JsonDocument.Parse(File.ReadAllText(file));
                            long hwnd = request.RootElement.GetProperty("hwnd").GetInt64();
                            string output = Path.GetFullPath(request.RootElement.GetProperty("output").GetString()!);
                            if (!output.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)) throw new InvalidOperationException("Capture output outside request directory");
                            object result = app.Dispatcher.Invoke(() => Capture(app, hwnd, output));
                            WriteResponse(response, JsonSerializer.Serialize(result, new JsonSerializerOptions { WriteIndented = true }));
                        }
                        catch (Exception ex)
                        {
                            WriteResponse(response, JsonSerializer.Serialize(new { error = ex.ToString() }));
                        }
                    }
                }
            }
            catch (Exception) { /* A partial atomic request is retried, or the app is shutting down. */ }
            Thread.Sleep(75);
        }
    }

    static void WriteResponse(string response, string text)
    {
        string temporary = response + ".tmp";
        File.WriteAllText(temporary, text);
        File.Move(temporary, response);
    }

    static object Capture(Application app, long hwnd, string output)
    {
        var window = app.Windows.Cast<Window>().Single(w => new WindowInteropHelper(w).Handle.ToInt64() == hwnd);
        var visual = window.Content as FrameworkElement ?? throw new InvalidOperationException("Window has no FrameworkElement content");
        if (!visual.IsLoaded || visual.ActualWidth <= 0 || visual.ActualHeight <= 0) throw new InvalidOperationException("Window content has no layout");
        var dpi = VisualTreeHelper.GetDpi(visual);
        int width = checked((int)Math.Ceiling(visual.ActualWidth * dpi.DpiScaleX));
        int height = checked((int)Math.Ceiling(visual.ActualHeight * dpi.DpiScaleY));
        var drawing = new DrawingVisual();
        using (var dc = drawing.RenderOpen())
        {
            var bounds = new Rect(0, 0, visual.ActualWidth, visual.ActualHeight);
            dc.DrawRectangle(Brushes.White, null, bounds);
            var brush = new VisualBrush(visual) { AutoLayoutContent = false, Stretch = Stretch.Fill };
            dc.DrawRectangle(brush, null, bounds);
        }
        var bitmap = new RenderTargetBitmap(width, height, dpi.PixelsPerInchX, dpi.PixelsPerInchY, PixelFormats.Pbgra32);
        bitmap.Render(drawing);
        var encoder = new PngBitmapEncoder(); encoder.Frames.Add(BitmapFrame.Create(bitmap));
        using (var stream = new FileStream(output, FileMode.CreateNew)) encoder.Save(stream);
        byte[] pixels = new byte[width * height * 4]; bitmap.CopyPixels(pixels, width * 4, 0);
        long nonwhite = 0;
        for (int p = 0; p < pixels.Length; p += 4) if (pixels[p] <= 245 || pixels[p + 1] <= 245 || pixels[p + 2] <= 245) nonwhite++;
        var texts = new List<object>();
        CollectText(visual, visual, texts);
        return new { method = "WPF RenderTargetBitmap / VisualBrush", output, hwnd, width, height,
            dpiX = dpi.PixelsPerInchX, dpiY = dpi.PixelsPerInchY, actualWidthDip = visual.ActualWidth, actualHeightDip = visual.ActualHeight,
            nonWhiteRatio = (double)nonwhite / (width * (long)height), threshold = 0.05, texts };
    }

    static void CollectText(DependencyObject node, FrameworkElement root, List<object> output)
    {
        if (node is TextBlock text && text.IsVisible && text.ActualWidth > 0 && text.ActualHeight > 0)
        {
            var bounds = text.TransformToAncestor(root).TransformBounds(new Rect(0, 0, text.ActualWidth, text.ActualHeight));
            output.Add(new { id = System.Windows.Automation.AutomationProperties.GetAutomationId(text), name = text.Name,
                text = text.Text, fontFamily = text.FontFamily.Source, fontSizeDip = text.FontSize, fontSizePt = text.FontSize * 0.75,
                x = bounds.X, y = bounds.Y, width = bounds.Width, height = bounds.Height,
                trimming = text.TextTrimming.ToString(), wrapping = text.TextWrapping.ToString() });
        }
        for (int i = 0; i < VisualTreeHelper.GetChildrenCount(node); i++) CollectText(VisualTreeHelper.GetChild(node, i), root, output);
    }
}
