// GUI-C-172 (Codex audit #8): the two ways a PERSON drives the window, tested as a person drives it.
using System.Diagnostics;
using FlaUI.Core.AutomationElements;
using FlaUI.Core.WindowsAPI;
using ImageProcTest.E2ETests.Fixtures;
using ImageProcTest.E2ETests.Scenarios.Smoke;
using Xunit;
using Xunit.Abstractions;
using static ImageProcTest.E2ETests.Scenarios.Workflows.WorkbenchObservation;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// The scenarios that need REAL mouse and keyboard input, and nothing else.
///
/// <para><b>Why only two.</b> GUI-C-171 moved every other scenario to UI Automation: a click or a key press is
/// addressed to whichever window is in front, which on a shared desktop is not the application (see
/// <see cref="GlobalInput"/>). The functional assertions all stayed. What the ordinary scenarios no longer
/// show is that a PERSON can open a menu with the mouse and can type into a box and Tab out of it, because they
/// open menus through <see cref="UiaMenu"/> and write text through UI Automation. These two scenarios are where
/// that is still shown, so the claim "the real path works" is covered by two named scenarios instead of by
/// fourteen incidental ones.</para>
///
/// <para><b>Gated.</b> Each begins with <see cref="GlobalInput.Require"/>: skipped, before anything is sent,
/// unless <c>XPE_E2E_ALLOW_GLOBAL_INPUT=1</c>. They are meant for a desktop that nothing else uses (the CI
/// runner), and are NOT run on a shared workstation.</para>
///
/// <para><b>Each has a control.</b> A check that cannot fail proves nothing, so each scenario first shows the
/// state in which the real input is needed — the menu closed, the stale indicator absent, the box holding the
/// keyboard focus — and only then sends the input.</para>
/// </summary>
[Collection(WorkflowApplicationCollection.Name)]
public sealed class RealInputScenarios(WorkflowApplicationFixture app, ITestOutputHelper output)
{
    private const string AlwaysInTheViewMenu = "ShowLogsPanelMenuItem";

    /// <summary>
    /// R01: a real mouse click on the View menu opens it.
    ///
    /// <para>The click is <see cref="GlobalInput.Click"/> — a real mouse event at the menu position — and not an
    /// Expand. Control: the menu is shown closed first (its items are not in the tree), so an item that is there
    /// afterwards came from the click.</para>
    ///
    /// <para><b>Precondition (GUI-C-173).</b> The app must be the foreground window before the click, as it is for
    /// a person who is using it. If it cannot be made so the scenario fails with that, not with "the click did not
    /// open the menu". What it claims is therefore: <i>with the app active, a real click opens the View menu.</i>
    /// It does not claim anything about a first click on an INACTIVE window.</para>
    ///
    /// <para>The first CI run failed here with the app in front and under the pointer (GUI-C-172). If the first click
    /// still does not open the menu, the failure message carries three probes that only explain it.</para>
    /// </summary>
    [SkippableFact]
    public void R01_TheViewMenu_OpensWithARealMouseClick()
    {
        GlobalInput.Require("R01 (a real mouse click on the View menu)");
        Measure("R01", window =>
        {
            // The state a person starts from: nothing is dropped open. What an earlier scenario left open is READ
            // first and reported, then closed through UI Automation (set-up, not what is under test). A drop-down
            // left open takes the next click outside it as "close me" — the first CI run of this scenario failed
            // that way with the keyboard focus still on the body-part combo box (GUI-C-173).
            var leftOpen = UiaMenu.CollapseEverythingOpen(window);
            var leftOpenText = leftOpen.Count == 0 ? "(nothing)" : string.Join(", ", leftOpen);
            output.WriteLine($"R01 expanded before the click, closed through UI Automation: {leftOpenText}");
            Thread.Sleep(300);
            Assert.True(
                ViewMenuItem(window) is null,
                $"'{AlwaysInTheViewMenu}' was already visible before the click, so the click cannot be shown to be what opened the menu.");

            // The state a person starts from: the app window is the active window. If it cannot be made so, the
            // run says that, instead of blaming a click that had no chance of reaching an inactive window.
            var activation = MakeTheAppTheForegroundWindow(window);
            output.WriteLine($"R01 before the click: {activation ?? "the app could not be made the foreground window"}");
            Assert.True(
                activation is not null,
                "The app window could not be made the foreground window within 2 s, so a click cannot be expected to open its menu. " +
                PanelToggleScenarios.DescribeWhatIsInFront(window));

            var viewMenu = window.FindFirstDescendant(cf => cf.ByAutomationId("ViewMenu"));
            Assert.True(viewMenu is not null, "ViewMenu is not in the automation tree.");

            try
            {
                GlobalInput.Click(viewMenu!);

                var item = WaitFor(() => ViewMenuItem(window));
                output.WriteLine($"R01 item after the click: {(item is null ? "(absent)" : "present")}");
                if (item is null)
                {
                    // The verdict is already decided: the FIRST click did not open the menu. What follows only
                    // explains why, and nothing in it can make this scenario pass.
                    Assert.Fail(
                        $"A real mouse click on the View menu did not open it: '{AlwaysInTheViewMenu}' was not visible within 2 s. " +
                        $"Expanded before the click and closed through UI Automation: {leftOpenText}. " +
                        ExplainAMenuThatDidNotOpen(window, viewMenu!, activation!));
                }
            }
            finally
            {
                UiaMenu.CollapseAll(window);
            }
        });
    }

    /// <summary>
    /// R02: real key presses edit a text box, and a real Tab moves the focus out of it.
    ///
    /// <para>Controls, in order: the image starts not stale; the center box really holds the keyboard focus (so
    /// the keystrokes can reach it); the typed text shows in the box (what a person would see). The edit is
    /// observed the way W22 observes one — the stale indicator, which only appears once the value has reached the
    /// view model. Then a real Tab. A Tab goes to whichever window holds the focus when it is sent, so it is
    /// checked on both sides: just before it, the keyboard focus must still be this box in this app; after it, the
    /// focus must be on an element INSIDE this app (compared by process id), not the box, and — read from the XAML
    /// — the next text box in tab order. A check on the box alone would also pass if another window had taken the
    /// focus and received the Tab (the GUI-C-171 failure shape; Codex audit #9).</para>
    ///
    /// <para><b>What this does not claim.</b> Every text box in this app binds with
    /// <c>UpdateSourceTrigger=PropertyChanged</c>, so the value reaches the view model as it is typed — a Tab is
    /// not what commits it. An earlier note that "the binding takes the value when the box loses focus" was
    /// wrong for these boxes (GUI-C-172). This scenario therefore shows typing and Tab-to-move, not
    /// Tab-to-commit.</para>
    /// </summary>
    [SkippableFact]
    public void R02_RealKeystrokes_EditTheVoiBox_AndARealTabMovesOn()
    {
        GlobalInput.Require("R02 (real key presses into a text box, then a real Tab)");
        Measure("R02", window =>
        {
            OpenParameters(window);          // set-up through UI Automation
            var original = BodyPart(window);
            try
            {
                // A preset change re-renders and leaves nothing stale (W22's first control): a known start.
                SelectBodyPart(window, original == "Bone" ? "Lung" : "Bone");
                Assert.True(
                    StaleIndicator(window) is null,
                    $"The image was already stale ('{StaleIndicator(window)}') before anything was typed, so the edit cannot be shown to be what made it so.");

                var input = window.FindFirstDescendant(cf => cf.ByAutomationId("VoiWindowCenterInput"))!.AsTextBox();
                window.SetForeground();
                input.Focus();               // puts the caret in the box; the KEYS below are real
                Assert.True(
                    input.Properties.HasKeyboardFocus.ValueOrDefault,
                    "The center box does not hold the keyboard focus, so real key presses would not reach it. " +
                    PanelToggleScenarios.DescribeWhatIsInFront(window));

                const string typed = "4321";
                Assert.NotEqual(typed, input.Text);

                // The order is the original one: ONE check just before the first key (read before it), Ctrl+A, ONE call that types the four characters, 600 ms, the judgment read. If the application is not in
                // front, nothing is sent and the scenario fails here, not skipped (a key goes to whichever window is in front, GUI-C-171). Nothing is read between the keys or between the last key and the
                // judgment read: what is read for the failure message is read AFTER the judgment (GUI-C-229c).
                var appPid = window.Properties.ProcessId.ValueOrDefault;
                var before = Observe(window, input);
                var inFront = AppIsForeground(window);
                var refusal = KeyLossDiagnosis.RefuseIfNotInFront(inFront, inFront ? string.Empty : PanelToggleScenarios.DescribeWhatIsInFront(window));
                if (refusal is not null)
                {
                    Assert.Fail(refusal + " Read before the keys: " + before);
                }

                GlobalInput.SelectAll();
                GlobalInput.Type(typed);
                var sinceTyped = Stopwatch.StartNew();

                var (center, readAtMs) = TakeJudgment(sinceTyped, input);
                var at600 = ObserveAfterJudgment(window, center);
                output.WriteLine("R02 observations: " + KeyLossDiagnosis.Facts(before, at600, readAtMs, null));

                // A read that is not about 600 ms after the last key is a timing failure whatever it read: a late value could turn the original failure into a pass.
                var timing = KeyLossDiagnosis.JudgeTiming(readAtMs);
                if (timing is not null)
                {
                    Assert.Fail(timing + $" Value read: '{center}'. " + KeyLossDiagnosis.Facts(before, at600, readAtMs, null));
                }

                // The assertion is the strict one it always was: all four characters, 600 ms after the last key. When it fails, the message says what was read around the input.
                if (center != typed)
                {
                    Assert.Fail(DescribeLostKeys(window, input, typed, before, at600, readAtMs, appPid));
                }

                Assert.Equal(typed, center);
                var stale = StaleIndicator(window);
                output.WriteLine($"R02 after typing: box='{input.Text}' indicator='{stale}'");
                Assert.True(
                    stale is not null && stale.Contains("parameters changed", StringComparison.Ordinal),
                    $"Real key presses changed the box to '{input.Text}' but the image was not marked stale (indicator: '{stale ?? "(absent)"}'), " +
                    "so the typed value did not reach the view model.");

                // Just BEFORE the Tab: the keys above took 600 ms, and a Tab goes to whichever window holds the
                // focus when it is sent. Re-establish that it is still this box, in this app.
                var appProcess = window.Properties.ProcessId.ValueOrDefault;
                var beforeTab = FocusedInfo(window);
                Assert.True(
                    input.Properties.HasKeyboardFocus.ValueOrDefault
                    && beforeTab is { AutomationId: CenterBox } && beforeTab.Value.ProcessId == appProcess,
                    $"Just before the Tab the keyboard focus was not in the center box of this app (focused: {Describe(beforeTab)}), " +
                    "so the Tab would not have been addressed to it. " + PanelToggleScenarios.DescribeWhatIsInFront(window));

                GlobalInput.Press(VirtualKeyShort.TAB);
                var afterTab = WaitForFocusToLeave(window, CenterBox);

                // In this order, because each says something different. A focus in ANOTHER PROCESS means the Tab
                // went to a different window (the GUI-C-171 failure shape) — a box that merely lost the focus
                // would have passed an assertion on the box alone.
                Assert.True(
                    afterTab is not null && afterTab.Value.ProcessId == appProcess,
                    $"After the Tab the keyboard focus is not inside the app (focused: {Describe(afterTab)}; app pid {appProcess}): " +
                    "the Tab went to a different window. " + PanelToggleScenarios.DescribeWhatIsInFront(window));
                Assert.True(
                    afterTab!.Value.AutomationId != CenterBox,
                    $"After a real Tab the center box still holds the keyboard focus: Tab did not move it on (focused: {Describe(afterTab)}).");
                // The next focusable control in document order. Both boxes sit in one Grid (AnalysisPanel.xaml) and
                // neither the panel nor MainWindow sets TabIndex, IsTabStop or KeyboardNavigation, so the default
                // order applies: Center, then Width. Read from the XAML, not yet observed in a run.
                Assert.True(
                    afterTab.Value.AutomationId == NextBoxInTabOrder,
                    $"After a real Tab the focus is on {Describe(afterTab)}, not on '{NextBoxInTabOrder}', the next text box in tab order.");
            }
            finally
            {
                // The fixture is shared: put the preset back, which also clears the typed value and the stale mark.
                SelectBodyPart(window, original == "Bone" ? "Lung" : "Bone");
                SelectBodyPart(window, original);
            }
        });
    }

    private const string CenterBox = "VoiWindowCenterInput";
    private const string NextBoxInTabOrder = "VoiWindowWidthInput";

    /// <summary>
    /// R02d (GUI-C-229b, 229c): the failure message that R02 builds when it fails can be built from the real window, with no key sent. R02 itself needs real input and runs only where that is allowed, so
    /// this is where the code that reads the boxes, the focus and the foreground for the message is shown to work. The readings are taken from the real window with nothing typed, so the box does not
    /// hold the text and the message is built as for a failure; the message must carry every section.
    /// </summary>
    [SkippableFact]
    public void R02d_TheLostKeysMessage_IsBuiltFromTheRealWindow_WithoutSendingAKey()
    {
        Measure("R02d", window =>
        {
            OpenParameters(window);
            var input = window.FindFirstDescendant(cf => cf.ByAutomationId(CenterBox))!.AsTextBox();
            Assert.NotEqual("4321", input.Text);   // read as a failure only if the box does not hold the text

            var appPid = window.Properties.ProcessId.ValueOrDefault;
            var before = Observe(window, input);
            var sinceTyped = Stopwatch.StartNew();
            var (center, readAtMs) = TakeJudgment(sinceTyped, input);
            var at600 = ObserveAfterJudgment(window, center);
            var message = DescribeLostKeys(window, input, "4321", before, at600, readAtMs, appPid);
            output.WriteLine("R02d message: " + message);

            Assert.Null(KeyLossDiagnosis.JudgeTiming(readAtMs));   // the read taken here is on time: the control for R02e
            Assert.Contains("typed '4321' but the center box held '" + at600.Center + "'", message, StringComparison.Ordinal);
            Assert.Contains("FACTS (observed):", message, StringComparison.Ordinal);
            Assert.Contains("before the keys: center '", message, StringComparison.Ordinal);
            Assert.Contains("600 ms after (judgment read at +", message, StringComparison.Ordinal);
            Assert.Contains("1.5 s later: center '", message, StringComparison.Ordinal);
            Assert.Contains("keyboard focus '", message, StringComparison.Ordinal);
            Assert.Contains($"foreground pid {at600.ForegroundProcessId}", message, StringComparison.Ordinal);
            Assert.Contains("READING (an inference from the facts above, not an observation):", message, StringComparison.Ordinal);
        });
    }

    /// <summary>
    /// R02e (GUI-C-229c): a delay put in front of the judgment read makes the scenario fail as a TIMING failure. The delay is injected into the same <see cref="TakeJudgment"/> that R02 uses, with no key
    /// sent: a read 800 ms later than it should be lands outside the window, whatever it reads, while the same call without the delay lands inside it (the control).
    /// </summary>
    [SkippableFact]
    public void R02e_ADelayBeforeTheJudgmentRead_IsATimingFailure_NotAJudgedValue()
    {
        Measure("R02e", window =>
        {
            OpenParameters(window);
            var input = window.FindFirstDescendant(cf => cf.ByAutomationId(CenterBox))!.AsTextBox();

            var onTime = TakeJudgment(Stopwatch.StartNew(), input);
            Assert.Null(KeyLossDiagnosis.JudgeTiming(onTime.ReadAtMs));

            var delayed = TakeJudgment(Stopwatch.StartNew(), input, injectBeforeRead: () => Thread.Sleep(800));
            var timing = KeyLossDiagnosis.JudgeTiming(delayed.ReadAtMs);
            output.WriteLine($"R02e on time: read at +{onTime.ReadAtMs} ms; delayed: read at +{delayed.ReadAtMs} ms -> {timing}");

            Assert.True(delayed.ReadAtMs >= KeyLossDiagnosis.JudgmentWaitMs + 800 - 50, $"The injected delay did not take effect (read at +{delayed.ReadAtMs} ms).");
            Assert.NotNull(timing);
            Assert.StartsWith("Timing failure:", timing, StringComparison.Ordinal);
        });
    }

    /// <summary>
    /// Waits until 600 ms have passed since the last key and reads Center, and nothing else: this read is the judgment. <paramref name="injectBeforeRead"/> exists for R02e only. Returns the value and how
    /// many ms after the last key the read began.
    /// </summary>
    private static (string Center, long ReadAtMs) TakeJudgment(Stopwatch sinceLastKey, TextBox input, Action? injectBeforeRead = null)
    {
        var remaining = KeyLossDiagnosis.JudgmentWaitMs - (int)sinceLastKey.ElapsedMilliseconds;
        if (remaining > 0)
        {
            Thread.Sleep(remaining);
        }

        injectBeforeRead?.Invoke();
        var readAt = sinceLastKey.ElapsedMilliseconds;
        return (SafeText(input), readAt);
    }

    /// <summary>What can be read right now: both text boxes, the keyboard focus (any process) and the process that holds the system foreground. UI Automation reads: never taken between keys.</summary>
    private static Observation Observe(Window window, TextBox input) => ObserveAfterJudgment(window, SafeText(input));

    /// <summary>The Width box, the keyboard focus and the foreground, read AFTER the judgment read of Center (<paramref name="center"/>), which is taken first and alone.</summary>
    private static Observation ObserveAfterJudgment(Window window, string center)
    {
        var width = SafeText(window.FindFirstDescendant(cf => cf.ByAutomationId(NextBoxInTabOrder))?.AsTextBox());
        var focused = FocusedInfo(window);
        return new Observation(center, width, focused?.AutomationId ?? "(none)", focused?.ProcessId ?? 0, ForegroundProcessId());
    }

    /// <summary>
    /// The failure message for lost keys. It re-reads once, 1.5 s after the judgment, and then lists the observations (facts, including a foreground that was another process, with no destination claimed)
    /// and, separately, which explanation they fit (an inference, and <c>Unclassified</c> when more than one or none fits: <see cref="KeyLossDiagnosis"/>). It only describes; the scenario has already failed.
    /// </summary>
    private static string DescribeLostKeys(Window window, TextBox input, string typed, Observation before, Observation at600, long readAtMs, int appPid)
    {
        Thread.Sleep(1500);
        var later = Observe(window, input);
        var cause = KeyLossDiagnosis.Classify(typed, before, at600, later, appPid, keysSent: true);
        return
            $"Real key presses typed '{typed}' but the center box held '{at600.Center}' 600 ms after the last key (application pid {appPid}). " +
            $"FACTS (observed): {KeyLossDiagnosis.Facts(before, at600, readAtMs, later)}. {KeyLossDiagnosis.ForegroundFacts(at600, later, appPid)} {PanelToggleScenarios.DescribeWhatIsInFront(window)} " +
            $"READING (an inference from the facts above, not an observation): {cause} - {KeyLossDiagnosis.Explain(cause)}";
    }

    private static string SafeText(TextBox? box)
    {
        try
        {
            return box?.Text ?? "(no box)";
        }
        catch (Exception ex)
        {
            return "(unreadable: " + ex.GetType().Name + ")";
        }
    }

    /// <summary>Who holds the keyboard focus right now, as UI Automation reports it — in any process.</summary>
    private readonly record struct FocusInfo(string AutomationId, int ProcessId, string Name);

    private static FocusInfo? FocusedInfo(Window window)
    {
        try
        {
            var focused = window.Automation.FocusedElement();
            return focused is null
                ? null
                : new FocusInfo(
                    focused.Properties.AutomationId.ValueOrDefault ?? string.Empty,
                    focused.Properties.ProcessId.ValueOrDefault,
                    focused.Properties.Name.ValueOrDefault ?? string.Empty);
        }
        catch (Exception)
        {
            return null;
        }
    }

    /// <summary>Waits (up to 2 s) until the focused element is no longer <paramref name="automationId"/>, and returns what it is.</summary>
    private static FocusInfo? WaitForFocusToLeave(Window window, string automationId)
    {
        var waited = Stopwatch.StartNew();
        FocusInfo? now;
        while ((now = FocusedInfo(window)) is { } info && info.AutomationId == automationId && waited.ElapsedMilliseconds < 2000)
        {
            Thread.Sleep(50);
        }

        return now;
    }

    private static string Describe(FocusInfo? info) =>
        info is { } i ? $"'{i.AutomationId}' (name '{i.Name}', pid {i.ProcessId})" : "(nothing, or it could not be read)";

    [System.Runtime.InteropServices.DllImport("user32.dll")]
    private static extern IntPtr GetForegroundWindow();

    [System.Runtime.InteropServices.DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr handle, out uint processId);

    /// <summary>The process that holds the system foreground right now, or 0.</summary>
    private static int ForegroundProcessId()
    {
        var foreground = GetForegroundWindow();
        if (foreground == IntPtr.Zero)
        {
            return 0;
        }

        GetWindowThreadProcessId(foreground, out var processId);
        return (int)processId;
    }

    /// <summary>True when the window that holds the system foreground belongs to the app under test.</summary>
    private static bool AppIsForeground(Window window)
    {
        var foreground = GetForegroundWindow();
        if (foreground == IntPtr.Zero)
        {
            return false;
        }

        GetWindowThreadProcessId(foreground, out var processId);
        return processId == (uint)window.Properties.ProcessId.ValueOrDefault;
    }

    /// <summary>
    /// Makes the app the foreground window and says how, or returns null when it could not within 2 s. It does
    /// not click: that is what is under test.
    /// </summary>
    private static string? MakeTheAppTheForegroundWindow(Window window)
    {
        if (AppIsForeground(window))
        {
            return "the app was already the foreground window";
        }

        window.SetForeground();
        var waited = Stopwatch.StartNew();
        while (waited.ElapsedMilliseconds < 2000)
        {
            if (AppIsForeground(window))
            {
                return $"the app became the foreground window through SetForeground, after {waited.ElapsedMilliseconds} ms";
            }

            Thread.Sleep(50);
        }

        return null;
    }

    private static string StateOf(AutomationElement menu)
    {
        try
        {
            var pattern = menu.Patterns.ExpandCollapse.PatternOrDefault;
            return pattern is null ? "has no ExpandCollapse pattern" : $"ExpandCollapseState={pattern.ExpandCollapseState.ValueOrDefault}";
        }
        catch (Exception ex)
        {
            return $"state unreadable ({ex.GetType().Name})";
        }
    }

    /// <summary>
    /// GUI-C-173: why the first real click did not open the View menu. Runs only after the verdict is decided,
    /// and only describes. Each probe starts from a closed menu (collapsed through UI Automation) and reports
    /// whether its own attempt opened it, so the failure message separates the candidate causes: a menu that is
    /// stuck open or half-open, a click that was spent on activating the window, state left by an earlier
    /// scenario that an ESC clears, and a menu that does not open at all.
    /// </summary>
    private string ExplainAMenuThatDidNotOpen(Window window, AutomationElement viewMenu, string activation)
    {
        var report = new System.Text.StringBuilder();
        var focus = FocusedInfo(window);
        report.Append($"[GUI-C-173] before the click: {activation}. After the first click: ViewMenu {StateOf(viewMenu)}; ");
        report.Append($"keyboard focus: {(focus is { } f ? $"'{f.AutomationId}' pid {f.ProcessId}" : "unreadable")}. ");

        void Settle()
        {
            UiaMenu.CollapseAll(window);
            Thread.Sleep(300);
        }

        Settle();
        GlobalInput.Click(viewMenu);
        var second = WaitFor(() => ViewMenuItem(window)) is not null;
        report.Append($"A SECOND real click on a closed menu opened it: {second}. ");

        Settle();
        GlobalInput.Press(VirtualKeyShort.ESCAPE);
        Thread.Sleep(120);
        GlobalInput.Click(viewMenu);
        var afterEscape = WaitFor(() => ViewMenuItem(window)) is not null;
        report.Append($"ESC then a real click opened it: {afterEscape}. ");

        Settle();
        UiaMenu.Open(window, "ViewMenu");
        var viaAutomation = WaitFor(() => ViewMenuItem(window)) is not null;
        report.Append($"UI Automation Expand opened it: {viaAutomation}. ");
        Settle();

        report.Append(PanelToggleScenarios.DescribeWhatIsInFront(window));
        output.WriteLine(report.ToString());
        return report.ToString();
    }

    private static AutomationElement? ViewMenuItem(Window window) =>
        window.FindFirstDescendant(cf => cf.ByAutomationId(AlwaysInTheViewMenu));

    private static AutomationElement? WaitFor(Func<AutomationElement?> look)
    {
        var waited = Stopwatch.StartNew();
        AutomationElement? found;
        while ((found = look()) is null && waited.ElapsedMilliseconds < 2000)
        {
            Thread.Sleep(50);
        }

        return found;
    }

    private void Measure(string scenario, Action<Window> body)
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");

        if (!string.IsNullOrEmpty(app.ReacquiredNote))
        {
            output.WriteLine($"{scenario} fixture: {app.ReacquiredNote}");
        }

        var stopwatch = Stopwatch.StartNew();
        try
        {
            body(app.MainWindow!);
        }
        finally
        {
            output.WriteLine($"{scenario}: {stopwatch.ElapsedMilliseconds} ms");
        }
    }
}
