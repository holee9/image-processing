// #225 rows 7 and 8 (GUI-C-170): the two panel flags are persisted Settings values.
using System.IO;
using ImageProcTest.Models;
using ImageProcTest.Services;
using Xunit;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Whether the panels are open is stored with the settings, and comes back.
///
/// <para>This is the file half of "turn it on, close the app, open it again": the E2E case A-13 does the
/// same through two real launches. It stays here as well because it isolates the SERIALIZER from the app —
/// a key that is not written, or not read back, fails here in milliseconds and by name.</para>
/// </summary>
public sealed class PanelFlagPersistenceTests
{
    [Fact]
    public void BothPanelFlags_AreOffByDefault()
    {
        var fresh = new AppSettings();
        Assert.False(fresh.ShowCalibrationPanel);
        Assert.False(fresh.ShowDisplayPanel);
    }

    [Fact]
    public void BothPanelFlags_SurviveASaveAndALoad_AndAreIndependent()
    {
        var path = Path.Combine(Path.GetTempPath(), $"xpe-c170-{Guid.NewGuid():N}.json");
        try
        {
            var service = new AppSettingsService(path);

            // One on, one off — a flag copied to its sibling, or one key shadowing the other, is caught here
            // and not by "both on".
            service.Save(new AppSettings { ShowCalibrationPanel = true, ShowDisplayPanel = false });
            var first = service.Load().Settings;
            Assert.True(first.ShowCalibrationPanel);
            Assert.False(first.ShowDisplayPanel);

            service.Save(new AppSettings { ShowCalibrationPanel = false, ShowDisplayPanel = true });
            var second = service.Load().Settings;
            Assert.False(second.ShowCalibrationPanel);
            Assert.True(second.ShowDisplayPanel);

            var json = File.ReadAllText(path);
            Assert.Contains("\"showCalibrationPanel\"", json, StringComparison.Ordinal);
            Assert.Contains("\"showDisplayPanel\"", json, StringComparison.Ordinal);
        }
        finally
        {
            File.Delete(path);
        }
    }

    /// <summary>
    /// Control: a settings file written BEFORE the calibration flag existed still loads everything else, and
    /// the missing key reads as the default (off) — without the control, "false" above could be a key that
    /// is never read at all.
    /// </summary>
    [Fact]
    public void AFileWithoutTheCalibrationKey_LoadsWithThePanelOff_AndKeepsTheRest()
    {
        var path = Path.Combine(Path.GetTempPath(), $"xpe-c170-old-{Guid.NewGuid():N}.json");
        File.WriteAllText(path, "{ \"voiWindowCenter\": 1234, \"showDisplayPanel\": true }");
        try
        {
            var loaded = new AppSettingsService(path).Load().Settings;
            Assert.Equal(1234, loaded.VoiWindowCenter);
            Assert.True(loaded.ShowDisplayPanel);
            Assert.False(loaded.ShowCalibrationPanel);
        }
        finally
        {
            File.Delete(path);
        }
    }
}
