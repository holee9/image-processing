// GUI-C-230 (#251): the legacy app's sigma_space box and the module's new upper bound (post QA-B-210 E7, 7.5) are one number, and the box reads its text as the decision says.
using System.Globalization;
using System.Text.RegularExpressions;
using ImageProcTest;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Post QA-B-210 E7 (dev/postprocess 22056f80) limits the bilateral filter's <c>sigma_space</c> to 7.5 and answers a larger one with <c>XPE_ERR_INVALID_INPUT</c>. The app's box used to take up to 100. The app's
/// limit now comes from <see cref="EnhanceBasicInputLimits"/> and nowhere else; what is checked here is that it is the decided number, that the window really reads the box through it, and that it does not
/// drift from the module's own statement of the bound.
///
/// <para><b>Safe before and after the module change.</b> E7 is not in every tree these tests run in. The module's statement of the bound is read from its header comment and its source constants
/// (<see cref="ModuleSigmaSpaceBound"/>): absent in both and no E7 marker anywhere = the module has not changed yet and the check passes; present in both = it must equal the app's number; present in one,
/// unreadable, or an E7 marker with no bound to read = RED. A pass therefore never comes from the scan being blind: the marker the module change leaves behind is looked for as well.</para>
/// </summary>
public sealed class EnhanceBasicInputLimitsTests
{
    private const string HeaderPath = "modules/enhance_basic/include/xpe/enhance_basic/enhance_basic_api.h";
    private const string NoiseSourcePath = "modules/enhance_basic/src/noise_reduce.cpp";
    private const string MainWindowPath = "clients/ImageProcTest/MainWindow.xaml.cs";

    // The module's own lines as post 22056f80 writes them (copied from that commit), and as main has them today.
    private const string HeaderFieldBeforeE7 = "    float              sigma_space;    /**< Bilateral: spatial sigma (default 3.0) */";
    private const string HeaderFieldE7 = "    float              sigma_space;    /**< Bilateral: spatial sigma, 0 < s <= 7.5 (default 3.0) */";
    private const string SourceE7 =
        "// QA-B-210 E7 (#251, user decision): the spatial kernel is truncated at 2 sigma and its radius capped at kMaxBilateralRadius\n" +
        "static constexpr int   kMaxBilateralRadius = 15;\n" +
        "static constexpr float kMaxSigmaSpace      = 0.5f * static_cast<float>(kMaxBilateralRadius);   // 7.5\n";
    private const string SourceBeforeE7 = "static XpeErrorCode apply_bilateral(XpeImageBuffer* img, float sigma_space, float sigma_range)\n{\n    int maxRad = std::min(15, std::min(w, h) / 2 - 1);\n}\n";

    private static string Read(string relative) => File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile(relative)).Replace("\r\n", "\n");

    // ---- the number --------------------------------------------------------------------------------------------------------------------------------------------------------

    [Fact]
    public void TheAppLimit_IsTheDecidedBound_7_5_AndAboveTheMinimum()
    {
        // Pinned from the decision (REQ-ENH-007/020, #251, 2026-10-03): the expectation is the decision's number, not read back from the code under test.
        Assert.Equal(7.5f, EnhanceBasicInputLimits.SigmaSpaceMax);
        Assert.True(EnhanceBasicInputLimits.SigmaSpaceMin > 0f && EnhanceBasicInputLimits.SigmaSpaceMin < EnhanceBasicInputLimits.SigmaSpaceMax);
        Assert.True(EnhanceBasicInputLimits.SigmaSpaceMax <= 100f, "the new limit must not be above what the box accepted before (100): lowering it is safe with and without the module change");
    }

    [Fact]
    public void TheWindow_ReadsTheSigmaSpaceBox_ThroughTheSharedLimits_AndHasNoOtherNumberForIt()
    {
        var lines = Read(MainWindowPath).Split('\n').Where(l => l.Contains("NoiseSigmaSpaceTextBox", StringComparison.Ordinal) && l.Contains("ReadFloat", StringComparison.Ordinal)).ToList();

        var line = Assert.Single(lines);
        Assert.Contains("min: EnhanceBasicInputLimits.SigmaSpaceMin", line);
        Assert.Contains("max: EnhanceBasicInputLimits.SigmaSpaceMax", line);
        Assert.DoesNotMatch(@"max:\s*[0-9]", line);   // no literal limit on that line

        var readFloat = Read(MainWindowPath);
        Assert.Contains("EnhanceBasicInputLimits.ParseClamped(textBox.Text, fallback, min, max)", readFloat);   // ReadFloat is the shared parser, so the tests below are about the window's behaviour
    }

    // ---- the module's statement of the bound -------------------------------------------------------------------------------------------------------------------------------

    [Fact]
    public void TheModulesOwnFiles_AgreeWithTheAppLimit_OrDoNotStateOneYet()
    {
        var header = Read(HeaderPath);
        var source = Read(NoiseSourcePath);

        // control: the scan looks at the real field, so "no bound found" cannot be a blind read
        Assert.Contains("sigma_space;", header);
        Assert.Contains("apply_bilateral", source);

        var verdict = ModuleSigmaSpaceBound.Verdict(EnhanceBasicInputLimits.SigmaSpaceMax, ModuleSigmaSpaceBound.FromHeader(header), ModuleSigmaSpaceBound.FromSource(source), header + source);

        Assert.Null(verdict);
    }

    [Fact]
    public void WithTheModuleChange_TheBoundIsRead_AndEqualsTheAppLimit()
    {
        var header = ModuleSigmaSpaceBound.FromHeader(HeaderFieldE7);
        var source = ModuleSigmaSpaceBound.FromSource(SourceE7);

        Assert.Equal(7.5f, header.Value);
        Assert.Equal(7.5f, source.Value);
        Assert.Null(ModuleSigmaSpaceBound.Verdict(EnhanceBasicInputLimits.SigmaSpaceMax, header, source, HeaderFieldE7 + SourceE7));
    }

    [Fact]
    public void WithoutTheModuleChange_NoBoundIsStated_AndThatPasses()
    {
        var header = ModuleSigmaSpaceBound.FromHeader(HeaderFieldBeforeE7);
        var source = ModuleSigmaSpaceBound.FromSource(SourceBeforeE7);

        Assert.Equal(ModuleSigmaSpaceBound.State.Absent, header.State);
        Assert.Equal(ModuleSigmaSpaceBound.State.Absent, source.State);
        Assert.Null(ModuleSigmaSpaceBound.Verdict(EnhanceBasicInputLimits.SigmaSpaceMax, header, source, HeaderFieldBeforeE7 + SourceBeforeE7));
    }

    [Theory]
    [InlineData("header says 10", "    float              sigma_space;    /**< Bilateral: spatial sigma, 0 < s <= 10 (default 3.0) */", SourceE7)]
    [InlineData("source says 10 (radius 20)", HeaderFieldE7, "// QA-B-210 E7\nstatic constexpr int   kMaxBilateralRadius = 20;\nstatic constexpr float kMaxSigmaSpace      = 0.5f * static_cast<float>(kMaxBilateralRadius);\n")]
    [InlineData("the bound is stated in the header only", HeaderFieldE7, "// QA-B-210 E7\nstatic XpeErrorCode apply_bilateral() {}\n")]
    [InlineData("the bound is stated in the source only", HeaderFieldBeforeE7, SourceE7)]
    [InlineData("E7 marker, no bound anywhere", HeaderFieldBeforeE7, "// QA-B-210 E7: a limit was added here\nstatic XpeErrorCode apply_bilateral() {}\n")]
    [InlineData("the header's field line is gone", "    float              sigma_range;    /**< Bilateral: range sigma */", SourceE7)]
    [InlineData("the source constant is there but not in a readable form", HeaderFieldE7, "// QA-B-210 E7\nstatic constexpr float kMaxSigmaSpace = compute_limit();\n")]
    public void ADisagreement_OrABlindRead_IsRed(string why, string headerText, string sourceText)
    {
        var verdict = ModuleSigmaSpaceBound.Verdict(EnhanceBasicInputLimits.SigmaSpaceMax, ModuleSigmaSpaceBound.FromHeader(headerText), ModuleSigmaSpaceBound.FromSource(sourceText), headerText + sourceText);

        Assert.True(verdict is not null, $"'{why}' must be red, but the check passed");
    }

    [Fact]
    public void AnAppLimitThatDiffersFromTheModulesBound_IsRed()
    {
        // what the old limit (100) would be against the module change
        var verdict = ModuleSigmaSpaceBound.Verdict(100f, ModuleSigmaSpaceBound.FromHeader(HeaderFieldE7), ModuleSigmaSpaceBound.FromSource(SourceE7), HeaderFieldE7 + SourceE7);

        Assert.NotNull(verdict);
        Assert.Contains("7.5", verdict);
    }

    // ---- what the box does with the text (the window's ReadFloat is ParseClamped) ----------------------------------------------------------------------------------

    private const float Fallback = 3.0f;

    private static float Sigma(string? text) =>
        EnhanceBasicInputLimits.ParseClamped(text, Fallback, EnhanceBasicInputLimits.SigmaSpaceMin, EnhanceBasicInputLimits.SigmaSpaceMax);

    [Theory]
    [InlineData("3.0", 3.0f)]
    [InlineData("7.4", 7.4f)]
    [InlineData("7.5", 7.5f)]
    [InlineData("7.6", 7.5f)]       // above the limit: cut to the limit, not rejected and not reset to the default
    [InlineData("8", 7.5f)]
    [InlineData("50", 7.5f)]
    [InlineData("100", 7.5f)]       // what the box accepted before
    [InlineData("1e6", 7.5f)]
    [InlineData("Infinity", 7.5f)]
    [InlineData("0.1", 0.1f)]
    [InlineData("0.05", 0.1f)]
    [InlineData("0", 0.1f)]
    [InlineData("-3", 0.1f)]
    [InlineData("abc", Fallback)]   // not a number: the default
    [InlineData("", Fallback)]
    [InlineData("3,5", Fallback)]   // a decimal comma is not read
    public void TheBoxText_BecomesTheValueTheStageUses_CutToTheLimits(string text, float expected) =>
        Assert.Equal(expected, Sigma(text));

    [Fact]
    public void NoText_IsTheDefault() =>
        Assert.Equal(Fallback, Sigma(null));

    [Fact]
    public void ATextOfNaN_PassesThroughAsNaN_AndTheModuleRefusesIt_PreExistingBehaviour()
    {
        // Observed, not changed (GUI-C-230): float.TryParse reads "NaN" and Math.Clamp keeps it. The module answers a non-finite sigma_space with INVALID_INPUT (REQ-ENH-010, QA-B-181d), so the
        // stage reports a failure rather than running with a made-up value.
        Assert.True(float.IsNaN(Sigma("NaN")));
        Assert.Contains("!std::isfinite(params->sigma_space)", Read(NoiseSourcePath));
    }
}

/// <summary>The module's statement of the bilateral <c>sigma_space</c> bound, read from its header comment and its source constants.</summary>
internal static class ModuleSigmaSpaceBound
{
    internal enum State { Absent, Stated, Unparsable }

    internal sealed record Reading(State State, float? Value, string Where);

    private const string E7Marker = "QA-B-210";

    /// <summary>The header's <c>sigma_space</c> field comment: "0 &lt; s &lt;= N" is a bound; the field without it is Absent; no field line at all is Unparsable (the read would be blind).</summary>
    internal static Reading FromHeader(string header)
    {
        var line = header.Split('\n').FirstOrDefault(l => l.Contains("sigma_space;", StringComparison.Ordinal));
        if (line is null)
        {
            return new Reading(State.Unparsable, null, "header: the sigma_space field line was not found");
        }

        var m = Regex.Match(line, @"0\s*<\s*s\s*<=\s*(?<v>\d+(?:\.\d+)?)");
        return m.Success
            ? new Reading(State.Stated, float.Parse(m.Groups["v"].Value, CultureInfo.InvariantCulture), "header")
            : new Reading(State.Absent, null, "header");
    }

    /// <summary>The source's <c>kMaxSigmaSpace = 0.5f * static_cast&lt;float&gt;(kMaxBilateralRadius)</c> with <c>kMaxBilateralRadius = N</c>: half the radius cap. Not mentioned = Absent; mentioned in another form = Unparsable.</summary>
    internal static Reading FromSource(string source)
    {
        if (!source.Contains("kMaxSigmaSpace", StringComparison.Ordinal))
        {
            return new Reading(State.Absent, null, "source");
        }

        var radius = Regex.Match(source, @"kMaxBilateralRadius\s*=\s*(?<r>\d+)\s*;");
        var half = Regex.Match(source, @"kMaxSigmaSpace\s*=\s*0\.5f\s*\*\s*static_cast<float>\(\s*kMaxBilateralRadius\s*\)");
        return radius.Success && half.Success
            ? new Reading(State.Stated, 0.5f * int.Parse(radius.Groups["r"].Value, CultureInfo.InvariantCulture), "source")
            : new Reading(State.Unparsable, null, "source: kMaxSigmaSpace is there but not as 0.5f * static_cast<float>(kMaxBilateralRadius) with an integer kMaxBilateralRadius");
    }

    /// <summary>Null when the app's limit and the module's statement agree (or the module states none yet and shows no sign of the change); otherwise what is wrong.</summary>
    internal static string? Verdict(float appMax, Reading header, Reading source, string wholeText)
    {
        if (header.State == State.Unparsable) return header.Where;
        if (source.State == State.Unparsable) return source.Where;

        if (header.State == State.Stated && source.State == State.Stated)
        {
            if (header.Value != source.Value) return $"the module states two bounds: {header.Value} (header) and {source.Value} (source)";
            return header.Value == appMax ? null : $"the module's sigma_space bound is {header.Value} but the app limit is {appMax}: change EnhanceBasicInputLimits.SigmaSpaceMax (or the module) so they are one number";
        }

        if (header.State == State.Stated || source.State == State.Stated)
        {
            return "the module states the sigma_space bound in only one of its header and its source: " + (header.State == State.Stated ? "header" : "source");
        }

        return wholeText.Contains(E7Marker, StringComparison.Ordinal)
            ? $"the module carries the {E7Marker} marker but no bound could be read from it: the check would pass blind"
            : null;
    }
}
