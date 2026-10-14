// GUI-C-238 / 238b (milestone 3, U1): the app started the way the operator starts it - tools/Start-ImageProcTestNative.ps1: --automation-backend Native --automation-calib <folder> --automation-width/-height,
// XPE_NATIVE_DIR=<folder with the DLLs> and XPE_NATIVE_DIR_EXCLUSIVE=1 - must be on the Native backend, and when it cannot be, it must say why. CI never ran this path: the E2E fixture launches the exe straight
// from the build folder, where no xpe_*.dll is, so the thing that broke the operator's launch (an OLD xpe_common.dll / xpe_display.dll left in the exe's own folder, which won over the folder the search was
// pinned to and made the app start on Mock) was invisible. These tests put such files beside a COPY of the exe and start it the operator's way.
//
// What "pinned" means (XPE_NATIVE_DIR_EXCLUSIVE=1): the app's native DLLs are loaded from XPE_NATIVE_DIR and from nowhere else - not from the exe's folder, not from the default .NET search. The tests below are
// the ways out of the pinned folder that Codex #193 found: the factory's fallback path, the resolver's fallback to the default loader, and a pin without a folder.
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
    private static readonly string[] RuntimeDlls = ["fmt.dll", "spdlog.dll"];

    /// <summary>
    /// A DLL with the right name and the wrong content stands in the exe's folder (an old build that lacks the exports this application calls, the shape that was found on the operator's machine).
    /// The search is pinned to the folder with the real DLLs (EXCLUSIVE=1), exactly as the start script does: the app must use THOSE and start on Native.
    /// </summary>
    [SkippableFact]
    public void OldDllsBesideTheExe_DoNotWinOverThePinnedFolder_TheAppStartsOnNative()
    {
        var nativeDirectory = NativeDirectoryOrSkip();
        using var launch = LaunchedCopy.Start(output, nativeDirectory, new Launch(Exclusive: true, AppFolder: AppFolderDlls.Wrong));

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
        using var launch = LaunchedCopy.Start(output, nativeDirectory, new Launch(Exclusive: false, AppFolder: AppFolderDlls.Wrong));

        output.WriteLine($"title: '{launch.Window.Title}'");
        Assert.Contains("[MOCK]", launch.Window.Title, StringComparison.Ordinal);
        Assert.True(launch.WaitForText("MOCK BACKEND"), "no MOCK BACKEND banner");
        var banner = launch.TextsContaining("MOCK BACKEND").First();
        output.WriteLine($"banner: {banner}");
        Assert.Contains("Native was requested but could not be used:", banner, StringComparison.Ordinal);
        Assert.Contains("xpe_common.dll at '" + Path.Combine(launch.AppFolderPath, "xpe_common.dll") + "'", banner, StringComparison.Ordinal);
        Assert.True(banner.Contains("could not be loaded", StringComparison.Ordinal) || banner.Contains("does not export", StringComparison.Ordinal),
            "the banner names the DLL but not what is wrong with it: " + banner);
    }

    /// <summary>
    /// The control the three tests below stand on: WORKING copies of the DLLs beside the exe, nothing pinned, no XPE_NATIVE_DIR - the application folder is the only place to look, and the app starts on
    /// Native from it. Without this, "Mock" in the next tests could be because the application folder's DLLs never worked.
    /// </summary>
    [SkippableFact]
    public void WorkingDllsBesideTheExe_WhenNothingIsPinned_AreUsed_TheAppStartsOnNative()
    {
        var nativeDirectory = NativeDirectoryOrSkip();
        using var launch = LaunchedCopy.Start(output, nativeDirectory, new Launch(Exclusive: false, AppFolder: AppFolderDlls.Working, SetNativeDir: false));

        Assert.True(launch.WaitForText("mode="), "the status bar never showed the backend line (mode=...)");
        var line = string.Join(" | ", launch.TextsContaining("mode="));
        output.WriteLine($"backend line: {line}");
        Assert.True(launch.AnyTextContains("mode=Native"), "working DLLs in the application folder did not give Native: " + line);
        Assert.True(launch.AnyTextContains("src=app"), "the DLLs were not loaded from the application folder: " + line);
    }

    /// <summary>
    /// Pinned to a folder that is missing xpe_common.dll, with WORKING DLLs in the application folder: the application folder must not rescue it. Mock, and the banner names the pinned folder and the missing DLL
    /// (it used to start Native from the application folder: the factory's fallback path was the application folder).
    /// </summary>
    [SkippableFact]
    public void PinnedFolderMissingCommon_WorkingDllsBesideTheExe_DoNotRescueIt_MockWithTheReason()
    {
        var nativeDirectory = NativeDirectoryOrSkip();
        using var launch = LaunchedCopy.Start(output, nativeDirectory, new Launch(Exclusive: true, AppFolder: AppFolderDlls.Working, Pinned: PinnedFolder.WithoutCommon));

        var banner = BannerOf(launch);
        Assert.Contains("xpe_common.dll was not found at '" + Path.Combine(launch.PinnedFolderPath!, "xpe_common.dll") + "'", banner, StringComparison.Ordinal);
        Assert.DoesNotContain(launch.AppFolderPath, banner, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>Pinned to a folder whose xpe_common.dll is a damaged file, with a WORKING one in the application folder: Mock, and the banner says the pinned file could not be loaded.</summary>
    [SkippableFact]
    public void PinnedFolderWithADamagedCommon_WorkingDllsBesideTheExe_DoNotRescueIt_MockWithTheReason()
    {
        var nativeDirectory = NativeDirectoryOrSkip();
        using var launch = LaunchedCopy.Start(output, nativeDirectory, new Launch(Exclusive: true, AppFolder: AppFolderDlls.Working, Pinned: PinnedFolder.WithADamagedCommon));

        var banner = BannerOf(launch);
        Assert.Contains("xpe_common.dll at '" + Path.Combine(launch.PinnedFolderPath!, "xpe_common.dll") + "'", banner, StringComparison.Ordinal);
        Assert.Contains("could not be loaded", banner, StringComparison.Ordinal);
    }

    /// <summary>
    /// GUI-C-243 (Codex #195): the DLLs a module IMPORTS are held to the pinned folder too. The pinned folder has xpe_common.dll but not the spdlog.dll it imports; a WORKING spdlog.dll (and the rest) stands in the application
    /// folder. The app must not take that copy: Mock, and the banner says the pinned xpe_common.dll could not be loaded (it used to load, with the application folder's spdlog.dll, and the app started on Native).
    /// </summary>
    [SkippableFact]
    public void PinnedFolderMissingARuntimeDll_AWorkingCopyBesideTheExe_IsNotTaken_MockWithTheReason()
    {
        var nativeDirectory = NativeDirectoryOrSkip();
        using var launch = LaunchedCopy.Start(output, nativeDirectory, new Launch(Exclusive: true, AppFolder: AppFolderDlls.Working, Pinned: PinnedFolder.WithoutARuntimeDll));

        var banner = BannerOf(launch);
        Assert.Contains("xpe_common.dll at '" + Path.Combine(launch.PinnedFolderPath!, "xpe_common.dll") + "'", banner, StringComparison.Ordinal);
        Assert.Contains("could not be loaded", banner, StringComparison.Ordinal);
    }

    /// <summary>XPE_NATIVE_DIR_EXCLUSIVE=1 and no XPE_NATIVE_DIR, with WORKING DLLs in the application folder: a pin that names no folder is not quietly turned into the default search. Mock, and the banner says so.</summary>
    [SkippableFact]
    public void ExclusiveWithoutAFolder_WorkingDllsBesideTheExe_AreNotUsed_MockWithTheReason()
    {
        var nativeDirectory = NativeDirectoryOrSkip();
        using var launch = LaunchedCopy.Start(output, nativeDirectory, new Launch(Exclusive: true, AppFolder: AppFolderDlls.Working, SetNativeDir: false));

        var banner = BannerOf(launch);
        Assert.Contains("XPE_NATIVE_DIR does not name it", banner, StringComparison.Ordinal);
    }

    private string BannerOf(LaunchedCopy launch)
    {
        output.WriteLine($"title: '{launch.Window.Title}'");
        Assert.Contains("[MOCK]", launch.Window.Title, StringComparison.Ordinal);
        Assert.True(launch.WaitForText("MOCK BACKEND"), "no MOCK BACKEND banner: the app did not fall back to Mock (backend line: " + string.Join(" | ", launch.TextsContaining("mode=")) + ")");
        var banner = launch.TextsContaining("MOCK BACKEND").First();
        output.WriteLine($"banner: {banner}");
        Assert.Contains("Native was requested but could not be used:", banner, StringComparison.Ordinal);
        return banner;
    }

    private static string NativeDirectoryOrSkip()
    {
        var directory = Environment.GetEnvironmentVariable(ApplicationFixture.NativeDirVariable);
        Skip.If(string.IsNullOrWhiteSpace(directory) || !File.Exists(Path.Combine(directory, "xpe_common.dll")),
            $"{ApplicationFixture.NativeDirVariable} does not name a folder with the staged native DLLs: this checks the Native launch path (the Native E2E job sets it).");
        return directory!;
    }

    private enum AppFolderDlls
    {
        /// <summary>Nothing native beside the exe.</summary>
        None,

        /// <summary>xpe_common.dll / xpe_display.dll with the right names and the wrong content.</summary>
        Wrong,

        /// <summary>The real xpe_common.dll, xpe_display.dll, fmt.dll and spdlog.dll of the staged set: Native can start from them.</summary>
        Working,
    }

    private enum PinnedFolder
    {
        /// <summary>The staged folder itself (the start script's case).</summary>
        Staged,

        /// <summary>A temp folder with xpe_display.dll, fmt.dll and spdlog.dll but no xpe_common.dll.</summary>
        WithoutCommon,

        /// <summary>A temp folder with the real xpe_display.dll, fmt.dll, spdlog.dll and a damaged xpe_common.dll.</summary>
        WithADamagedCommon,

        /// <summary>GUI-C-243: a temp folder with the real xpe_common.dll, xpe_display.dll and fmt.dll but NO spdlog.dll (which xpe_common.dll imports: measured with a PE import read, GUI-C-243 dll_imports.txt).</summary>
        WithoutARuntimeDll,
    }

    private sealed record Launch(bool Exclusive, AppFolderDlls AppFolder, bool SetNativeDir = true, PinnedFolder Pinned = PinnedFolder.Staged);

    /// <summary>A copy of the app's output folder with the chosen DLLs in it, started the way the start script starts the app.</summary>
    private sealed class LaunchedCopy : IDisposable
    {
        private readonly UIA3Automation _automation;
        private readonly Application _application;
        private readonly ITestOutputHelper _output;

        public string AppFolderPath { get; }

        public string? PinnedFolderPath { get; }

        public Window Window { get; }

        private LaunchedCopy(ITestOutputHelper output, UIA3Automation automation, string appFolder, string? pinnedFolder, Application application, Window window)
        {
            _output = output;
            _automation = automation;
            AppFolderPath = appFolder;
            PinnedFolderPath = pinnedFolder;
            _application = application;
            Window = window;
        }

        public static LaunchedCopy Start(ITestOutputHelper output, string nativeDirectory, Launch launch)
        {
            var built = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", "..", "gui", "ImageProcTest", "bin", "Debug", "net8.0-windows"));
            Skip.IfNot(File.Exists(Path.Combine(built, "ImageProcTest.exe")), "gui/ImageProcTest/bin/Debug/net8.0-windows/ImageProcTest.exe was not found: build gui/ImageProcTest first.");

            var root = Path.Combine(Path.GetTempPath(), "xpe_c238_" + Guid.NewGuid().ToString("N"));
            var appFolder = Path.Combine(root, "app");
            CopyFolder(built, appFolder);

            switch (launch.AppFolder)
            {
                case AppFolderDlls.Wrong:
                    // wrong content under the right names: another module's DLL of the staged set (it loads, or it does not; either way it lacks the exports the app needs), or garbage when there is none
                    File.WriteAllBytes(Path.Combine(appFolder, "xpe_common.dll"), BytesOfAnotherModule(nativeDirectory, "xpe_preprocess.dll"));
                    File.WriteAllBytes(Path.Combine(appFolder, "xpe_display.dll"), BytesOfAnotherModule(nativeDirectory, "xpe_enhance_basic.dll"));
                    break;
                case AppFolderDlls.Working:
                    foreach (var name in new[] { "xpe_common.dll", "xpe_display.dll" }.Concat(RuntimeDlls))
                    {
                        File.Copy(Path.Combine(nativeDirectory, name), Path.Combine(appFolder, name), overwrite: true);
                    }

                    break;
            }

            string? pinnedFolder = null;
            switch (launch.Pinned)
            {
                case PinnedFolder.Staged:
                    pinnedFolder = nativeDirectory;
                    break;
                case PinnedFolder.WithoutARuntimeDll:
                    pinnedFolder = Path.Combine(root, "pinned");
                    Directory.CreateDirectory(pinnedFolder);
                    foreach (var name in new[] { "xpe_common.dll", "xpe_display.dll", "fmt.dll" })
                    {
                        File.Copy(Path.Combine(nativeDirectory, name), Path.Combine(pinnedFolder, name));
                    }

                    break;
                case PinnedFolder.WithoutCommon:
                case PinnedFolder.WithADamagedCommon:
                    pinnedFolder = Path.Combine(root, "pinned");
                    Directory.CreateDirectory(pinnedFolder);
                    foreach (var name in new[] { "xpe_display.dll" }.Concat(RuntimeDlls))
                    {
                        File.Copy(Path.Combine(nativeDirectory, name), Path.Combine(pinnedFolder, name));
                    }

                    if (launch.Pinned == PinnedFolder.WithADamagedCommon)
                    {
                        File.WriteAllBytes(Path.Combine(pinnedFolder, "xpe_common.dll"), [0x4D, 0x5A, 0x00, 0x00, 0x01, 0x02]);
                    }

                    break;
            }

            var calibration = Path.Combine(root, "calib");
            Directory.CreateDirectory(calibration);

            var info = new ProcessStartInfo(Path.Combine(appFolder, "ImageProcTest.exe")) { WorkingDirectory = appFolder, UseShellExecute = false };
            foreach (var argument in new[] { "--automation-settings", Path.Combine(root, "appsettings.json"), "--automation-backend", "Native", "--automation-calib", calibration, "--automation-width", "1024", "--automation-height", "1024" })
            {
                info.ArgumentList.Add(argument);
            }

            if (launch.SetNativeDir)
            {
                info.Environment["XPE_NATIVE_DIR"] = pinnedFolder!;
            }
            else
            {
                info.Environment.Remove("XPE_NATIVE_DIR");
            }

            if (launch.Exclusive)
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
                return new LaunchedCopy(output, automation, appFolder, launch.SetNativeDir ? pinnedFolder : null, application, window);
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
            var root = Path.GetDirectoryName(AppFolderPath)!;
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
