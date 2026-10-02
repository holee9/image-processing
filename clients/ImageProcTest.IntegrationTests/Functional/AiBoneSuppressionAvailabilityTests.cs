// #225 (GUI-C-198): "Run AI Bone Suppression" is usable only where the AI module can be reached (MENU-001 section 8). The rule itself, and the wiring that keeps the
// menu entry, the command and the tooltip on that one rule.
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

[Trait("Category", "Functional")]
public sealed class AiBoneSuppressionAvailabilityTests
{
    [Theory]
    [InlineData(true, true, false, true, true)]    // native session, initialized, idle, DLL found: the only enabled state
    [InlineData(false, true, false, true, false)]  // Mock: no AI session
    [InlineData(true, false, false, true, false)]  // shut down / not initialized
    [InlineData(true, true, true, true, false)]    // starting or stopping
    [InlineData(true, true, false, false, false)]  // xpe_ai.dll missing
    [InlineData(false, false, false, false, false)]
    public void TheEntryIsEnabledExactlyWhenEveryFactHolds(bool hasSession, bool initialized, bool transitioning, bool dll, bool expected)
    {
        var availability = AiBoneSuppressionAvailability.Evaluate(hasSession, initialized, transitioning, dll);

        Assert.Equal(expected, availability.CanRun);
        Assert.Equal(expected, availability.Reason.Length == 0);
    }

    [Fact]
    public void EveryDisabledStateNamesItsOwnReason()
    {
        var reasons = new[]
        {
            AiBoneSuppressionAvailability.Evaluate(false, true, false, true).Reason,   // Mock
            AiBoneSuppressionAvailability.Evaluate(true, false, false, true).Reason,   // not initialized
            AiBoneSuppressionAvailability.Evaluate(true, true, true, true).Reason,     // transitioning
            AiBoneSuppressionAvailability.Evaluate(true, true, false, false).Reason,   // DLL missing
        };

        Assert.All(reasons, reason => Assert.False(string.IsNullOrWhiteSpace(reason)));
        Assert.Equal(reasons.Length, reasons.Distinct(StringComparer.Ordinal).Count());
        Assert.Contains("Mock", reasons[0], StringComparison.Ordinal);
        Assert.Contains("xpe_ai.dll", reasons[3], StringComparison.Ordinal);
    }

    [Fact]
    public void TheCommandCarriesTheRule_TheHandlerRefusesForItself_AndTheMenuAsksAgainWhenTheBackendMoves()
    {
        var code = Read("gui/ImageProcTest/ViewModels/MainWindowViewModel.cs");

        // canExecute is the availability rule, not a constant and not a second copy of the conditions.
        Assert.Contains("new RelayCommand(RunAiBoneSuppression, () => AiBoneSuppressionAvailability.CanRun)", code, StringComparison.Ordinal);

        // The handler asks the same rule first: a command can be executed without the menu (the self-run), and a refusal there is on the status line, not silence.
        var handler = Between(code, "private async void RunAiBoneSuppression()", "if (ActiveImageFrame is null)");
        Assert.Contains("if (!AiBoneSuppressionAvailability.CanRun)", handler, StringComparison.Ordinal);
        Assert.Contains("StatusText = ", handler, StringComparison.Ordinal);

        // The facts it reads come from the backend itself (a session interface, the runtime state), not from the requested mode.
        var property = Between(code, "public AiBoneSuppressionAvailability AiBoneSuppressionAvailability =>", "public string AiBoneSuppressionMenuToolTip");
        Assert.Contains("_backend is IAiSessionBackend", property, StringComparison.Ordinal);
        Assert.Contains("RuntimeInfo.State", property, StringComparison.Ordinal);
        Assert.Contains("Lifecycle.IsTransitioning", property, StringComparison.Ordinal);
        Assert.DoesNotContain("BackendMode", property, StringComparison.Ordinal);

        // The menu is told to ask again at each change of the backend's state: the runtime info, a shutdown's start and its end.
        Assert.Contains("RefreshAiBoneSuppressionAvailability();", Between(code, "public BackendRuntimeInfo RuntimeInfo", "// #175 HAZ-GUI-005"), StringComparison.Ordinal);
        Assert.Contains("RefreshAiBoneSuppressionAvailability();", Between(code, "Log(\"Backend shutdown requested.\");", "else"), StringComparison.Ordinal);
        Assert.Contains("RefreshAiBoneSuppressionAvailability();", Between(code, "private void FinishShutdown(", "StatusText = error is null"), StringComparison.Ordinal);
        Assert.Contains("RunAiBoneSuppressionCommand.RaiseCanExecuteChanged();", code, StringComparison.Ordinal);
    }

    [Fact]
    public void TheMenuEntryIsBoundToTheCommand_ItsTooltipToTheReason_AndItIsNotOnTheDisabledList()
    {
        var xaml = Read("gui/ImageProcTest/MainWindow.xaml");
        var item = Between(xaml, "x:Name=\"RunFullPipelineMenuItem\"", "/>");
        Assert.Contains("Command=\"{Binding RunAiBoneSuppressionCommand}\"", item, StringComparison.Ordinal);
        // The item is also bound to the rule (as the baseline entry is): a menu item that has not been loaded yet keeps a command's CanExecute unread, so the binding is what makes the
        // enabled state true before the menu is first opened.
        Assert.Contains("IsEnabled=\"{Binding AiBoneSuppressionAvailability.CanRun, Mode=OneWay}\"", item, StringComparison.Ordinal);
        Assert.Contains("ToolTip=\"{Binding AiBoneSuppressionMenuToolTip}\"", item, StringComparison.Ordinal);
        Assert.DoesNotContain("IsEnabled=\"False\"", item, StringComparison.Ordinal);

        // It is a command that is sometimes unavailable, not a placeholder: it stays out of the "not built yet" name list.
        var report = Read("gui/ImageProcTest/MainWindow.xaml.cs");
        var list = Between(report, "report.DisabledFutureCommandCount = new[]", ".Count(item => !item.IsEnabled)");
        Assert.DoesNotContain("RunFullPipelineMenuItem", list, StringComparison.Ordinal);
    }

    private static string Read(string relative) => File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile(relative)).Replace("\r\n", "\n");

    private static string Between(string text, string start, string end)
    {
        var from = text.IndexOf(start, StringComparison.Ordinal);
        Assert.True(from >= 0, "missing: " + start);
        var to = text.IndexOf(end, from + start.Length, StringComparison.Ordinal);
        Assert.True(to > from, "missing end: " + end);
        return text[from..to];
    }
}
