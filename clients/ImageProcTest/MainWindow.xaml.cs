using System;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows;
using System.Windows.Media;

namespace ImageProcTest
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            RefreshNativeHealth();
        }

        private void RefreshButton_Click(object sender, RoutedEventArgs e)
        {
            RefreshNativeHealth();
        }

        private void RefreshNativeHealth()
        {
            SetStatus("Checking native common backend...", Brushes.Goldenrod);
            NativePathText.Text = $"DLL: {XpeCommonApi.ResolvedDllPath}";

            try
            {
                var result = XpeCommonApi.xpe_init(null);
                InitText.Text = $"Init: {FormatResult(result)}";

                if (result != XpeCommonApi.XpeErrorCode.OK)
                {
                    SetStatus("Native common unavailable: init failed.", Brushes.OrangeRed);
                    DetailsText.Text = "xpe_init returned a failure code. Image-processing modules stay disabled.";
                    return;
                }

                NativePathText.Text = $"DLL: {XpeCommonApi.ResolvedDllPath}";
                var version = PtrToString(XpeCommonApi.xpe_version());
                VersionText.Text = $"Version: {version}";
                VersionCheckText.Text = $"Version: {version}";

                var errorText = PtrToString(XpeCommonApi.xpe_error_string(XpeCommonApi.XpeErrorCode.INVALID_INPUT));
                DetailsText.Text = $"xpe_error_string(INVALID_INPUT): {errorText}";

                var paramResult = XpeCommonApi.xpe_get_param_range("CHEST", "window_center",
                    out var minValue, out var maxValue, out var defaultValue);
                ParamRangeText.Text = paramResult == XpeCommonApi.XpeErrorCode.OK
                    ? $"Param range: OK min={minValue:0.###}, max={maxValue:0.###}, default={defaultValue:0.###}"
                    : $"Param range: {FormatResult(paramResult)}";

                MemoryAbiText.Text = RunMemoryAbiSmoke();

                var alertCount = XpeCommonApi.xpe_get_pending_alert_count();
                AlertText.Text = $"Alerts: pending={alertCount}";

                var aedResult = XpeCommonApi.xpe_aed_get_status(out var aedState);
                if (aedResult == XpeCommonApi.XpeErrorCode.OK)
                {
                    DetailsText.Text += Environment.NewLine + $"Auto Exposure Detection state: {aedState}";
                }

                SetStatus("Native common backend ready. Image-processing modules remain gated.", Brushes.ForestGreen);
            }
            catch (DllNotFoundException ex)
            {
                MarkUnavailable("xpe_common.dll not found", ex);
            }
            catch (EntryPointNotFoundException ex)
            {
                MarkUnavailable("xpe_common.dll entry point mismatch", ex);
            }
            catch (BadImageFormatException ex)
            {
                MarkUnavailable("xpe_common.dll architecture mismatch", ex);
            }
            catch (Exception ex)
            {
                MarkUnavailable("native common health check failed", ex);
            }
        }

        private void Window_Closed(object? sender, EventArgs e)
        {
            try { XpeCommonApi.xpe_shutdown(); } catch { }
        }

        private string RunMemoryAbiSmoke()
        {
            var allocResult = XpeCommonApi.xpe_alloc_image(16, 16, XpeCommonApi.XpePixelFormat.UInt16, out var buffer);
            if (allocResult != XpeCommonApi.XpeErrorCode.OK)
            {
                return $"Memory ABI: alloc {FormatResult(allocResult)}";
            }

            try
            {
                var expectedSize = 16u * 16u * 2u;
                var ok = buffer.Width == 16 &&
                         buffer.Height == 16 &&
                         buffer.BitsAllocated == 16 &&
                         buffer.BitsStored == 16 &&
                         buffer.Format == XpeCommonApi.XpePixelFormat.UInt16 &&
                         buffer.Data != IntPtr.Zero &&
                         buffer.DataSize == expectedSize;

                return ok
                    ? $"Memory ABI: OK buffer={buffer.Width}x{buffer.Height}, bytes={buffer.DataSize}"
                    : $"Memory ABI: unexpected layout/data size, bytes={buffer.DataSize}";
            }
            finally
            {
                XpeCommonApi.xpe_free_image(ref buffer);
            }
        }

        private void MarkUnavailable(string reason, Exception ex)
        {
            SetStatus($"Native common unavailable: {reason}.", Brushes.OrangeRed);
            VersionText.Text = "Version: Unavailable";
            NativePathText.Text = $"DLL: {XpeCommonApi.ResolvedDllPath}";
            InitText.Text = "Init: Unavailable";
            VersionCheckText.Text = "Version: Unavailable";
            ParamRangeText.Text = "Param range: Skipped";
            MemoryAbiText.Text = "Memory ABI: Skipped";
            AlertText.Text = "Alerts: Skipped";
            DetailsText.Text = ex.Message;
        }

        private static string PtrToString(IntPtr ptr)
        {
            return ptr == IntPtr.Zero ? "<null>" : Marshal.PtrToStringAnsi(ptr) ?? "<empty>";
        }

        private static string FormatResult(XpeCommonApi.XpeErrorCode result)
        {
            return $"{result} ({(int)result})";
        }

        private void SetStatus(string message, Brush brush)
        {
            StatusText.Text = $"Status: {message}";
            StatusText.Foreground = brush;
        }
    }
}
