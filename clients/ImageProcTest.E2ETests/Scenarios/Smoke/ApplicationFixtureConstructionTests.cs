// GUI-C-219 (#249): the user-app fixture's constructor leaves no app behind when it fails after the app started.
using System.Diagnostics;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Smoke;

/// <summary>
/// GUI-C-218 found, in the legacy app's test launcher, that a constructor which throws is never disposed: a UIA timeout after <c>Application.Launch</c> left the app running, and the leaked
/// app (it inherits the test host's output handles) kept <c>dotnet test</c> looking hung. This fixture had the same shape (<c>Application.Launch</c> and <c>GetMainWindow</c> in the constructor,
/// nothing around them). The failure is made certain by giving the window a 1 ms budget; the check is on the system (are there app processes that were not there before), not on the
/// fixture's own bookkeeping, and a launch that failed for another reason (stale binary, missing file) does not count as the case under test.
/// </summary>
public sealed class ApplicationFixtureConstructionTests(ITestOutputHelper output)
{
    [SkippableFact]
    public void AConstructionThatFailsAfterTheAppStarted_LeavesNoAppRunning()
    {
        var exe = ApplicationFixture.ResolveApplicationExecutable();
        Skip.If(exe is null, "gui/ImageProcTest/bin/Debug/net8.0-windows/ImageProcTest.exe was not found: build gui/ImageProcTest first.");
        var before = AppProcessIds(exe!);

        var failure = Record.Exception(() => new ApplicationFixture(TimeSpan.FromMilliseconds(1)));

        var leaked = AppProcessIds(exe!).Except(before).ToList();
        foreach (var pid in leaked) { try { Process.GetProcessById(pid).Kill(entireProcessTree: true); } catch (ArgumentException) { } }   // a failing run must not leak either
        output.WriteLine($"construction threw: {failure}");
        Assert.NotNull(failure);
        // With a 1 ms budget FlaUI's GetMainWindow returns no window, and the constructor then fails on it (a NullReferenceException), or it times out: both can only happen AFTER Application.Launch,
        // which is the case under test. A stale binary or a missing file throws something else, before any app exists, and must not pass for this.
        Assert.True(failure is TimeoutException or NullReferenceException,
            $"the construction did not fail after the app started (the case under test): {failure!.GetType().Name}: {failure.Message}");
        Assert.True(leaked.Count == 0, $"a failed construction left {leaked.Count} app instance(s) running: {string.Join(", ", leaked)}");
    }

    private static HashSet<int> AppProcessIds(string exe) =>
        Process.GetProcessesByName(Path.GetFileNameWithoutExtension(exe))
            .Where(p => { try { return string.Equals(p.MainModule?.FileName, exe, StringComparison.OrdinalIgnoreCase); } catch (Exception) { return false; } })
            .Select(p => p.Id)
            .ToHashSet();
}
