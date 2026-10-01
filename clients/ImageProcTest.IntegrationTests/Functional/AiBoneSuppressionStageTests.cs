// #225 row 10 (GUI-C-184): AI bone suppression — what the stage makes of the module's answer, where the label comes from,
// and where the init/shutdown calls sit.
using ImageProcTest.Models;
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

[Trait("Category", "Functional")]
public sealed class AiBoneSuppressionStageTests
{
    private static StageRequest AiStage => new(StageIds.AiBoneSuppression, true);

    private static ChainResult RunWith(ushort[] raw, StageExecution answer) =>
        ProcessingChainRunner.Run(raw, [AiStage], (_, _) => answer);

    // ---- The rule this stage exists for: a failed call must not read as "ran, nothing to change" ---------------------------

    /// <summary>
    /// A failed worker call leaves the output equal to the input. If that output were taken as the stage's result, the
    /// chain runner would compare equal pixels and record AppliedNoChange. The same pixels with return code 0 ARE
    /// AppliedNoChange; with a failure code they must be RequestedNotApplied, and the display must stay on the raw frame.
    /// </summary>
    [Theory]
    [InlineData(-3, "echo")]
    [InlineData(-4, "echo")]
    [InlineData(-9, "echo")]
    [InlineData(-3, "untouched")]
    [InlineData(-9, "untouched")]
    public void AFailedCall_WhateverItLeftInTheOutput_IsRequestedNotApplied_NotAppliedNoChange(int failureCode, string leftInOutput)
    {
        ushort[] raw = [100, 200, 300, 400];
        // The worker path copies the INPUT into the output on a failure; the in-process path does not touch the output
        // buffer at all (post's QA-B-176 repro). The GUI reads the output only for code 0, so neither can matter.
        var echo = leftInOutput == "echo" ? AiBoneSuppressionStage.ToFloat(raw) : new float[raw.Length];

        var failed = RunWith(raw, AiBoneSuppressionStage.Interpret(failureCode, echo));
        var stage = Assert.Single(failed.Stages);
        Assert.Equal(StageStatus.RequestedNotApplied, stage.Status);
        Assert.Null(stage.Pixels);
        Assert.True(failed.DisplaysRaw);
        Assert.Equal(string.Empty, AiBoneSuppressionStage.LabelFor(failed));

        // The control: the INPUT handed back with return code 0 is the unchanged-image case the runner already knows.
        var succeededUnchanged = RunWith(raw, AiBoneSuppressionStage.Interpret(AiBoneSuppressionStage.Ok, AiBoneSuppressionStage.ToFloat(raw)));
        Assert.Equal(StageStatus.AppliedNoChange, Assert.Single(succeededUnchanged.Stages).Status);
    }

    [Fact]
    public void ASuccessfulCall_ThatChangedThePixels_IsApplied_AndOnlyThenIsLabelled()
    {
        ushort[] raw = [1000, 2000, 3000, 4000];
        var softTissue = AiBoneSuppressionStage.ToFloat(raw).Select(v => v * 0.5f).ToArray();

        var chain = RunWith(raw, AiBoneSuppressionStage.Interpret(AiBoneSuppressionStage.Ok, softTissue));

        var stage = Assert.Single(chain.Stages);
        Assert.Equal(StageStatus.Applied, stage.Status);
        Assert.Equal<ushort[]>([500, 1000, 1500, 2000], stage.Pixels!);
        Assert.Equal("AI-processed: bone suppression", AiBoneSuppressionStage.LabelFor(chain));
    }

    [Fact]
    public void TheLabel_IsNeverGiven_ToAnyStatusButApplied_OrToAnotherStagesSuccess()
    {
        ushort[] raw = [1, 2, 3, 4];
        ChainResult With(StageStatus status) => new(raw, [new StageOutcome(StageIds.AiBoneSuppression, status, null, "x")]);

        foreach (var status in new[] { StageStatus.NotRequested, StageStatus.AppliedNoChange, StageStatus.RequestedNotApplied })
        {
            Assert.Equal(string.Empty, AiBoneSuppressionStage.LabelFor(With(status)));
        }

        Assert.Equal(AiBoneSuppressionStage.ProcessedLabel, AiBoneSuppressionStage.LabelFor(With(StageStatus.Applied)));

        // Another stage being Applied is not the AI stage being Applied.
        var other = new ChainResult(raw,
        [
            new StageOutcome(StageIds.Preprocess, StageStatus.Applied, [9, 9, 9, 9], "x"),
            new StageOutcome(StageIds.AiBoneSuppression, StageStatus.RequestedNotApplied, null, "x"),
        ]);
        Assert.Equal(string.Empty, AiBoneSuppressionStage.LabelFor(other));
    }

    [Fact]
    public void TheSuccessMessage_DoesNotCarryTheLabel()
    {
        // A stage that returned 0 with an unchanged image is AppliedNoChange and shows its Reason on screen; the label
        // must not be in that text, because the label means "the image was processed".
        var answer = AiBoneSuppressionStage.Interpret(AiBoneSuppressionStage.Ok, [0.5f]);
        Assert.True(answer.Ran);
        Assert.DoesNotContain("AI-processed", answer.Message, StringComparison.Ordinal);
    }

    // ---- Reading the return code -------------------------------------------------------------------------------------------

    [Theory]
    [InlineData(0, "Succeeded")]
    [InlineData(-1, "NotAttempted")]
    [InlineData(-6, "NotAttempted")]
    [InlineData(-7, "NotAttempted")]
    [InlineData(-3, "Failed")]
    [InlineData(-4, "Failed")]
    [InlineData(-9, "Failed")]
    public void TheReturnCode_IsClassified(int code, string expected) =>
        Assert.Equal(expected, AiBoneSuppressionStage.Classify(code).ToString());

    [Fact]
    public void NotAttempted_IsSaidApartFromFailed()
    {
        var refused = AiBoneSuppressionStage.Interpret(AiBoneSuppressionStage.UnsupportedFormat, null);
        Assert.False(refused.Ran);
        Assert.Contains("not attempted (code -7", refused.Message, StringComparison.Ordinal);
        Assert.DoesNotContain("NOT applied", refused.Message, StringComparison.Ordinal);

        var failed = AiBoneSuppressionStage.Interpret(AiBoneSuppressionStage.ProcessingFailed, null);
        Assert.False(failed.Ran);
        Assert.Contains("NOT applied (code -3)", failed.Message, StringComparison.Ordinal);
        Assert.DoesNotContain("not attempted", failed.Message, StringComparison.Ordinal);
    }

    /// <summary>
    /// GUI-C-185: the failure message does not claim what a code means. The same -9 comes from a missing model, a worker
    /// executable that was not found and a worker that could not be started, so naming a cause from the code would be a
    /// guess. It reports "code N" as the module gave it.
    /// </summary>
    [Fact]
    public void TheFailureMessage_ReportsTheCode_WithoutClaimingACause()
    {
        foreach (var code in new[] { -3, -9 })
        {
            var message = AiBoneSuppressionStage.Interpret(code, null).Message;
            Assert.Contains($"(code {code})", message, StringComparison.Ordinal);
            Assert.DoesNotContain("inference runtime", message, StringComparison.Ordinal);
            Assert.DoesNotContain("always", message, StringComparison.OrdinalIgnoreCase);
        }
    }

    // ---- The model file is looked for before the module is asked (GUI-C-185) --------------------------------------------

    [Fact]
    public void AMissingModelFile_IsNotAttempted_WithThePathItLookedAt()
    {
        var directory = Path.Combine(Path.GetTempPath(), $"xpe-nomodel-{Guid.NewGuid():N}");
        var answer = AiBoneSuppressionStage.CheckModelFile(directory);

        Assert.NotNull(answer);
        Assert.False(answer!.Ran);
        Assert.Null(answer.Pixels);
        Assert.Contains($"no model at {Path.Combine(directory, "bone_suppress.onnx")}", answer.Message, StringComparison.Ordinal);
        Assert.Contains("not attempted", answer.Message, StringComparison.Ordinal);
        Assert.Contains("no failure was counted", answer.Message, StringComparison.Ordinal);
        Assert.DoesNotContain("(code", answer.Message, StringComparison.Ordinal); // no module was asked, so no code exists
    }

    /// <summary>GUI-C-186: the message names the directory the check really looked at, not the relative text that was typed.</summary>
    [Fact]
    public void TheMessage_PrintsTheAbsolutePath_AARelativeDirectoryResolvesTo()
    {
        var answer = AiBoneSuppressionStage.CheckModelFile(Path.Combine("no-such-models-dir-" + Guid.NewGuid().ToString("N")));

        Assert.NotNull(answer);
        Assert.Contains(Path.GetFullPath("."), answer!.Message, StringComparison.OrdinalIgnoreCase);
        Assert.True(Path.IsPathRooted(answer.Message.Split("no model at ")[1].Split(". ")[0]), answer.Message);
    }

    // ---- A changed model directory is a new session (GUI-C-186) ----------------------------------------------------------

    /// <summary>
    /// xpe_ai_init while initialised returns OK and ignores what it was given (ai.cpp:501-504), so a directory changed in the
    /// settings is seen by the file check and NOT by the module until the session is restarted. The same directory written
    /// another way is not a change.
    /// </summary>
    [Theory]
    [InlineData(null, @"D:\models\a", false)]                    // no session yet: a plain init
    [InlineData(@"D:\models\a", @"D:\models\a", false)]
    [InlineData(@"D:\models\a", @"D:\models\a\", false)]       // a trailing separator is the same place
    [InlineData(@"D:\models\a", @"d:\MODELS\A", false)]        // Windows paths ignore case
    [InlineData(@"D:\models\a", @"D:\models\b", true)]
    [InlineData(@"D:\models\a", @"D:\models\a\..\b", true)]   // resolved before comparing
    public void ANewDirectory_NeedsANewSession_TheSameOneDoesNot(string? started, string requested, bool expected) =>
        Assert.Equal(expected, AiBoneSuppressionStage.NeedsNewSession(started, requested));

    [Fact]
    public void ARelativeDirectory_IsComparedByWhereItResolves()
    {
        Assert.False(AiBoneSuppressionStage.NeedsNewSession("data/models", Path.GetFullPath("data/models")));
        Assert.True(AiBoneSuppressionStage.NeedsNewSession("data/models", "data/other"));
        Assert.Equal(Path.GetFullPath(AiBoneSuppressionStage.DefaultModelDirectory), AiBoneSuppressionStage.NormalizeDirectory("  "));
    }

    /// <summary>The init the GUI calls consults that rule and shuts the old session down under the same lock. Source reading, as the call-site test.</summary>
    [Fact]
    public void TheInit_StartsANewSession_WhenTheDirectoryChanged()
    {
        var runner = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/Services/Native/GuiAiRunner.cs"));
        Assert.Matches(@"Tracker\.NeedsNewSession\(directory\)\)\s*\{\s*Shutdown\(\);", runner);
        Assert.Contains("XpeAiNative.xpe_ai_init(directory,", runner, StringComparison.Ordinal); // the module gets the absolute form
    }

    // ---- Relative directories resolve against a base fixed at startup (GUI-C-186b, decision 2) ------------------------------------

    /// <summary>
    /// The same relative setting means the same absolute path whatever the working directory is, because it is resolved against the
    /// captured base. The first capture wins; a later one changes nothing. (Touches the static base, so it is in this class, whose
    /// tests run one after another, and puts the base back.)
    /// </summary>
    [Fact]
    public void ARelativeDirectory_IsResolvedAgainstTheBaseFixedAtStartup_NotTheWorkingDirectory()
    {
        var baseA = Path.Combine(Path.GetTempPath(), "xpe-base-a");
        var baseB = Path.Combine(Path.GetTempPath(), "xpe-base-b");
        AiBoneSuppressionStage.ResetBaseDirectoryForTests();
        try
        {
            AiBoneSuppressionStage.CaptureBaseDirectory(baseA);
            AiBoneSuppressionStage.CaptureBaseDirectory(baseB); // ignored: the first capture wins
            Assert.False(string.Equals(Path.GetFullPath(Environment.CurrentDirectory), Path.GetFullPath(baseA), StringComparison.OrdinalIgnoreCase),
                "the test needs a base that is not the working directory");

            var first = AiBoneSuppressionStage.NormalizeDirectory("data/models");
            Assert.Equal(Path.Combine(Path.GetFullPath(baseA), "data", "models"), first);
            Assert.Equal(first, AiBoneSuppressionStage.NormalizeDirectory("data/models")); // same setting, same path, every time

            // Same setting after the base: no new session; another directory: a new one.
            Assert.False(AiBoneSuppressionStage.NeedsNewSession(first, "data/models"));
            Assert.True(AiBoneSuppressionStage.NeedsNewSession(first, "data/other"));

            // An absolute directory ignores the base.
            Assert.Equal(Path.GetFullPath(baseB), AiBoneSuppressionStage.NormalizeDirectory(baseB));
        }
        finally
        {
            AiBoneSuppressionStage.ResetBaseDirectoryForTests();
        }
    }

    [Fact]
    public void ExplicitBase_ResolvesWithoutTouchingTheCapturedOne()
    {
        var other = Path.Combine(Path.GetTempPath(), "xpe-explicit-base");
        Assert.Equal(Path.Combine(Path.GetFullPath(other), "data", "models"), AiBoneSuppressionStage.NormalizeDirectory("data/models", other));
    }

    [Fact]
    public void ThePresentModelFile_LetsTheCallThrough()
    {
        var directory = Path.Combine(Path.GetTempPath(), $"xpe-model-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            File.WriteAllText(Path.Combine(directory, "bone_suppress.onnx"), "not a model");
            Assert.Null(AiBoneSuppressionStage.CheckModelFile(directory));

            // A file of another name does not count: the module reads exactly this one.
            File.Move(Path.Combine(directory, "bone_suppress.onnx"), Path.Combine(directory, "other.onnx"));
            Assert.NotNull(AiBoneSuppressionStage.CheckModelFile(directory));
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    // ---- The persistent mark for a worker switched off (GUI-C-185) --------------------------------------------------------

    [Fact]
    public void TheBanner_IsShownOnlyForADisabledWorker_AndCarriesTheModulesNumbers()
    {
        Assert.Equal(string.Empty, AiBoneSuppressionStage.BannerFor(AiWorkerStatus.Unknown));
        Assert.Equal(string.Empty, AiBoneSuppressionStage.BannerFor(new AiWorkerStatus(AiWorkerState.NotUsed, 0, 0)));
        Assert.Equal(string.Empty, AiBoneSuppressionStage.BannerFor(new AiWorkerStatus(AiWorkerState.Active, 2, 3)));

        var disabled = AiBoneSuppressionStage.BannerFor(new AiWorkerStatus(AiWorkerState.Disabled, 3, 3));
        Assert.Contains("switched off", disabled, StringComparison.Ordinal);
        Assert.Contains("3 of 3", disabled, StringComparison.Ordinal);

        // The ceiling is the module's: a module that reports 7 gets 7, with no 3 written anywhere in this text.
        var other = AiBoneSuppressionStage.BannerFor(new AiWorkerStatus(AiWorkerState.Disabled, 7, 7));
        Assert.Contains("7 of 7", other, StringComparison.Ordinal);
        Assert.DoesNotContain("3", other, StringComparison.Ordinal);
    }

    [Fact]
    public void TheMark_GoesWhenTheModuleReportsANewSession()
    {
        // After a restart the module reports the worker active with a clean count; the same function that showed the mark
        // for Disabled shows nothing for that.
        Assert.NotEqual(string.Empty, AiBoneSuppressionStage.BannerFor(new AiWorkerStatus(AiWorkerState.Disabled, 3, 3)));
        Assert.Equal(string.Empty, AiBoneSuppressionStage.BannerFor(new AiWorkerStatus(AiWorkerState.Active, 0, 3)));
    }

    [Theory]
    [InlineData(0, 2, 3u, 3u, "Disabled")]
    [InlineData(0, 1, 1u, 3u, "Active")]
    [InlineData(0, 0, 0u, 0u, "NotUsed")]
    [InlineData(-6, 2, 3u, 3u, "Unknown")]   // not initialised: the outputs are untouched, so they are not read
    [InlineData(0, 9, 0u, 0u, "Unknown")]    // a state this build does not know shows nothing
    public void TheWorkerStateAnswer_IsRead(int code, int state, uint failures, uint ceiling, string expected) =>
        Assert.Equal(expected, AiBoneSuppressionStage.ReadWorkerState(code, state, failures, ceiling).State.ToString());

    // ---- A failed (re)start is an error state with the retry button kept (Codex #24 B1, GUI-C-185b) ------------------------------

    /// <summary>
    /// The sequence the audit named: the worker is switched off, Restart shuts the session down and init fails. The status must
    /// not become Unknown (which shows nothing): it is InitFailed with the reason, the mark and the button stay, and the next
    /// restart that works clears it.
    /// </summary>
    [Fact]
    public void AFailedRestart_LeavesAnErrorState_NotUnknown_AndTheNextSuccessClearsIt()
    {
        var session = new AiSessionTracker();
        session.InitSucceeded(@"D:\models\a");
        Assert.Null(session.OwnStatus()); // started: the module is asked

        session.Stopped();                                      // Restart: the shutdown half
        Assert.Equal(AiWorkerState.Unknown, session.OwnStatus()!.State); // between the halves
        session.InitFailed("xpe_ai_init refused the configuration (code -9).");   // ...and init fails

        var failed = session.OwnStatus()!;
        Assert.Equal(AiWorkerState.InitFailed, failed.State);
        Assert.True(AiBoneSuppressionStage.ShowsMark(failed));
        Assert.Contains("code -9", AiBoneSuppressionStage.BannerFor(failed), StringComparison.Ordinal);
        Assert.Contains("Restart AI", AiBoneSuppressionStage.BannerFor(failed), StringComparison.Ordinal);

        // A second failure replaces the reason, and the state stays.
        session.InitFailed("xpe_ai.dll was not found beside the other native modules.");
        Assert.Contains("was not found", AiBoneSuppressionStage.BannerFor(session.OwnStatus()!), StringComparison.Ordinal);

        session.InitSucceeded(@"D:\models\a");                 // the retry works
        Assert.Null(session.OwnStatus());                       // back to asking the module
        Assert.False(AiBoneSuppressionStage.ShowsMark(new AiWorkerStatus(AiWorkerState.Active, 0, 3)));
    }

    [Fact]
    public void ADeliberateStop_ClearsARecordedFailure_NothingIsRunningAndNothingIsWrong()
    {
        var session = new AiSessionTracker();
        session.InitFailed("xpe_ai_init refused the configuration (code -9).");
        session.Stopped();
        Assert.Equal(AiWorkerState.Unknown, session.OwnStatus()!.State);
        Assert.False(AiBoneSuppressionStage.ShowsMark(session.OwnStatus()!));
    }

    /// <summary>The directory-change path of GUI-C-186 (shutdown, then init) leaves the same state when its init fails.</summary>
    [Fact]
    public void ADirectoryChange_WhoseInitFails_LeavesTheSameErrorState()
    {
        var session = new AiSessionTracker();
        session.InitSucceeded(@"D:\models\a");
        Assert.True(session.NeedsNewSession(@"D:\models\b"));
        session.Stopped();                                     // what Init does first when the directory changed
        session.InitFailed("xpe_ai_init refused the configuration (code -9).");
        Assert.Equal(AiWorkerState.InitFailed, session.OwnStatus()!.State);
        Assert.False(session.NeedsNewSession(@"D:\models\b")); // nothing started, so a plain init is next
    }

    [Fact]
    public void TheStatusLine_CarriesTheModulesNumbers_AndTheReasonForAFailedStart()
    {
        Assert.Equal("worker=Active; failures=0; ceiling=3", AiBoneSuppressionStage.DescribeStatus(new AiWorkerStatus(AiWorkerState.Active, 0, 3)));
        Assert.Equal("worker=Disabled; failures=7; ceiling=7", AiBoneSuppressionStage.DescribeStatus(new AiWorkerStatus(AiWorkerState.Disabled, 7, 7)));
        Assert.Equal("worker=Unknown", AiBoneSuppressionStage.DescribeStatus(AiWorkerStatus.Unknown));
        Assert.Contains("detail=xpe_ai_init refused", AiBoneSuppressionStage.DescribeStatus(
            new AiWorkerStatus(AiWorkerState.InitFailed, 0, 0, "xpe_ai_init refused the configuration (code -9).")), StringComparison.Ordinal);
    }

    /// <summary>
    /// Every way a start can fail is recorded where init is called: the refused code, a missing DLL, a missing export and anything
    /// else it throws. Source reading of this tree, as the call-site test; without these the tracker never hears of the failure and
    /// the status falls back to "unknown".
    /// </summary>
    [Fact]
    public void TheInit_RecordsEveryWayAStartCanFail()
    {
        var runner = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/Services/Native/GuiAiRunner.cs"));
        var init = runner[runner.IndexOf("public static int Init(", StringComparison.Ordinal)..runner.IndexOf("QueryWorkerState()", StringComparison.Ordinal)];
        Assert.Equal(4, System.Text.RegularExpressions.Regex.Matches(init, @"Tracker\.InitFailed\(").Count);
        Assert.Contains("Tracker.InitSucceeded(directory)", init, StringComparison.Ordinal);
        Assert.Contains("Tracker.OwnStatus()", runner, StringComparison.Ordinal);
    }

    /// <summary>The toolbar mark is shown by the "mark visible" rule (worker off OR start failed), not by the worker-off flag alone.</summary>
    [Fact]
    public void TheToolbarMark_FollowsTheMarkVisibleRule()
    {
        var xaml = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/MainWindow.xaml"));
        Assert.Contains("<DataTrigger Binding=\"{Binding AiWorkerMarkVisible}\" Value=\"True\">", xaml, StringComparison.Ordinal);
        Assert.DoesNotContain("<DataTrigger Binding=\"{Binding AiWorkerDisabled}\"", xaml, StringComparison.Ordinal);
    }

    [Fact]
    public void TheRestartAnswer_SaysWhichWay()
    {
        Assert.True(AiBoneSuppressionStage.InterpretRestart(0).Ok);
        var refused = AiBoneSuppressionStage.InterpretRestart(-9);
        Assert.False(refused.Ok);
        Assert.Contains("code -9", refused.Message, StringComparison.Ordinal);
    }

    /// <summary>
    /// The state call, init and shutdown all go through the one lock, and a restart is one step under it. The same source
    /// reading as the call-site test: it sees this tree's text only.
    /// </summary>
    [Fact]
    public void TheRestart_IsOneStepUnderTheLock()
    {
        var runner = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/Services/Native/GuiAiRunner.cs"));
        Assert.Matches(@"public static AiRestartResult Restart\(string modelDirectory\) =>\s*WithLock\(", runner);
        // The status read takes the same gate, bounded (a frame that waits on a worker must not freeze the UI thread behind it),
        // and does its work under the lock once it has it.
        Assert.Contains("Gate.TryWithLock(StateReadWait, ReadWorkerStateLocked", runner, StringComparison.Ordinal);
        Assert.Matches(@"ReadWorkerStateLocked\(\) =>\s*WithLock\(", runner);
    }

    [Fact]
    public void ACodeZero_WithOutputTheChainCannotCarry_IsNotAResult()
    {
        var withNaN = AiBoneSuppressionStage.Interpret(AiBoneSuppressionStage.Ok, [0.1f, float.NaN]);
        Assert.False(withNaN.Ran);
        Assert.Null(withNaN.Pixels);

        var withoutOutput = AiBoneSuppressionStage.Interpret(AiBoneSuppressionStage.Ok, null);
        Assert.False(withoutOutput.Ran);
    }

    /// <summary>
    /// The Native E2E case (C-08) accepts only a reason that carries a return code from xpe_bone_suppress, so a job that
    /// did not stage xpe_ai.dll cannot pass as "the failure path". This pins the module-answer messages to that pattern
    /// and the messages of everything else (DLL not found, init refused) to NOT match it. The pattern is copied into
    /// C-08; the DLL-missing text is copied from GuiAiRunner.cs, which this project cannot link.
    /// </summary>
    [Fact]
    public void TheMessagesOfAModuleAnswer_MatchTheNativeCasePattern()
    {
        var pattern = new System.Text.RegularExpressions.Regex(@"ai_bone_suppress: AI bone suppression (NOT applied|not attempted) \(code -?\d+");

        foreach (var code in new[] { -3, -4, -9, -1, -6, -7 })
        {
            var message = "ai_bone_suppress: " + AiBoneSuppressionStage.Interpret(code, null).Message;
            Assert.Matches(pattern, message);
        }

        Assert.DoesNotMatch(pattern, "ai_bone_suppress: " + AiBoneSuppressionStage.InterpretInit(-9).Message);
        Assert.DoesNotMatch(pattern,
            "ai_bone_suppress: AI bone suppression not started: xpe_ai.dll was not found beside the other native modules; the original image is shown.");
    }

    [Fact]
    public void AnInitFailure_IsSaidAsNotStarted()
    {
        var answer = AiBoneSuppressionStage.InterpretInit(-9);
        Assert.False(answer.Ran);
        Assert.Contains("not started", answer.Message, StringComparison.Ordinal);
        Assert.Contains("code -9", answer.Message, StringComparison.Ordinal);
    }

    // ---- The assumed [0, 1] scale ---------------------------------------------------------------------------------------------

    [Fact]
    public void TheScaleRoundTrip_IsExactForEvery16BitValue()
    {
        var all = Enumerable.Range(0, 65536).Select(i => (ushort)i).ToArray();
        Assert.True(AiBoneSuppressionStage.TryToUInt16(AiBoneSuppressionStage.ToFloat(all), out var back));
        Assert.Equal(all, back);
    }

    [Fact]
    public void ValuesOutsideTheScale_AreClamped()
    {
        Assert.True(AiBoneSuppressionStage.TryToUInt16([-0.5f, 0f, 1f, 1.5f], out var pixels));
        Assert.Equal<ushort[]>([0, 0, 65535, 65535], pixels);
    }

    // ---- The plan and the settings ------------------------------------------------------------------------------------------

    [Fact]
    public void ThePlan_PutsTheAiStageThirdAndOffByDefault()
    {
        var stages = ProcessingChainPlan.BuildStages(new AppSettings());
        Assert.Equal([StageIds.Preprocess, StageIds.Gsvg, StageIds.AiBoneSuppression], stages.Select(s => s.StageId));
        Assert.False(stages[2].Enabled);

        var on = ProcessingChainPlan.BuildStages(new AppSettings { AiBoneSuppressionInChain = true });
        Assert.True(on[2].Enabled);
    }

    [Fact]
    public void TheAiSettings_SurviveACopy_AndAnEmptyModelDirectoryFallsBack()
    {
        var copy = new AppSettings { AiBoneSuppressionInChain = true, AiModelDirectory = @"D:\models\ai" }.Snapshot();
        Assert.True(copy.AiBoneSuppressionInChain);
        Assert.Equal(@"D:\models\ai", copy.AiModelDirectory);

        Assert.Equal("data/models", new AppSettings { AiModelDirectory = "  " }.AiModelDirectory);
    }

    // ---- Where xpe_ai_init and xpe_ai_shutdown are called ----------------------------------------------------------------

    /// <summary>
    /// xpe_ai_worker_state (next card) must not run together with init or shutdown, so the GUI calls them from one place
    /// under one lock. This reads the sources: each call must appear exactly once outside the DllImport declaration, in
    /// GuiAiRunner.cs, inside a <c>WithLock(</c> lambda of its own method. It cannot see a call made by reflection or from
    /// another assembly; the scope is the GUI's own source tree.
    /// </summary>
    [Theory]
    [InlineData("XpeAiNative.xpe_ai_init(")]
    [InlineData("XpeAiNative.xpe_ai_shutdown(")]
    [InlineData("XpeAiNative.xpe_ai_worker_state(")]
    public void InitShutdownAndTheStateCall_AreCalledInOnePlace_UnderTheLock(string call)
    {
        // ResolveRepositoryFile finds FILES; the project file names the directory.
        var root = Path.GetDirectoryName(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/ImageProcTest.csproj"))!;
        var hits = Directory.EnumerateFiles(root, "*.cs", SearchOption.AllDirectories)
            .Where(path => !path.Contains($"{Path.DirectorySeparatorChar}obj{Path.DirectorySeparatorChar}") &&
                           !path.Contains($"{Path.DirectorySeparatorChar}bin{Path.DirectorySeparatorChar}"))
            .SelectMany(path => FindAll(File.ReadAllText(path), call).Select(index => (path, text: File.ReadAllText(path), index)))
            .ToList();

        var hit = Assert.Single(hits);
        Assert.EndsWith("GuiAiRunner.cs", hit.path, StringComparison.Ordinal);
        // Inside the lock: the nearest "WithLock(" before the call is nearer than the start of the method it is in. (A fixed window
        // of characters stopped being enough once the init recorded its failures between the lock and the call.)
        var before = hit.text[..hit.index];
        Assert.True(before.LastIndexOf("WithLock(", StringComparison.Ordinal) > before.LastIndexOf("public static", StringComparison.Ordinal),
            $"{call} is not inside a WithLock( lambda of its own method.");
    }

    private static IEnumerable<int> FindAll(string text, string needle)
    {
        for (var at = text.IndexOf(needle, StringComparison.Ordinal); at >= 0; at = text.IndexOf(needle, at + 1, StringComparison.Ordinal))
        {
            yield return at;
        }
    }
}
