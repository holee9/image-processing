// GUI-C-212 (#249): the one question every screen asks about whether a module's stages may run.
namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// <c>ModuleReadinessGrading.IsProcessingEnabled</c> replaced two different questions about the same module (the Evaluation tab asked "are the exports ready", the Diagnostics and
/// Calibration tabs asked "is processing enabled"). It must answer from the snapshot's <c>ProcessingEnabled</c> alone, by module name, and say no for a module that is absent.
/// </summary>
public sealed class ModuleReadinessProcessingEnabledTests
{
    private static ModuleReadinessSnapshot Snapshot(string module, string level, bool enabled) =>
        new(module, level, "status", "evidence", "next", enabled);

    [Fact]
    public void AnEnabledModule_IsEnabled_AndADisabledOneIsNot_EvenAtTheSameLevel()
    {
        var modules = new[] { Snapshot("xpe_preprocess", "R2", enabled: false), Snapshot("xpe_enhance_basic", "R2", enabled: true) };

        Assert.False(ModuleReadinessGrading.IsProcessingEnabled(modules, "xpe_preprocess"));   // R2 with the oracle failed: exports ready, processing off
        Assert.True(ModuleReadinessGrading.IsProcessingEnabled(modules, "xpe_enhance_basic"));
    }

    [Fact]
    public void AnEnabledNeighbour_DoesNotEnableAnotherModule_AndAnAbsentModuleIsNotEnabled()
    {
        var modules = new[] { Snapshot("xpe_enhance_basic", "R3", enabled: true) };

        Assert.False(ModuleReadinessGrading.IsProcessingEnabled(modules, "xpe_preprocess"));
        Assert.False(ModuleReadinessGrading.IsProcessingEnabled([], "xpe_preprocess"));
    }

    [Fact]
    public void TheModuleNameIsMatchedWithoutRegardToCase()
    {
        var modules = new[] { Snapshot("XPE_PREPROCESS", "R3", enabled: true) };

        Assert.True(ModuleReadinessGrading.IsProcessingEnabled(modules, "xpe_preprocess"));
    }
}
