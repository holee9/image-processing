// #175 HAZ-GUI-005 (GUI-C-82): a Mock backend must never look like the native one.
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;
using static ImageProcTest.E2ETests.Scenarios.Workflows.WorkbenchObservation;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// E-01 made real: Native requested, the native DLLs absent, so the factory falls back to Mock without a
/// word (<c>XpeBackendFactory.Create</c>). GUI-C-81 measured the status bar reading <c>mode=Native</c> in
/// exactly this state. Each fact below maps to one HAZ-GUI-005 control and is falsified on its own.
/// </summary>
[Collection(MockFallbackApplicationCollection.Name)]
public sealed class MockFallbackScenarios(MockFallbackApplicationFixture app, ITestOutputHelper output)
{
    /// <summary>E-01a: the status bar names the backend that actually runs, and the request beside it.</summary>
    [SkippableFact]
    public void E01a_Fallback_StatusBarNamesTheActualBackend()
    {
        var window = Ready();
        var summary = RuntimeSummary(window);
        output.WriteLine($"E01a status-bar='{summary}'");
        Assert.True(
            summary.StartsWith("mode=Mock (requested Native)", StringComparison.Ordinal),
            $"Native was requested and the DLL directory is empty, but the status bar reads '{summary}' (#175).");
    }

    /// <summary>
    /// E-01b: the warning banner is shown and says the requested backend was not used.
    ///
    /// <para>"Not dismissible" is by construction — the banner is a Border holding one TextBlock, with no
    /// control — and is not asserted here: the Border has no automation peer, so there is no subtree to
    /// search for a close button.</para>
    /// </summary>
    [SkippableFact]
    public void E01b_Fallback_ShowsAPersistentBanner()
    {
        var window = Ready();
        var text = MockBannerText(window);
        output.WriteLine($"E01b banner text='{text}'");
        Assert.True(
            text is not null && text.Contains("Native was requested", StringComparison.Ordinal),
            $"The Mock backend is running and the warning banner reads '{text ?? "(absent)"}' (#175).");
    }

    /// <summary>E-01c: <c>[MOCK]</c> in the title, of the main window and of the detached viewer.</summary>
    [SkippableFact]
    public void E01c_Fallback_TitlesSayMock()
    {
        var window = Ready();
        output.WriteLine($"E01c main title='{window.Title}'");
        Assert.StartsWith("[MOCK] ", window.Title, StringComparison.Ordinal);

        CloseDetached(window);
        var detached = OpenDetached(window);
        try
        {
            var title = detached.Name;
            output.WriteLine($"E01c detached title='{title}' banner='{DetachedMockBannerText(detached)}'");
            Assert.StartsWith("[MOCK] ", title, StringComparison.Ordinal);
            Assert.True(DetachedMockBannerText(detached) is not null, "The detached viewer shows the same images with no Mock banner.");
        }
        finally
        {
            try { detached.AsWindow().Close(); } catch (Exception) { /* reported by the next case */ }
            Thread.Sleep(400);
        }
    }

    private Window Ready()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        return app.MainWindow!;
    }
}

/// <summary>
/// E-01 control: the ordinary app, whichever backend the suite requested.
///
/// <para>Under Native this is the case that stops an always-on banner from passing: a native backend
/// that loaded shows no banner, no <c>[MOCK]</c>, and no "requested" note. Under Mock it records the
/// decision for an intentional Mock run (#175): HAZ-GUI-005 says "Mock mode", so the warning is shown
/// there too — without the "requested" note, because nothing was substituted.</para>
/// </summary>
[Collection(WorkflowApplicationCollection.Name)]
public sealed class BackendDisclosureControlScenarios(WorkflowApplicationFixture app, ITestOutputHelper output)
{
    [SkippableFact]
    public void E01d_OrdinaryLaunch_DisclosesExactlyTheRequestedBackend()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        var window = app.MainWindow!;
        var summary = RuntimeSummary(window);
        var banner = MockBannerText(window);
        output.WriteLine($"E01d requested={app.BackendMode} title='{window.Title}' status-bar='{summary}' banner='{banner}'");

        Assert.DoesNotContain("requested", summary, StringComparison.Ordinal);
        Assert.StartsWith($"mode={app.BackendMode}", summary, StringComparison.Ordinal);

        if (app.BackendMode == "Native")
        {
            Assert.True(banner is null, $"The native backend loaded, yet the Mock banner reads '{banner}'.");
            Assert.False(window.Title.Contains("[MOCK]", StringComparison.Ordinal), $"The native backend loaded, yet the title is '{window.Title}'.");
        }
        else
        {
            Assert.True(banner is not null, "An intentional Mock run shows no Mock banner.");
            Assert.StartsWith("[MOCK] ", window.Title, StringComparison.Ordinal);
        }
    }
}

/// <summary>
/// Native requested with an empty native directory — the fallback, whatever backend the suite runs.
/// </summary>
public sealed class MockFallbackApplicationFixture : ApplicationFixture
{
    public MockFallbackApplicationFixture()
        : base(@"fixtures\gui-s0\raw\synthetic_1024x1024.raw", EmptyNativeDirectory())
    {
    }

    private static DirectoryInfo EmptyNativeDirectory()
    {
        var dir = new DirectoryInfo(Path.Combine(Path.GetTempPath(), $"xpe-e2e-empty-native-{Environment.ProcessId}"));
        dir.Create();
        foreach (var file in dir.GetFiles()) file.Delete();
        return dir;
    }
}

[CollectionDefinition(Name)]
public sealed class MockFallbackApplicationCollection : ICollectionFixture<MockFallbackApplicationFixture>
{
    public const string Name = "gui-mock-fallback-application";
}
