// GUI-C-229, 229b: the reading aid of a failed real-keystroke scenario, tested on its own. No input is sent and no application is started.
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

    // ---- the explanations ---------------------------------------------------------------------------------------------------------------------------------------------------------

    [Fact]
    public void AnotherProcessInFrontAfterTheKeysBegan_IsTheForegroundLeaving() =>
        Assert.Equal("ForegroundLeftDuringTyping", ClassifyName(Obs("300"), Obs("43", foreground: Other, focusPid: Other, focus: "Popup"), Obs("43", foreground: Other, focusPid: Other, focus: "Popup")));

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
    public void ObservationsThatFitTwoExplanations_AreUnclassified() =>
        // another process in front AND the box complete a moment later: the foreground reading and the late-read reading both fit
        Assert.Equal("Unclassified", ClassifyName(Obs("300"), Obs("43", foreground: Other), Obs(Typed, foreground: Other)));

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

        var facts = Facts(Obs("300"), Obs("4"), Obs("43"), Obs(Typed));

        Assert.Contains("before the keys: center '300'", facts);
        Assert.Contains("right after the keys: center '4'", facts);
        Assert.Contains("600 ms after: center '43'", facts);
        Assert.Contains("1.5 s later: center '4321'", facts);
        Assert.Contains("(not read)", Facts(Obs("300"), Obs("4"), Obs("43"), null));
    }
}
