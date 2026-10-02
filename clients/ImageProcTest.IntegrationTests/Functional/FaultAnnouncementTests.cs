// #225 (GUI-C-192): the loud "fault injection armed" line says which fault is armed.
namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// The announcement is the line an operator reads to learn that this app is not a normal one. It described the display fault only; with
/// <c>--automation-fault ai-worker-disabled</c> alone it claimed that display calls throw, which is false there. A source reading: it sees
/// this tree's text, not a running app (the window title marker is observed by E2E W-23).
/// </summary>
[Trait("Category", "Functional")]
public sealed class FaultAnnouncementTests
{
    private static string AnnounceBody()
    {
        var source = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/ViewModels/MainWindowViewModel.cs"));
        var start = source.IndexOf("public void AnnounceFaultInjection()", StringComparison.Ordinal);
        Assert.True(start >= 0, "AnnounceFaultInjection was not found.");
        return source[start..source.IndexOf("_faultInjectionAnnounced = true;", start, StringComparison.Ordinal)];
    }

    [Fact]
    public void TheDisplayFaultSentence_IsOnlyWrittenWhenTheDisplayFaultIsArmed()
    {
        var body = AnnounceBody();

        Assert.Contains("armed.Contains(AutomationArgs.DisplayPipelineFaultPrefix, StringComparison.Ordinal) ? \" Display pipeline calls past the limit throw on purpose.\"", body, StringComparison.Ordinal);
    }

    [Fact]
    public void TheAiWorkerSentence_IsWrittenWhenThatFaultIsArmed()
    {
        var body = AnnounceBody();

        Assert.Contains("armed.Contains(AutomationArgs.AiWorkerDisabledFault, StringComparison.Ordinal)", body, StringComparison.Ordinal);
        Assert.Contains("switched off, 3 of 3", body, StringComparison.Ordinal);
    }

    [Fact]
    public void NoUnconditionalDisplayFaultSentence_RemainsInTheAnnouncement()
    {
        var body = AnnounceBody();

        Assert.DoesNotContain("{FaultInjectionStatus}. Display pipeline calls past the limit throw on purpose.\"", body, StringComparison.Ordinal);
    }
}
