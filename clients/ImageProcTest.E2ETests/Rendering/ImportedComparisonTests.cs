using System.Reflection;
using System.Security.Cryptography;
using System.Text.Json.Nodes;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using System.Windows.Interop;
using FlaUI.UIA3;
using FlaUI.Core.AutomationElements;
using Button = System.Windows.Controls.Button;
using TextBox = System.Windows.Controls.TextBox;
using Image = System.Windows.Controls.Image;

using ImageProcTest.Services.Comparison;
using ImageProcTest.Views;
using Xunit;

namespace ImageProcTest.E2ETests.Rendering;

public sealed class ImportedComparisonTests
{
    [Fact]
    public void C270_NotComparable_StillDisplaysLosslesslyPromotedOutputs()
        => OnWindow((w, _) =>
        {
            Assert.NotNull(Field<Image>(w, "_imageA").Source);
            Assert.NotNull(Field<Image>(w, "_imageB").Source);
            Assert.Contains("not comparable", Field<TextBlock>(w, "_verdict").Text);
            Assert.Contains("UNITS_MISMATCH", Field<TextBox>(w, "_reasons").Text);
            Assert.False(Field<Button>(w, "_computeButton").IsEnabled);
        });

    [Fact]
    public void C270_OnePositionToggle_AndSignedRawDifference_PreserveTheTwoSources()
        => OnWindow((w, _) =>
        {
            var a = Field<Image>(w, "_imageA").Source;
            var b = Field<Image>(w, "_imageB").Source;
            Click(w, "_toggleAb");
            Assert.Same(b, Field<Image>(w, "_imageA").Source);
            Click(w, "_toggleAb");
            Assert.Same(a, Field<Image>(w, "_imageA").Source);
            Click(w, "_difference");
            var image = Assert.IsAssignableFrom<BitmapSource>(Field<Image>(w, "_imageA").Source);
            var pixels = new byte[image.PixelWidth * image.PixelHeight * 4];
            image.CopyPixels(pixels, image.PixelWidth * 4, 0);
            // Raw uint16 A={1,2,6}, float32 B={2,2,4}: signed {-1,0,+2}, symmetric range +/-2.
            Assert.Equal(new byte[] { 255, 128, 128, 255 }, pixels[..4]);
            Assert.Equal(new byte[] { 255, 255, 255, 255 }, pixels[4..8]);
            Assert.Equal(new byte[] { 0, 0, 255, 255 }, pixels[8..12]);
            Click(w, "_difference");
            Assert.Same(a, Field<Image>(w, "_imageA").Source);
        });

    [Fact]
    public void C270_ZoomPanAndWindowLevel_AreShared()
        => OnWindow((w, _) =>
        {
            Invoke(w, "SetZoom", 8.0);
            Assert.Equal(8, Field<ScaleTransform>(w, "_scaleA").ScaleX);
            Assert.Equal(8, Field<ScaleTransform>(w, "_scaleB").ScaleX);
            var a = Field<System.Windows.Controls.ScrollViewer>(w, "_scrollA"); var b = Field<System.Windows.Controls.ScrollViewer>(w, "_scrollB");
            a.Width = b.Width = 160; a.Height = b.Height = 80;
            w.UpdateLayout(); a.ScrollToHorizontalOffset(64); a.ScrollToVerticalOffset(16); Pump();
            Assert.True(a.HorizontalOffset > 0);
            Assert.Equal(a.HorizontalOffset, b.HorizontalOffset);
            Assert.Equal(a.VerticalOffset, b.VerticalOffset);
            Field<TextBox>(w, "_window").Text = "8"; Field<TextBox>(w, "_level").Text = "4";
            Click(w, "_applyWl");
            Assert.Equal(64, FirstGray(Field<Image>(w, "_imageB"))); // B first value2 under [0,8]
            Assert.Equal(32, FirstGray(Field<Image>(w, "_imageA"))); // A first value1 under [0,8]
        });

    [Fact]
    public void C270_UnsupportedByteOrderAndStride_AreNotSilentlyReadAsPackedLittleEndian()
    {
        using var files = new ImportedComparisonFiles();
        var output = JsonNode.Parse(files.Manifest.ToJsonString())!["engines"]!["A"]!["output"]!;
        Assert.NotNull(ComparisonDisplayReader.ReadLayout(output, files.Directory));
        output["endian"] = "big";
        Assert.Null(ComparisonDisplayReader.ReadLayout(output, files.Directory));
        output["endian"] = "little"; output["stride_bytes"] = 258;
        Assert.Null(ComparisonDisplayReader.ReadLayout(output, files.Directory));
    }

    [Fact]
    public void C270_DifferentDimensions_AreDisplayedWithoutResizing_AndDifferenceIsUnavailable()
        => OnWindow((w, _) =>
        {
            var a = (BitmapSource)Field<Image>(w, "_imageA").Source;
            var b = (BitmapSource)Field<Image>(w, "_imageB").Source;
            Assert.Equal((128, 4), (a.PixelWidth, a.PixelHeight));
            Assert.Equal((64, 8), (b.PixelWidth, b.PixelHeight));
            Assert.False(Field<Button>(w, "_difference").IsEnabled);
            Assert.Contains("dimensions differ", Field<TextBlock>(w, "_viewHint").Text);
        }, files =>
        {
            var b = files.Manifest["engines"]!["B"]!["output"]!;
            b["width"] = 64; b["height"] = 8; b["stride_bytes"] = 256;
            File.WriteAllText(files.ManifestPath, files.Manifest.ToJsonString());
        });

    [Fact]
    public void C270_ZeroDifferenceIsWhite_AndNonFiniteIsExplicitlyMagenta()
        => OnWindow((w, _) =>
        {
            Click(w, "_difference");
            var image = (BitmapSource)Field<Image>(w, "_imageA").Source;
            var pixels = new byte[image.PixelWidth * image.PixelHeight * 4];
            image.CopyPixels(pixels, image.PixelWidth * 4, 0);
            Assert.Equal(new byte[] { 255, 0, 255, 255 }, pixels[..4]);
            Assert.Equal(new byte[] { 255, 255, 255, 255 }, pixels[4..8]);
            Assert.Contains("Magenta = non-finite", Field<TextBlock>(w, "_modeText").Text);
        }, files =>
        {
            using (var b = new BinaryWriter(File.Create(Path.Combine(files.Directory, "b.raw"))))
                for (var i = 0; i < 128 * 4; i++) b.Write(i == 0 ? float.NaN : i % 3 == 0 ? 1f : i % 3 == 1 ? 2f : 6f);
            files.Manifest["engines"]!["B"]!["output"]!["sha256"] = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(files.Directory, "b.raw")))).ToLowerInvariant();
            File.WriteAllText(files.ManifestPath, files.Manifest.ToJsonString());
        });

    [Fact]
    public async Task C270_ImportedOutputs_DisplayBothAndToggleWithoutComparabilityBlock()
    {
        using var files = new ImportedComparisonFiles();
        ComparisonImportWindow? hosted = null;
        var ready = new TaskCompletionSource<nint>(TaskCreationOptions.RunContinuationsAsynchronously);
        var thread = new Thread(() =>
        {
            SynchronizationContext.SetSynchronizationContext(new DispatcherSynchronizationContext(Dispatcher.CurrentDispatcher));
            hosted = new ComparisonImportWindow((_, _, _) => new NotComparableBackend());
            hosted.GetType().GetField("_rememberFile", BindingFlags.Instance | BindingFlags.NonPublic)!.SetValue(hosted, Path.Combine(files.Directory, "remember.json"));
            hosted.Show();
            hosted.Dispatcher.BeginInvoke(async () =>
            {
                try
                {
                    Field<TextBox>(hosted, "_manifestPath").Text = files.ManifestPath;
                    await (Task)Invoke(hosted, "OpenAsync")!;
                    hosted.UpdateLayout(); Field<Image>(hosted, "_imageA").BringIntoView();
                    ready.SetResult(new WindowInteropHelper(hosted).Handle);
                }
                catch (Exception e) { ready.SetException(e); }
            });
            Dispatcher.Run();
        });
        thread.SetApartmentState(ApartmentState.STA); thread.Start();
        try
        {
            var handle = await ready.Task;
            using var automation = new UIA3Automation();
            var window = FlaUI.Core.AutomationElements.AutomationElementExtensions.AsWindow(automation.FromHandle(handle));
            FlaUI.Core.AutomationElements.AutomationElement Find(string id) => window.FindFirstDescendant(cf => cf.ByAutomationId(id))
                ?? throw new InvalidOperationException("Missing " + id);
            Assert.Contains("A —", Find("ComparisonImageA").Name);
            Assert.Contains("B —", Find("ComparisonImageB").Name);
            Assert.Contains("Displayed 128×4", Find("ComparisonImageA").HelpText);
            Assert.Contains("Displayed 128×4", Find("ComparisonImageB").HelpText);
            Assert.Contains("UNITS_MISMATCH", Find("ComparisonReasonsList").AsTextBox().Text);
            Find("ComparisonToggleAbButton").AsButton().Invoke();
            // WPF's Invoke pattern enqueues the click. Drain that dispatcher operation, not a timed retry.
            hosted!.Dispatcher.Invoke(() => { }, DispatcherPriority.ContextIdle);
            Assert.Contains("B — new PRE", Find("ComparisonImageA").Name);
            Assert.Contains("one-position", Find("ComparisonDisplayMode").Name);
            Find("ComparisonToggleAbButton").AsButton().Invoke();
            hosted.Dispatcher.Invoke(() => { }, DispatcherPriority.ContextIdle);
            Assert.Contains("A —", Find("ComparisonImageA").Name);
        }
        finally
        {
            if (hosted is not null) hosted.Dispatcher.Invoke(() => { hosted.Close(); Dispatcher.CurrentDispatcher.BeginInvokeShutdown(DispatcherPriority.Send); });
            thread.Join();
        }
    }

    private static byte FirstGray(Image image)
    {
        var bmp = (BitmapSource)image.Source; var bytes = new byte[bmp.PixelWidth * bmp.PixelHeight];
        bmp.CopyPixels(bytes, bmp.PixelWidth, 0); return bytes[0];
    }

    private static void OnWindow(Action<ComparisonImportWindow, ImportedComparisonFiles> body, Action<ImportedComparisonFiles>? configure = null)
        => ComparisonRenderHarness.OnStaThread(() =>
        {
            SynchronizationContext.SetSynchronizationContext(new DispatcherSynchronizationContext(Dispatcher.CurrentDispatcher));
            using var files = new ImportedComparisonFiles();
            configure?.Invoke(files);
            var w = new ComparisonImportWindow((_, _, _) => new NotComparableBackend());
            try
            {
                w.GetType().GetField("_rememberFile", BindingFlags.Instance | BindingFlags.NonPublic)!.SetValue(w, Path.Combine(files.Directory, "remember.json"));
                w.Show();
                Field<TextBox>(w, "_manifestPath").Text = files.ManifestPath;
                var task = (Task)Invoke(w, "OpenAsync")!;
                var frame = new DispatcherFrame();
                task.ContinueWith(_ => w.Dispatcher.BeginInvoke(() => frame.Continue = false));
                Dispatcher.PushFrame(frame); task.GetAwaiter().GetResult(); w.UpdateLayout();
                body(w, files);
            }
            finally { w.Close(); }
            return true;
        });

    private static void Pump() => Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.ContextIdle);
    private static T Field<T>(object obj, string name) => (T)(obj.GetType().GetField(name, BindingFlags.Instance | BindingFlags.NonPublic)?.GetValue(obj)
        ?? throw new InvalidOperationException("Missing field " + name));
    private static object? Invoke(object obj, string name, params object[] args) => obj.GetType().GetMethod(name, BindingFlags.Instance | BindingFlags.NonPublic)!.Invoke(obj, args);
    private static void Click(object obj, string name) => Field<Button>(obj, name).RaiseEvent(new RoutedEventArgs(Button.ClickEvent));

    private sealed class NotComparableBackend : IComparisonBackend
    {
        public Task<ValidateResponse> ValidateAsync(ManifestSnapshot manifest, CancellationToken cancel) => Task.FromResult(new ValidateResponse(true, null,
            new JsonObject { ["comparable"] = false, ["reasons"] = new JsonArray("UNITS_MISMATCH") }, null, 2, "", ""));
        public Task<CompareResponse> CompareAsync(ManifestSnapshot manifest, CancellationToken cancel) => throw new InvalidOperationException("Statistics must stay disabled.");
    }
}

internal sealed class ImportedComparisonFiles : IDisposable
{
    public string Directory { get; } = Path.Combine(Path.GetTempPath(), "xpe-c270-" + Guid.NewGuid().ToString("N"));
    public string ManifestPath => Path.Combine(Directory, "pair.json");
    public JsonObject Manifest { get; }
    public ImportedComparisonFiles()
    {
        System.IO.Directory.CreateDirectory(Directory);
        using (var a = new BinaryWriter(File.Create(Path.Combine(Directory, "a.raw"))))
            for (var i = 0; i < 128 * 4; i++) a.Write((ushort)(i % 3 == 0 ? 1 : i % 3 == 1 ? 2 : 6));
        using (var b = new BinaryWriter(File.Create(Path.Combine(Directory, "b.raw"))))
            for (var i = 0; i < 128 * 4; i++) b.Write(i % 3 == 0 ? 2f : i % 3 == 1 ? 2f : 4f);
        JsonObject Output(string file, string dtype, int size) => new() { ["path"] = file, ["dtype"] = dtype, ["width"] = 128, ["height"] = 4,
            ["endian"] = "little", ["stride_bytes"] = 128 * size, ["offset_bytes"] = 0,
            ["sha256"] = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(Directory, file)))).ToLowerInvariant() };
        Manifest = new JsonObject { ["dtype_promotion"] = "float64-lossless", ["engines"] = new JsonObject {
            ["A"] = new JsonObject { ["output"] = Output("a.raw", "uint16", 2) }, ["B"] = new JsonObject { ["output"] = Output("b.raw", "float32", 4) } },
            ["roi"] = new JsonObject { ["x"] = 0, ["y"] = 0, ["width"] = 128, ["height"] = 4 } };
        File.WriteAllText(ManifestPath, Manifest.ToJsonString());
    }
    public void Dispose()
    {
        foreach (var f in System.IO.Directory.EnumerateFiles(Directory)) File.Delete(f);
        System.IO.Directory.Delete(Directory);
    }
}
