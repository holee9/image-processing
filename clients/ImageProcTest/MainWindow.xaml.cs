using System;
using System.Runtime.InteropServices;
using System.Windows;

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
            try
            {
                var result = XpeCommonApi.xpe_init();
                if (result == XpeCommonApi.XpeErrorCode.OK)
                {
                    StatusText.Text = "Status: Ready";
                    var versionPtr = XpeCommonApi.xpe_version();
                    var version = Marshal.PtrToStringAnsi(versionPtr);
                    VersionText.Text = $"Version: {version}";
                }
                else
                {
                    StatusText.Text = $"Status: Init failed (error {result})";
                }
            }
            catch (DllNotFoundException ex)
            {
                MessageBox.Show($"xpe_common.dll not found: {ex.Message}", "Error");
            }
        }

        private void Window_Closed(object? sender, EventArgs e)
        {
            try { XpeCommonApi.xpe_shutdown(); } catch { }
        }
    }
}
