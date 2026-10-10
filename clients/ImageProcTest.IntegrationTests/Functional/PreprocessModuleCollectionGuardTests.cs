namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// GUI-C-233g (Codex #176, finding 2): xpe_preprocess.dll has one process-wide state, so every test file that initialises or runs it is declared in <c>PreprocessModuleCollection</c>. Today
/// xunit.runner.json runs everything serially, so this is a protective device for a future parallel run, not a cure; a new file that calls the module and forgets the attribute would be
/// exposed then. This finds it by what the file DOES (its calls), not by its name.
/// </summary>
public sealed class PreprocessModuleCollectionGuardTests
{
    private static readonly string[] CallMarkers =
    [
        "xpe_preprocess_init(",
        "XpePreprocessNative.InitDelegate>(",
        "GuiPreprocessRunner.Run(",
    ];

    private static string ProjectDirectory() =>
        Path.GetDirectoryName(BenchmarkRunnerServiceTests.ResolveRepositoryFile("clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj"))!;

    [Fact]
    public void EveryTestFileThatInitialisesOrRunsThePreprocessModule_IsInThePreprocessModuleCollection()
    {
        var project = ProjectDirectory();
        var callers = new List<string>();
        var seen = 0;
        foreach (var file in Directory.EnumerateFiles(project, "*.cs", SearchOption.AllDirectories))
        {
            var relative = Path.GetRelativePath(project, file).Replace(Path.DirectorySeparatorChar, '/');
            if (relative.StartsWith("obj/", StringComparison.Ordinal) || relative.StartsWith("bin/", StringComparison.Ordinal) || relative.StartsWith("PInvoke/", StringComparison.Ordinal)
                || relative == "Functional/PreprocessModuleCollectionGuardTests.cs")
            {
                continue;
            }

            seen++;
            var text = File.ReadAllText(file);
            if (CallMarkers.Any(marker => text.Contains(marker, StringComparison.Ordinal)) && !text.Contains("PreprocessModuleCollection.Name", StringComparison.Ordinal))
            {
                callers.Add(relative);
            }
        }

        Assert.True(seen > 50, $"the scan saw only {seen} files: it is not looking at the test project");   // control: the scan reads the project
        Assert.True(callers.Count == 0, "these files call the preprocess module but are not in PreprocessModuleCollection: " + string.Join(", ", callers));
    }

    [Fact]
    public void TheScan_FindsTheKnownCallers_SoAnEmptyResultMeansSomething()
    {
        // positive control: the scan's markers do occur in the files this guard exists for
        var project = ProjectDirectory();
        foreach (var known in new[] { "P1AReady/PreprocessCorrectionChainSmokeTests.cs", "Functional/BaselineReviewFixTests.cs", "Functional/DataSizeContractTests.cs" })
        {
            var text = File.ReadAllText(Path.Combine(project, known));
            Assert.Contains(CallMarkers, marker => text.Contains(marker, StringComparison.Ordinal));
        }
    }
}
