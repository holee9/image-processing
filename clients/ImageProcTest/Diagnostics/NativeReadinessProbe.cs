using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text.Json;

namespace ImageProcTest
{
    internal sealed record NativeReadinessReportWriteResult(string ReportPath, string DisplaySummary);

    internal sealed record DisplayHealthResult(
        string Status,
        string Version,
        string DllPath,
        string Details,
        bool IsReady);

    internal static class NativeReadinessProbe
    {
        public static NativeReadinessReportWriteResult WriteReport(BackendHealthResult commonResult)
        {
            var display = XpeDisplayVersionProbe.Check();
            var report = new
            {
                schema = "xpe-native-readiness-v1",
                timestampUtc = DateTimeOffset.UtcNow,
                common = commonResult,
                abi = new
                {
                    imageBufferSize = Marshal.SizeOf<XpeCommonApi.XpeImageBuffer>(),
                    imageMetadataSize = Marshal.SizeOf<XpeCommonApi.XpeImageMetadata>()
                },
                display
            };

            var path = Path.Combine(AppContext.BaseDirectory, "native-readiness-report.json");
            var json = JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true });
            File.WriteAllText(path, json);

            return new NativeReadinessReportWriteResult(path, $"{display.Status} ({display.Version})");
        }
    }
}
