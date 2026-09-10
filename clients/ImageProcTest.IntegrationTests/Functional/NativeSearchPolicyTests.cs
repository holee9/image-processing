// #129: the default DLL search must not reach repository build directories or sibling checkouts.
using ImageProcTest.IntegrationTests.Fixtures;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Regression for the search-scope policy (#129), applied to every search implementation that owns
/// a candidate list. There are four — the generic module locator, enhance_basic, preprocess, and
/// xpe_common inside the P/Invoke wrapper — and each used to walk repository build directories and
/// then SIBLING CHECKOUTS (../image-processing, ../xpe-post, ../xpe-pre). A DLL found that way had
/// no recorded provenance: GUI-C-14 measured a load from another repository entirely.
///
/// All four are covered here. The xpe_common list used to sit inside PInvokeWrapper.cs, which this
/// assembly cannot compile (its static constructor registers a DllImport resolver and one is already
/// registered — measured in GUI-C-13); GUI-C-17 split it into XpeCommonLibraryLocator so the same
/// regression applies to the module every other one depends on.
/// </summary>
[Trait("Category", "Functional")]
public sealed class NativeSearchPolicyTests
{
    /// <summary>Locators whose candidate lists this assembly can enumerate directly.</summary>
    public static IEnumerable<object[]> LinkedLocators() =>
    [
        ["NativeModuleLibraryLocator"],
        ["XpeEnhanceBasicLibraryLocator"],
        ["XpePreprocessLibraryLocator"],
        ["XpeCommonLibraryLocator"],
    ];

    private static IEnumerable<string> Candidates(string locator) => locator switch
    {
        "NativeModuleLibraryLocator" =>
            NativeModuleLibraryLocator.GetDllCandidates("gsvg.dll", "image-processing", "xpe-post", "xpe-pre"),
        "XpeEnhanceBasicLibraryLocator" => XpeEnhanceBasicLibraryLocator.GetDllCandidates(),
        "XpePreprocessLibraryLocator" => XpePreprocessLibraryLocator.GetDllCandidates(),
        "XpeCommonLibraryLocator" => XpeCommonLibraryLocator.GetDllCandidates(),
        _ => throw new ArgumentOutOfRangeException(nameof(locator), locator, "Unknown locator."),
    };

    /// <summary>#129: by default the candidate list stops at the app directory and XPE_NATIVE_DIR.</summary>
    [Theory]
    [MemberData(nameof(LinkedLocators))]
    public void ByDefault_NoBuildDirectoryOrSiblingCheckoutIsOffered(string locator)
    {
        using var _ = new PolicyScope(devSearch: false);

        var candidates = Candidates(locator).ToArray();

        Assert.All(candidates, candidate =>
            Assert.False(
                LooksLikeFallback(candidate),
                $"{locator}: default search offered a build/sibling candidate: {candidate}"));

        // Sanity: the narrow search still offers something, or the case proves nothing.
        Assert.NotEmpty(candidates);
    }

    /// <summary>#129: the fallbacks are still available, but only on request.</summary>
    [Theory]
    [MemberData(nameof(LinkedLocators))]
    public void WithDeveloperSearch_BuildDirectoriesReturn(string locator)
    {
        using var _ = new PolicyScope(devSearch: true);

        var candidates = Candidates(locator).ToArray();

        Assert.Contains(candidates, LooksLikeFallback);
    }

    /// <summary>A candidate under a repository build tree — the shape the default search must not emit.</summary>
    private static bool LooksLikeFallback(string candidate) =>
        candidate.Replace('\\', '/').Contains("/build/", StringComparison.OrdinalIgnoreCase);

    /// <summary>
    /// Sets the policy environment for one test and restores it. XPE_NATIVE_DIR is pointed at a
    /// directory that exists so the narrow search has a second candidate to offer.
    /// </summary>
    private sealed class PolicyScope : IDisposable
    {
        private readonly string? _previousDev;
        private readonly string? _previousExclusive;

        public PolicyScope(bool devSearch)
        {
            _previousDev = Environment.GetEnvironmentVariable("XPE_NATIVE_DEV_SEARCH");
            _previousExclusive = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR_EXCLUSIVE");
            Environment.SetEnvironmentVariable("XPE_NATIVE_DEV_SEARCH", devSearch ? "1" : null);
            // Exclusive would cut the search short before the fallbacks, hiding what is under test.
            Environment.SetEnvironmentVariable("XPE_NATIVE_DIR_EXCLUSIVE", null);
        }

        public void Dispose()
        {
            Environment.SetEnvironmentVariable("XPE_NATIVE_DEV_SEARCH", _previousDev);
            Environment.SetEnvironmentVariable("XPE_NATIVE_DIR_EXCLUSIVE", _previousExclusive);
        }
    }
}

/// <summary>
/// #129: the readiness snapshot records WHICH file backed each module, so "which build was that?"
/// can be answered after the fact. The reporting helpers are pure and linked, so this runs without
/// the probes (which reach P/Invoke code this assembly cannot load twice).
/// </summary>
[Trait("Category", "Functional")]
public sealed class ModuleReadinessReportingTests
{
    [Fact]
    public void ResolvedPathOrEmpty_KeepsRootedPaths_AndDropsBareNames()
    {
        var rooted = Path.Combine(Path.GetTempPath(), "gsvg.dll");

        Assert.Equal(rooted, ModuleReadinessReporting.ResolvedPathOrEmpty(rooted));

        // The probes put the bare DLL name here when they found nothing — not a resolution.
        Assert.Equal(string.Empty, ModuleReadinessReporting.ResolvedPathOrEmpty("gsvg.dll"));
        Assert.Equal(string.Empty, ModuleReadinessReporting.ResolvedPathOrEmpty(null));
        Assert.Equal(string.Empty, ModuleReadinessReporting.ResolvedPathOrEmpty("   "));
    }

    [Fact]
    public void DescribeResolvedModules_NamesTheFileOrSaysItWasNotResolved()
    {
        var rooted = Path.Combine(Path.GetTempPath(), "xpe_display.dll");
        var snapshots = new[]
        {
            Snapshot("xpe_display", rooted),
            Snapshot("gsvg", string.Empty),
        };

        var line = ModuleReadinessReporting.DescribeResolvedModules(snapshots);

        Assert.Contains($"xpe_display={rooted}", line);
        Assert.Contains("gsvg=<not resolved>", line);
        Assert.DoesNotContain("\n", line);
    }

    /// <summary>
    /// #56 / #128 (BP-10 "all_optional_absent"): with only the required floor staged, the grade the
    /// report carries for each optional module is R0 and the grade for each required one is not.
    ///
    /// DegradedModeReadinessTests observes the same staging one level lower (does the DLL resolve?).
    /// This case is the level the old BP-10 CI job actually reported at: what the app SAYS about that
    /// discovery. Without it, a grading regression that turned every module into R1 would be invisible
    /// here — resolution would still be null and the lower test would still pass.
    /// </summary>
    [Fact]
    public void AllOptionalDllsAbsent_GradesEveryOptionalR0_AndNoRequiredOne()
    {
        var dir = Path.Combine(Path.GetTempPath(), $"xpe_grades_{Guid.NewGuid():N}");
        Directory.CreateDirectory(dir);
        foreach (var dll in new[] { "xpe_common.dll", "xpe_preprocess.dll" })
            File.WriteAllBytes(Path.Combine(dir, dll), [0x4D, 0x5A]);

        var previousDir = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
        var previousExclusive = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR_EXCLUSIVE");
        Environment.SetEnvironmentVariable("XPE_NATIVE_DIR", dir);
        Environment.SetEnvironmentVariable("XPE_NATIVE_DIR_EXCLUSIVE", "1");
        try
        {
            foreach (var dll in new[]
                     { "gsvg.dll", "xpe_enhance_advanced.dll", "xpe_dicom.dll", "xpe_display.dll" })
            {
                var path = NativeModuleLibraryLocator.TryFindDll(dll, "image-processing");
                Assert.Equal("R0", ModuleReadinessGrading.GradeDiscovery(path, "R1"));
                Assert.Equal(ModuleReadinessGrading.NotReady, ModuleReadinessGrading.GradeDiscovery(path, "R1"));
                Assert.Equal(string.Empty, ModuleReadinessReporting.ResolvedPathOrEmpty(path));
            }

            var basicPath = XpeEnhanceBasicLibraryLocator.TryFindDll();
            Assert.Equal(ModuleReadinessGrading.NotReady, ModuleReadinessGrading.GradeDiscovery(basicPath, "R1"));

            foreach (var required in new[]
                     { XpeCommonLibraryLocator.TryFindDll(), XpePreprocessLibraryLocator.TryFindDll() })
            {
                Assert.NotEqual(
                    ModuleReadinessGrading.NotReady,
                    ModuleReadinessGrading.GradeDiscovery(required, "R1"));
                Assert.NotEqual(string.Empty, ModuleReadinessReporting.ResolvedPathOrEmpty(required));
            }
        }
        finally
        {
            Environment.SetEnvironmentVariable("XPE_NATIVE_DIR", previousDir);
            Environment.SetEnvironmentVariable("XPE_NATIVE_DIR_EXCLUSIVE", previousExclusive);
            try { Directory.Delete(dir, recursive: true); } catch (IOException) { /* temp dir, best effort */ }
        }
    }

    private static ModuleReadinessSnapshot Snapshot(string module, string resolvedPath) =>
        new(module, "R1", "status", "evidence", "next", ProcessingEnabled: false,
            ResolvedDllPath: resolvedPath);
}
