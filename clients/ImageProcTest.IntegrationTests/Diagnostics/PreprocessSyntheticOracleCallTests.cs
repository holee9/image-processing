// GUI-C-212 (#249): the legacy app's runtime smoke of xpe_preprocess.dll is called by a test.
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.Diagnostics;

/// <summary>
/// <c>XpePreprocessSyntheticOracle</c> decides whether the legacy diagnostic app shows "Synthetic oracle ready" and turns the preprocess stages on. It called the correction functions with a
/// signature the header never had and loaded no calibration, and nothing noticed because NOTHING CALLED IT in a test: it went from passing by accident to failing for the wrong reason
/// (GUI-C-211, measured on screen) and stayed that way for weeks. These tests call it for real, against the staged DLL, and hold what a pass has to mean.
/// </summary>
[Trait("Category", "P1AReady")]
public sealed class PreprocessSyntheticOracleCallTests
{
    private static readonly string? DllPath = XpePreprocessNative.TryFindDll();

    /// <summary>
    /// A pass means the chain RAN: init, calibration generated and loaded, offset → gain → defect each returning OK and each having an effect, input untouched, nothing non-finite, the two runs
    /// identical, and a non-zero output. The old oracle's "NaN/Inf 0, RMSE 0" were zeros of an output nothing wrote; here the output must have a range.
    /// </summary>
    [SkippableFact]
    public void TheOracle_AgainstTheStagedDll_Passes_BecauseEveryStageRan()
    {
        Skip.If(DllPath is null, "Skipped: xpe_preprocess.dll not staged");

        var result = XpePreprocessSyntheticOracle.Run(DllPath!);

        Assert.True(result.Executed, result.Details);
        Assert.True(result.Passed, $"{result.Status}: {result.Details}; stages: {string.Join(", ", result.Stages.Select(s => $"{s.Stage}={s.ErrorCode}/{s.Passed}"))}");
        Assert.Equal("Synthetic oracle pass", result.Status);
        Assert.Equal(["offset", "gain", "defect"], result.Stages.Select(s => s.Stage));
        Assert.All(result.Stages, s =>
        {
            Assert.Equal("OK", s.ErrorCode);
            Assert.True(s.Passed, $"stage {s.Stage}");
            // The gain map of a uniform flat field is 1.0, so the gain stage's only visible effect is the UINT16 -> FLOAT32 conversion (value difference 0); its
            // non-vacuity is the oracle's own check that its output is not all zero. Offset and defect must have changed their input.
            if (s.Stage != "gain")
            {
                Assert.True(s.MaxAbsError > 0, $"stage {s.Stage} left its input unchanged (MaxAbsError {s.MaxAbsError}) — a stage that did nothing is not a pass");
            }
        });
        Assert.True(result.InputPreserved);
        Assert.Equal(result.RawSha256Before, result.RawSha256After);
        Assert.Equal(0, result.NaNInfCount);
        Assert.Equal(0.0, result.DeterminismRmse);
        Assert.True(result.OutputMax > result.OutputMin, $"the output has no range ({result.OutputMin}..{result.OutputMax}): an all-zero output must not pass");
        Assert.True(result.OutputMax > 0);
        Assert.False(string.IsNullOrEmpty(result.OutputSha256));
    }

    /// <summary>The oracle's temporary calibration folder is removed (the card: written at run time, deleted afterwards — nothing left in the user's temp path).</summary>
    [SkippableFact]
    public void TheOracle_LeavesNoTemporaryFolderBehind()
    {
        Skip.If(DllPath is null, "Skipped: xpe_preprocess.dll not staged");

        // Control: the counter sees a folder of that name.
        var marker = Path.Combine(Path.GetTempPath(), $"xpe_oracle_control_{Guid.NewGuid():N}");
        var baseline = CountOracleFolders();
        Directory.CreateDirectory(marker);
        try
        {
            Assert.Equal(baseline + 1, CountOracleFolders());
        }
        finally
        {
            Directory.Delete(marker);
        }

        var before = CountOracleFolders();
        var result = XpePreprocessSyntheticOracle.Run(DllPath!);
        Assert.True(result.Executed, result.Details);
        Assert.Equal(before, CountOracleFolders());
    }

    /// <summary>The negative: a DLL that is not there is "Not run" and never a pass (a verdict that cannot say no would pass everything).</summary>
    [Fact]
    public void TheOracle_WithoutTheDll_IsNotRun_AndNeverPasses()
    {
        var result = XpePreprocessSyntheticOracle.Run(Path.Combine(Path.GetTempPath(), $"no_such_{Guid.NewGuid():N}", "xpe_preprocess.dll"));

        Assert.False(result.Executed);
        Assert.False(result.Passed);
        Assert.Equal("Not run", result.Status);
    }

    private static int CountOracleFolders() => Directory.GetDirectories(Path.GetTempPath(), "xpe_oracle_*").Length;
}
