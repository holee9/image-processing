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
            Assert.Contains("\"showDisplaySettingsPanel\"", json, StringComparison.Ordinal);
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

            // GUI-C-170b changed this line from Assert.True to Assert.False, on purpose. "showDisplayPanel":
            // true is how EVERY file written before the panel existed looks: the flag defaulted to true, its
            // only menu item was disabled and Reset Layout put it back to true, so it was never a choice. The
            // earlier assertion kept that true and opened the new panel on the first launch after an upgrade.
            Assert.False(loaded.ShowDisplayPanel);
            Assert.False(loaded.ShowCalibrationPanel);
        }
        finally
        {
            File.Delete(path);
        }
    }

    /// <summary>
    /// GUI-C-170b, claim 1: the settings file that shipped BEFORE this card — the real one, byte for byte from
    /// f7ca055 and not a hand-made minimum — loads with the Display Settings panel closed, and every other
    /// value in it still arrives. The second half is the control: a loader that discarded the whole file would
    /// also leave the panel closed.
    /// </summary>
    [Fact]
    public void TheSettingsFileThatShippedBeforeThePanels_LoadsWithTheDisplayPanelClosed_AndKeepsTheRest()
    {
        const string shippedAtF7ca055 = """
            {
              "backendMode": "Mock",
              "rawWidth": 3072,
              "rawHeight": 3072,
              "rawPixelFormat": "UInt16LE",
              "calibOffsetDir": "data/calibration/offset",
              "calibGainDir": "data/calibration/gain",
              "calibDefectDir": "data/calibration/defect",
              "calibOffsetMode": "Auto",
              "calibGainMode": "Auto",
              "calibDefectMode": "Auto",
              "calibGhostMode": "Auto",
              "calibTemperatureMode": "Auto",
              "calibNonlinearityMode": "Auto",
              "calibBinningMode": "Auto",
              "lastRawDir": "",
              "voiWindowCenter": 32768.0,
              "voiWindowWidth": 65535.0,
              "voiLutMode": "Linear",
              "selectedBodyPart": "Abdomen",
              "gsdfEnabled": false,
              "modalityRescaleSlope": 1.0,
              "modalityRescaleIntercept": 0.0,
              "showDisplayPanel": true,
              "comparisonMode": "SwipeVertical",
              "comparisonZoomScale": 0.0,
              "comparisonPanX": 0.0,
              "comparisonPanY": 0.0,
              "comparisonSwipePosition": 0.5,
              "comparisonOverlayOpacity": 0.5
            }
            """;
        var path = Path.Combine(Path.GetTempPath(), $"xpe-c170b-old-{Guid.NewGuid():N}.json");
        File.WriteAllText(path, shippedAtF7ca055);
        try
        {
            var loaded = new AppSettingsService(path).Load().Settings;
            Assert.False(loaded.ShowDisplayPanel);
            Assert.False(loaded.ShowCalibrationPanel);

            Assert.Equal(3072, loaded.RawWidth);
            Assert.Equal(32768.0f, loaded.VoiWindowCenter);
            Assert.Equal("data/calibration/offset", loaded.OffsetCalibrationDirectory);
            Assert.Equal("SwipeVertical", loaded.ComparisonMode);
        }
        finally
        {
            File.Delete(path);
        }
    }

    /// <summary>
    /// GUI-C-170b, claim 2: a panel turned on AFTER the upgrade stays on. Starts from nothing rather than from
    /// the old file, so it isolates "a value this code saved is read back" from claim 1; the two together
    /// show the new key is neither ignored on the way in nor the old one honoured.
    /// </summary>
    [Fact]
    public void ADisplayPanelTurnedOnWithThisCode_StaysOnAfterASaveAndALoad()
    {
        var path = Path.Combine(Path.GetTempPath(), $"xpe-c170b-new-{Guid.NewGuid():N}.json");
        try
        {
            var service = new AppSettingsService(path);
            service.Save(new AppSettings { ShowDisplayPanel = true });

            Assert.True(service.Load().Settings.ShowDisplayPanel);
        }
        finally
        {
            File.Delete(path);
        }
    }
}
