// AC-3: DLL resolution is deterministic; AC-8: DLL path within build tree.
using ImageProcTest.IntegrationTests.Fixtures;

namespace ImageProcTest.IntegrationTests.Safety;

/// <summary>
/// Verifies that the DLL resolver uses the project-controlled search path
/// and not arbitrary system directories.
/// Covers REQ-GUI-IT-008, AC-3, AC-9.
/// </summary>
[Trait("Category", "Safety")]
[Collection(NativeLibraryCollection.Name)]
public sealed class DllSearchPathSafetyTests
{
    private readonly NativeLibraryFixture _fixture;

    public DllSearchPathSafetyTests(NativeLibraryFixture fixture)
    {
        _fixture = fixture;
    }

    /// <summary>
    /// REQ-GUI-IT-008: ResolvedDllPath must point to the test output dir or repo build/** tree.
    /// A system PATH load (e.g. C:\Windows\System32) is not acceptable.
    /// </summary>
    [SkippableFact]
    public void ResolvedDllPath_IsUnderBuildTreeOrTestOutput()
    {
        SkipHelper.SkipIf(!_fixture.IsAvailable, _fixture.SkipReason);

        var path = _fixture.ResolvedPath;
        Assert.False(string.IsNullOrEmpty(path), "ResolvedPath must be set when DLL is available.");

        // Check it is NOT from a system directory
        var normalized = path.Replace('\\', '/').ToLowerInvariant();
        Assert.DoesNotContain("windows/system32", normalized);
        Assert.DoesNotContain("windows/syswow64", normalized);

        // Must be a real file
        Assert.True(File.Exists(path), $"Resolved path does not exist on disk: {path}");
    }

    /// <summary>
    /// AC-9 (structural): When a decoy xpe_common.dll is placed in a temp directory
    /// that is on the PATH, the fixture's resolver must still win (env var or AppContext takes priority).
    /// This test creates a temp decoy, adds it to PATH, and verifies our DLL path is unchanged.
    /// </summary>
    /// <summary>The DLL that is actually in the process is the resolved file, and the decoy never was one.</summary>
    [SkippableFact]
    public void TheLoadedModule_IsTheResolvedFile()
    {
        SkipHelper.SkipIf(!_fixture.IsAvailable, _fixture.SkipReason);
        var loaded = System.Diagnostics.Process.GetCurrentProcess().Modules.Cast<System.Diagnostics.ProcessModule>()
            .Where(m => string.Equals(m.ModuleName, "xpe_common.dll", StringComparison.OrdinalIgnoreCase))
            .Select(m => m.FileName)
            .ToList();
        Assert.Contains(loaded, f => string.Equals(f, _fixture.ResolvedPath, StringComparison.OrdinalIgnoreCase));
        Assert.DoesNotContain(loaded, f => f.Contains("xpe_decoy_", StringComparison.OrdinalIgnoreCase));
    }

    [SkippableFact]
    public void ADecoyEarlierOnPath_IsNotWhatTheLocatorReturns_WhenItIsPlacedBeforeTheLocatorRuns()
    {
        SkipHelper.SkipIf(!_fixture.IsAvailable, _fixture.SkipReason);

        var tempDir = Path.Combine(Path.GetTempPath(), $"xpe_decoy_{Guid.NewGuid():N}");
        Directory.CreateDirectory(tempDir);
        var decoyPath = Path.Combine(tempDir, "xpe_common.dll");

        try
        {
            // Write a 2-byte "decoy" (not a valid PE, load will fail for it).
            File.WriteAllBytes(decoyPath, new byte[] { 0x4D, 0x5A }); // MZ only, not a real DLL

            // Prepend decoy dir to PATH
            var originalPath = Environment.GetEnvironmentVariable("PATH") ?? string.Empty;
            Environment.SetEnvironmentVariable("PATH", tempDir + ";" + originalPath);
            try
            {
                // GUI-C-208 (D3): the old version compared the fixture's path to the decoy AFTER the fixture had resolved (once, at session start), so the decoy could not have
                // influenced it. Here the locator runs again with the decoy already first on PATH.
                var (found, located) = NativeLibraryFixture.TryLocateDll();
                Assert.True(found, $"the locator found nothing with a decoy on PATH: {located}");
                Assert.NotEqual(decoyPath, located, StringComparer.OrdinalIgnoreCase);
                Assert.Equal(_fixture.ResolvedPath, located, StringComparer.OrdinalIgnoreCase);
            }
            finally
            {
                Environment.SetEnvironmentVariable("PATH", originalPath);
            }
        }
        finally
        {
            if (File.Exists(decoyPath)) File.Delete(decoyPath);
            if (Directory.Exists(tempDir)) Directory.Delete(tempDir, recursive: true);
        }
    }
}
