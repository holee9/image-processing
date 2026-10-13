// #173 (GUI-C-115): the workbench's algorithm options, and what a stored choice from before the
// rename resolves to.
using ImageProcTest.Models;
using Xunit;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Pins the option names and the old-name mapping.
///
/// <para><b>Why a test and not a comment.</b> The options were renamed from version labels
/// ("Production v1.2") to what each one runs ("Grid suppression"), because a version label asserts a
/// released build that has no separate code path. Renaming moves every stored choice: a settings file
/// written before the rename names an option that no longer exists, and <see cref="AlgorithmPreset.For"/>
/// falls back to the first preset. Without the mapping below, someone who chose "Candidate v1.5-rc"
/// yesterday would today be looking at NO correction, with nothing on screen saying so.</para>
///
/// <para>The fallback itself is deliberate and stays — refusing to render an unknown name would be a
/// worse answer than rendering the baseline. What this file prevents is the fallback swallowing names
/// that DO have a meaning.</para>
/// </summary>
public sealed class AlgorithmPresetTests
{
    /// <summary>Each old name resolves to the preset that runs what it always ran.</summary>
    [Theory]
    [InlineData("Baseline v1.0", "No correction", false, GsvgModes.None)]
    [InlineData("Production v1.2", "Grid suppression", false, GsvgModes.GridSuppression)]
    [InlineData("Candidate v1.4", "Virtual grid", false, GsvgModes.VirtualGrid)]
    [InlineData("Candidate v1.5-rc", "Preprocess + virtual grid", true, GsvgModes.VirtualGrid)]
    public void AStoredNameFromBeforeTheRename_StillRunsWhatItRan(
        string stored, string expectedName, bool expectedPreprocess, string expectedGsvg)
    {
        var preset = AlgorithmPreset.For(stored);

        Assert.Equal(expectedName, preset.Name);
        Assert.Equal(expectedPreprocess, preset.PreprocessInChain);
        Assert.Equal(expectedGsvg, preset.GsvgMode);
    }

    /// <summary>
    /// Every old name maps to a preset that exists. A rename that drops one would otherwise leave an
    /// entry here pointing at nothing, and the lookup would fall through to the baseline again.
    /// </summary>
    [Fact]
    public void EveryOldName_MapsToALivePreset()
    {
        foreach (var (old, current) in AlgorithmPreset.RenamedFrom)
        {
            Assert.True(
                AlgorithmPreset.All.Any(p => p.Name == current),
                $"'{old}' is mapped to '{current}', which is not one of the presets any more.");
        }
    }

    /// <summary>
    /// The names say what runs: no option may carry a version number. This is the defect the rename
    /// fixed, stated so it cannot come back by habit.
    /// </summary>
    [Fact]
    public void NoOptionName_ClaimsAVersion()
    {
        foreach (var preset in AlgorithmPreset.All)
        {
            Assert.DoesNotMatch(@"v\d", preset.Name);
        }
    }

    /// <summary>
    /// GUI-C-234c (Codex #181 finding 4): the Reference lane choice (<see cref="AlgorithmPreset.ApplyToMain"/>) is the grid correction only. No preset switches the main preprocessing OFF (three of the four
    /// are defined with it off, for the Candidate lane own copy); the one that includes preprocessing may switch it ON. Control: <see cref="AlgorithmPreset.ApplyTo"/>, the Candidate lane copy, still writes it.
    /// </summary>
    [Fact]
    public void ChoosingAPresetForTheMainChain_ChangesTheGridCorrection_AndNeverSwitchesPreprocessingOff()
    {
        foreach (var preset in AlgorithmPreset.All)
        {
            var main = new AppSettings { PreprocessInChain = true, GsvgMode = GsvgModes.None };
            preset.ApplyTo(main);
            Assert.True(main.PreprocessInChain, $"'{preset.Name}' switched the main preprocessing off.");
            Assert.Equal(preset.GsvgMode, main.GsvgMode);

            var operatorOff = new AppSettings { PreprocessInChain = false };
            preset.ApplyTo(operatorOff);
            Assert.Equal(preset.PreprocessInChain, operatorOff.PreprocessInChain);   // only a preset that includes preprocessing turns it on; none turns it off
        }

        // GUI-C-234f: the Candidate lane copy follows the same rule (one rule for both lanes) - a lane is a function of its algorithm and the shared preprocessing switch.
        var candidate = new AppSettings { PreprocessInChain = true };
        AlgorithmPreset.For("Virtual grid").ApplyTo(candidate);
        Assert.True(candidate.PreprocessInChain);
        Assert.Equal(GsvgModes.VirtualGrid, candidate.GsvgMode);
    }

    /// <summary>A name nobody ever used still falls back rather than throwing.</summary>
    [Fact]
    public void AnUnknownName_FallsBackToTheFirstPreset()
    {
        Assert.Equal(AlgorithmPreset.All[0], AlgorithmPreset.For("something from another build"));
        Assert.Equal(AlgorithmPreset.All[0], AlgorithmPreset.For(null));
    }
}
