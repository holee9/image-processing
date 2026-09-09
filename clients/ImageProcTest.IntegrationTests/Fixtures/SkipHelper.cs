namespace ImageProcTest.IntegrationTests.Fixtures;

/// <summary>
/// Runtime skip helper. Tests that cannot run without a native DLL call
/// <see cref="SkipIf"/> so the runner reports them as Skipped rather than Passed.
///
/// Mechanism, by xUnit major version:
/// <list type="bullet">
/// <item>v2 (current, 2.9.3): runtime skip needs the Xunit.SkippableFact package —
/// <c>Skip.If</c> plus the <c>[SkippableFact]</c> / <c>[SkippableTheory]</c> attributes.
/// The v2 core has no built-in path: <c>Xunit.Sdk.SkipException.ForSkip</c> compiles,
/// but the <c>$XunitDynamicSkip$</c> token it emits has no consumer in
/// xunit.extensibility.execution / .core / xunit.runner.visualstudio, so the test is
/// reported Failed. Measured, not assumed — see
/// .moai/reports/lane-gui/GUI-C-06/probe4-onecase.log and report.md §2-3.</item>
/// <item>v3: skipping is built in (<c>Assert.Skip</c>); the package is not needed.</item>
/// </list>
///
/// The <c>Skip.If</c> call is deliberately confined to this one file so an xUnit v3
/// migration changes one method body here, plus a mechanical attribute rename at the
/// call sites (<c>SkippableFact</c> to <c>Fact</c>).
/// </summary>
public static class SkipHelper
{
    /// <summary>
    /// Skips the current test when <paramref name="condition"/> is true, recording
    /// <paramref name="reason"/> as the skip reason in the test report.
    /// The calling test method must carry [SkippableFact] or [SkippableTheory].
    /// Usage: SkipHelper.SkipIf(!fixture.IsAvailable, fixture.SkipReason);
    /// </summary>
    public static void SkipIf(bool condition, string reason) => Skip.If(condition, reason);
}
