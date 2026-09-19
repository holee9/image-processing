// #173 (GUI-C-119): a corrupt settings file, put in front of a real launch.
using System;
using System.IO;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// Whether the running app actually says its settings could not be read.
///
/// <para><b>Why this is an E2E case and not an assertion on the view model.</b> GUI-C-118 reported
/// "nothing appears on screen" from reading the code path, and named that as its largest gap. The fix
/// added a message; claiming the message appears by reading the code would be the same class of claim
/// the fix was for. So the file is written to disk, the real executable is launched against it, and
/// the status bar of the running window is read through UIA.</para>
///
/// <para><b>Both directions.</b> A case that only checks the corrupt launch passes just as well on an
/// app that shows this warning always. The good-file launch is what separates those.</para>
///
/// <para>Each case owns its app: the launch reads the file once, at startup, so the file has to be in
/// place before the process starts.</para>
/// </summary>
public sealed class UnreadableSettingsScenarios(ITestOutputHelper output)
{
    /// <summary>Well-formed, and carrying a value that is visibly not a default.</summary>
    private const string GoodSettings = """
    { "voiWindowCenter": 1234 }
    """;

    /// <summary>Truncated mid-key — one of the three shapes measured in GUI-C-118.</summary>
    private const string CorruptSettings = """
    { "voiWindowCenter": 1234, "laneBAlgorithm":
    """;

    [SkippableFact]
    public void ACorruptSettingsFile_IsReportedOnScreen()
    {
        var path = NewSettingsPath();
        File.WriteAllText(path, CorruptSettings);

        try
        {
            using var app = new SettingsFileApplicationFixture(path);
            Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");

            var status = StatusBarText(app);
            output.WriteLine($"corrupt launch status: '{status}'");

            // ① it could not be read ② the session started from defaults ③ where the original is.
            Assert.Contains("could not be read", status, StringComparison.OrdinalIgnoreCase);
            Assert.Contains("defaults", status, StringComparison.OrdinalIgnoreCase);
            Assert.Contains(".unreadable-", status, StringComparison.Ordinal);

            // And the original really is there — the message would otherwise point at nothing.
            var preserved = Directory.GetFiles(
                Path.GetDirectoryName(path)!, Path.GetFileName(path) + ".unreadable-*");
            output.WriteLine($"preserved: {string.Join(", ", preserved)}");
            Assert.True(preserved.Length == 1, $"Expected exactly one preserved original, found {preserved.Length}.");
            Assert.Equal(CorruptSettings.Trim(), File.ReadAllText(preserved[0]).Trim());
        }
        finally
        {
            CleanUp(path);
        }
    }

    /// <summary>The other direction: a readable file produces no such message.</summary>
    [SkippableFact]
    public void AReadableSettingsFile_SaysNothingAboutIt()
    {
        var path = NewSettingsPath();
        File.WriteAllText(path, GoodSettings);

        try
        {
            using var app = new SettingsFileApplicationFixture(path);
            Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");

            var status = StatusBarText(app);
            output.WriteLine($"good launch status: '{status}'");

            Assert.DoesNotContain("could not be read", status, StringComparison.OrdinalIgnoreCase);
            Assert.DoesNotContain(".unreadable-", status, StringComparison.Ordinal);

            // The control: nothing was moved aside either.
            var preserved = Directory.GetFiles(
                Path.GetDirectoryName(path)!, Path.GetFileName(path) + ".unreadable-*");
            Assert.True(preserved.Length == 0, $"A readable file was moved aside anyway: {string.Join(", ", preserved)}");
        }
        finally
        {
            CleanUp(path);
        }
    }

    private static string StatusBarText(SettingsFileApplicationFixture app)
    {
        var element = app.MainWindow!.FindFirstDescendant(cf => cf.ByAutomationId("StatusBarText"));
        Assert.True(element is not null, "StatusBarText is not in the automation tree.");
        return element!.Name ?? string.Empty;
    }

    private static string NewSettingsPath()
    {
        var directory = Path.Combine(Path.GetTempPath(), $"xpe-c119-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        return Path.Combine(directory, "appsettings.json");
    }

    private static void CleanUp(string path)
    {
        try
        {
            Directory.Delete(Path.GetDirectoryName(path)!, recursive: true);
        }
        catch
        {
            // A leftover temp directory is not worth failing a measurement over.
        }
    }
}
