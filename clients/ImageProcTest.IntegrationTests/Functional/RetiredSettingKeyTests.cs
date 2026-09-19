// #173 / #193 (GUI-C-117): what happens to a settings file written before the Lane B overrides changed.
using System.IO;
using ImageProcTest.Models;
using ImageProcTest.Services;
using Xunit;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// A stored settings file naming keys that no longer exist must still load.
///
/// <para><b>Why this is not obvious.</b> <see cref="AppSettingsService.Load"/> catches every exception
/// and returns a fresh <see cref="AppSettings"/>. So a key that made deserialization throw would not
/// crash the app — it would silently discard EVERY stored setting, which is worse than a crash because
/// nothing reports it. This file measures that the retired keys are skipped rather than thrown on, and
/// pairs that with a control: a live key in the same file is still read.</para>
///
/// <para>The two retired keys: <c>laneBSharpeningSigma</c>, removed because the chain has no sharpening
/// stage for it to mean (#193), and <c>laneBDenoiseStrength</c>, renamed to <c>laneBGsvgDenoiseK</c>
/// after what it overrides.</para>
/// </summary>
public sealed class RetiredSettingKeyTests
{
    [Fact]
    public void ASettingsFileWithTheRetiredLaneBKeys_StillLoadsEverythingElse()
    {
        var path = Path.Combine(Path.GetTempPath(), $"xpe-c117-{Guid.NewGuid():N}.json");
        File.WriteAllText(path, """
        {
          "laneBSharpeningSigma": 0.85,
          "laneBDenoiseStrength": 0.42,
          "voiWindowCenter": 1234,
          "laneBAlgorithm": "Virtual grid"
        }
        """);

        try
        {
            var loaded = new AppSettingsService(path).Load();

            // The control: this file WAS read, rather than thrown away and replaced by defaults.
            Assert.Equal(1234, loaded.VoiWindowCenter);
            Assert.Equal("Virtual grid", loaded.LaneBAlgorithm);

            // The renamed key is not carried over — 0.42 is not resurrected under the new name.
            Assert.Equal(2.0, loaded.LaneBGsvgDenoiseK, 4);
        }
        finally
        {
            File.Delete(path);
        }
    }
}
