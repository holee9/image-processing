using System.Reflection;
using System.Security.Cryptography;
using System.Text.Json.Nodes;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Threading;
using System.Windows.Interop;
using FlaUI.UIA3;
using FlaUI.Core.Definitions;
using ImageProcTest.Services.Comparison;
using ImageProcTest.Views;
using Xunit;

namespace ImageProcTest.E2ETests.Rendering;

public sealed class ImportedMarkerTests
{
    [Fact]
    public async Task C271_RegionsAreListed_AndSelectingOneZoomsAndCentersBothViews()
    {
        using var files = new ImportedComparisonFiles();
        const int width = 256, height = 256;
        using (var a = new BinaryWriter(File.Create(Path.Combine(files.Directory, "a.raw"))))
            for (var y = 0; y < height; y++) for (var x = 0; x < width; x++)
            { var value = 100 + (x + y) % 3 - 1; if (x >= 20 && x < 26 && y >= 20 && y < 26) value += 20; if (x >= 100 && x < 106 && y >= 100 && y < 106) value += 30; if (x >= 210 && x < 216 && y >= 210 && y < 216) value += 40; a.Write((ushort)value); }
        using (var b = new BinaryWriter(File.Create(Path.Combine(files.Directory, "b.raw")))) for (var i = 0; i < width * height; i++) b.Write(100f);
        foreach (var key in new[] { "A", "B" })
        {
            var o = files.Manifest["engines"]![key]!["output"]!; o["width"] = width; o["height"] = height; o["stride_bytes"] = width * (key == "A" ? 2 : 4);
            o["sha256"] = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(files.Directory, key == "A" ? "a.raw" : "b.raw"))));
        }
        files.Manifest["roi"] = new JsonObject { ["x"] = 0, ["y"] = 0, ["width"] = width, ["height"] = height };
        File.WriteAllText(files.ManifestPath, files.Manifest.ToJsonString());
        ComparisonImportWindow? hosted = null; var ready = new TaskCompletionSource<nint>(TaskCreationOptions.RunContinuationsAsynchronously);
        var thread = new Thread(() =>
        {
            SynchronizationContext.SetSynchronizationContext(new DispatcherSynchronizationContext(Dispatcher.CurrentDispatcher));
            hosted = new ComparisonImportWindow((_, _, _) => new ContractStub());
            hosted.GetType().GetField("_rememberFile", BindingFlags.NonPublic | BindingFlags.Instance)!.SetValue(hosted, Path.Combine(files.Directory, "remember.json")); hosted.Show();
            hosted.Dispatcher.BeginInvoke(async () =>
            {
                try
                {
                    Field<TextBox>(hosted, "_manifestPath").Text = files.ManifestPath;
                    await (Task)hosted.GetType().GetMethod("OpenAsync", BindingFlags.NonPublic | BindingFlags.Instance)!.Invoke(hosted, null)!;
                    foreach (var name in new[] { "_scrollA", "_scrollB" }) { var scroll = Field<ScrollViewer>(hosted, name); scroll.Width = 180; scroll.Height = 100; }
                    Assert.Equal("5", Field<TextBox>(hosted, "_markerK").Text);
                    Assert.Equal("16", Field<TextBox>(hosted, "_markerN").Text);
                    Field<TextBox>(hosted, "_markerK").Text = "4";
                    Field<TextBox>(hosted, "_markerN").Text = "4";
                    await (Task)hosted.GetType().GetMethod("UpdateMarkersAsync", BindingFlags.NonPublic | BindingFlags.Instance)!.Invoke(hosted, null)!;
                    hosted.UpdateLayout();
                    var list = Field<ListBox>(hosted, "_markerList"); Assert.Equal(3, list.Items.Count); list.BringIntoView();
                    ready.SetResult(new WindowInteropHelper(hosted).Handle);
                }
                catch (Exception e) { ready.SetException(e); }
            }); Dispatcher.Run();
        });
        thread.SetApartmentState(ApartmentState.STA); thread.Start();
        try
        {
            var handle = await ready.Task;
            using var automation = new UIA3Automation(); var window = automation.FromHandle(handle);
            var list = window.FindFirstDescendant(cf => cf.ByAutomationId("ComparisonMarkerList")); Assert.NotNull(list);
            var items = list.FindAllChildren(cf => cf.ByControlType(ControlType.ListItem)); Assert.Equal(3, items.Length);
            Assert.Contains("210", items[0].Name); items[0].Patterns.SelectionItem.Pattern.Select();
            hosted!.Dispatcher.Invoke(() => { }, DispatcherPriority.ContextIdle);
            hosted.Dispatcher.Invoke(() =>
            {
                var a = Field<ScrollViewer>(hosted, "_scrollA"); var b = Field<ScrollViewer>(hosted, "_scrollB");
                Assert.True(a.HorizontalOffset > 0); Assert.True(a.VerticalOffset > 0);
                Assert.Equal(a.HorizontalOffset, b.HorizontalOffset); Assert.Equal(a.VerticalOffset, b.VerticalOffset);
                Assert.True(Field<System.Windows.Media.ScaleTransform>(hosted, "_scaleA").ScaleX > 1);
                Assert.Contains("off", Field<TextBlock>(hosted, "_markerStatus").Text); // no invented defect map.
            });
            // Change parameters before the posted async continuation can run: cancellation must not strand Apply disabled.
            var superseded = hosted.Dispatcher.Invoke(() =>
            {
                var task = (Task)hosted.GetType().GetMethod("UpdateMarkersAsync", BindingFlags.NonPublic | BindingFlags.Instance)!.Invoke(hosted, null)!;
                Field<TextBox>(hosted, "_markerK").Text = "5";
                return task;
            });
            await superseded;
            hosted.Dispatcher.Invoke(() => Assert.True(Field<Button>(hosted, "_markerApply").IsEnabled));
            // Reopen the same output pair with two explicit, hash-bound maps; cyan is map XOR, not an application audit.
            var mapA = new byte[width * height]; var mapB = new byte[mapA.Length]; mapA[5 * width + 5] = 1; mapB[6 * width + 6] = 1;
            File.WriteAllBytes(Path.Combine(files.Directory, "a.map"), mapA);
            var header = new byte[152]; System.Text.Encoding.ASCII.GetBytes("XCAL").CopyTo(header, 0);
            void U32(int offset, uint value) => System.Buffers.Binary.BinaryPrimitives.WriteUInt32LittleEndian(header.AsSpan(offset), value);
            U32(4, 1); U32(8, 2); U32(12, 2); U32(16, width); U32(20, height);
            System.Buffers.Binary.BinaryPrimitives.WriteUInt64LittleEndian(header.AsSpan(112), (ulong)mapB.Length); SHA256.HashData(mapB).CopyTo(header, 120);
            File.WriteAllBytes(Path.Combine(files.Directory, "b.xcal"), header.Concat(mapB).ToArray());
            JsonObject MapEntry(string file) => new() { ["path"] = file, ["sha256"] = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(files.Directory, file)))) };
            files.Manifest["engines"]!["A"]!["maps"] = new JsonObject { ["bpm"] = MapEntry("a.map") };
            files.Manifest["engines"]!["B"]!["maps"] = new JsonObject { ["defect.xcal"] = MapEntry("b.xcal") };
            File.WriteAllText(files.ManifestPath, files.Manifest.ToJsonString());
            var reopened = hosted.Dispatcher.Invoke(() => (Task)hosted.GetType().GetMethod("OpenAsync", BindingFlags.NonPublic | BindingFlags.Instance)!.Invoke(hosted, null)!);
            await reopened;
            hosted.Dispatcher.Invoke(() =>
            {
                Assert.True(Field<CheckBox>(hosted, "_showDefectMarks").IsEnabled);
                var overlay = Field<Canvas>(hosted, "_overlayA").Children.OfType<System.Windows.Controls.Image>().Single();
                var bitmap = (System.Windows.Media.Imaging.BitmapSource)overlay.Source; var bytes = new byte[width * height * 4]; bitmap.CopyPixels(bytes, width * 4, 0);
                Assert.Equal(new byte[] { 255, 255, 0, 180 }, bytes[((5 * width + 5) * 4)..((5 * width + 5) * 4 + 4)]);
                Field<CheckBox>(hosted, "_showDefectMarks").IsChecked = false;
                Assert.Empty(Field<Canvas>(hosted, "_overlayA").Children.OfType<System.Windows.Controls.Image>());
            });
            // A dense set still has 25 real candidates, but automatic multi-box clutter is suppressed.
            using (var a = new BinaryWriter(File.Create(Path.Combine(files.Directory, "a.raw"))))
                for (var y = 0; y < height; y++) for (var x = 0; x < width; x++)
                { var value = 100 + (x + y) % 3 - 1; for (var i = 0; i < 25; i++) if (x >= 8 + i % 5 * 40 && x < 13 + i % 5 * 40 && y >= 8 + i / 5 * 40 && y < 13 + i / 5 * 40) value += 20 + i; a.Write((ushort)value); }
            files.Manifest["engines"]!["A"]!["output"]!["sha256"] = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(files.Directory, "a.raw"))));
            File.WriteAllText(files.ManifestPath, files.Manifest.ToJsonString());
            var denseOpen = hosted.Dispatcher.Invoke(() =>
            {
                Field<TextBox>(hosted, "_markerK").Text = "5"; Field<TextBox>(hosted, "_markerN").Text = "16";
                return (Task)hosted.GetType().GetMethod("OpenAsync", BindingFlags.NonPublic | BindingFlags.Instance)!.Invoke(hosted, null)!;
            });
            await denseOpen;
            hosted.Dispatcher.Invoke(() =>
            {
                Assert.Equal(20, Field<ListBox>(hosted, "_markerList").Items.Count);
                Assert.Contains("Widespread differences — showing the top 20 of 25 candidates", Field<TextBlock>(hosted, "_markerWarning").Text);
                var canvas = Field<Canvas>(hosted, "_overlayA");
                Assert.Empty(canvas.Children.OfType<System.Windows.Shapes.Rectangle>().Where(x => Equals(x.Tag, "marker")));
                Field<ListBox>(hosted, "_markerList").SelectedIndex = 0;
                Assert.Single(canvas.Children.OfType<System.Windows.Shapes.Rectangle>().Where(x => Equals(x.Tag, "marker")));
            });
        }
        finally
        {
            if (hosted is not null) hosted.Dispatcher.Invoke(() => { hosted.Close(); Dispatcher.CurrentDispatcher.BeginInvokeShutdown(DispatcherPriority.Send); }); thread.Join();
        }
    }
    private static T Field<T>(object obj, string name) => (T)(obj.GetType().GetField(name, BindingFlags.NonPublic | BindingFlags.Instance)?.GetValue(obj) ?? throw new InvalidOperationException("Missing " + name));
    private sealed class ContractStub : IComparisonBackend
    {
        public Task<ValidateResponse> ValidateAsync(ManifestSnapshot manifest, CancellationToken cancel) => Task.FromResult(new ValidateResponse(true, null, new JsonObject { ["comparable"] = false, ["reasons"] = new JsonArray("UNITS_MISMATCH") }, null, 2, "", ""));
        public Task<CompareResponse> CompareAsync(ManifestSnapshot manifest, CancellationToken cancel) => throw new InvalidOperationException("No statistics/engine called.");
    }
}
