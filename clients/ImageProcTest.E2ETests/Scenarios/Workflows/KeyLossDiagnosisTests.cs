// GUI-C-229, 229b, 229c, 229d: the reading aid of a failed real-keystroke scenario, tested on its own. No input is sent and no application is started.
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using static ImageProcTest.E2ETests.Fixtures.KeyLossDiagnosis;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

public sealed class KeyLossDiagnosisTests
{
    private const int App = 100;
    private const int Other = 200;
    private const string Typed = "4321";
    private const string Box = "VoiWindowCenterInput";
    private const string WidthBox = "VoiWindowWidthInput";

    private static Observation Obs(string center, string width = "1000", string focus = Box, int focusPid = App, int foreground = App) =>
        new(center, width, focus, focusPid, foreground);

    private static string ClassifyName(Observation before, Observation at600, Observation? later, bool keysSent = true) =>
        Classify(Typed, before, at600, later, App, keysSent).ToString();

    [Fact]
    public void WhatWasTyped_IsNoCause() =>
        Assert.Equal("None", ClassifyName(Obs("300"), Obs(Typed), null));

    // ---- Codex #131 (1): keys not sent, the box happens to hold the text -----------------------------------------------------------------------------------------------------

    [Fact]
    public void KeysNotSent_IsNotSent_EvenWhenTheBoxHoldsTheText() =>
        Assert.Equal("NotSentForegroundWasNotTheApp", ClassifyName(Obs("300"), Obs(Typed), Obs(Typed), keysSent: false));

    [Fact]
    public void WhenTheAppIsNotInFront_TheRefusalIsAMessage_AndNothingIsSent()
    {
        var refusal = RefuseIfNotInFront(appInFront: false, whatIsInFront: "In front: PickerHost.");

        Assert.NotNull(refusal);
        Assert.Contains("NO key was sent", refusal);
        Assert.Contains("In front: PickerHost.", refusal);
        Assert.Null(RefuseIfNotInFront(appInFront: true, whatIsInFront: string.Empty));
    }

    // ---- GUI-C-229d: the times of the judgment read are a record, not a gate ------------------------------------------------------------------------------------------------

    [Fact]
    public void TheJudgmentLine_CarriesTheStartAndTheCompletion_AndSaysTheVerdictDoesNotDependOnThem()
    {
        var line = JudgmentLine(startedAtMs: 604, completedAtMs: 1104);

        Assert.Contains("started +604 ms", line);
        Assert.Contains("completed +1104 ms", line);
        Assert.Contains("the verdict does not depend on these times", line);
    }

    [Fact]
    public void TheClassWorksWithNoTimingGate()
    {
        // 229d: the verdict is the original one. There is no member that turns a time into a pass or a fail.
        var members = typeof(KeyLossDiagnosis).GetMembers(System.Reflection.BindingFlags.Static | System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Public).Select(m => m.Name).ToList();

        Assert.DoesNotContain(members, n => n.Contains("JudgeTiming", StringComparison.Ordinal) || n.Contains("JudgmentSlack", StringComparison.Ordinal) || n.Contains("EarlyTolerance", StringComparison.Ordinal));
    }

    // ---- Codex #132 (2): the foreground is a fact, not a destination ------------------------------------------------------------------------------------------------------

    [Fact]
    public void AnotherProcessInFrontAtTheJudgment_IsUnclassified_NotTheKeysGoingThere() =>
        // the application held the foreground before the keys; at the 600 ms reading another process does. When that changed is not observed (it may be after the last key).
        Assert.Equal("Unclassified", ClassifyName(Obs("300"), Obs("43", foreground: Other, focusPid: Other, focus: "Popup"), Obs("43", foreground: Other, focusPid: Other, focus: "Popup")));

    [Fact]
    public void AnotherProcessInFrontOnlyAtTheReRead_IsUnclassified_EvenThoughTheRestWouldFitAnExplanation() =>
        // without the foreground fact these readings are KeysNotInBoxFocusStayed
        Assert.Equal("Unclassified", ClassifyName(Obs("300"), Obs("43"), Obs("43", foreground: Other)));

    [Fact]
    public void AForegroundThatChangedAfterTheInputEnded_IsNotReadAsALossDuringTyping()
    {
        // Codex #132 (2) reproduction: the application was in front before the keys, the keys all arrived (the box is complete at the re-read), and a popup took the foreground later.
        var cause = Classify(Typed, Obs("300"), Obs("43", foreground: Other), Obs(Typed, foreground: Other), App, keysSent: true);

        Assert.Equal(Cause.Unclassified, cause);
        Assert.DoesNotContain("while typing", Explain(cause));
        Assert.DoesNotContain("went to that window", Explain(cause));
    }

    [Fact]
    public void ForegroundFacts_StateTheProcessesAndClaimNoDestination()
    {
        var facts = ForegroundFacts(Obs("43", foreground: Other), Obs("43", foreground: 300), App);

        Assert.Contains("At the 600 ms reading the foreground was process 200, not the application (pid 100).", facts);
        Assert.Contains("At the 1.5 s re-read the foreground was process 300, not the application (pid 100).", facts);
        Assert.Contains("is not observed, so where the keys went is not either", facts);
        Assert.Equal(string.Empty, ForegroundFacts(Obs("43"), Obs("43"), App));
        Assert.DoesNotContain("went to", ForegroundFacts(Obs("43", foreground: Other), null, App).Replace("where the keys went is not either", string.Empty));
    }

    // ---- the explanations ---------------------------------------------------------------------------------------------------------------------------------------------------------

    [Fact]
    public void AFullBoxALaterRead_IsAReadThatCameTooEarly() =>
        Assert.Equal("ReadTooEarly", ClassifyName(Obs("300"), Obs("43"), Obs(Typed)));

    [Fact]
    public void WidthThatGrewByExactlyTheMissingTail_IsTheKeysGoingToAnotherBox() =>
        Assert.Equal("KeysWentToAnotherBox", ClassifyName(Obs("300", width: "1000"), Obs("43", width: "100021", focus: WidthBox), Obs("43", width: "100021", focus: WidthBox)));

    [Fact]
    public void AShortBoxThatStaysShortWithTheFocusStillInIt_IsKeysNotInBoxFocusStayed() =>
        Assert.Equal("KeysNotInBoxFocusStayed", ClassifyName(Obs("300"), Obs("43"), Obs("43")));

    [Fact]
    public void AShortBoxThatStaysShortWithTheFocusElsewhereInTheApp_IsFocusLeftTheBox() =>
        Assert.Equal("FocusLeftTheBoxStayedInApp", ClassifyName(Obs("300"), Obs("43", focus: "OtherControl"), Obs("43", focus: "OtherControl")));

    // ---- Codex #131 (3): Width's text before the keys -----------------------------------------------------------------------------------------------------------------------

    [Fact]
    public void AWidthThatAlreadyEndedInTheTailBeforeTheKeys_SaysNothing_SoTheAnswerIsUnclassified()
    {
        // Width held "21" before the keys and holds "21" after them. That is what "the keys never went there" and "the keys replaced a selected 21 with 21" both look like.
        var before = Obs("300", width: "21");
        var after = Obs("43", width: "21");

        Assert.Equal("Unclassified", ClassifyName(before, after, after));
    }

    [Fact]
    public void AWidthThatEndedInTheTailBeforeTheKeys_IsNotTakenForTheKeysGoingThere() =>
        Assert.Equal("Unclassified", ClassifyName(Obs("300", width: "100021"), Obs("43", width: "100021"), Obs("43", width: "100021")));

    // ---- more than one explanation, or none ---------------------------------------------------------------------------------------------------------------------------------

    [Fact]
    public void ABoxThatIsNotAPrefixOfTheText_AndNothingElseFits_IsUnclassified() =>
        Assert.Equal("Unclassified", ClassifyName(Obs("300"), Obs("4231"), Obs("4231")));

    [Fact]
    public void ABoxThatKeepsGrowing_IsNotAnExplanationEitherWay() =>
        Assert.Equal("Unclassified", ClassifyName(Obs("300"), Obs("43"), Obs("432")));

    [Fact]
    public void EveryCause_HasAnExplanation_AndTheFactsNameEveryReading()
    {
        Assert.All(Enum.GetValues<Cause>(), c => Assert.False(string.IsNullOrWhiteSpace(Explain(c))));

        var facts = Facts(Obs("300"), Obs("43"), 604, 612, Obs(Typed));

        Assert.Contains("before the keys: center '300'", facts);
        Assert.Contains("600 ms after (judgment read +604..+612 ms): center '43'", facts);
        Assert.Contains("1.5 s later: center '4321'", facts);
        Assert.Contains("(not read)", Facts(Obs("300"), Obs("43"), 604, 612, null));
        Assert.DoesNotContain(Enum.GetNames<Cause>(), name => name.Contains("DuringTyping", StringComparison.Ordinal));
    }
}
