// #225 row 10 (GUI-C-186b, Codex #25): one AI frame is one hold of the session gate; the directory is resolved once.
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// The real <see cref="AiFrame"/> and <see cref="AiSessionGate"/> with fake operations in place of the DLL: no native code runs,
/// so the order and the locking of the frame are what is tested, not the module.
/// </summary>
[Trait("Category", "Functional")]
public sealed class AiFrameTests
{
    private static readonly string Directory_ = Path.Combine(Path.GetTempPath(), "xpe-frame-tests", "models");
    private static readonly TimeSpan Short = TimeSpan.FromMilliseconds(300);
    private static readonly TimeSpan Long = TimeSpan.FromSeconds(10);

    private sealed class FakeOps(Func<string, bool>? exists = null, int initCode = 0, Func<AiSuppressResult>? suppress = null) : IAiFrameOps
    {
        public readonly List<string> Calls = [];
        public string? InitDirectory;
        public string? CheckedPath;

        public bool ModelFileExists(string path)
        {
            lock (Calls) Calls.Add("exists");
            CheckedPath = path;
            return exists?.Invoke(path) ?? true;
        }

        public int InitSession(string absoluteDirectory)
        {
            lock (Calls) Calls.Add("init");
            InitDirectory = absoluteDirectory;
            return initCode;
        }

        public AiSuppressResult Suppress()
        {
            lock (Calls) Calls.Add("suppress");
            return suppress?.Invoke() ?? new AiSuppressResult(AiBoneSuppressionStage.Ok, [0.5f]);
        }
    }

    // ---- Order ----------------------------------------------------------------------------------------------------------------

    [Fact]
    public void TheFrame_ChecksTheFile_ThenInits_ThenCalls()
    {
        var ops = new FakeOps();
        var answer = AiFrame.Run(new AiSessionGate(), ops, Directory_);

        Assert.Equal(["exists", "init", "suppress"], ops.Calls);
        Assert.True(answer.Ran);
    }

    [Fact]
    public void AMissingModel_StopsTheFrame_BeforeTheSessionIsTouched()
    {
        var ops = new FakeOps(exists: _ => false);
        var answer = AiFrame.Run(new AiSessionGate(), ops, Directory_);

        Assert.Equal(["exists"], ops.Calls); // no init, so no worker, and no failure counted
        Assert.False(answer.Ran);
        Assert.Contains("not attempted: no model at", answer.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void AnInitThatFails_StopsTheFrame_BeforeTheCall()
    {
        var ops = new FakeOps(initCode: -9);
        var answer = AiFrame.Run(new AiSessionGate(), ops, Directory_);

        Assert.Equal(["exists", "init"], ops.Calls);
        Assert.False(answer.Ran);
        Assert.Contains("not started", answer.Message, StringComparison.Ordinal);
        Assert.Contains("code -9", answer.Message, StringComparison.Ordinal);
    }

    // ---- One string for the check and the init (GUI-C-186b, decision 2) -------------------------------------------------------

    [Fact]
    public void TheCheckAndTheInit_ReceiveTheSameDirectoryString()
    {
        var ops = new FakeOps();
        AiFrame.Run(new AiSessionGate(), ops, Directory_);

        Assert.Equal(Directory_, ops.InitDirectory);
        Assert.Equal(Path.Combine(Directory_, "bone_suppress.onnx"), ops.CheckedPath);
        Assert.Equal(ops.InitDirectory, Path.GetDirectoryName(ops.CheckedPath));
    }

    [Fact]
    public void TheFrame_RefusesARelativeDirectory_ItIsResolvedOncePerFrameByTheCaller()
    {
        var ops = new FakeOps();
        Assert.Throws<ArgumentException>(() => AiFrame.Run(new AiSessionGate(), ops, Path.Combine("data", "models")));
        Assert.Empty(ops.Calls);
    }

    // ---- The whole frame is one hold of the gate (Codex #25 finding 1) ---------------------------------------------------------

    /// <summary>
    /// Frame A stops inside the call, after its init. While it is there another thread tries what a Restart or a directory change
    /// does (shutdown, then init). It must wait for A to finish: before the fix the gate ended with A's init, and the replacement
    /// could happen between A's init and A's call.
    /// </summary>
    [Fact]
    public void ARestart_AttemptedWhileAFrameIsInTheCall_WaitsForTheFrame()
    {
        var gate = new AiSessionGate();
        var log = new List<string>();
        var inCall = new ManualResetEventSlim();
        var releaseCall = new ManualResetEventSlim();
        var ops = new FakeOps(suppress: () =>
        {
            lock (log) log.Add("A: in the call");
            inCall.Set();
            Assert.True(releaseCall.Wait(Long), "frame A was never released");
            lock (log) log.Add("A: call returned");
            return new AiSuppressResult(AiBoneSuppressionStage.Ok, [0.5f]);
        });

        var frame = Task.Run(() => AiFrame.Run(gate, ops, Directory_));
        Assert.True(inCall.Wait(Long), "frame A never reached the call");

        var restartDone = new ManualResetEventSlim();
        var restart = Task.Run(() =>
        {
            gate.WithLock(() =>
            {
                lock (log) log.Add("B: shutdown + init");
                return 0;
            });
            restartDone.Set();
        });

        Assert.False(restartDone.Wait(Short), "the restart got in while frame A was inside its call");

        releaseCall.Set();
        Assert.True(frame.Wait(Long));
        Assert.True(restart.Wait(Long));

        Assert.Equal(["A: in the call", "A: call returned", "B: shutdown + init"], log);
    }

    /// <summary>The same for a second frame whose directory differs: its init (which would replace the session) comes after the first frame's call.</summary>
    [Fact]
    public void ASecondFrameWithAnotherDirectory_InitsOnlyAfterTheFirstFrameIsDone()
    {
        var gate = new AiSessionGate();
        var log = new List<string>();
        var inCall = new ManualResetEventSlim();
        var releaseCall = new ManualResetEventSlim();
        var first = new FakeOps(suppress: () =>
        {
            inCall.Set();
            Assert.True(releaseCall.Wait(Long));
            lock (log) log.Add("first: call returned");
            return new AiSuppressResult(AiBoneSuppressionStage.Ok, [0.5f]);
        });
        var second = new SecondOps(log);

        var a = Task.Run(() => AiFrame.Run(gate, first, Directory_));
        Assert.True(inCall.Wait(Long));
        var b = Task.Run(() => AiFrame.Run(gate, second, Path.Combine(Path.GetTempPath(), "xpe-frame-tests", "other")));

        Assert.False(b.Wait(Short), "the second frame got through while the first was inside its call");
        releaseCall.Set();
        Assert.True(a.Wait(Long) && b.Wait(Long));
        Assert.Equal(["first: call returned", "second: init"], log.Take(2));
    }

    private sealed class SecondOps(List<string> log) : IAiFrameOps
    {
        public bool ModelFileExists(string path) => true;

        public int InitSession(string absoluteDirectory)
        {
            lock (log) log.Add("second: init");
            return 0;
        }

        public AiSuppressResult Suppress() => new(AiBoneSuppressionStage.Ok, [0.5f]);
    }

    /// <summary>
    /// The status read shares the gate and waits for a running frame with no limit (GUI-C-186d: it runs in the background, so the
    /// wait costs the UI thread nothing); it gets through as soon as the real <see cref="AiFrame"/> is done.
    /// </summary>
    [Fact]
    public void TheStatusRead_WaitsForARunningFrame_AndGetsThroughWhenItIsDone()
    {
        var gate = new AiSessionGate();
        var inCall = new ManualResetEventSlim();
        var releaseCall = new ManualResetEventSlim();
        var ops = new FakeOps(suppress: () =>
        {
            inCall.Set();
            Assert.True(releaseCall.Wait(Long));
            return new AiSuppressResult(AiBoneSuppressionStage.Ok, [0.5f]);
        });
        var frame = Task.Run(() => AiFrame.Run(gate, ops, Directory_));
        Assert.True(inCall.Wait(Long));

        var read = Task.Run(() => gate.WithLock(() => 7));
        Assert.False(read.Wait(Short), "the read got through while the frame held the gate");

        releaseCall.Set();
        Assert.True(frame.Wait(Long));
        Assert.True(read.Wait(Long));
        Assert.Equal(7, read.Result);
    }

    [Fact]
    public void TheGate_IsReentrant_ARestartCanInitUnderItsOwnHold()
    {
        var gate = new AiSessionGate();
        var result = gate.WithLock(() => gate.WithLock(() => gate.WithLock(() => 3)));
        Assert.Equal(3, result);
    }

    // ---- Where the real code puts the call (source reading of this tree) -----------------------------------------------------

    [Fact]
    public void TheRealRunner_GivesTheFrameTheOneGate_AndResolvesTheDirectoryOnce()
    {
        var runner = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/Services/Native/GuiAiRunner.cs"));
        var run = runner[runner.IndexOf("public static StageExecution Run(", StringComparison.Ordinal)..runner.IndexOf("private sealed class RealAiFrameOps", StringComparison.Ordinal)];

        Assert.Contains("AiFrame.Run(GuiAiSession.Gate, ops, directory)", run, StringComparison.Ordinal);
        Assert.Single(System.Text.RegularExpressions.Regex.Matches(run, @"NormalizeDirectory\("));

        // xpe_bone_suppress is called in exactly one place, inside the real operations the frame invokes under the gate.
        var calls = System.Text.RegularExpressions.Regex.Matches(runner, @"XpeAiNative\.xpe_bone_suppress\(");
        var only = Assert.Single(calls);
        Assert.True(runner[..only.Index].LastIndexOf("class RealAiFrameOps", StringComparison.Ordinal) >= 0);
        Assert.True(runner[..only.Index].LastIndexOf("class RealAiFrameOps", StringComparison.Ordinal) >
                    runner[..only.Index].LastIndexOf("internal static class GuiAiRunner", StringComparison.Ordinal));
    }

    [Fact]
    public void TheApplication_FixesTheBaseDirectoryAtStartup()
    {
        var app = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/App.xaml.cs"));
        var startup = app[app.IndexOf("protected override void OnStartup", StringComparison.Ordinal)..];
        var install = startup.IndexOf("GuiNativeLibraryResolver.Install();", StringComparison.Ordinal);
        var capture = startup.IndexOf("AiBoneSuppressionStage.CaptureBaseDirectory();", StringComparison.Ordinal);
        Assert.True(install >= 0 && capture > install, "OnStartup does not capture the base directory right after the resolver is installed.");
        Assert.True(capture < startup.IndexOf("AutomationArgs.Parse(", StringComparison.Ordinal), "the base is captured after the command line was parsed");
    }
}
