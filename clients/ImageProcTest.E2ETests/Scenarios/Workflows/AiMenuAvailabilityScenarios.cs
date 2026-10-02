// #225 (GUI-C-198, MENU-001 section 8): "Run AI Bone Suppression" is enabled exactly when the backend has an AI module, is initialized and xpe_ai.dll can be found. The expectation is
// derived here, from the runtime line the user sees and from the directory the run pinned, and not from the application's own rule. UI Automation patterns only; no Keyboard, no Mouse.
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;
using static ImageProcTest.E2ETests.Scenarios.Workflows.WorkbenchObservation;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

[Collection(WorkflowApplicationCollection.Name)]
public sealed class AiMenuAvailabilityScenarios(WorkflowApplicationFixture app, ITestOutputHelper output)
{
    private const string ItemId = "RunFullPipelineMenuItem";

    /// <summary>
    /// E-01: the entry's enablement follows the backend and the module. Mock: disabled, with the reason in its tooltip. Native with xpe_ai.dll in the pinned directory: enabled.
    /// Native without it: disabled, and the tooltip names the DLL.
    /// </summary>
    [SkippableFact]
    public void E01_TheEntry_IsEnabledExactlyWhereTheAiModuleCanBeReached()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        var window = app.MainWindow!;
        CloseDetached(window);

        var expected = ExpectedEnabled(window, backendInitialized: true, out var why);
        var (enabled, tip) = ReadItem(window);
        output.WriteLine($"E01 (backend {app.BackendMode}): runtime='{RuntimeSummary(window)}' expected enabled={expected} ({why}); actual enabled={enabled}; tooltip='{tip}'");

        Assert.Equal(expected, enabled);
        if (!enabled)
        {
            Assert.Contains("Disabled now:", tip, StringComparison.Ordinal);
        }
    }

    /// <summary>
    /// E-02: a backend that is shut down has no AI module to ask: the entry goes disabled while it is down and comes back, if it is allowed to, when it is initialized again.
    /// The backend is brought back in the finally block.
    /// </summary>
    [SkippableFact]
    public void E02_AfterShutdown_TheEntryIsDisabled_AndFollowsTheBackendBackUp()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        var window = app.MainWindow!;
        CloseDetached(window);

        var expectedUp = ExpectedEnabled(window, backendInitialized: true, out _);
        try
        {
            Press(window, "BackendMenu", "ShutdownBackendMenuItem");
            var down = PollFor(() => !ReadItem(window).Enabled, TimeSpan.FromSeconds(8));
            var (enabledDown, tipDown) = ReadItem(window);
            output.WriteLine($"E02 after shutdown: enabled={enabledDown}; tooltip='{tipDown}'");
            Assert.True(down && !enabledDown, "The entry is still enabled after the backend was shut down.");
            Assert.Contains("Disabled now:", tipDown, StringComparison.Ordinal);
        }
        finally
        {
            Press(window, "BackendMenu", "InitializeBackendMenuItem");
        }

        var back = PollFor(() => ReadItem(window).Enabled == expectedUp, TimeSpan.FromSeconds(10));
        var (enabledUp, tipUp) = ReadItem(window);
        output.WriteLine($"E02 after initialize: expected enabled={expectedUp}; actual enabled={enabledUp}; tooltip='{tipUp}'");
        Assert.True(back, $"After Initialize the entry should be enabled={expectedUp} (backend {app.BackendMode}) but is {enabledUp}.");
    }

    // The independent expectation: native means the status line says mode=Native AND names where it loaded from (src=), as GUI-C-81 established (the factory falls back to Mock
    // silently, so the requested mode alone proves nothing); the DLL means the file is in the directory this run pinned for the application.
    private bool ExpectedEnabled(Window window, bool backendInitialized, out string why)
    {
        var runtime = RuntimeSummary(window);
        var native = runtime.StartsWith("mode=Native", StringComparison.Ordinal) && runtime.Contains("src=", StringComparison.Ordinal);
        var dll = !string.IsNullOrWhiteSpace(app.NativeDirectory) && File.Exists(Path.Combine(app.NativeDirectory, "xpe_ai.dll"));
        why = $"native backend={native}, xpe_ai.dll in '{app.NativeDirectory}'={dll}, initialized={backendInitialized}";
        return native && dll && backendInitialized;
    }

    private static bool PollFor(Func<bool> condition, TimeSpan limit)
    {
        var until = DateTime.UtcNow + limit;
        while (DateTime.UtcNow < until)
        {
            if (condition()) return true;
            Thread.Sleep(150);
        }

        return condition();
    }

    private static (bool Enabled, string Tip) ReadItem(Window window)
    {
        UiaMenu.Open(window, "PipelineMenu");
        try
        {
            AutomationElement? item = null;
            for (var attempt = 0; attempt < 20 && item is null; attempt++)
            {
                item = window.FindFirstDescendant(cf => cf.ByAutomationId(ItemId));
                if (item is null) Thread.Sleep(100);
            }

            Assert.True(item is not null, $"{ItemId} did not appear under PipelineMenu.");
            return (item!.IsEnabled, item.HelpText ?? string.Empty);
        }
        finally
        {
            UiaMenu.Close();
        }
    }

    private static void Press(Window window, string menuId, string itemId)
    {
        UiaMenu.Open(window, menuId);
        try
        {
            AutomationElement? item = null;
            for (var attempt = 0; attempt < 20 && item is null; attempt++)
            {
                item = window.FindFirstDescendant(cf => cf.ByAutomationId(itemId));
                if (item is null) Thread.Sleep(100);
            }

            Assert.True(item is not null, $"{itemId} did not appear under {menuId}.");
            item!.AsMenuItem().Invoke();
        }
        finally
        {
            UiaMenu.Close();
        }
    }
}
