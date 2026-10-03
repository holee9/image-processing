// #225 / #130 (GUI-C-202): C-09 on an app whose native directory has no xpe_ai_worker.exe. UI Automation patterns only.
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

[Collection(AiWorkerAbsentApplicationCollection.Name)]
public sealed class AiWorkerAbsentScenarios(AiWorkerAbsentApplicationFixture app, ITestOutputHelper output)
{
    /// <summary>
    /// C-09 (moved here by GUI-C-202): the worker cannot be started, so each bone suppression call fails and the module counts it; after the ceiling the persistent mark shows with the
    /// module's own numbers, and Restart AI removes it. The scenario itself is <see cref="ProcessingChainScenarios.RunWorkerSwitchedOffScenario"/>. Native only, and only where the
    /// run's native directory holds xpe_ai.dll with its worker (the copy is made without the worker).
    /// </summary>
    [SkippableFact]
    public void C09_AWorkerThatCannotBeStarted_IsSwitchedOffByRepeatedFailures_ShowsAMark_ThatRestartRemoves()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        Skip.If(!app.Prepared, app.PrepareNote);
        var window = app.MainWindow!;
        var runtime = WorkbenchObservation.RuntimeSummary(window);
        Skip.IfNot(runtime.StartsWith("mode=Native", StringComparison.Ordinal) && runtime.Contains("src=", StringComparison.Ordinal),
            $"the app did not come up on the native backend ('{runtime}').");
        output.WriteLine($"C09 setup: {app.PrepareNote}; runtime='{runtime}'");

        ProcessingChainScenarios.RunWorkerSwitchedOffScenario(window, output);
    }

    /// <summary>
    /// C-11a (GUI-C-222, #254; the C-11 of GUI-C-205 moved here): the clean-up helper on a worker switched off. The state is made on this app, where every call fails and is counted; the module
    /// no longer counts the refusal the shared app used to make it with (QA-B-195, -4). The premise is asserted, not skipped on.
    /// </summary>
    [SkippableFact]
    public void C11a_RestoringTheAiSession_RemovesTheMarkAndTheCount_OfAWorkerThatWasSwitchedOff()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        Skip.If(!app.Prepared, app.PrepareNote);
        var window = app.MainWindow!;
        var runtime = WorkbenchObservation.RuntimeSummary(window);
        Skip.IfNot(runtime.StartsWith("mode=Native", StringComparison.Ordinal) && runtime.Contains("src=", StringComparison.Ordinal),
            $"the app did not come up on the native backend ('{runtime}').");

        ProcessingChainScenarios.RunRestoreAfterTheMark(window, output);
    }

    /// <summary>C-11b (GUI-C-222): the helper's other branch: failures counted, ceiling not reached, no mark. One and two counted failures.</summary>
    [SkippableFact]
    public void C11b_RestoringTheAiSession_PutsACountBackToZero_WhenNoMarkWasShown()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        Skip.If(!app.Prepared, app.PrepareNote);
        var window = app.MainWindow!;
        var runtime = WorkbenchObservation.RuntimeSummary(window);
        Skip.IfNot(runtime.StartsWith("mode=Native", StringComparison.Ordinal) && runtime.Contains("src=", StringComparison.Ordinal),
            $"the app did not come up on the native backend ('{runtime}').");

        ProcessingChainScenarios.RunRestoreAfterACountWithoutTheMark(window, output);
    }
}
