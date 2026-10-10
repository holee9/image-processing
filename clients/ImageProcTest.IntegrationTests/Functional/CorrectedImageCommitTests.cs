// GUI-C-232b (Codex #165): the Save Corrected Image candidate is committed with the render, from the result being committed, and from nowhere else.
namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Source pins for the ordering that keeps a late, overtaken Apply from replacing the save candidate: the backend keeps no "latest run" slot (the candidate hangs on the chain result of its own run), and the
/// view model asks for the candidate of the result it is committing, after the checks that drop a stale or cancelled result and before the frame it belongs to is made current.
/// </summary>
public sealed class CorrectedImageCommitTests
{
    private static string Read(string relative) => File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile(relative)).Replace("\r\n", "\n");

    [Fact]
    public void TheBackend_KeepsNoGlobalLatestCandidate_ItAttachesTheCandidateToItsOwnChainResult()
    {
        var backend = Read("gui/ImageProcTest/Services/RealXpeBackend.cs");

        Assert.DoesNotContain("Volatile.Write(ref _corrected", backend);
        Assert.DoesNotContain("private CorrectedImage? _corrected;", backend);
        Assert.Contains("ConditionalWeakTable<ChainResult, CorrectedImage> _candidates", backend);
        Assert.Contains("_candidates.Add(result, candidate);", backend);
        Assert.Contains("public CorrectedImage? CorrectedFor(ChainResult chain)", backend);
    }

    [Fact]
    public void TheViewModel_CommitsTheCandidateOfTheCommittedResult_AfterTheStaleAndCancelChecks()
    {
        var vm = Read("gui/ImageProcTest/ViewModels/MainWindowViewModel.cs");
        var start = vm.IndexOf("private async Task ApplyDisplayPipelineAsync()", StringComparison.Ordinal);
        Assert.True(start > 0, "ApplyDisplayPipelineAsync was not found");
        var body = vm[start..];

        var stale = body.IndexOf("if (!IsCurrent(ticket))", StringComparison.Ordinal);
        var cancelled = body.IndexOf("if (cancellation.IsCancellationRequested)", StringComparison.Ordinal);
        var commit = body.IndexOf("_committedCorrected = (backend as ICorrectedImageSource)?.CorrectedFor(chain);", StringComparison.Ordinal);
        var frame = body.IndexOf("ActiveImageFrame = processedFrame;", StringComparison.Ordinal);

        Assert.True(stale > 0 && cancelled > stale, "the stale and cancel checks were not found in the expected order");
        Assert.True(commit > cancelled, "the candidate is committed before the stale/cancel checks have run");
        Assert.True(frame > commit, "the candidate must be committed before the frame it belongs to becomes current");
        Assert.Single(System.Text.RegularExpressions.Regex.Matches(vm, @"_committedCorrected\s*=\s*(?!\s*null\b)"));   // one writer of a committed candidate; clearing it (= null) is InvalidateRender's job (GUI-C-233g) and is not a writer
    }
}
