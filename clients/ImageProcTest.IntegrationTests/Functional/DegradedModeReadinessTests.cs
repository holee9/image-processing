// #128 (BP-06..BP-10): a module DLL missing must leave that module undiscoverable while the
// others stay discoverable, and must not throw.
using ImageProcTest.IntegrationTests.Fixtures;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Degraded-mode coverage for the native-module search that feeds readiness.
///
/// The native placeholders (test_post_degraded_mode.cpp BP-06..BP-10) only assert that a file is
/// absent; they never ask what the app concludes from that absence. The conclusion is C# — a module
/// whose DLL is not found reports R0 in ModuleReadinessService — so the question belongs here.
///
/// Every module in scope resolves its DLL through <see cref="NativeModuleLibraryLocator"/>
/// (gsvg / dicom / display probes call GetDllCandidates directly; enhance_advanced goes through
/// ModuleReadinessService.EvaluateDllPresence). This class exercises that shared decision input:
/// with one DLL removed from an otherwise complete directory, that one must not resolve and the
/// rest must.
///
/// Scope note — this stops at the locator, one step below the R0 string. Asserting the snapshot
/// itself would mean loading the probes, and three of them reach XpeCommonApi, whose static
/// constructor registers a second DllImport resolver on this assembly and throws
/// (measured in GUI-C-13). See the GUI-C-14 report §"남은 범위".
/// </summary>
[Trait("Category", "Functional")]
public sealed class DegradedModeReadinessTests : IDisposable
{
    /// <summary>DLL name per degraded-mode case. enhance_basic is absent — it uses a different locator.</summary>
    public static IEnumerable<object[]> DegradedCases() =>
    [
        ["BP-06", "gsvg.dll"],
        ["BP-07", "xpe_enhance_advanced.dll"],
        ["BP-08", "xpe_enhance_basic.dll"],
        ["BP-09", "xpe_dicom.dll"],
        ["BP-10", "xpe_display.dll"],
    ];

    /// <summary>All names the cases above manipulate, so "the others still resolve" has something to check.</summary>
    private static readonly string[] ModuleDlls =
        ["gsvg.dll", "xpe_enhance_advanced.dll", "xpe_enhance_basic.dll", "xpe_dicom.dll", "xpe_display.dll"];

    /// <summary>Level a discovered module would be graded; the value is irrelevant, only "not R0" is.</summary>
    private const string ReadyLevel = "R1";

    private readonly List<string> _tempDirs = [];

    /// <summary>
    /// #128: with one module DLL missing from the search directory, that module does not resolve,
    /// every other module still does, and nothing throws.
    /// </summary>
    [Theory]
    [MemberData(nameof(DegradedCases))]
    public void ModuleDllRemoved_OnlyThatModuleFailsToResolve(string caseId, string removedDll)
    {
        var dir = CreateStagingWithout(removedDll);

        using var _ = new NativeSearchScope(dir);

        var removedPath = NativeModuleLibraryLocator.TryFindDll(removedDll, "image-processing");
        Assert.True(
            removedPath is null,
            $"{caseId}: {removedDll} was removed from {dir} but still resolved to {removedPath}. " +
            "The search escaped the injected directory — XPE_NATIVE_DIR_EXCLUSIVE did not take.");

        // The grade the app derives from that discovery result — the point of #128.
        Assert.Equal(
            ModuleReadinessGrading.NotReady,
            ModuleReadinessGrading.GradeDiscovery(removedPath, ReadyLevel));

        foreach (var other in ModuleDlls.Where(d => d != removedDll))
        {
            var otherPath = NativeModuleLibraryLocator.TryFindDll(other, "image-processing");
            Assert.True(
                otherPath is not null,
                $"{caseId}: removing {removedDll} also made {other} unresolvable — degradation is not isolated.");

            Assert.NotEqual(
                ModuleReadinessGrading.NotReady,
                ModuleReadinessGrading.GradeDiscovery(otherPath, ReadyLevel));
        }
    }

    /// <summary>
    /// #128 / BP-09: xpe_dicom.dll is not in any CI artifact (dicom builds only under
    /// coverage-dicom), so "absent" is its default state rather than something a test arranges.
    /// Both states are observed: absent from a complete staging, and absent because never staged.
    /// </summary>
    [Fact]
    public void DicomDll_AbsentFromAnEmptyDirectory_DoesNotResolve()
    {
        var empty = CreateTempDir();

        using var _ = new NativeSearchScope(empty);

        var path = NativeModuleLibraryLocator.TryFindDll("xpe_dicom.dll", "image-processing");
        Assert.Null(path);
        Assert.Equal(ModuleReadinessGrading.NotReady, ModuleReadinessGrading.GradeDiscovery(path, ReadyLevel));
    }

    /// <summary>
    /// Control case: with nothing removed, every module resolves. Without this, a bug that made
    /// TryFindDll always return null would make every case above pass.
    /// </summary>
    [Fact]
    public void CompleteStaging_ResolvesEveryModule()
    {
        var dir = CreateStagingWithout(removedDll: null);

        using var _ = new NativeSearchScope(dir);

        foreach (var dll in ModuleDlls)
        {
            var path = NativeModuleLibraryLocator.TryFindDll(dll, "image-processing");
            Assert.True(path is not null, $"{dll} did not resolve from a complete staging directory {dir}.");
            Assert.NotEqual(ModuleReadinessGrading.NotReady, ModuleReadinessGrading.GradeDiscovery(path, ReadyLevel));
        }
    }

    // ---------- helpers ----------

    /// <summary>
    /// Builds a temp directory holding a placeholder for every module DLL except
    /// <paramref name="removedDll"/>. Placeholders are enough: the locator decides on
    /// File.Exists, and writing bytes rather than copying keeps the real staging untouched.
    /// </summary>
    private string CreateStagingWithout(string? removedDll)
    {
        var dir = CreateTempDir();
        foreach (var dll in ModuleDlls.Where(d => d != removedDll))
            File.WriteAllBytes(Path.Combine(dir, dll), [0x4D, 0x5A]); // "MZ"
        return dir;
    }

    private string CreateTempDir()
    {
        var dir = Path.Combine(Path.GetTempPath(), $"xpe_degraded_{Guid.NewGuid():N}");
        Directory.CreateDirectory(dir);
        _tempDirs.Add(dir);
        return dir;
    }

    public void Dispose()
    {
        foreach (var dir in _tempDirs)
        {
            try { Directory.Delete(dir, recursive: true); } catch (IOException) { /* temp dir, best effort */ }
        }
    }

    /// <summary>
    /// Pins the module search to one directory for the duration of a test and restores the previous
    /// environment afterwards. Also asserts the precondition the whole experiment rests on: the test
    /// output directory, which the locator checks BEFORE the injected one, must not already hold the
    /// module DLLs — otherwise "removed" would resolve there and the case would prove nothing.
    /// </summary>
    private sealed class NativeSearchScope : IDisposable
    {
        private readonly string? _previousDir;
        private readonly string? _previousExclusive;

        public NativeSearchScope(string directory)
        {
            foreach (var dll in ModuleDlls)
                Assert.False(
                    File.Exists(Path.Combine(AppContext.BaseDirectory, dll)),
                    $"Precondition failed: {dll} is present in the test output directory " +
                    $"({AppContext.BaseDirectory}), which the locator searches before the injected " +
                    "directory. The degraded-mode cases cannot observe absence while it is there.");

            _previousDir = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
            _previousExclusive = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR_EXCLUSIVE");
            Environment.SetEnvironmentVariable("XPE_NATIVE_DIR", directory);
            Environment.SetEnvironmentVariable("XPE_NATIVE_DIR_EXCLUSIVE", "1");
        }

        public void Dispose()
        {
            Environment.SetEnvironmentVariable("XPE_NATIVE_DIR", _previousDir);
            Environment.SetEnvironmentVariable("XPE_NATIVE_DIR_EXCLUSIVE", _previousExclusive);
        }
    }
}
