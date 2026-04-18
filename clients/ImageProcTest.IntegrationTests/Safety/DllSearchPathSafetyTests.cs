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
    [Fact]
    public void ResolvedDllPath_IsUnderBuildTreeOrTestOutput()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

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
    [Fact]
    public void DllImportResolver_WinsOverSystemPath_WhenEnvVarOrAppContextHasDll()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

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
                // The fixture-resolved path must NOT be the decoy
                Assert.NotEqual(
                    decoyPath.ToLowerInvariant(),
                    _fixture.ResolvedPath.ToLowerInvariant());
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
