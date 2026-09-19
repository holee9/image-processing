// #173 (GUI-C-118): what a corrupt settings file costs. MEASUREMENT — these cases record the current
// behaviour, they do not assert a desired one.
using System.IO;
using System.Text;
using ImageProcTest.Models;
using ImageProcTest.Services;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// <see cref="AppSettingsService.Load"/> catches every exception and returns a fresh
/// <see cref="AppSettings"/>. So a settings file that cannot be parsed does not crash the app — it
/// silently replaces every stored setting with a default, and nothing on the return path says so.
///
/// <para><b>These cases measure, they do not prescribe.</b> Three ways a file goes bad (truncated,
/// wrong type, wrong encoding), and for each: what survives, whether the caller can tell, and what the
/// next Save writes over. The fix direction is the lead's call once the measurements are in.</para>
///
/// <para>The control in every case is a file whose OTHER keys are perfectly good — so "nothing was
/// read" is distinguishable from "the file was read and one key was skipped" (the shape
/// <c>RetiredSettingKeyTests</c> established).</para>
/// </summary>
public sealed class CorruptSettingsFileTests(ITestOutputHelper output)
{
    /// <summary>A file that would load cleanly, used to show the values are otherwise readable.</summary>
    private const string Good = """
    {
      "voiWindowCenter": 1234,
      "laneBAlgorithm": "Virtual grid",
      "comparisonZoomScale": 2.5
    }
    """;

    public static TheoryData<string, string> Corruptions() => new()
    {
        // 1. truncated — the writer died mid-save, or the disk filled.
        { "truncated", """
          {
            "voiWindowCenter": 1234,
            "laneBAlgorithm": "Virtual grid",
            "comparisonZoomScale":
          """ },
        // 2. wrong type — a hand edit, or a key whose type changed between versions.
        { "wrong-type", """
          {
            "voiWindowCenter": "not a number",
            "laneBAlgorithm": "Virtual grid",
            "comparisonZoomScale": 2.5
          }
          """ },
    };

    [Theory]
    [MemberData(nameof(Corruptions))]
    public void ACorruptFile_Measured(string kind, string content)
    {
        var path = NewPath();
        File.WriteAllText(path, content);
        Measure(kind, path);
    }

    /// <summary>
    /// 3. wrong encoding — the file is UTF-16, which is what a Windows tool writing "Unicode" produces.
    /// Written as bytes rather than text so the encoding really is wrong on disk.
    /// </summary>
    [Fact]
    public void AUtf16File_Measured()
    {
        var path = NewPath();
        File.WriteAllBytes(path, Encoding.Unicode.GetBytes(Good));
        Measure("utf-16", path);
    }

    /// <summary>The control: the same values in a well-formed file ARE read.</summary>
    [Fact]
    public void TheControl_AGoodFileIsRead()
    {
        var path = NewPath();
        File.WriteAllText(path, Good);
        try
        {
            var loaded = new AppSettingsService(path).Load().Settings;
            output.WriteLine($"control: center={loaded.VoiWindowCenter} algorithm='{loaded.LaneBAlgorithm}' zoom={loaded.ComparisonZoomScale}");
            Assert.Equal(1234, loaded.VoiWindowCenter);
            Assert.Equal("Virtual grid", loaded.LaneBAlgorithm);
        }
        finally
        {
            File.Delete(path);
        }
    }

    /// <summary>
    /// One corruption, measured on three axes: what survived, what the caller can tell, and what the
    /// next Save leaves on disk.
    /// </summary>
    private void Measure(string kind, string path)
    {
        try
        {
            var before = File.ReadAllBytes(path);
            var service = new AppSettingsService(path);
            var defaults = new AppSettings();

            var result = service.Load();
            var loaded = result.Settings;

            // (a) what survived
            var survivedCenter = loaded.VoiWindowCenter == 1234;
            var survivedAlgorithm = loaded.LaneBAlgorithm == "Virtual grid";
            var atDefaults = loaded.VoiWindowCenter == defaults.VoiWindowCenter
                && loaded.LaneBAlgorithm == defaults.LaneBAlgorithm
                && Math.Abs(loaded.ComparisonZoomScale - defaults.ComparisonZoomScale) < 0.0001;

            output.WriteLine($"[{kind}] survived: center={survivedCenter} algorithm={survivedAlgorithm}; everything at defaults={atDefaults}");
            output.WriteLine($"[{kind}] loaded: center={loaded.VoiWindowCenter} algorithm='{loaded.LaneBAlgorithm}' zoom={loaded.ComparisonZoomScale}");

            // (b) what the caller can tell — GUI-C-119 gave the failure a place to live.
            output.WriteLine($"[{kind}] Load reports: failedToRead={result.FailedToRead}; preserved='{result.PreservedOriginalPath}'");
            Assert.True(result.FailedToRead, "The load did not report that the file was unreadable.");

            // (c) what the next Save does to the original
            service.Save(loaded);
            var after = File.ReadAllBytes(path);
            var originalGone = !before.AsSpan().SequenceEqual(after);
            output.WriteLine($"[{kind}] after Save: original bytes preserved={!originalGone}; file is now {after.Length} bytes (was {before.Length})");
            output.WriteLine($"[{kind}] backup alongside: {string.Join(", ", Directory.GetFiles(Path.GetDirectoryName(path)!, Path.GetFileNameWithoutExtension(path) + "*"))}");
        }
        finally
        {
            File.Delete(path);
        }
    }

    private static string NewPath() =>
        Path.Combine(Path.GetTempPath(), $"xpe-c118-{Guid.NewGuid():N}.json");
}
