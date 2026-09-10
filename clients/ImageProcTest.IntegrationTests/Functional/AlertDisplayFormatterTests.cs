// #110 / SRS-ALERT-007: the queue-loss alert must reach the user as an Error, counted.
namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Regression for the alert-queue loss notice (api-spec §5.17).
///
/// The native side (QA-A-28) is in flight, so these cases feed the contract's message string
/// directly. That is not a weaker test of THIS unit: the formatter's whole job is string in,
/// string out, and the contract fixes the string. What it does not cover is whether the native
/// side emits exactly that message — named in the GUI-C-19 report as a gap.
/// </summary>
[Trait("Category", "Functional")]
public sealed class AlertDisplayFormatterTests
{
    /// <summary>§5.17: the message carries a count; the display says how many were lost.</summary>
    [Theory]
    [InlineData("alert queue overflow: 7 alert(s) dropped", "7 alerts were dropped because the alert queue overflowed.")]
    [InlineData("alert queue overflow: 1 alert(s) dropped", "1 alert was dropped because the alert queue overflowed.")]
    [InlineData("alert queue overflow: 128 alert(s) dropped", "128 alerts were dropped because the alert queue overflowed.")]
    public void OverflowAlert_WithCount_IsRenderedAsCountedSentence(string nativeMessage, string expected)
    {
        Assert.Equal(expected, AlertDisplayFormatter.FormatMessage(nativeMessage));
    }

    /// <summary>
    /// Prefix present but no number: the queue still lost alerts, so the raw text is shown rather
    /// than a made-up count — and nothing throws. Losing the alert would be the worse failure.
    /// </summary>
    [Theory]
    [InlineData("alert queue overflow: alert(s) dropped")]
    [InlineData("alert queue overflow:")]
    public void OverflowAlert_WithoutCount_FallsBackToRawText(string nativeMessage)
    {
        Assert.Equal(nativeMessage, AlertDisplayFormatter.FormatMessage(nativeMessage));
    }

    /// <summary>An unrelated alert is passed through untouched — the prefix is the only trigger.</summary>
    [Theory]
    [InlineData("exposure index out of range")]
    [InlineData("Alert queue overflow: 3 alert(s) dropped")]   // capitalised: not the contract prefix
    [InlineData(" alert queue overflow: 3 alert(s) dropped")]  // leading space: not a prefix match
    public void UnrelatedMessage_IsReturnedUnchanged(string nativeMessage)
    {
        Assert.Equal(nativeMessage, AlertDisplayFormatter.FormatMessage(nativeMessage));
    }

    /// <summary>
    /// §5.17: "Consumers MUST render the loss alert like any other Error alert." The formatter is
    /// given no way to change severity — this case pins that, so a future edit that adds a
    /// severity override has to break a test rather than slip through.
    /// </summary>
    [Fact]
    public void Formatter_ExposesNoSeverityOverride()
    {
        var members = typeof(AlertDisplayFormatter)
            .GetMembers(System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static)
            .Select(m => m.Name)
            .ToArray();

        Assert.DoesNotContain(members, name => name.Contains("Severity", StringComparison.OrdinalIgnoreCase));

        // The identification helper exists for badging, and must not be read as "hide it".
        Assert.True(AlertDisplayFormatter.IsOverflowAlert("alert queue overflow: 2 alert(s) dropped"));
        Assert.False(AlertDisplayFormatter.IsOverflowAlert("exposure index out of range"));
    }

    /// <summary>Null in, empty out — a missing message must not crash the alert list.</summary>
    [Fact]
    public void NullMessage_BecomesEmpty()
    {
        Assert.Equal(string.Empty, AlertDisplayFormatter.FormatMessage(null));
    }
}
