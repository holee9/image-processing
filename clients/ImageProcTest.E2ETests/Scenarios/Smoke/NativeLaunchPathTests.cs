// GUI-C-238 (milestone 3, U1): the app started the way the operator starts it - tools/Start-ImageProcTestNative.ps1: --automation-backend Native --automation-calib <folder> --automation-width/-height,
// XPE_NATIVE_DIR=<folder with the DLLs> and XPE_NATIVE_DIR_EXCLUSIVE=1 - must be on the Native backend. CI never ran this path: the E2E fixture launches the exe straight from the build folder, where no
// xpe_*.dll is, so the one thing that broke the operator's launch (an OLD xpe_common.dll / xpe_display.dll left in the exe's own folder, which won over the folder the search was pinned to and made the
// app start on Mock) was invisible. These tests put such files beside a copy of the exe and start it the operator's way.
using System.Diagnostics;
using FlaUI.Core;
using FlaUI.Core.AutomationElements;
using FlaUI.Core.Definitions;
using FlaUI.UIA3;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Smoke;

[Collection(NativeLaunchPathCollection.Name)]
[Trait("Category", "Smoke")]
public sealed class NativeLaunchPathTests(ITestOutputHelper output)
{
    /// <summary>
    /// A DLL with the right name and the wrong content stands in the exe's folder (an old build that lacks the exports this application calls, the shape that was found on the operator's machine).
    /// The search is pinned to the folder with the real DLLs (EXCLUSIVE=1), exactly as the start script does: the app must use THOSE and start on Native.
    /// </summary>
    [SkippableFact]
    public void OldDllsBesideTheExe_DoNotWinOverThePinnedFolder_TheAppStartsOnNative()
    {
        var nativeDirectory = NativeDirectoryOrSkip();
        using var launch = LaunchedCopy.Start(output, nativeDirectory, exclusive: true);

        output.WriteLine($"title: '{launch.Window.Title}'");
        Assert.DoesNotContain("[MOCK]", launch.Window.Title, StringComparison.Ordinal);
        Assert.True(launch.WaitForText("mode="), "the status bar never showed the backend line (mode=...)");
        Assert.True(launch.AnyTextContains("mode=Native"), "the backend line does not say mode=Native: " + string.Join(" | ", launch.TextsContaining("mode=")));
        Assert.False(launch.AnyTextContains("MOCK BACKEND"), "the MOCK BACKEND banner is showing although the search was pinned to the folder with the real DLLs");
        Assert.True(launch.AnyTextContains("src=" + new DirectoryInfo(nativeDirectory).Name), "the DLLs were not loaded from the pinned folder: " + string.Join(" | ", launch.TextsContaining("mode=")));
    }

    /// <summary>
    /// The same old DLLs, but the search is NOT pinned (the default: the exe's own folder comes first). Mock is the honest answer then - and the banner must say WHY, naming the DLL, its path and what is wrong
    /// with it, instead of the bare "could not be used" that left the operator guessing.
    /// </summary>
    [SkippableFact]
    public void OldDllsBesideTheExe_WhenNotPinned_MockStartsAndTheBannerNamesTheDllAndTheProblem()
    {
        var nativeDirectory = NativeDirectoryOrSkip();
        using var launch = LaunchedCopy.Start(output, nativeDirectory, exclusive: false);

        output.WriteLine($"title: '{launch.Window.Title}'");
        Assert.Contains("[MOCK]", launch.Window.Title, StringComparison.Ordinal);
        Assert.True(launch.WaitForText("MOCK BACKEND"), "no MOCK BACKEND banner");
        var banner = launch.TextsContaining("MOCK BACKEND").First();
        output.WriteLine($"banner: {banner}");
        Assert.Contains("Native was requested but could not be used:", banner, StringComparison.Ordinal);
        Assert.Contains("xpe_common.dll at '" + Path.Combine(launch.AppFolder, "xpe_common.dll") + "'", banner, StringComparison.Ordinal);
        Assert.True(banner.Contains("could not be loaded", StringComparison.Ordinal) || banner.Contains("does not export", StringComparison.Ordinal),
            "the banner names the DLL but not what is wrong with it: " + banner);
    }

    private static string NativeDirectoryOrSkip()
    {
        var directory = Environment.GetEnvironmentVariable(ApplicationFixture.NativeDirVariable);
        Skip.If(string.IsNullOrWhiteSpace(directory) || !File.Exists(Path.Combine(directory, "xpe_common.dll")),
            $"{ApplicationFixture.NativeDirVariable} does not name a folder with the staged native DLLs: this checks the Native launch path (the Native E2E job sets it).");
        return directory!;
    }

    /// <summary>A copy of the app's output folder with two wrong DLLs in it, started the way the start script starts the app.</summary>
    private sealed class LaunchedCopy : IDisposable
    {
        private readonly UIA3Automation _automation;
        private readonly Application _application;
        private readonly ITestOutputHelper _output;

        public string AppFolder { get; }

        public Window Window { get; }

        private LaunchedCopy(ITestOutputHelper output, UIA3Automation automation, string appFolder, Application application, Window window)
        {
            _output = output;
            _automation = automation;
            AppFolder = appFolder;
            _application = application;
            Window = window;
        }

        public static LaunchedCopy Start(ITestOutputHelper output, string nativeDirectory, bool exclusive)
        {
            var built = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", "..", "gui", "ImageProcTest", "bin", "Debug", "net8.0-windows"));
            Skip.IfNot(File.Exists(Path.Combine(built, "ImageProcTest.exe")), "gui/ImageProcTest/bin/Debug/net8.0-windows/ImageProcTest.exe was not found: build gui/ImageProcTest first.");

            var root = Path.Combine(Path.GetTempPath(), "xpe_c238_" + Guid.NewGuid().ToString("N"));
            var appFolder = Path.Combine(root, "app");
            CopyFolder(built, appFolder);

            // wrong content under the right names: another module's DLL of the staged set (it loads, or it does not; either way it lacks the exports the app needs), or garbage when there is none
            File.WriteAllBytes(Path.Combine(appFolder, "xpe_common.dll"), BytesOfAnotherModule(nativeDirectory, "xpe_preprocess.dll"));
            File.WriteAllBytes(Path.Combine(appFolder, "xpe_display.dll"), BytesOfAnotherModule(nativeDirectory, "xpe_enhance_basic.dll"));

            var calibration = Path.Combine(root, "calib");
            Directory.CreateDirectory(calibration);

            var info = new ProcessStartInfo(Path.Combine(appFolder, "ImageProcTest.exe")) { WorkingDirectory = appFolder, UseShellExecute = false };
            foreach (var argument in new[] { "--automation-settings", Path.Combine(root, "appsettings.json"), "--automation-backend", "Native", "--automation-calib", calibration, "--automation-width", "1024", "--automation-height", "1024" })
            {
                info.ArgumentList.Add(argument);
            }

            info.Environment["XPE_NATIVE_DIR"] = nativeDirectory;
            if (exclusive)
            {
                info.Environment["XPE_NATIVE_DIR_EXCLUSIVE"] = "1";
            }
            else
            {
                info.Environment.Remove("XPE_NATIVE_DIR_EXCLUSIVE");
            }

            var automation = new UIA3Automation();
            Application? application = null;
            try
            {
                application = Application.Launch(info);
                var window = application.GetMainWindow(automation, TimeSpan.FromSeconds(60));
                Thread.Sleep(3000);
                return new LaunchedCopy(output, automation, appFolder, application, window);
            }
            catch
            {
                try { application?.Kill(); } catch (Exception) { /* already gone */ }
                automation.Dispose();
                throw;
            }
        }

        private static byte[] BytesOfAnotherModule(string nativeDirectory, string name)
        {
            var path = Path.Combine(nativeDirectory, name);
            return File.Exists(path) ? File.ReadAllBytes(path) : [0x4D, 0x5A, 0x00, 0x00];
        }

        private static void CopyFolder(string from, string to)
        {
            Directory.CreateDirectory(to);
            foreach (var file in Directory.GetFiles(from))
            {
                File.Copy(file, Path.Combine(to, Path.GetFileName(file)));
            }

            foreach (var directory in Directory.GetDirectories(from))
            {
                CopyFolder(directory, Path.Combine(to, Path.GetFileName(directory)));
            }
        }

        public IReadOnlyList<string> TextsContaining(string fragment) =>
            Window.FindAllDescendants(cf => cf.ByControlType(ControlType.Text)).Select(e => Safe(() => e.Name) ?? string.Empty).Where(t => t.Contains(fragment, StringComparison.Ordinal)).Distinct().ToList();

        public bool AnyTextContains(string fragment) => TextsContaining(fragment).Count > 0;

        public bool WaitForText(string fragment)
        {
            var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(30);
            while (DateTime.UtcNow < deadline)
            {
                if (AnyTextContains(fragment))
                {
                    return true;
                }

                Thread.Sleep(500);
            }

            return false;
        }

        private static string? Safe(Func<string?> read)
        {
            try { return read(); } catch (Exception) { return null; }
        }

        public void Dispose()
        {
            try { _application.Kill(); } catch (Exception) { /* already gone */ }
            _application.Dispose();
            _automation.Dispose();
            var root = Path.GetDirectoryName(AppFolder)!;
            for (var attempt = 0; attempt < 10; attempt++)
            {
                try { Directory.Delete(root, recursive: true); return; }
                catch (IOException) { Thread.Sleep(300); }
                catch (UnauthorizedAccessException) { Thread.Sleep(300); }
            }

            _output.WriteLine($"temporary folder {root} could not be deleted");
        }
    }
}

/// <summary>Its own collection: these tests start an extra app and must not overlap another.</summary>
[CollectionDefinition(Name)]
public sealed class NativeLaunchPathCollection
{
    public const string Name = "NativeLaunchPath";
}
