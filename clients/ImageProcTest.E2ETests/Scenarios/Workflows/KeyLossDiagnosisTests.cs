// GUI-C-229: the reading aid of a failed real-keystroke scenario, tested on its own. No input is sent and no application is started.
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using static ImageProcTest.E2ETests.Fixtures.KeyLossDiagnosis;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

public sealed class KeyLossDiagnosisTests
{
    private static List<KeyStep> AllSent() =>
        "4321".Select((c, i) => new KeyStep(i, c, i * 8, AppInFrontBefore: true, Sent: true)).ToList();

    [Fact]
    public void WhatWasTyped_IsNoCause() =>
        Assert.Equal(Cause.None, Classify(AllSent(), "4321", "4321", "4321", otherBoxText: "", focusInTheBoxAfterwards: true));

    [Fact]
    public void AKeyThatWasNotSent_IsTheForegroundLeaving_WhateverTheBoxHoldsLater()
    {
        var steps = AllSent();
        steps[2] = steps[2] with { AppInFrontBefore = false, Sent = false };
        steps[3] = steps[3] with { Sent = false };

        Assert.Equal(Cause.ForegroundLeft, Classify(steps, "4321", "43", "43", otherBoxText: "", focusInTheBoxAfterwards: true));
        Assert.Equal(Cause.ForegroundLeft, Classify(steps, "4321", "43", "4321", otherBoxText: "", focusInTheBoxAfterwards: true));
    }

    [Fact]
    public void AFullBoxALaterRead_IsAReadThatCameTooEarly() =>
        Assert.Equal(Cause.ReadTooEarly, Classify(AllSent(), "4321", "43", "4321", otherBoxText: "", focusInTheBoxAfterwards: true));

    [Fact]
    public void TheMissingCharactersInAnotherBox_AreTheFocusMovingInsideTheApp() =>
        Assert.Equal(Cause.KeysWentToAnotherBox, Classify(AllSent(), "4321", "43", "43", otherBoxText: "1000x21", focusInTheBoxAfterwards: false));

    [Theory]
    [InlineData(true, "BoxOverwritten")]
    [InlineData(false, "FocusLeftTheBox")]
    public void AShortBoxThatStaysShort_IsToldApartByWhereTheFocusIs(bool focusInTheBox, string expected) =>
        Assert.Equal(Enum.Parse<Cause>(expected), Classify(AllSent(), "4321", "43", "43", otherBoxText: "1000", focusInTheBoxAfterwards: focusInTheBox));

    [Fact]
    public void ReadingsThatFitNothing_AreSaidToFitNothing() =>
        Assert.Equal(Cause.Unclassified, Classify(AllSent(), "4321", "43", "432", otherBoxText: "1000", focusInTheBoxAfterwards: true));

    [Fact]
    public void TheTrace_NamesEveryKey_AndMarksTheOnesNotSent()
    {
        var steps = AllSent();
        steps[3] = steps[3] with { AppInFrontBefore = false, Sent = false };

        var trace = Trace(steps);

        Assert.Contains("key 0 '4'", trace);
        Assert.Contains("key 3 '1' at +24 ms, app in front before it: NO, NOT sent", trace);
        Assert.Equal(4, trace.Split(';').Length);
        Assert.All(Enum.GetValues<Cause>(), c => Assert.False(string.IsNullOrWhiteSpace(Explain(c))));
    }
}
