namespace ImageProcTest.IntegrationTests.P1AReady;

public sealed class PreprocessPipelineCallSiteTests
{
    [Fact]
    public void TheGuiRunnerReachesTheStages_ThroughThePipelineFunction_NotByCallingThemItself()
    {
        // GUI-C-267: preserve the single-entry-point guard after the retired stage tests are removed.
        var source = File.ReadAllText(RunnerSource());

        Assert.Contains("xpe_preprocess_pipeline_out(ref input, ref output, ref metadata", source);
        foreach (var byHand in new[] { "xpe_offset_correct(", "xpe_gain_correct(", "xpe_defect_correct(" })
        {
            Assert.DoesNotContain(byHand, source);
        }
    }

    private static string RunnerSource()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null && !Directory.Exists(Path.Combine(dir.FullName, "gui")))
        {
            dir = dir.Parent;
        }

        Assert.True(dir is not null, "The repository root was not found from the test output directory.");
        var path = Path.Combine(dir!.FullName, "gui", "ImageProcTest", "Services", "Native", "GuiPreprocessRunner.cs");
        Assert.True(File.Exists(path), $"{path} does not exist.");
        return path;
    }
}
