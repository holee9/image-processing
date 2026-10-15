// GUI-C-219d (#249, Codex #116): what may start a processing command, the one gap in the job's end, the UI-thread guard without a window, and which loaded modules are the snapshot's.
using System.Text.RegularExpressions;

namespace ImageProcTest.IntegrationTests.Diagnostics;

/// <summary>
/// Threat scope decided with the card: an ACCIDENTAL change of the DLLs while the application runs (a rebuild, a redeploy) is in scope and is what these tests pin; a deliberate replacement aimed at
/// the milliseconds between a check and the load is out of scope (whoever can write the application folder is not stopped by this application), and is written down as a limit in the code and in
/// the report.
/// </summary>
[Collection("PreprocessOracleVerdicts")]   // the holder is process-wide state: these tests take turns with the ones in PreprocessOracleVerdictsTests
[Trait("Category", "P1AReady")]
public sealed class PreprocessOracleVerdicts219dTests : IDisposable
{
    private readonly string _directory;
    private readonly string _dll;

    public PreprocessOracleVerdicts219dTests()
    {
        PreprocessOracleVerdicts.ResetForTests();
        _directory = Path.Combine(Path.GetTempPath(), $"xpe_verdict219d_{Guid.NewGuid():N}");
        Directory.CreateDirectory(_directory);
        _dll = Path.Combine(_directory, "xpe_preprocess.dll");
        File.WriteAllText(_dll, "x");
    }

    public void Dispose()
    {
        OracleThreadGuard.OnUiThread = null;
        PreprocessOracleVerdicts.ResetForTests();
        try { Directory.Delete(_directory, recursive: true); } catch (IOException) { /* temp folder */ }
    }

    private static PreprocessSyntheticOracleResult Verdict(string status) => PreprocessSyntheticOracleResult.Failed(status, "stand-in");

    private void ReplaceKeepingTheTimestamp(string path, string content)
    {
        var stamp = File.GetLastWriteTimeUtc(path);
        File.WriteAllText(path, content);
        File.SetLastWriteTimeUtc(path, stamp);
    }

    // ---------------------------------------------------------------------------------------------------------------------------------- item 1: the pre-run check

    /// <summary>
    /// Codex #116 finding 2 (high). A passing verdict is stored; the original DLL then changes (same size, same timestamp: nothing but the bytes differ). The check that every processing command
    /// makes just before it runs says no, starts the verification again, and says yes only once the NEW content has a verdict of its own.
    /// </summary>
    [Fact]
    public async Task AProcessingCommand_DoesNotRun_WhenTheOriginalChangedAfterTheVerdict_AndRunsAgainOnceTheNewContentIsJudged()
    {
        var runs = 0;
        PreprocessOracleVerdicts.Runner = _ => Verdict($"run{Interlocked.Increment(ref runs)}");
        using var settled = new SemaphoreSlim(0);
        PreprocessOracleVerdicts.Completed += _ => settled.Release();

        Assert.Equal("run1", PreprocessOracleVerdicts.Wait(_dll).Status);
        Assert.True(settled.Wait(VerdictTestWaits.OuterWait), "the announcement of run 1 never came: " + PreprocessOracleVerdicts.DescribeState(_dll));
        Assert.True(await ProcessingContentGate.ConfirmAsync(_dll), "the files are the ones the verdict was made for: the command may run");

        ReplaceKeepingTheTimestamp(_dll, "y");

        Assert.False(await ProcessingContentGate.ConfirmAsync(_dll), "the original changed after the verdict: the command must not run");
        Assert.True(await settled.WaitAsync(VerdictTestWaits.OuterWait), $"the new content was never verified (runs={Volatile.Read(ref runs)}); {PreprocessOracleVerdicts.DescribeState(_dll)}");
        Assert.Equal("run2", PreprocessOracleVerdicts.TryGet(_dll)!.Status);
        Assert.True(await ProcessingContentGate.ConfirmAsync(_dll), "the new content has its own verdict now");
    }

    [Fact]
    public async Task TheCheck_SaysNo_ForADependencyThatChanged_ADllThatIsGone_AndWhenNothingWasVerified()
    {
        var dependency = Path.Combine(_directory, "fmt.dll");   // named for xpe_preprocess.dll by the application's own loader
        File.WriteAllText(dependency, "d1");
        PreprocessOracleVerdicts.Runner = _ => Verdict("ok");
        Assert.False(await ProcessingContentGate.ConfirmAsync(_dll), "nothing has been verified yet");   // (this ask also starts the verification)
        await VerdictTestWaits.PollAsync(() => PreprocessOracleVerdicts.TryGet(_dll), "the first verdict exists", () => PreprocessOracleVerdicts.DescribeState(_dll));
        Assert.True(await ProcessingContentGate.ConfirmAsync(_dll));

        ReplaceKeepingTheTimestamp(dependency, "d2");
        Assert.False(await ProcessingContentGate.ConfirmAsync(_dll), "a dependency changed");

        File.Delete(dependency);
        File.Delete(_dll);
        Assert.False(await ProcessingContentGate.ConfirmAsync(_dll), "the DLL is gone");
    }

    [Fact]
    public void TheCheck_IsNotMadeOnTheCallersThread_AndThereIsNothingToConfirmWithoutADllPath()
    {
        PreprocessOracleVerdicts.Runner = _ => Verdict("ok");
        PreprocessOracleVerdicts.Wait(_dll);
        bool? confirmed = null, noPath = null, blank = null;
        Exception? syncForm = null;
        var thread = new Thread(() =>   // a dedicated thread stands for the UI thread: it stays the same thread for the whole test (an awaiting test method does not)
        {
            var uiThread = Environment.CurrentManagedThreadId;
            OracleThreadGuard.OnUiThread = () => Environment.CurrentManagedThreadId == uiThread;

            // asked from the "UI thread": the hashing runs on a pool thread, so the guard (which throws for work done on the UI thread) is not hit
            confirmed = ProcessingContentGate.ConfirmAsync(_dll).GetAwaiter().GetResult();
            noPath = ProcessingContentGate.ConfirmAsync(null).GetAwaiter().GetResult();
            blank = ProcessingContentGate.ConfirmAsync("  ").GetAwaiter().GetResult();
            // the synchronous form is for callers that are not on a UI thread, and says so
            try { PreprocessOracleVerdicts.IsCurrent(_dll); } catch (Exception ex) { syncForm = ex; }
        });
        thread.Start();
        thread.Join();

        Assert.True(confirmed);
        Assert.True(noPath);
        Assert.True(blank);
        Assert.IsType<InvalidOperationException>(syncForm);
    }

    /// <summary>
    /// The commands are listed by a scan, because the guarantee is "every command that can run the native preprocess DLL asks first". The three click handlers of the window await the check before
    /// they do anything else; every file that calls the service that opens the DLL is one of the known callers, and the headless ones ask too. A new caller fails this test and has to say how it asks.
    /// </summary>
    [Fact]
    public void EveryEntryPointThatCanRunTheNativePreprocessDll_AsksTheCheckFirst()
    {
        var appDir = FindAppDir();
        var window = File.ReadAllText(Path.Combine(appDir, "MainWindow.xaml.cs"));
        foreach (var (handler, core) in new[]
        {
            ("ApplyNativePreviewButton_Click", "ApplyNativePreview();"),
            ("RunSelectedAlgorithmValidationButton_Click", "RunSelectedAlgorithmValidation();"),
            ("WorkflowRunButton_Click", "RunWorkflow();"),
        })
        {
            var body = BodyOf(window, "private async void " + handler + "(");
            var ask = body.IndexOf("await ConfirmProcessingContentAsync()", StringComparison.Ordinal);
            var run = body.IndexOf(core, StringComparison.Ordinal);
            Assert.True(ask >= 0 && run > ask, $"{handler} must `await ConfirmProcessingContentAsync()` and only then call {core}");
            Assert.Contains("return;", body[ask..run], StringComparison.Ordinal);
        }

        var confirm = BodyOf(window, "private async Task<bool> ConfirmProcessingContentAsync(");
        Assert.Contains("ProcessingContentGate.ConfirmAsync(", confirm, StringComparison.Ordinal);

        var callers = Directory.EnumerateFiles(appDir, "*.cs", SearchOption.AllDirectories)
            .Where(f => !f.Contains(Path.DirectorySeparatorChar + "obj" + Path.DirectorySeparatorChar, StringComparison.Ordinal))
            .Where(f => File.ReadAllText(f).Contains("NativePreprocessPreviewService.Run(", StringComparison.Ordinal))
            .Select(Path.GetFileName)
            .Order()
            .ToArray();
        Assert.Equal(new[] { "MainWindow.xaml.cs", "Phase1bFixtureE2eService.cs", "PreprocessFixtureE2eService.cs" }, callers);
        foreach (var headless in new[] { "Phase1bFixtureE2eService.cs", "PreprocessFixtureE2eService.cs" })
        {
            Assert.Contains("PreprocessOracleVerdicts.IsCurrent(", File.ReadAllText(Path.Combine(appDir, "Services", headless)), StringComparison.Ordinal);
        }

        // inside the window the service is reached from one method, which only the three cores (below the handlers) reach
        Assert.Single(Regex.Matches(window, @"NativePreprocessPreviewService\.Run\("));
    }

    // ---------------------------------------------------------------------------------------------------------------------------------- GUI-C-226: a dependency that vanishes during the copy

    /// <summary>
    /// GUI-C-226 (Codex #118, low): a DLL the preprocess DLL depends on is found, and is gone when the copy reaches it. The set that would be judged is then smaller than the set that is installed,
    /// so the check fails closed: ONE failed verdict is stored (the oracle is never run on the smaller set), the processing check says no, and asking again does not start a refresh storm (the same
    /// failure is announced once). The race is made to happen on EVERY pass (the file is put back before each ask and removed again at the moment of the copy), because a file that is gone for good
    /// is a different, legitimate situation: the next check then judges what is installed now.
    /// </summary>
    [Fact]
    public async Task ADependencyThatVanishesDuringTheCopy_IsOneStoredFailure_ProcessingStaysBlocked_AndThereIsNoRefreshStorm()
    {
        var dependency = Path.Combine(_directory, "fmt.dll");   // named for xpe_preprocess.dll by the application's own loader
        File.WriteAllText(dependency, "d1");
        var runnerCalls = 0;
        PreprocessOracleVerdicts.Runner = _ => { Interlocked.Increment(ref runnerCalls); return Verdict("judged"); };
        PreprocessOracleSnapshot.BeforeCopy = source => { if (string.Equals(Path.GetFileName(source), "fmt.dll", StringComparison.OrdinalIgnoreCase)) File.Delete(source); };
        var announced = 0;
        PreprocessOracleVerdicts.Completed += _ => Interlocked.Increment(ref announced);

        var verdict = PreprocessOracleVerdicts.Wait(_dll);

        Assert.Equal("Synthetic oracle setup failed", verdict.Status);
        Assert.Contains("fmt.dll", verdict.Details, StringComparison.Ordinal);
        Assert.Equal(0, Volatile.Read(ref runnerCalls));   // the smaller set was never judged
        Assert.Equal(1, Volatile.Read(ref announced));

        // the window asks again after every announcement; the same failure must not be announced again and again, and processing stays blocked
        for (var i = 0; i < 5; i++)
        {
            File.WriteAllText(dependency, "d1");           // the file is there again; the race removes it again at the copy
            Assert.False(await ProcessingContentGate.ConfirmAsync(_dll), "processing must stay blocked while the check keeps failing");
            await Task.Delay(150);                           // the pass that this ask started
        }

        Assert.Equal(1, Volatile.Read(ref announced));
        Assert.Equal(0, Volatile.Read(ref runnerCalls));
    }

    // ---------------------------------------------------------------------------------------------------------------------------------- GUI-C-226b: one writer for the preview text

    /// <summary>
    /// Codex #122: the refusal notice is kept in front of the preview text only if every write to that text goes through one function. A direct assignment anywhere else would overwrite the notice
    /// (and a later refresh would bring the stale one back). The number of assignments to the text box in the app's sources is held at exactly one, inside <c>RenderNativePreviewText</c>; the
    /// text box is named nowhere else in code.
    /// </summary>
    [Fact]
    public void TheNativePreviewText_HasExactlyOneWriter_AndNothingElseNamesTheTextBox()
    {
        var appDir = FindAppDir();
        var assignments = new List<string>();
        foreach (var file in Directory.EnumerateFiles(appDir, "*.cs", SearchOption.AllDirectories).Where(f => !f.Contains(Path.DirectorySeparatorChar + "obj" + Path.DirectorySeparatorChar, StringComparison.Ordinal)))
        {
            var text = File.ReadAllText(file);
            foreach (Match m in Regex.Matches(text, @"NativePreviewText\s*\.\s*Text\s*\+?=(?!=)"))
            {
                assignments.Add(Path.GetFileName(file) + " @ " + text[Math.Max(0, m.Index - 160)..m.Index].Replace("\r", " ").Replace("\n", " "));
            }

            if (Path.GetFileName(file) != "MainWindow.xaml.cs" && Path.GetFileName(file) != "MainWindow.g.cs")
            {
                Assert.DoesNotContain("NativePreviewText", text, StringComparison.Ordinal);
            }
        }

        Assert.True(assignments.Count == 1, "the native preview text must be written in exactly one place; found: " + string.Join(" || ", assignments));
        Assert.Contains("RenderNativePreviewText()", assignments[0], StringComparison.Ordinal);
    }

    // ---------------------------------------------------------------------------------------------------------------------------------- item 3: the end of the job

    /// <summary>Codex #116 finding 3: an ask that arrives just BEFORE the job decides it is over gets another pass.</summary>
    [Fact]
    public void AnAskJustBeforeTheJobDecidesItIsOver_GetsAnotherPass()
    {
        var stamp = File.GetLastWriteTimeUtc(_dll);
        var runs = 0;
        PreprocessOracleVerdicts.Runner = _ => Verdict($"run{Interlocked.Increment(ref runs)}");
        using var gap = new VerdictTestWaits.Gap();
        PreprocessOracleVerdicts.BeforeRerunDecision = _ => gap.Hit();
        using var second = new ManualResetEventSlim();
        var announced = 0;
        PreprocessOracleVerdicts.Completed += _ => { if (Interlocked.Increment(ref announced) == 2) second.Set(); };

        Assert.Null(PreprocessOracleVerdicts.TryGet(_dll));
        VerdictTestWaits.Expect(gap.Reached, "the first pass is over and the job has not decided yet", () => $"runs={Volatile.Read(ref runs)}, announced={Volatile.Read(ref announced)}; {PreprocessOracleVerdicts.DescribeState(_dll)}");
        File.WriteAllText(_dll, "q"); File.SetLastWriteTimeUtc(_dll, stamp);
        PreprocessOracleVerdicts.TryGet(_dll);
        gap.Release();

        VerdictTestWaits.Expect(second, "the ask that came before the decision gets its own pass", () => $"runs={Volatile.Read(ref runs)}, announced={Volatile.Read(ref announced)}; {PreprocessOracleVerdicts.DescribeState(_dll)}");
        Assert.Equal("run2", PreprocessOracleVerdicts.TryGet(_dll)!.Status);
        gap.AssertHeldUntilReleased("before the rerun decision");
    }

    /// <summary>
    /// Codex #116 finding 3, exactly: the job has decided that no other pass is wanted and has not yet gone away. An ask that arrives in that gap must not be recorded for a job that is already
    /// leaving (where the job's tidy-up then erased it); it finds no job and starts one of its own.
    /// </summary>
    [Fact]
    public void AnAskJustAfterTheJobDecidedItIsOver_StartsAJobOfItsOwn()
    {
        var stamp = File.GetLastWriteTimeUtc(_dll);
        var runs = 0;
        PreprocessOracleVerdicts.Runner = _ => Verdict($"run{Interlocked.Increment(ref runs)}");
        using var gap = new VerdictTestWaits.Gap();
        PreprocessOracleVerdicts.AfterRerunDecision = _ => gap.Hit();
        using var second = new ManualResetEventSlim();
        var announced = 0;
        PreprocessOracleVerdicts.Completed += _ => { if (Interlocked.Increment(ref announced) == 2) second.Set(); };

        Assert.Null(PreprocessOracleVerdicts.TryGet(_dll));
        VerdictTestWaits.Expect(gap.Reached, "the decision is made and the job is still on its way out", () => $"runs={Volatile.Read(ref runs)}, announced={Volatile.Read(ref announced)}; {PreprocessOracleVerdicts.DescribeState(_dll)}");
        File.WriteAllText(_dll, "q"); File.SetLastWriteTimeUtc(_dll, stamp);
        PreprocessOracleVerdicts.TryGet(_dll);
        gap.Release();

        VerdictTestWaits.Expect(second, "the ask that came after the decision starts a job of its own (it was swallowed by the job that was leaving)", () => $"runs={Volatile.Read(ref runs)}, announced={Volatile.Read(ref announced)}; {PreprocessOracleVerdicts.DescribeState(_dll)}");
        Assert.Equal("run2", PreprocessOracleVerdicts.TryGet(_dll)!.Status);
        gap.AssertHeldUntilReleased("after the rerun decision");
    }

    // ---------------------------------------------------------------------------------------------------------------------------------- item 4: the guard without a window

    /// <summary>
    /// Codex #116 finding 4: the guard is the application's, installed at its startup before any window exists, and nothing a window does removes it. It asks "is this the dispatcher thread" at the
    /// time of each call, so a thread that becomes the UI thread after the guard was installed is covered.
    /// </summary>
    [Fact]
    public void TheGuard_IsInstalledByTheApplicationNotTheWindow_AndDecidesAtCallTime()
    {
        var appDir = FindAppDir();
        var app = File.ReadAllText(Path.Combine(appDir, "App.xaml.cs"));
        var install = app.IndexOf("OracleThreadGuard.OnUiThread = () => Dispatcher.CheckAccess();", StringComparison.Ordinal);
        var window = app.IndexOf("new MainWindow()", StringComparison.Ordinal);
        Assert.True(install >= 0 && window > install, "App.Application_Startup must install the guard before it creates the window");
        Assert.DoesNotContain("OracleThreadGuard", File.ReadAllText(Path.Combine(appDir, "MainWindow.xaml.cs")), StringComparison.Ordinal);

        PreprocessOracleVerdicts.Runner = _ => Verdict("ok");
        var uiThreadId = -1;
        OracleThreadGuard.OnUiThread = () => Environment.CurrentManagedThreadId == Volatile.Read(ref uiThreadId);   // installed first, the "dispatcher thread" appears later
        Exception? onUi = null;
        var thread = new Thread(() =>
        {
            Volatile.Write(ref uiThreadId, Environment.CurrentManagedThreadId);   // a UI entry point that has no window at all
            try { PreprocessOracleVerdicts.Wait(_dll); } catch (Exception ex) { onUi = ex; }
        });
        thread.Start();
        thread.Join();

        Assert.IsType<InvalidOperationException>(onUi);
    }

    // ---------------------------------------------------------------------------------------------------------------------------------- item 2: which modules are the snapshot's

    [Fact]
    public void TheAudit_NamesEveryLoadedModuleThatSharesANameWithASnapshotFileAndIsNotThatFile()
    {
        var snap = Path.Combine(Path.GetTempPath(), "xpe-oracle-snap-1-abc");
        var names = new[] { "xpe_preprocess.dll", "xpe_common.dll", "fmt.dll" };
        var loaded = new[]
        {
            Path.Combine(snap, "xpe_preprocess.dll"),
            Path.Combine(snap, "xpe_common.dll"),
            Path.Combine(snap, "fmt.dll"),
            @"C:\Windows\System32\kernel32.dll",            // a system module: not the snapshot's business
            @"C:\Program Files\dotnet\host\hostfxr.dll",
        };
        Assert.Empty(OracleModuleConfinement.Offenders(loaded, snap, names));

        var decoyed = loaded.Append(@"C:\app\bin\xpe_common.dll").ToArray();   // a module of the snapshot's name that came from the application's folder
        var offenders = OracleModuleConfinement.Offenders(decoyed, snap, names);
        Assert.Equal([@"C:\app\bin\xpe_common.dll"], offenders);

        // the folder test is on a whole path segment: a sibling folder that merely starts with the same text is not "inside"
        Assert.Single(OracleModuleConfinement.Offenders([snap + "-other" + Path.DirectorySeparatorChar + "fmt.dll"], snap, names));
        // names compare without case
        Assert.Single(OracleModuleConfinement.Offenders([@"C:\app\bin\XPE_COMMON.DLL"], snap, names));
    }

    [Fact]
    public void TheWorkerLoadsTheDllConfined_AndAuditsTheModulesItLoaded()
    {
        var worker = File.ReadAllText(Path.Combine(FindAppDir(), "Diagnostics", "XpePreprocessOracleProcess.cs"));
        Assert.Contains("ConfinedLoadFolder: Path.GetDirectoryName(Path.GetFullPath(dllPath))", worker, StringComparison.Ordinal);
        // GUI-C-225b: there is no switch to turn the confinement off, in the worker or anywhere else in the app's sources
        foreach (var file in Directory.EnumerateFiles(FindAppDir(), "*.cs", SearchOption.AllDirectories).Where(f => !f.Contains(Path.DirectorySeparatorChar + "obj" + Path.DirectorySeparatorChar, StringComparison.Ordinal)))
        {
            Assert.DoesNotContain("confineLoad", File.ReadAllText(file), StringComparison.OrdinalIgnoreCase);
        }
        var oracle = File.ReadAllText(Path.Combine(FindAppDir(), "Diagnostics", "XpePreprocessSyntheticOracle.cs"));
        Assert.Contains("OracleModuleConfinement.TryLoad(", oracle, StringComparison.Ordinal);
        Assert.Contains("OracleModuleConfinement.AuditProcess(", oracle, StringComparison.Ordinal);
    }

    private static string BodyOf(string text, string signature)
    {
        var start = text.IndexOf(signature, StringComparison.Ordinal);
        Assert.True(start >= 0, $"{signature} was not found");
        var open = text.IndexOf("{\r\n", start, StringComparison.Ordinal);
        var lineEnd = "\r\n";
        if (open < 0) { open = text.IndexOf("{\n", start, StringComparison.Ordinal); lineEnd = "\n"; }
        var close = text.IndexOf(lineEnd + "        }" + lineEnd, open, StringComparison.Ordinal);
        Assert.True(close > open, $"the end of {signature} was not found");
        return text[open..close];
    }

    private static string FindAppDir()
    {
        for (var dir = new DirectoryInfo(AppContext.BaseDirectory); dir is not null; dir = dir.Parent)
        {
            var candidate = Path.Combine(dir.FullName, "clients", "ImageProcTest");
            if (File.Exists(Path.Combine(candidate, "App.xaml.cs"))) return candidate;
        }

        throw new DirectoryNotFoundException("clients/ImageProcTest was not found above the test output.");
    }
}
