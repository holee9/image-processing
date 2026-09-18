// XPE-GUI-E2E-001 §3: launches the WPF app under test and guarantees it is gone afterwards.
using System.Diagnostics;
using FlaUI.Core;
using FlaUI.Core.AutomationElements;
using FlaUI.Core.Exceptions;
using FlaUI.UIA3;
using Xunit;

namespace ImageProcTest.E2ETests.Fixtures;

/// <summary>
/// Starts one instance of the GUI for a test class and disposes it, whatever the tests did.
///
/// Two properties are load-bearing:
///
/// <list type="bullet">
/// <item>The app is launched in <b>Mock</b> backend mode through <c>--automation-backend</c>
/// (GUI-C-26), so the smoke suite never depends on native DLLs being staged.</item>
/// <item>It is launched WITHOUT <c>--automation-report</c>, so <c>App.IsAutomationMode</c> stays
/// false and the self-driving scenario does not run — that harness closes the window on its own,
/// which would race every assertion here.</item>
/// </list>
///
/// A leaked app process holds a window that the next test class would find and drive, so disposal
/// kills rather than asks: <c>Close()</c> on a WPF window can be refused or blocked by a dialog.
/// </summary>
public class ApplicationFixture : IDisposable
{
    private readonly Application? _application;

    // #170 (GUI-C-93): null unless XPE_C93_TIMING=1 — records launch timings, changes nothing.
    private readonly LaunchTimingRecorder? _timing;

    public ApplicationFixture()
        : this(rawImageRelativePath: null)
    {
    }

    /// <summary>
    /// Test-only construction that arms the seams for ONE fixture (GUI-C-52).
    ///
    /// The seams used to be static fields reset in a <c>finally</c>. GUI-C-51 recorded the risk and
    /// this closes it: <c>finally</c> is not a guarantee — a killed test host never runs it — and a
    /// leaked static would arm the NEXT fixture, putting a re-acquire note on an innocent scenario.
    /// As instance state the leak is not merely unlikely, it is impossible: the values live and die
    /// with the fixture that asked for them, and parallel execution cannot share them either.
    /// </summary>
    internal ApplicationFixture(int simulateUnreadableChecks, bool skipLeftoverSweep)
        : this(rawImageRelativePath: null, simulateUnreadableChecks, skipLeftoverSweep)
    {
    }

    /// <summary>
    /// #136 (GUI-C-34): a workflow run needs an image on screen. The app already loads one when
    /// given <c>--automation-raw</c>, and that path is reused rather than driving the file dialog
    /// through UIA — a modal Win32 dialog is the most brittle thing a suite can automate, and the
    /// app offers a supported way in.
    /// </summary>
    protected ApplicationFixture(string? rawImageRelativePath)
        : this(rawImageRelativePath, simulateUnreadableChecks: 0, skipLeftoverSweep: false)
    {
    }

    /// <summary>
    /// #171 (GUI-C-79): a launch with extra switches (the fault seam). Its own app, so the sweep is
    /// skipped for the same reason <c>WindowReacquireTests</c> skips it.
    /// </summary>
    protected ApplicationFixture(string? rawImageRelativePath, IReadOnlyList<string> extraArguments)
        : this(rawImageRelativePath, simulateUnreadableChecks: 0, skipLeftoverSweep: true, extraArguments)
    {
    }

    /// <summary>
    /// #175 (GUI-C-82): Native requested, with the native search pinned to
    /// <paramref name="forcedNativeDirectory"/> whatever <c>XPE_E2E_BACKEND</c> says. Pointed at a
    /// directory without the DLLs, this is the silent Mock fallback HAZ-GUI-005 is about.
    /// </summary>
    protected ApplicationFixture(string? rawImageRelativePath, DirectoryInfo forcedNativeDirectory)
        : this(rawImageRelativePath, simulateUnreadableChecks: 0, skipLeftoverSweep: true, null, forcedNativeDirectory.FullName)
    {
    }

    private ApplicationFixture(
        string? rawImageRelativePath,
        int simulateUnreadableChecks,
        bool skipLeftoverSweep,
        IReadOnlyList<string>? extraArguments = null,
        string? forcedNativeDirectory = null)
    {
        _simulateUnreadableChecks = simulateUnreadableChecks;
        Automation = new UIA3Automation();

        var exePath = ResolveApplicationExecutable();
        if (exePath is null)
        {
            SkipReason =
                "gui/ImageProcTest/bin/Debug/net8.0-windows/ImageProcTest.exe was not found. " +
                "Build the gui project (or the clients solution, which now includes it) before the E2E suite.";
            return;
        }

        // #163 / GUI-C-109: the exe that is about to be launched must be newer than the sources it was
        // built from. This throws rather than skipping: a stale-binary run is the failure mode that
        // reports a green for code that never ran, and a skip would hide it the same way.
        EnsureApplicationIsFresh(exePath);

        BackendMode = forcedNativeDirectory is null ? ResolveBackendMode() : "Native";
        if (BackendMode is null)
        {
            SkipReason =
                $"XPE_E2E_BACKEND='{Environment.GetEnvironmentVariable(BackendVariable)}' is not one of " +
                $"{string.Join(" | ", AcceptedBackendModes)}.";
            return;
        }

        LeftoverNote = skipLeftoverSweep ? "sweep skipped (test seam)" : KillLeftovers(exePath);

        var startInfo = new ProcessStartInfo(exePath)
        {
            WorkingDirectory = Path.GetDirectoryName(exePath)!,
            UseShellExecute = false,
        };
        startInfo.ArgumentList.Add("--automation-backend");
        startInfo.ArgumentList.Add(BackendMode);

        if (rawImageRelativePath is not null)
        {
            var raw = Path.Combine(Path.GetDirectoryName(exePath)!, rawImageRelativePath);
            if (!File.Exists(raw))
            {
                SkipReason = $"Fixture image not found: {raw}";
                return;
            }

            // Width/height must accompany the path — the loader reads raw bytes and cannot infer them.
            startInfo.ArgumentList.Add("--automation-raw");
            startInfo.ArgumentList.Add(raw);
            startInfo.ArgumentList.Add("--automation-width");
            startInfo.ArgumentList.Add("1024");
            startInfo.ArgumentList.Add("--automation-height");
            startInfo.ArgumentList.Add("1024");
            RawImagePath = raw;
        }

        if (BackendMode == "Native")
        {
            CalibrationDirectory = SharedCalibrationSet(out var calibrationNote);
            CalibrationNote = calibrationNote;

            if (CalibrationDirectory is not null)
            {
                startInfo.ArgumentList.Add("--automation-calib");
                startInfo.ArgumentList.Add(CalibrationDirectory);
            }

            // The app resolves native DLLs through XPE_NATIVE_DIR; pinning it EXCLUSIVE keeps the
            // search from wandering into build directories or sibling checkouts (GUI-C-16/#129), so
            // a Native run names exactly which binaries it exercised.
            var nativeDir = forcedNativeDirectory ?? Environment.GetEnvironmentVariable(NativeDirVariable);
            if (!string.IsNullOrWhiteSpace(nativeDir))
            {
                startInfo.Environment[NativeDirVariable] = nativeDir;
                startInfo.Environment["XPE_NATIVE_DIR_EXCLUSIVE"] = "1";
                NativeDirectory = nativeDir;
            }
        }

        foreach (var argument in extraArguments ?? [])
        {
            startInfo.ArgumentList.Add(argument);
        }

        _timing = LaunchTimingRecorder.StartIfEnabled();
        _application = Application.Launch(startInfo);
        var launched = _application.GetMainWindow(Automation, TimeSpan.FromSeconds(30));
        if (_timing is not null)
        {
            _timing.MainWindowReturnedUtc = DateTime.UtcNow;
            _timing.ProcessId = _application.ProcessId;
        }

        ProbeAcquisitionRoutes(launched);
        MainWindow = WithReadableProperties(launched);
        ExecutablePath = exePath;
    }

    /// <summary>
    /// Returns a window element whose WPF properties are readable, re-locating it if the one we were
    /// handed is the Win32 shell of the same HWND.
    ///
    /// <para>GUI-C-50 measured this rather than guessing. Across 60 launches, 3 produced a window on
    /// which <c>AutomationId</c> threw <c>PropertyNotSupportedException [#30011]</c> — the failure
    /// that took down <c>S01_Launch_HasMainWindow</c> intermittently since GUI-C-46. In every one of
    /// the three the element reported <c>framework=Win32</c> (not WPF) while <c>Title</c>,
    /// <c>Name</c> and <c>ClassName</c> read fine, so the suite held the generic Win32 provider for
    /// the HWND rather than the WPF provider that owns the automation ids.</para>
    ///
    /// <para><b>Why not a wait.</b> Retrying the SAME element is useless: the probe hammered it for
    /// 10 seconds — 114 248, 121 978 and 126 861 attempts in the three cases — and it never became
    /// readable. A sleep would have slowed every run and left the intermittent in place. Re-locating
    /// the window, on the other hand, returned <c>AutomationId='MainWindow'</c> immediately in all
    /// three. The element does not ripen; it has to be replaced.</para>
    ///
    /// <para>The bound is two seconds, not a number chosen for comfort: the replacement succeeded on
    /// the first re-location in every observed case, so this only has to survive a slow desktop
    /// enumeration. A failure to replace leaves the original element in place, so the scenario still
    /// reports the property exception rather than a fixture error that hides it.</para>
    /// </summary>
    private Window WithReadableProperties(Window window)
    {
        if (CanReadAutomationId(window)) return window;

        var processId = window.Properties.ProcessId.ValueOrDefault;
        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(2);

        while (DateTime.UtcNow < deadline)
        {
            var fresh = Automation.GetDesktop()
                .FindFirstChild(cf => cf.ByProcessId(processId))?.AsWindow();

            if (fresh is not null && CanReadAutomationId(fresh))
            {
                // GUI-C-56: the note carries the window handles because the decisive question about
                // this defect is whether the two elements are the SAME window seen through a
                // different provider, or two different windows. Without the handles the next
                // occurrence would leave the same open question it has left every time so far.
                ReacquiredNote =
                    $"The launched window did not support AutomationId (framework=" +
                    $"{SafeFramework(window)}); re-located it by process id {processId}. " +
                    $"hwnd launched={Handle(window)} reacquired={Handle(fresh)} " +
                    $"topLevelWindowsForProcess={TopLevelWindowCount(processId)}";
                return fresh;
            }
        }

        return window;
    }

    /// <summary>
    /// Test seam (GUI-C-51/52): how many readability checks report failure before the real read.
    ///
    /// The re-acquire path fires on roughly one launch in twenty, so 74 consecutive healthy runs
    /// never entered it — a branch no test can reach is a branch nobody has checked.
    ///
    /// A counter rather than a flag: the measured shape is "the FIRST element fails, the re-located
    /// one succeeds". A flag would fail both and exercise the give-up path instead — the test device
    /// testing something other than what it claims.
    ///
    /// Instance state, not static (GUI-C-52): see the seam constructor above.
    /// </summary>
    private int _simulateUnreadableChecks;

    /// <summary>True when the element answers the property the scenarios read first.</summary>
    private bool CanReadAutomationId(Window window)
    {
        if (_simulateUnreadableChecks > 0)
        {
            _simulateUnreadableChecks--;
            return false;
        }

        try
        {
            _ = window.AutomationId;
            return true;
        }
        catch (PropertyNotSupportedException)
        {
            return false;
        }
    }

    /// <summary>
    /// GUI-C-56 investigation probe: asks the SAME window for AutomationId through three different
    /// acquisition routes and prints the answers.
    ///
    /// <para>The re-acquire path already shows that a desktop tree walk succeeds where the launched
    /// element fails, on the same HWND, at the same moment. What it cannot show is whether the tree
    /// walk is merely a second chance or actually a different provider: that needs both routes probed
    /// on launches where the launched element WORKED too. This prints on every launch, so the
    /// per-route failure rates can be compared rather than inferred.</para>
    ///
    /// <para>It writes to the console rather than to a note because it is not a scenario's business:
    /// the note reports a defect that fired, this reports a measurement taken on every launch. It
    /// reads AutomationId with a raw try/catch rather than through
    /// <see cref="CanReadAutomationId"/>, so it never consumes the simulation seam.</para>
    ///
    /// <para><b>What it measured, over 32 launches each (GUI-C-56).</b> 14 launches handed back an
    /// element that threw <c>PropertyNotSupportedException</c>; on every one of those,
    /// <c>FromHandle</c> and the tree walk answered correctly at the same moment on the same HWND.
    /// A fourth column, since removed, repeated <c>GetMainWindow</c> itself a moment later: it
    /// answered correctly on all 9 failing launches of that batch. So FlaUI does not remember a
    /// broken element and the acquisition route is not the difference — <b>only the moment the
    /// element was created is</b>.</para>
    /// </summary>
    private void ProbeAcquisitionRoutes(Window launched)
    {
        static string Readable(AutomationElement? element)
        {
            if (element is null) return "null";
            try { _ = element.Properties.AutomationId.Value; return "ok"; }
            catch (Exception ex) { return ex.GetType().Name; }
        }

        try
        {
            var processId = launched.Properties.ProcessId.ValueOrDefault;
            var handle = launched.Properties.NativeWindowHandle.ValueOrDefault;

            var fromHandle = Automation.FromHandle(handle);
            var fromTree = Automation.GetDesktop().FindFirstChild(cf => cf.ByProcessId(processId));

            // GUI-C-93: the launched element's first AutomationId read, timed at the same point in the
            // sequence as before (after the ProcessId/handle reads and the two lookups).
            if (_timing is not null) _timing.QueryUtc = DateTime.UtcNow;
            var launchedReadable = Readable(launched);
            if (_timing is not null)
            {
                _timing.QueryResult = launchedReadable;
                _timing.Framework = SafeFramework(launched);
            }

            Console.WriteLine(
                $"XPE-C56-PROBE hwnd=0x{handle.ToInt64():X} pid={processId} " +
                $"launched={launchedReadable} fromHandle={Readable(fromHandle)} " +
                $"fromTree={Readable(fromTree)} framework={SafeFramework(launched)}");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"XPE-C56-PROBE could not run: {ex.GetType().Name}");
        }
    }

    /// <summary>The element's native window handle, or a marker when it cannot be read.</summary>
    private static string Handle(AutomationElement element)
    {
        try { return $"0x{element.Properties.NativeWindowHandle.ValueOrDefault.ToInt64():X}"; }
        catch (Exception ex) { return $"<{ex.GetType().Name}>"; }
    }

    /// <summary>How many top-level windows the desktop reports for this process.</summary>
    private string TopLevelWindowCount(int processId)
    {
        try { return Automation.GetDesktop().FindAllChildren(cf => cf.ByProcessId(processId)).Length.ToString(); }
        catch (Exception ex) { return $"<{ex.GetType().Name}>"; }
    }

    private static string SafeFramework(Window window)
    {
        try { return window.FrameworkType.ToString(); }
        catch (Exception) { return "unknown"; }
    }

    /// <summary>Non-empty when the window had to be re-located; scenarios report it.</summary>
    public string ReacquiredNote { get; private set; } = string.Empty;

    /// <summary>Environment variable selecting which backend the suite exercises.</summary>
    public const string BackendVariable = "XPE_E2E_BACKEND";

    /// <summary>Environment variable naming the directory the app loads native DLLs from.</summary>
    public const string NativeDirVariable = "XPE_NATIVE_DIR";

    /// <summary>Accepted values, matching the app's own rule (GUI-C-27).</summary>
    public static readonly string[] AcceptedBackendModes = ["Mock", "Native"];

    /// <summary>The backend this run drives, or null when the variable held an unusable value.</summary>
    public string? BackendMode { get; }

    /// <summary>The pinned native directory, when one was supplied for a Native run.</summary>
    public string? NativeDirectory { get; }

    /// <summary>The raw image this run was launched with, when the fixture asked for one.</summary>
    public string? RawImagePath { get; }

    /// <summary>What the pre-launch sweep found and did. Empty when nothing was left behind.</summary>
    public string LeftoverNote { get; } = string.Empty;

    /// <summary>Directory holding this run's generated XCal set, or null when none was produced.</summary>
    public string? CalibrationDirectory { get; }

    /// <summary>How the calibration set was obtained, or why it was not. Reported by scenarios.</summary>
    public string CalibrationNote { get; } = string.Empty;

    /// <summary>The UIA layer, shared by every scenario in the class.</summary>
    public UIA3Automation Automation { get; }

    /// <summary>The app's main window, or null when the app could not be started.</summary>
    public Window? MainWindow { get; }

    /// <summary>Path the app was launched from — reported so a run names what it exercised.</summary>
    public string? ExecutablePath { get; }

    /// <summary>Non-null when the app was not started; scenarios skip with this text.</summary>
    public string? SkipReason { get; }

    /// <summary>True when a window is available to drive.</summary>
    public bool IsAvailable => MainWindow is not null;

    public void Dispose()
    {
        _timing?.Dispose();

        try
        {
            _application?.Kill();
        }
        catch (InvalidOperationException)
        {
            // Already exited — nothing to kill.
        }
        finally
        {
            _application?.Dispose();
            Automation.Dispose();
        }
    }

    /// <summary>
    /// Produces an XCal set for this run with <c>xpe_calib_fixture_gen</c> (QA-A-36).
    ///
    /// The repository carries no calibration fixtures and this lane does not build native binaries
    /// (CI owns that, #98), so the generator is used only when a CI-staged copy is already present.
    /// When it is not, this returns null and the preprocess scenario SKIPS with the reason — a
    /// missing tool is "not measured", never "measured and fine".
    /// </summary>
    /// <summary>
    /// The XCal set for this test run, produced once and reused by every fixture.
    ///
    /// GUI-C-48 measured why this matters: <c>xpe_calib_fixture_gen</c> takes <b>53 seconds</b> for
    /// one 1024×1024 set, while launching the app takes 9. Generating per fixture was the whole
    /// Native budget — two fixtures meant two generations and a suite that ran 120 s instead of 67.
    ///
    /// Sharing is safe here in a way that sharing an app instance is not. What is shared is a
    /// directory of <b>read-only input files</b>: the app opens them, never writes them, and one
    /// fixture cannot leave state in them for the next. No GUI state, no settings, no window is
    /// shared — each fixture still launches and kills its own app, so the isolation the suite relies
    /// on (GUI-C-23, GUI-C-35) is untouched.
    ///
    /// Deterministic by construction: the generator runs with <c>--seed 0</c>, so a per-fixture set
    /// and the shared set are the same bytes. The reuse changes cost, not what is measured.
    /// </summary>
    private static readonly Lazy<(string? Directory, string Note)> SharedCalibration =
        new(() =>
        {
            var directory = GenerateCalibrationSet(out var note);
            return (directory, note);
        }, LazyThreadSafetyMode.ExecutionAndPublication);

    /// <summary>Returns the shared set, generating it on first use.</summary>
    private static string? SharedCalibrationSet(out string note)
    {
        var (directory, generatedNote) = SharedCalibration.Value;
        note = generatedNote;
        return directory;
    }

    private static string? GenerateCalibrationSet(out string note)
    {
        var generator = ResolveGenerator(out var searched);
        if (generator is null)
        {
            // The searched paths are named, not just the outcome. GUI-C-44: this skip fired on
            // EVERY CI run — CI stages into build/e2e-native-dlls while this looked only under
            // build/ci-common/bin — and the reason said "not found under build/ci-common/bin",
            // which reads as "nothing was staged" rather than "I looked in the wrong place".
            note = "xpe_calib_fixture_gen.exe was not found, so the preprocess success path is NOT " +
                   $"measured. Looked in: {string.Join(" ; ", searched)}. Stage the " +
                   "xpe-ci-preprocess-binaries artifact into one of these, or point " +
                   $"{NativeDirVariable} at a directory holding the generator.";
            return null;
        }

        var directory = Path.Combine(Path.GetTempPath(), $"xpe_calib_{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);

        var startInfo = new ProcessStartInfo(generator)
        {
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
        };
        foreach (var argument in new[]
                 {
                     "--out", directory, "--width", "1024", "--height", "1024", "--seed", "0",
                 })
        {
            startInfo.ArgumentList.Add(argument);
        }

        using var process = Process.Start(startInfo);
        if (process is null)
        {
            note = $"Could not start {generator}.";
            return null;
        }

        var stdout = process.StandardOutput.ReadToEnd();
        var stderr = process.StandardError.ReadToEnd();
        process.WaitForExit(60_000);

        var produced = Directory.Exists(directory)
            ? Directory.GetFiles(directory, "*.xcal").Select(Path.GetFileName).Order().ToArray()
            : [];

        if (process.ExitCode != 0 || produced.Length == 0)
        {
            note = $"{Path.GetFileName(generator)} exit={process.ExitCode}, files=[{string.Join(", ", produced)}]. " +
                   $"stdout: {stdout.Trim()} stderr: {stderr.Trim()}";
            return null;
        }

        note = $"{Path.GetFileName(generator)} produced [{string.Join(", ", produced)}] in {directory}.";
        return directory;
    }

    /// <summary>Walks up from the test output directory to a repository-relative file, or null.</summary>
    /// <summary>
    /// Finds <c>xpe_calib_fixture_gen.exe</c>, preferring the directory the run actually pinned.
    ///
    /// GUI-C-44. The generator used to be looked up only under <c>build/ci-common/bin</c> — the path
    /// this lane stages into locally. CI stages into <c>build/e2e-native-dlls</c> and names it via
    /// <see cref="NativeDirVariable"/>, so the lookup missed every time and W-02 skipped on every CI
    /// run since it was written. The skip was correct behaviour (a missing tool is "not measured");
    /// what was wrong was looking in one lane's directory and calling that "not staged".
    ///
    /// <paramref name="searched"/> carries every candidate so a failure names where it looked.
    /// </summary>
    private static string? ResolveGenerator(out IReadOnlyList<string> searched)
    {
        const string generatorName = "xpe_calib_fixture_gen.exe";
        var candidates = new List<string>();

        // The pinned native directory first: it is what THIS run exercises, and CI supplies it.
        var nativeDir = Environment.GetEnvironmentVariable(NativeDirVariable);
        if (!string.IsNullOrWhiteSpace(nativeDir))
        {
            candidates.Add(Path.Combine(nativeDir, generatorName));
        }

        // Then the lane's own staging path, walked up from the test output directory. Kept as a
        // fallback rather than replaced: a local run without the variable set must keep working.
        var relative = Path.Combine("build", "ci-common", "bin", generatorName);
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null)
        {
            candidates.Add(Path.Combine(dir.FullName, relative));
            dir = dir.Parent;
        }

        searched = candidates;
        return candidates.FirstOrDefault(File.Exists);
    }

    private static string? ResolveRepositoryFile(string relativePath)
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null)
        {
            var candidate = Path.Combine(dir.FullName, relativePath);
            if (File.Exists(candidate)) return candidate;
            dir = dir.Parent;
        }

        return null;
    }

    /// <summary>
    /// Kills app instances left over from an earlier run BEFORE launching a new one.
    ///
    /// GUI-C-35 measured the damage: one surviving instance held ImageProcTest.exe, which made the
    /// solution build fail with MSB3027 and took four E2E scenarios down with it — a failure that
    /// looks like a code defect and is not one.
    ///
    /// Only processes running THIS exe are touched. Matching by name alone would kill a developer's
    /// own session of the app, and a test suite must not do that.
    /// </summary>
    private static string KillLeftovers(string exePath)
    {
        var killed = new List<int>();

        foreach (var process in Process.GetProcessesByName(
                     Path.GetFileNameWithoutExtension(exePath)))
        {
            try
            {
                if (!string.Equals(process.MainModule?.FileName, exePath, StringComparison.OrdinalIgnoreCase))
                {
                    continue;   // A different build or a different app with the same name.
                }

                process.Kill(entireProcessTree: true);
                process.WaitForExit(5000);
                killed.Add(process.Id);
            }
            catch (Exception ex) when (ex is InvalidOperationException or System.ComponentModel.Win32Exception)
            {
                // Already gone, or its module is not readable (elevated / exiting). Not fatal:
                // the launch below will fail loudly if the file is still locked.
            }
            finally
            {
                process.Dispose();
            }
        }

        return killed.Count == 0
            ? string.Empty
            : $"Killed {killed.Count} leftover instance(s) before launch: {string.Join(", ", killed)}";
    }

    /// <summary>
    /// Reads XPE_E2E_BACKEND, defaulting to Mock. An unrecognised value is REFUSED rather than
    /// silently treated as Mock: the whole point of the Native variant is to know which backend was
    /// measured, and a typo that quietly downgrades makes the run report the wrong thing
    /// (the same hazard GUI-C-27 removed from --automation-backend).
    /// </summary>
    private static string? ResolveBackendMode()
    {
        var requested = Environment.GetEnvironmentVariable(BackendVariable);
        if (string.IsNullOrWhiteSpace(requested))
        {
            return "Mock";
        }

        return AcceptedBackendModes.FirstOrDefault(
            m => string.Equals(m, requested, StringComparison.OrdinalIgnoreCase));
    }

    /// <summary>
    /// Walks up from the test output directory to the repository, then to the gui build output.
    /// Returns null rather than throwing so the suite can report "not built" as a skip instead of
    /// a failure that looks like a defect in the app.
    /// </summary>
    /// <summary>
    /// Fails when the app executable is older than the gui sources it is built from (GUI-C-109).
    ///
    /// <para>The hazard is structural, not hypothetical: this test project deliberately does NOT
    /// reference the WPF app — a ProjectReference made the suite depend on ImageProcTest.exe, and a
    /// leftover app instance holding that exe broke builds (see the csproj note). The cost of that
    /// choice is that <c>dotnet build</c> on THIS project does not rebuild the app, so editing app code
    /// and building only the tests launches the previous binary. Every assertion then passes against
    /// code that never ran, and the run is green.</para>
    ///
    /// <para>Nothing detected that before: the resolver above only checks the exe exists. The native
    /// DLLs have had a provenance record since GUI-C-41 for exactly this reason; the app had none.</para>
    ///
    /// <para>The reference is NOT restored — that would bring back the locking fault. This reads
    /// timestamps instead, which needs no build-graph edge.</para>
    /// </summary>
    private static void EnsureApplicationIsFresh(string exePath)
    {
        var guiRoot = FindGuiSourceRoot(exePath);
        if (guiRoot is null) return;   // cannot locate sources: stay quiet rather than invent a failure

        var exeWrittenUtc = File.GetLastWriteTimeUtc(exePath);
        FileInfo? newest = null;

        foreach (var file in Directory.EnumerateFiles(guiRoot, "*", SearchOption.AllDirectories))
        {
            // bin/ and obj/ are build output, not sources — obj in particular is written DURING the
            // build, so including it would compare the build against itself.
            if (file.Contains($"{Path.DirectorySeparatorChar}bin{Path.DirectorySeparatorChar}", StringComparison.OrdinalIgnoreCase) ||
                file.Contains($"{Path.DirectorySeparatorChar}obj{Path.DirectorySeparatorChar}", StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            // Compile inputs only. Two reasons, both measured:
            //
            // The app WRITES into its own project directory — automation-report.json lands beside the
            // sources when the hidden-window automation step runs, which in CI happens AFTER the build.
            // Treating that as a source turned this guard into a false red on every gui-automation run
            // (main, run 35399591335: 97 of 127 cases failed in 13 s, all on this exception).
            //
            // And content files would be wrong here even without that: editing appsettings.json and
            // rebuilding copies the file without relinking, so the exe keeps its old timestamp and the
            // guard would fire on a build that IS current. This guard answers "was the exe compiled
            // from these sources", and only compile inputs can answer it.
            var extension = Path.GetExtension(file);
            if (extension is not (".cs" or ".xaml" or ".csproj" or ".resx")) continue;

            var info = new FileInfo(file);
            if (newest is null || info.LastWriteTimeUtc > newest.LastWriteTimeUtc) newest = info;
        }

        if (newest is null || newest.LastWriteTimeUtc <= exeWrittenUtc) return;

        throw new InvalidOperationException(
            $"The gui app executable is older than its sources, so this run would test the PREVIOUS build. " +
            $"exe '{exePath}' written {exeWrittenUtc:O}; newest source '{newest.FullName}' written " +
            $"{newest.LastWriteTimeUtc:O}. Build the app first: " +
            $"dotnet build gui/ImageProcTest/ImageProcTest.csproj -c Debug. " +
            $"(This project does not reference the app on purpose — see the csproj note — so building " +
            $"the tests alone never rebuilds it.)");
    }

    /// <summary>The gui project directory that produced <paramref name="exePath"/>, or null.</summary>
    private static string? FindGuiSourceRoot(string exePath)
    {
        // <gui root>/ImageProcTest/bin/Debug/net8.0-windows/ImageProcTest.exe -> <gui root>/ImageProcTest
        var dir = new DirectoryInfo(Path.GetDirectoryName(exePath)!);
        for (var i = 0; i < 3 && dir is not null; i++) dir = dir.Parent;
        return dir is not null && dir.Exists ? dir.FullName : null;
    }

    internal static string? ResolveApplicationExecutable()
    {
        var relative = Path.Combine(
            "gui", "ImageProcTest", "bin", "Debug", "net8.0-windows", "ImageProcTest.exe");

        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null)
        {
            var candidate = Path.Combine(dir.FullName, relative);
            if (File.Exists(candidate)) return candidate;
            dir = dir.Parent;
        }

        return null;
    }
}

/// <summary>One app instance per test class — launching is the expensive part of the gate.</summary>
[CollectionDefinition(Name)]
public sealed class ApplicationCollection : ICollectionFixture<ApplicationFixture>
{
    public const string Name = "gui-application";
}
