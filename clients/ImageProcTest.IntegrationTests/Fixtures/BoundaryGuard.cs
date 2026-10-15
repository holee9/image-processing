using System.Collections.Concurrent;
using System.Runtime.InteropServices;
using Xunit.Sdk;

namespace ImageProcTest.IntegrationTests.Fixtures;

/// <summary>
/// GUI-C-225 (REQ-GUI-IT-050, the wording decided 2026-10-03): the one place a native call made by a negative test is watched for a <see cref="SEHException"/>. The requirement is: every nominal and
/// negative test runs to completion with the host alive, and a <see cref="SEHException"/> caught at the boundary is LOGGED and FAILS the containing test. An access violation cannot be caught in
/// .NET (it ends the host), so that half is not a thing a test can assert: a host that dies fails the whole run, which is the detection.
///
/// <see cref="Invoke{T}"/> records a line (kept in <see cref="Recorded"/> and carried in the failure message) and throws a test failure; every other exception passes through unchanged, so the guard
/// is not a catch-all that could hide a different defect.
/// </summary>
internal static class BoundaryGuard
{
    private static readonly ConcurrentQueue<string> RecordedLines = new();

    /// <summary>The lines recorded for the SEHExceptions caught so far in this process.</summary>
    public static IReadOnlyList<string> Recorded => RecordedLines.ToArray();

    /// <summary>Runs <paramref name="call"/>; a <see cref="SEHException"/> from it is recorded under <paramref name="label"/> and fails the test.</summary>
    public static T Invoke<T>(string label, Func<T> call)
    {
        try
        {
            return call();
        }
        catch (SEHException ex)
        {
            var line = $"REQ-GUI-IT-050: SEHException at {label}: {ex.Message}";
            RecordedLines.Enqueue(line);
            throw new XunitException(line, ex);
        }
    }
}
