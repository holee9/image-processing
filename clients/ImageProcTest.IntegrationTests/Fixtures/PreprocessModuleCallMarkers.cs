namespace ImageProcTest.IntegrationTests.Fixtures;

/// <summary>
/// GUI-C-233k (Codex #179): the source text that means "this test file initialises or runs the preprocess module". ONE list for the collection guard and the leak sweep. Direct calls are not enough:
/// a test that only calls a helper which initialises the module (<c>LoadCalibrated</c>, <c>PrepareMapsThroughTheModule</c>) leaves the module in the same state, so the helpers are markers too.
/// A new helper that initialises the module belongs in this list.
/// </summary>
internal static class PreprocessModuleCallMarkers
{
    internal static readonly string[] All =
    [
        "xpe_preprocess_init(",
        "XpePreprocessNative.InitDelegate>(",
        "GuiPreprocessRunner.Run(",
        "LoadCalibrated(",
        "PrepareMapsThroughTheModule(",
    ];

    /// <summary>Files that only NAME the markers (this list, and the two tests that scan with it) and are not themselves callers.</summary>
    internal static readonly string[] FilesThatOnlyNameThem =
    [
        "Fixtures/PreprocessModuleCallMarkers.cs",
        "Functional/PreprocessModuleCollectionGuardTests.cs",
        "Functional/PreprocessModuleLeakTests.cs",
    ];
}
