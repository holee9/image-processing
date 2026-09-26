// #196 (GUI-C-132) §5: is the gui a host that never re-arms the process-global no-op latch?
using System.Text.RegularExpressions;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// The #196 no-op report is latched on <c>g_calib.nonlin_noop_reported</c>, which is process-global and
/// re-armed in exactly two places — <c>xpe_calib_load_nonlin_lut.cpp:76</c> (load) and <c>:102</c>
/// (unload). pre reported the consequence without checking any particular host: a host that changes
/// calibration profiles WITHOUT calling <c>xpe_calib_unload_nonlin_lut()</c> never gets a second report.
///
/// <para><b>The question is whether the gui is that host, and the answer is yes.</b> The gui loads
/// offset, gain and defect calibration by name and has no call site for the nonlinearity LUT at all —
/// neither load nor unload. So the latch arms on the first preprocess run of a process and is never
/// re-armed for the life of that process.</para>
///
/// <para><b>What that means for a real user</b> is milder than it first sounds, and the reason is the
/// same absence: because the gui can never LOAD a nonlinearity LUT either, the no-op condition never
/// stops holding. One report per process launch is therefore complete rather than lossy — there is no
/// second condition to report. It would become lossy the moment the gui gains a way to load a LUT, and
/// at that point the unload call has to arrive with it.</para>
///
/// <para><b>Why a source guard and not a runtime one.</b> A runtime measurement cannot tell "never
/// called" from "called and the outcome was the same" — the frame is byte-identical either way. Only
/// the call sites say it, so this reads them, and pairs the absence claim with a control: the same
/// search, over the same files, finds the loaders the gui DOES call. Without that control an empty
/// result is indistinguishable from a search that was looking in the wrong place.</para>
/// </summary>
[Trait("Category", "Functional")]
public sealed class NonlinearityLatchRearmTests(Xunit.Abstractions.ITestOutputHelper output)
{
    [Fact]
    public void TheGui_NeverLoadsOrUnloadsTheNonlinearityLut_WhileItDoesLoadTheOthers()
    {
        var sources = GuiSources();
        output.WriteLine($"files searched: {sources.Length}");

        var text = string.Join("\n", sources.Select(File.ReadAllText));

        // The control first: if these are absent too, the search is not reading what it thinks it is,
        // and the absence claim below would pass vacuously.
        var offset = Count(text, "xpe_calib_load_offset");
        var gain = Count(text, "xpe_calib_load_gain");
        var defect = Count(text, "xpe_calib_load_defect_map");
        output.WriteLine($"control — load_offset: {offset}, load_gain: {gain}, load_defect_map: {defect}");

        Assert.True(offset > 0 && gain > 0 && defect > 0,
            "The control failed: the gui's known calibration loaders were not found, so this search " +
            "cannot support any claim about what is absent.");

        // The claim.
        var load = Count(text, "xpe_calib_load_nonlin_lut");
        var unload = Count(text, "xpe_calib_unload_nonlin_lut");
        output.WriteLine($"claim — load_nonlin_lut: {load}, unload_nonlin_lut: {unload}");

        Assert.Equal(0, load);
        Assert.Equal(0, unload);
    }

    /// <summary>
    /// The stage IS invoked — the latch is armed by a real call, not by nothing happening. Without this
    /// the case above would also pass on a gui that never ran the nonlinearity stage at all, which is a
    /// different world with the same absence.
    /// </summary>
    [Fact]
    public void TheGui_DoesInvokeTheNonlinearityStage_WithANullConfig()
    {
        var text = File.ReadAllText(RunnerSource());
        var match = Regex.Match(text, @"xpe_nonlinearity_correct\(\s*ref\s+\w+\s*,\s*(\w+)\s*\)");

        Assert.True(match.Success, "GuiPreprocessRunner no longer calls xpe_nonlinearity_correct.");
        output.WriteLine($"call site passes config = {match.Groups[1].Value}");
        Assert.Equal("null", match.Groups[1].Value);
    }

    private static int Count(string text, string needle) =>
        Regex.Matches(text, Regex.Escape(needle)).Count;

    private static string[] GuiSources() =>
        Directory.GetFiles(Path.Combine(RepositoryRoot(), "gui"), "*.cs", SearchOption.AllDirectories);

    private static string RunnerSource()
    {
        var path = Path.Combine(RepositoryRoot(), "gui", "ImageProcTest", "Services", "Native",
                                "GuiPreprocessRunner.cs");
        Assert.True(File.Exists(path), $"{path} does not exist.");
        return path;
    }

    private static string RepositoryRoot()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null && !Directory.Exists(Path.Combine(dir.FullName, "gui")))
        {
            dir = dir.Parent;
        }

        Assert.True(dir is not null, "The repository root was not found.");
        return dir!.FullName;
    }
}
