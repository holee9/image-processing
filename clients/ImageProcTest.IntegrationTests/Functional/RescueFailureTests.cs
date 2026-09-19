// #173 (GUI-C-121): what happens when the rescue itself cannot be performed.
using System.IO;
using ImageProcTest.Services;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// A run that could NOT rescue the original must not overwrite it.
///
/// <para><b>This is the safety condition under the GUI-C-121 decision.</b> A rescuing run replaces the
/// file it just emptied with defaults, so the next launch is quiet instead of repeating a warning
/// nobody reads. That is only sound while the rescue actually happened: replacing a file whose
/// original was never moved aside destroys exactly what GUI-C-119 set out to keep.</para>
///
/// <para><b>The failure is real, not injected.</b> The file is held open by another handle, which is
/// what a backup agent or an editor holding it looks like. An injected exception would measure the
/// handler rather than the situation.</para>
///
/// <para><b>Which share mode matters, and that was measured.</b> With <c>FileShare.None</c> this case
/// passed on a build that overwrote unconditionally — the overwrite failed for the same reason the
/// move did, so the guard was never exercised. <c>FileShare.ReadWrite</c> (without Delete) blocks only
/// the rename, so the write would succeed and the guard is the only thing preventing it.</para>
/// </summary>
public sealed class RescueFailureTests(ITestOutputHelper output)
{
    private const string Corrupt = """
    { "voiWindowCenter": 1234, "laneBAlgorithm":
    """;

    [Fact]
    public void WhenTheOriginalCannotBeMovedAside_ItIsLeftUntouched()
    {
        var directory = Path.Combine(Path.GetTempPath(), $"xpe-c121-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        var path = Path.Combine(directory, "appsettings.json");
        File.WriteAllText(path, Corrupt);

        try
        {
            // The rescue target name is blocked by a DIRECTORY of the same name, so File.Move fails
            // while the settings file itself stays readable and writable. That separation is the point:
            // a lock that blocks everything lets an unconditional overwrite pass this case, because the
            // overwrite fails for the same reason the move did (measured — it did exactly that).
            //
            // Two names are blocked because the name carries a whole-second timestamp and the load may
            // land in the next second.
            var now = DateTime.Now;
            foreach (var stamp in new[] { now, now.AddSeconds(1) })
            {
                Directory.CreateDirectory($"{path}.unreadable-{stamp:yyyyMMdd-HHmmss}");
            }

            var result = new AppSettingsService(path).Load();

            output.WriteLine($"preserved='{result.PreservedOriginalPath}' fromEarlier={result.PreservedIsFromAnEarlierFailure}");
            output.WriteLine($"file after: {File.ReadAllText(path).Trim()}");

            // Nothing was rescued, so nothing is claimed to have been.
            Assert.Null(result.PreservedOriginalPath);
            Assert.False(result.FailedToRead);

            // And the user's file is exactly as it was — not replaced by defaults.
            Assert.Equal(Corrupt.Trim(), File.ReadAllText(path).Trim());
            Assert.Empty(Directory.GetFiles(directory, "appsettings.json.unreadable-*"));   // files, not the blocking directories
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    /// <summary>
    /// The control: with the file NOT held open, the same content is rescued and replaced. Without
    /// this, the case above would pass on a build that never rescues anything.
    /// </summary>
    [Fact]
    public void TheControl_AnUnlockedFileIsRescuedAndReplaced()
    {
        var directory = Path.Combine(Path.GetTempPath(), $"xpe-c121-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        var path = Path.Combine(directory, "appsettings.json");
        File.WriteAllText(path, Corrupt);

        try
        {
            var result = new AppSettingsService(path).Load();
            output.WriteLine($"preserved='{result.PreservedOriginalPath}'");

            Assert.NotNull(result.PreservedOriginalPath);
            Assert.Equal(Corrupt.Trim(), File.ReadAllText(result.PreservedOriginalPath!).Trim());

            // The emptied path was written again, so the next load is quiet.
            Assert.True(File.Exists(path), "The rescuing load left no settings file behind.");
            Assert.False(new AppSettingsService(path).Load().FailedToRead,
                "A second load still reports a failure, so the warning would repeat every launch.");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }
}
