// #196 (GUI-C-132; doc refreshed GUI-C-136 after QA-A-142 removed the latch): the gui's LUT call sites.
using System.Text.RegularExpressions;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// What the gui does and does not call around the nonlinearity LUT. Two facts, both about call sites.
///
/// <para><b>Written for a question that has since been answered elsewhere.</b> GUI-C-132 asked whether
/// the gui is a host that never re-arms the #196 suppression latch, because that latch was process-global
/// and re-armed only on LUT load/unload. The answer was yes — and the measurement went further: the latch
/// did not survive a single frame here either, because <c>xpe_preprocess_shutdown</c> clears every module
/// global (#176) and put it back on every run. <c>QA-A-142</c> (<c>f772b31</c>) then REMOVED the latch,
/// citing exactly that. So there is no latch to re-arm any more.</para>
///
/// <para><b>The two facts outlived the latch, which is why this file stays.</b> The gui loads offset, gain
/// and defect calibration by name and has NO call site for the nonlinearity LUT — neither load nor unload
/// — and it does invoke the stage, with a null config. Together they say why the no-op condition holds on
/// every frame: the gui has no way to stop it holding. If a way to load a LUT is ever added, the unload
/// call has to arrive with it, and the first case here is what will notice its absence.</para>
///
/// <para><b>Why a source guard and not a runtime one.</b> A runtime measurement cannot tell "never
/// called" from "called and the outcome was the same" — the frame is byte-identical either way. Only
/// the call sites say it, so this reads them, and pairs the absence claim with a control: the same
/// search, over the same files, finds the loaders the gui DOES call. Without that control an empty
/// result is indistinguishable from a search that was looking in the wrong place.</para>
/// </summary>
[Trait("Category", "Functional")]
public sealed class NonlinearityLutCallSiteTests(Xunit.Abstractions.ITestOutputHelper output)
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
    /// The stage IS invoked — the no-op condition is reached by a real call, not by nothing happening.
    /// Without this
    /// the case above would also pass on a gui that never ran the nonlinearity stage at all, which is a
    /// different world with the same absence.
    /// </summary>
    [Fact]
    public void TheGui_CallsPipelineOut_AndHandsTheNonlinearitySettingOverInTheConfig()
    {
        // GUI-C-232b (leader decision): the shipped path is xpe_preprocess_pipeline_out alone (SRS-CALIB-SAFE-004); the app no longer calls xpe_nonlinearity_correct by hand. This used to pin that
        // hand call; what the app owes the nonlinearity stage now is that it reaches it through the pipeline function and states its setting in the config it passes (bypassNonlinearity).
        var text = File.ReadAllText(RunnerSource());

        Assert.DoesNotContain("xpe_nonlinearity_correct(", text);

        // QA-A-322: since QA-A-274e (a3a573a8) the config is built per run from the operator's offset/gain/defect selection, so the pipeline call passes a
        // parameter, not the PipelineConfigJson constant. The property is unchanged — the config the pipeline RECEIVES states the nonlinearity setting — so this
        // follows the argument actually passed back to where it is built (call -> RunPipelineOut parameter -> its one call site -> the serialized object),
        // instead of reading a constant the call no longer uses.
        var call = Regex.Match(text, @"xpe_preprocess_pipeline_out\(\s*ref\s+\w+\s*,\s*ref\s+\w+\s*,\s*ref\s+\w+\s*,\s*IntPtr\.Zero\s*,\s*IntPtr\.Zero\s*,\s*(?<arg>\w+)\s*\)");
        Assert.True(call.Success, "The xpe_preprocess_pipeline_out call (ref, ref, ref, IntPtr.Zero, IntPtr.Zero, <config>) was not found.");
        var passed = call.Groups["arg"].Value;

        var declaration = Regex.Match(text, @"RunPipelineOut\((?<params>[^)]*)\)\s*\{");
        Assert.True(declaration.Success, "The RunPipelineOut declaration was not found.");
        var parameters = declaration.Groups["params"].Value.Split(',')
            .Select(p => Regex.Match(p.Trim(), @"(\w+)$").Groups[1].Value).ToList();
        var position = parameters.IndexOf(passed);
        Assert.True(position >= 0, $"The config passed to the pipeline ('{passed}') is not a RunPipelineOut parameter: [{string.Join(", ", parameters)}].");

        // Exactly one call site: a second path handing the pipeline a different config would otherwise go unread.
        var callSite = Assert.Single(Regex.Matches(text, @"(?:return|=)\s+RunPipelineOut\((?<args>[^;]*?)\);"));
        var arguments = callSite.Groups["args"].Value.Split(',');
        Assert.True(arguments.Length > position, "The RunPipelineOut call site has fewer arguments than its declaration.");
        var source = arguments[position].Trim();

        var built = Regex.Match(text, @"var\s+" + Regex.Escape(source) + @"\s*=\s*System\.Text\.Json\.JsonSerializer\.Serialize\(\s*new\s*\{(?<body>[^}]*)\}");
        Assert.True(built.Success, $"'{source}' (the argument the call site passes as '{passed}') is not built by JsonSerializer.Serialize(new {{ ... }}).");
        output.WriteLine($"config handed to the pipeline: {passed} <- {source} = {{{Regex.Replace(built.Groups["body"].Value, @"\s+", " ").Trim()}}}");
        Assert.Matches(@"\bbypassNonlinearity\s*=\s*true\b", built.Groups["body"].Value);
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
