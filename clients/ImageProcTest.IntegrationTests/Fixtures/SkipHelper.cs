namespace ImageProcTest.IntegrationTests.Fixtures;

/// <summary>
/// Runtime skip helper for xUnit v2 compatibility.
/// xUnit v2 does not support runtime skip; tests that "skip" simply return early.
/// Use this helper to centralize the pattern for future migration to xUnit v3.
/// </summary>
public static class SkipHelper
{
    /// <summary>
    /// Returns true when the test should be skipped (DLL not available).
    /// Caller should return early: if (SkipHelper.ShouldSkip(condition)) return;
    /// </summary>
    public static bool ShouldSkip(bool condition) => condition;
}
