// #225 (GUI-C-198, Codex #78 findings 2 and 3). UI Automation patterns only; no Keyboard, no Mouse.
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;
using static ImageProcTest.E2ETests.Scenarios.Workflows.WorkbenchObservation;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// L-01 (finding 2): the app launched with a test fault wraps its backend in <c>FaultInjectingBackend</c>, which implements the AI session interface for every backend it wraps. The
/// entry must still follow what the WRAPPED backend has: disabled over the Mock, enabled over a native backend with xpe_ai.dll. The expectation is derived as in E-01, from the runtime line
/// and the pinned directory.
/// </summary>
[Collection(AiWorkerDisabledApplicationCollection.Name)]
public sealed class AiMenuWrappedBackendScenarios(AiWorkerDisabledApplicationFixture app, ITestOutputHelper output)
{
    [SkippableFact]
    public void L01_TheEntry_FollowsTheWrappedBackend_NotTheWrappersInterface()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        var window = app.MainWindow!;
        CloseDetached(window);

        var runtime = RuntimeSummary(window);
        var native = runtime.StartsWith("mode=Native", StringComparison.Ordinal) && runtime.Contains("src=", StringComparison.Ordinal);
        var dll = !string.IsNullOrWhiteSpace(app.NativeDirectory) && File.Exists(Path.Combine(app.NativeDirectory, "xpe_ai.dll"));
        var expected = native && dll;
        var (enabled, tip) = AiMenuAvailabilityScenarios.ReadItem(window);
        output.WriteLine($"L01 (backend {app.BackendMode}, wrapped by the fault seam): runtime='{runtime}' expected enabled={expected}; actual enabled={enabled}; title='{window.Title}'");

        Assert.Contains("FAULT INJECTION ARMED", window.Title, StringComparison.Ordinal);   // the wrapper really is in place
        Assert.Equal(expected, enabled);
        if (!enabled)
        {
            Assert.Contains("Disabled now:", tip, StringComparison.Ordinal);
        }
    }
}

/// <summary>
/// L-02 (finding 3): the DLL's presence is read again each time the Pipeline menu opens. The app is pinned to a private copy of the native directory without xpe_ai.dll: the entry is
/// disabled; the DLL is copied in while the app runs and the next opening shows it enabled; it is deleted again and the next opening shows it disabled. (The DLL is never loaded
/// by this scenario: nothing runs the module, so the file is free to delete.)
/// </summary>
[Collection(AiDllLateApplicationCollection.Name)]
public sealed class AiMenuLateDllScenarios(AiDllLateApplicationFixture app, ITestOutputHelper output)
{
    [SkippableFact]
    public void L02_ADllThatAppearsOrVanishesWhileTheAppRuns_IsSeenAtTheNextMenuOpening()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        Skip.If(app.SourceAiDll is null, app.PrepareNote);
        var window = app.MainWindow!;
        CloseDetached(window);

        var runtime = RuntimeSummary(window);
        Skip.IfNot(runtime.StartsWith("mode=Native", StringComparison.Ordinal) && runtime.Contains("src=", StringComparison.Ordinal),
            $"the app did not come up on the native backend ('{runtime}'), so the file's presence is not what decides the entry.");

        var target = Path.Combine(app.PrivateDirectory.FullName, "xpe_ai.dll");
        Assert.False(File.Exists(target), "the private directory must start without xpe_ai.dll");
        try
        {
            var (before, _) = AiMenuAvailabilityScenarios.ReadItem(window);
            File.Copy(app.SourceAiDll!, target);
            var (after, _) = AiMenuAvailabilityScenarios.ReadItem(window);
            File.Delete(target);
            var (removed, tip) = AiMenuAvailabilityScenarios.ReadItem(window);
            output.WriteLine($"L02: enabled before the DLL exists={before}; after it was copied in={after}; after it was deleted={removed}; tooltip='{tip}'");

            Assert.False(before, "the entry is enabled with no xpe_ai.dll in the pinned directory");
            Assert.True(after, "a DLL that appeared while the app ran was not seen when the menu opened");
            Assert.False(removed, "a DLL that was removed while the app ran is still counted when the menu opens");
            Assert.Contains("xpe_ai.dll", tip, StringComparison.Ordinal);
        }
        finally
        {
            try { File.Delete(target); } catch (IOException) { }
        }
    }
}
