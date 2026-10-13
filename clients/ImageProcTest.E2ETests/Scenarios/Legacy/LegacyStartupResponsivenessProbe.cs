// GUI-C-235: a MEASUREMENT of the legacy app's start-up, not a regression test (it asserts only that it measured something). Env-gated: skipped unless XPE_C235_RUNS is set.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;
using FlaUI.UIA3;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Legacy;

/// <summary>
/// The LegacyPreprocessReadiness scenarios failed on CI with <c>UIA Timeout</c> thrown by <c>Application.GetMainWindow</c> -> <c>UIA3Automation.FromHandle</c> (a COM call that gives up after FlaUI's 2 s
/// connection timeout) BEFORE the scenario body. That cannot say whether the app was stuck or UI Automation was slow, so this probe tells them apart on every launch:
/// <list type="bullet">
/// <item>the message pump: <c>SendMessageTimeout(WM_NULL, SMTO_ABORTIFHUNG)</c> every 25 ms from a second thread - a window that does not answer is a blocked UI thread, however UIA behaves;</item>
/// <item>the harness' own call: <c>FromHandle</c> tried again and again from the moment the window handle exists, counting the timeouts and the time to the first success.</item>
/// </list>
/// Variables (all optional except the first): <c>XPE_C235_RUNS</c>, <c>XPE_C235_COPY=1</c> (launch from a fresh COPY of <c>XPE_NATIVE_DIR</c>, as R05/R06/R08 do), <c>XPE_C235_AFFINITY</c> (hex CPU mask for the app,
/// to imitate a smaller runner), <c>XPE_C235_WINDOW_MS</c> (how long to watch after the handle appears, default 8000).
/// </summary>
public sealed class LegacyStartupResponsivenessProbe(ITestOutputHelper output)
{
    private const uint WM_NULL = 0;
    private const uint SMTO_ABORTIFHUNG = 0x0002;

    [DllImport("user32.dll", SetLastError = true)]
    private static extern IntPtr SendMessageTimeout(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam, uint flags, uint timeout, out IntPtr result);

    private sealed record Launch(double HandleMs, double FirstReplyMs, double LongestBlockedMs, double TotalBlockedMs, int UiaTimeouts, double UiaFirstOkMs, double UiaFirstAttemptMs, bool UiaFirstAttemptOk);

    [SkippableFact]
    public void MeasureTheLegacyAppsStartUp_MessagePumpAndUiAutomation()
    {
        var runsText = Environment.GetEnvironmentVariable("XPE_C235_RUNS");
        Skip.If(string.IsNullOrEmpty(runsText), "XPE_C235_RUNS is not set (this is a measurement, not a regression test).");
        var runs = int.Parse(runsText!);
        var native = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
        Skip.If(string.IsNullOrEmpty(native) || !File.Exists(Path.Combine(native, "xpe_preprocess.dll")), "XPE_NATIVE_DIR does not name a folder containing xpe_preprocess.dll.");
        var exe = FindExecutable();
        Skip.If(exe is null, "clients/ImageProcTest/bin/Debug/net8.0-windows/ImageProcTest.exe was not found.");
        var copy = Environment.GetEnvironmentVariable("XPE_C235_COPY") == "1";
        var affinity = Environment.GetEnvironmentVariable("XPE_C235_AFFINITY");
        var watchMs = int.Parse(Environment.GetEnvironmentVariable("XPE_C235_WINDOW_MS") ?? "8000");
        output.WriteLine($"config: runs={runs} copy={copy} affinity={affinity ?? "(all)"} watchMs={watchMs} logicalCpus={Environment.ProcessorCount}");
        output.WriteLine("run | handle ms | first reply ms | longest blocked ms | total blocked ms | FromHandle timeouts | first FromHandle ok at ms | first attempt ok?");

        var all = new List<Launch>();
        for (var i = 1; i <= runs; i++)
        {
            string? dir = null;
            try
            {
                if (copy)
                {
                    dir = Path.Combine(Path.GetTempPath(), $"xpe_c235_native_{Guid.NewGuid():N}");
                    Directory.CreateDirectory(dir);
                    foreach (var file in Directory.GetFiles(native!)) File.Copy(file, Path.Combine(dir, Path.GetFileName(file)));
                }

                var launch = LaunchAndWatch(exe!, dir ?? native!, affinity, watchMs);
                all.Add(launch);
                output.WriteLine($"{i,3} | {launch.HandleMs,9:0} | {launch.FirstReplyMs,14:0} | {launch.LongestBlockedMs,18:0} | {launch.TotalBlockedMs,16:0} | {launch.UiaTimeouts,19} | {launch.UiaFirstOkMs,25:0} | {launch.UiaFirstAttemptOk}");
            }
            finally
            {
                if (dir is not null)
                {
                    try { Directory.Delete(dir, recursive: true); } catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { }
                }
            }
        }

        output.WriteLine($"summary: runs={all.Count} firstFromHandleFailed={all.Count(l => !l.UiaFirstAttemptOk)} " +
                         $"maxLongestBlockedMs={all.Max(l => l.LongestBlockedMs):0} medianLongestBlockedMs={Median(all.Select(l => l.LongestBlockedMs)):0} " +
                         $"medianFirstReplyMs={Median(all.Select(l => l.FirstReplyMs)):0} medianHandleMs={Median(all.Select(l => l.HandleMs)):0}");
        Assert.NotEmpty(all);
    }

    /// <summary>
    /// The harness way of getting a window - FlaUI's <c>Application.GetMainWindow</c>, i.e. one UI Automation call the moment the window handle exists, with its 2 s budget - against an app whose start-up
    /// probing is made slow on purpose (<c>XPE_STARTUP_PROBE_DELAY_MS</c>, a test hook of the app). This is the failure mode of the CI runs reproduced on a fast machine: an app whose UI thread is
    /// blocked longer than the budget while it starts gives "UIA Timeout" before any scenario step. XPE_C235_DELAYS (comma-separated ms, e.g. "0,1500,3000,6000"), XPE_C235_REPEAT (default 2).
    /// </summary>
    [SkippableFact]
    public void MeasureTheFlaUiWindowCall_WithASlowStartUpProbe()
    {
        var delays = Environment.GetEnvironmentVariable("XPE_C235_DELAYS");
        Skip.If(string.IsNullOrEmpty(delays), "XPE_C235_DELAYS is not set (this is a measurement, not a regression test).");
        var repeat = int.Parse(Environment.GetEnvironmentVariable("XPE_C235_REPEAT") ?? "2");
        var native = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
        Skip.If(string.IsNullOrEmpty(native) || !File.Exists(Path.Combine(native, "xpe_preprocess.dll")), "XPE_NATIVE_DIR does not name a folder containing xpe_preprocess.dll.");
        var exe = FindExecutable();
        Skip.If(exe is null, "clients/ImageProcTest/bin/Debug/net8.0-windows/ImageProcTest.exe was not found.");
        output.WriteLine("injected probe delay ms | run | outcome | GetMainWindow ms | UI thread unanswered (longest ms)");

        var failures = 0;
        var total = 0;
        foreach (var delay in delays!.Split(',').Select(int.Parse))
        {
            for (var run = 1; run <= repeat; run++)
            {
                total++;
                var info = new ProcessStartInfo(exe!) { WorkingDirectory = Path.GetDirectoryName(exe)!, UseShellExecute = false };
                info.Environment["XPE_NATIVE_DIR"] = native;
                info.Environment["XPE_STARTUP_PROBE_DELAY_MS"] = delay.ToString();
                using var automation = new UIA3Automation();
                FlaUI.Core.Application? application = null;
                var clock = Stopwatch.StartNew();
                double longestBlocked = 0;
                try
                {
                    application = FlaUI.Core.Application.Launch(info);
                    // the pump probe runs beside the harness call, exactly as in the other probe
                    var stop = false;
                    var probe = new Thread(() =>
                    {
                        IntPtr handle = IntPtr.Zero;
                        double blockedSince = -1;
                        while (!Volatile.Read(ref stop))
                        {
                            if (handle == IntPtr.Zero)
                            {
                                try { using var process = Process.GetProcessById(application.ProcessId); handle = process.MainWindowHandle; } catch (Exception ex) when (ex is ArgumentException or InvalidOperationException or System.ComponentModel.Win32Exception) { }
                                Thread.Sleep(5);
                                continue;
                            }

                            var before = clock.Elapsed.TotalMilliseconds;
                            var ok = SendMessageTimeout(handle, WM_NULL, IntPtr.Zero, IntPtr.Zero, SMTO_ABORTIFHUNG, 25, out _) != IntPtr.Zero;
                            if (ok && blockedSince >= 0) { longestBlocked = Math.Max(longestBlocked, clock.Elapsed.TotalMilliseconds - blockedSince); blockedSince = -1; }
                            else if (!ok && blockedSince < 0) blockedSince = before;
                            Thread.Sleep(5);
                        }
                    }) { IsBackground = true };
                    probe.Start();
                    try
                    {
                        var window = application.GetMainWindow(automation, TimeSpan.FromSeconds(60));
                        var ms = clock.Elapsed.TotalMilliseconds;
                        Thread.Sleep(delay + 1500);   // let the rest of the start-up finish so the pump probe sees all of it
                        output.WriteLine($"{delay,23} | {run,3} | ok{(window is null ? " (no window)" : string.Empty)} | {ms,16:0} | {longestBlocked:0}");
                    }
                    finally
                    {
                        Volatile.Write(ref stop, true);
                        probe.Join(2000);
                    }
                }
                catch (TimeoutException ex)
                {
                    failures++;
                    output.WriteLine($"{delay,23} | {run,3} | FAILED {ex.GetType().Name}: {ex.Message} | {clock.Elapsed.TotalMilliseconds,16:0} | {longestBlocked:0}");
                }
                finally
                {
                    try
                    {
                        if (application is not null) { using var process = Process.GetProcessById(application.ProcessId); process.Kill(entireProcessTree: true); process.WaitForExit(5000); }
                    }
                    catch (Exception ex) when (ex is ArgumentException or InvalidOperationException or System.ComponentModel.Win32Exception) { }
                }
            }
        }

        output.WriteLine($"summary: GetMainWindow failed {failures}/{total}");
        Assert.True(total > 0);
    }

    private static double Median(IEnumerable<double> values)
    {
        var sorted = values.OrderBy(v => v).ToArray();
        return sorted.Length == 0 ? 0 : sorted[sorted.Length / 2];
    }

    private static Launch LaunchAndWatch(string exe, string nativeDir, string? affinityHex, int watchMs)
    {
        var info = new ProcessStartInfo(exe) { WorkingDirectory = Path.GetDirectoryName(exe)!, UseShellExecute = false };
        info.Environment["XPE_NATIVE_DIR"] = nativeDir;
        var clock = Stopwatch.StartNew();
        using var process = Process.Start(info)!;
        try
        {
            if (!string.IsNullOrEmpty(affinityHex)) process.ProcessorAffinity = (IntPtr)Convert.ToInt64(affinityHex, 16);

            // 1) the window handle (what FlaUI's GetMainWindow waits for)
            IntPtr handle = IntPtr.Zero;
            while (clock.Elapsed.TotalSeconds < 60 && handle == IntPtr.Zero)
            {
                process.Refresh();
                handle = process.MainWindowHandle;
                if (handle == IntPtr.Zero) Thread.Sleep(5);
            }

            Assert.NotEqual(IntPtr.Zero, handle);
            var handleMs = clock.Elapsed.TotalMilliseconds;

            // 2) message pump probe on its own thread
            double firstReplyMs = -1, longest = 0, total = 0;
            var stop = false;
            var probe = new Thread(() =>
            {
                double blockedSince = -1;
                while (!Volatile.Read(ref stop))
                {
                    var before = clock.Elapsed.TotalMilliseconds;
                    var ok = SendMessageTimeout(handle, WM_NULL, IntPtr.Zero, IntPtr.Zero, SMTO_ABORTIFHUNG, 25, out _) != IntPtr.Zero;
                    var now = clock.Elapsed.TotalMilliseconds;
                    if (ok)
                    {
                        if (firstReplyMs < 0) firstReplyMs = now;
                        if (blockedSince >= 0)
                        {
                            var span = now - blockedSince;
                            total += span;
                            longest = Math.Max(longest, span);
                            blockedSince = -1;
                        }
                    }
                    else if (blockedSince < 0)
                    {
                        blockedSince = before;
                    }

                    Thread.Sleep(5);
                }

                if (blockedSince >= 0)
                {
                    var span = clock.Elapsed.TotalMilliseconds - blockedSince;
                    total += span;
                    longest = Math.Max(longest, span);
                }
            }) { IsBackground = true };
            probe.Start();

            // 3) the harness' call, again and again until it works
            using var automation = new UIA3Automation();
            var timeouts = 0;
            double firstOkMs = -1;
            var firstAttemptOk = false;
            var attempt = 0;
            var deadline = handleMs + watchMs;
            while (clock.Elapsed.TotalMilliseconds < deadline)
            {
                attempt++;
                try
                {
                    var window = automation.FromHandle(handle);
                    if (window is not null)
                    {
                        if (firstOkMs < 0) firstOkMs = clock.Elapsed.TotalMilliseconds;
                        if (attempt == 1) firstAttemptOk = true;
                        break;
                    }
                }
                catch (TimeoutException)
                {
                    timeouts++;
                }

                Thread.Sleep(50);
            }

            // keep watching the pump for the rest of the window
            while (clock.Elapsed.TotalMilliseconds < deadline) Thread.Sleep(50);
            Volatile.Write(ref stop, true);
            probe.Join(2000);
            return new Launch(handleMs, firstReplyMs, longest, total, timeouts, firstOkMs, 0, firstAttemptOk);
        }
        finally
        {
            try { process.Kill(entireProcessTree: true); process.WaitForExit(5000); } catch (Exception ex) when (ex is InvalidOperationException or System.ComponentModel.Win32Exception) { }
        }
    }

    private static string? FindExecutable()
    {
        for (var dir = new DirectoryInfo(AppContext.BaseDirectory); dir is not null; dir = dir.Parent)
        {
            var candidate = Path.Combine(dir.FullName, "clients", "ImageProcTest", "bin", "Debug", "net8.0-windows", "ImageProcTest.exe");
            if (File.Exists(candidate)) return candidate;
        }

        return null;
    }
}
