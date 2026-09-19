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

    /// <summary>
    /// Truncated mid-key — one of the three shapes measured in GUI-C-118.
    ///
    /// <para><b>Why only one shape here.</b> GUI-C-118 measured all three (truncated, wrong type,
    /// wrong encoding) and found they converge on the same place: the single catch in
    /// <c>AppSettingsService.Load</c>, with identical results on every axis. The unit-level cases
    /// still cover all three; this launch exercises the path they share, and a second launch would
    /// re-measure the same branch at the cost of a second process.</para>
    /// </summary>
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
            var preserved = Rescued(path);
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
            var preserved = Rescued(path);
            Assert.True(preserved.Length == 0, $"A readable file was moved aside anyway: {string.Join(", ", preserved)}");
        }
        finally
        {
            CleanUp(path);
        }
    }

    /// <summary>Corrupt a second time, and different from the first, so the two are distinguishable.</summary>
    private const string CorruptAgain = """
    { "voiWindowCenter": 9999, "backendMode":
    """;

    /// <summary>
    /// A second read failure must not overwrite the FIRST rescued original (#173, GUI-C-120).
    ///
    /// <para><b>The intuition here is backwards, so it is worth stating.</b> The file rescued at the
    /// FIRST failure is the user's real settings. After that, <c>appsettings.json</c> is rewritten from
    /// defaults — GUI-C-118 measured the next Save putting 1482 bytes over it — so anything a later
    /// failure rescues is closer to defaults than to what the user had. Keeping the newest and dropping
    /// the oldest would discard precisely the thing worth keeping.</para>
    ///
    /// <para>So the second failure leaves the first rescue alone, and the file count stays at one
    /// rather than growing. Not accumulating beats cleaning up: no one has to own the cleanup.</para>
    ///
    /// <para><b>Both halves are asserted.</b> Keeping the file without saying where it is leaves the
    /// user unable to find their settings, which is the same silence this issue has been removing —
    /// so the second launch's message has to name the earlier rescue.</para>
    /// </summary>
    [SkippableFact]
    public void ASecondFailure_DoesNotOverwriteTheFirstRescue()
    {
        var path = NewSettingsPath();
        File.WriteAllText(path, CorruptSettings);

        try
        {
            using (var first = new SettingsFileApplicationFixture(path))
            {
                Skip.If(!first.IsAvailable, first.SkipReason ?? "The application is not available.");
                output.WriteLine($"first launch status: '{StatusBarText(first)}'");
            }

            var afterFirst = Rescued(path);
            Assert.True(afterFirst.Length == 1, $"The first launch rescued {afterFirst.Length} files, expected 1.");
            Assert.Equal(CorruptSettings.Trim(), File.ReadAllText(afterFirst[0]).Trim());

            // The app was left with no settings file; a later run writes a fresh one, which can go bad
            // in its turn. That second failure is the case under test.
            File.WriteAllText(path, CorruptAgain);

            string secondStatus;
            using (var second = new SettingsFileApplicationFixture(path))
            {
                Skip.If(!second.IsAvailable, second.SkipReason ?? "The application is not available.");
                secondStatus = StatusBarText(second);
            }

            output.WriteLine($"second launch status: '{secondStatus}'");

            var afterSecond = Rescued(path);
            output.WriteLine($"rescued after second: {string.Join(", ", afterSecond)}");

            // ① the count does not grow ② and the survivor is the FIRST original, not the second.
            Assert.True(afterSecond.Length == 1,
                $"A second failure left {afterSecond.Length} rescued files; they accumulate with nobody to remove them.");
            Assert.Equal(CorruptSettings.Trim(), File.ReadAllText(afterSecond[0]).Trim());

            // ③ and the user is told where that earlier original is.
            Assert.Contains(Path.GetFileName(afterSecond[0]), secondStatus, StringComparison.Ordinal);
            Assert.Contains("earlier", secondStatus, StringComparison.OrdinalIgnoreCase);

            // ④ and the second unreadable file is left exactly where it was (#173, GUI-C-121). Only a
            // run that rescued may replace the file; this one rescued nothing, so replacing it would
            // destroy a file while telling the user their settings are safe somewhere else.
            Assert.Equal(CorruptAgain.Trim(), File.ReadAllText(path).Trim());
        }
        finally
        {
            CleanUp(path);
        }
    }

    /// <summary>
    /// After a rescue, the next launch is quiet (#173, GUI-C-121).
    ///
    /// <para>Once the original is safely aside, leaving the unreadable file in place means every later
    /// launch repeats the same warning — and a warning that always appears is one nobody reads. So a
    /// run that DID rescue writes a fresh defaults file over the path it just emptied; the next run
    /// reads it and says nothing.</para>
    ///
    /// <para>Both halves again: "the second launch is quiet" alone would pass on an app that never
    /// warns at all, so the first launch's warning is asserted in the same case.</para>
    /// </summary>
    [SkippableFact]
    public void AfterARescue_TheNextLaunchIsQuiet()
    {
        var path = NewSettingsPath();
        File.WriteAllText(path, CorruptSettings);

        try
        {
            string firstStatus;
            using (var first = new SettingsFileApplicationFixture(path))
            {
                Skip.If(!first.IsAvailable, first.SkipReason ?? "The application is not available.");
                firstStatus = StatusBarText(first);
            }

            output.WriteLine($"first launch status: '{firstStatus}'");
            Assert.Contains("could not be read", firstStatus, StringComparison.OrdinalIgnoreCase);

            // The rescuing run replaced the file it emptied, so the path is readable again.
            Assert.True(File.Exists(path), "The rescuing run left no settings file behind at all.");
            output.WriteLine($"file after rescue: {new FileInfo(path).Length} bytes");

            string secondStatus;
            using (var second = new SettingsFileApplicationFixture(path))
            {
                Skip.If(!second.IsAvailable, second.SkipReason ?? "The application is not available.");
                secondStatus = StatusBarText(second);
            }

            output.WriteLine($"second launch status: '{secondStatus}'");
            Assert.DoesNotContain("could not be read", secondStatus, StringComparison.OrdinalIgnoreCase);

            // And the rescue is still the only one, still holding the user's file.
            var rescued = Rescued(path);
            Assert.True(rescued.Length == 1, $"Expected one rescue, found {rescued.Length}.");
            Assert.Equal(CorruptSettings.Trim(), File.ReadAllText(rescued[0]).Trim());
        }
        finally
        {
            CleanUp(path);
        }
    }

    private static string[] Rescued(string path) =>
        Directory.GetFiles(Path.GetDirectoryName(path)!, Path.GetFileName(path) + ".unreadable-*");

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
