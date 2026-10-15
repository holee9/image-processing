// GUI-C-225 (REQ-GUI-IT-050): a SEHException caught at the boundary is logged and fails the containing test.
using System.Runtime.InteropServices;
using ImageProcTest.IntegrationTests.Fixtures;
using Xunit.Sdk;

namespace ImageProcTest.IntegrationTests.Safety;

/// <summary>
/// REQ-GUI-IT-050 (decided 2026-10-03): "Every nominal and negative test shall run to completion with the test host alive, and a SEHException caught at the boundary shall be logged and shall fail
/// the containing test." An access violation cannot be caught in .NET, so what is testable is the guard that stands where the managed code meets the native call: with a SEHException it fails
/// with a recorded line; with anything else it gets out of the way. The other half (the rows really all run) is
/// <c>NegativeInputPathTests.EveryRegisteredNegativeRow_RunsToCompletion_…</c>.
/// </summary>
[Trait("Category", "Safety")]
public sealed class BoundaryGuardTests
{
    [Fact]
    public void ASehExceptionAtTheBoundary_FailsTheTest_AndLeavesARecordedLineNamingWhereItHappened()
    {
        var label = $"c225-seh-{Guid.NewGuid():N}";
        var seh = new SEHException("the native call faulted");

        var failure = Assert.Throws<XunitException>(() => BoundaryGuard.Invoke<int>(label, () => throw seh));

        Assert.Contains("SEHException", failure.Message, StringComparison.Ordinal);
        Assert.Contains(label, failure.Message, StringComparison.Ordinal);
        Assert.Contains("the native call faulted", failure.Message, StringComparison.Ordinal);
        Assert.Same(seh, failure.InnerException);
        var recorded = BoundaryGuard.Recorded.Where(l => l.Contains(label, StringComparison.Ordinal)).ToList();
        Assert.Single(recorded);
        Assert.Contains("REQ-GUI-IT-050", recorded[0], StringComparison.Ordinal);
    }

    /// <summary>The control: a call that returns is passed through and records nothing, and an exception that is not a SEHException is not swallowed or renamed (the guard is not a catch-all).</summary>
    [Fact]
    public void ACallThatReturns_IsPassedThrough_AndAnotherExceptionIsNotTouched()
    {
        var label = $"c225-ok-{Guid.NewGuid():N}";

        Assert.Equal(42, BoundaryGuard.Invoke(label, () => 42));
        Assert.Throws<InvalidOperationException>(() => BoundaryGuard.Invoke<int>(label, () => throw new InvalidOperationException("not a SEH")));
        Assert.DoesNotContain(BoundaryGuard.Recorded, l => l.Contains(label, StringComparison.Ordinal));
    }
}
