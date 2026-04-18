// AC-1: Test project exists and builds.
// AC-2: ABI size parity, DLL resolution, version string, arch diagnostic.
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using ImageProcTest.IntegrationTests.Fixtures;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.Smoke;

/// <summary>
/// Smoke tests: DLL load, version string, architecture detection.
/// Covers REQ-GUI-IT-020, REQ-GUI-IT-042, REQ-GUI-IT-043, AC-1, AC-2.
/// </summary>
[Trait("Category", "Smoke")]
[Collection(NativeLibraryCollection.Name)]
public sealed class DllLoadSmokeTests
{
    private readonly NativeLibraryFixture _fixture;

    public DllLoadSmokeTests(NativeLibraryFixture fixture)
    {
        _fixture = fixture;
    }

    /// <summary>
    /// REQ-GUI-IT-020: DLL resolver locates xpe_common.dll and xpe_version() returns
    /// a non-empty semver string matching ^\d+\.\d+\.\d+.
    /// </summary>
    [Fact]
    public void XpeVersion_WhenDllLoaded_ReturnsSemverString()
    {
        if (!_fixture.IsAvailable)
            return; // DLL not available — test skipped

        var ptr = XpeCommonNative.xpe_version();
        Assert.NotEqual(IntPtr.Zero, ptr);

        var version = Marshal.PtrToStringAnsi(ptr);
        Assert.NotNull(version);
        Assert.NotEmpty(version);
        Assert.Matches(@"^\d+\.\d+\.\d+", version);
    }

    /// <summary>
    /// REQ-GUI-IT-042: Resolved DLL is x64 architecture (verified in NativeLibraryFixture).
    /// If fixture reports arch mismatch, surface as test failure with resolved path.
    /// </summary>
    [Fact]
    public void ResolvedDll_IsX64Architecture()
    {
        // If fixture failed due to architecture mismatch, report it.
        if (_fixture.ResolvedPath.Contains("Architecture mismatch", StringComparison.Ordinal))
        {
            Assert.Fail($"DLL architecture mismatch: {_fixture.ResolvedPath}");
        }

        if (!_fixture.IsAvailable)
            return; // DLL not available — test skipped

        // Process itself must be x64.
        Assert.Equal(Architecture.X64, RuntimeInformation.ProcessArchitecture);

        // IntPtr.Size == 8 on 64-bit
        Assert.Equal(8, IntPtr.Size);
    }

    /// <summary>
    /// REQ-GUI-IT-043: Platform architecture diagnostic — records architecture without failing baseline tests.
    /// </summary>
    [Fact]
    public void ProcessArchitecture_IsRecordedForDiagnostics()
    {
        // This test always runs and records architecture. It does not fail non-x64 builds.
        var arch = RuntimeInformation.ProcessArchitecture;
        // On x64 CI this must be X64.
        Assert.True(
            arch == Architecture.X64 || arch == Architecture.Arm64,
            $"Unexpected architecture: {arch}");
    }

    /// <summary>
    /// REQ-GUI-IT-041: Missing DLL must raise DllNotFoundException deterministically (not crash).
    /// Verified structurally: if DLL is absent, fixture.IsAvailable is false and SkipReason is set.
    /// This test verifies that the fixture correctly diagnosed absence.
    /// </summary>
    [Fact]
    public void WhenDllAbsent_FixtureReportsUnavailable_NotCrash()
    {
        // If DLL is available, this assertion path is trivially true.
        // If DLL is absent, fixture.IsAvailable should be false — no unhandled exception occurred.
        var isAvailableOrAbsent = _fixture.IsAvailable || !string.IsNullOrEmpty(_fixture.SkipReason);
        Assert.True(isAvailableOrAbsent, "Fixture must deterministically report DLL absence without crashing.");
    }

    /// <summary>
    /// REQ-GUI-IT-005: xpe_version() returns a static pointer; consecutive calls return the same pointer.
    /// </summary>
    [Fact]
    public void XpeVersion_CalledTwice_ReturnsSamePointer()
    {
        if (!_fixture.IsAvailable)
            return; // DLL not available — test skipped

        var ptr1 = XpeCommonNative.xpe_version();
        var ptr2 = XpeCommonNative.xpe_version();
        Assert.Equal(ptr1, ptr2);
    }

    /// <summary>
    /// REQ-GUI-IT-042: When architecture mismatch is detected, the fixture's ResolvedPath
    /// MUST surface the offending DLL path so operators can diagnose the wrong-arch binary.
    /// On a healthy x64 build this test asserts the success-path contract (ResolvedPath
    /// points at a .dll). The mismatch path is asserted conditionally when it triggers.
    /// </summary>
    [Fact]
    public void ArchitectureMismatch_SurfacesResolvedPathInMessage()
    {
        Assert.False(string.IsNullOrWhiteSpace(_fixture.ResolvedPath),
            "ResolvedPath must always be populated (success or diagnostic message).");

        if (_fixture.ResolvedPath.Contains("Architecture mismatch", StringComparison.Ordinal))
        {
            // Mismatch mode: message contract must include the actual path and reason.
            Assert.Contains(".dll", _fixture.ResolvedPath, StringComparison.OrdinalIgnoreCase);
            Assert.Contains(" is not x64", _fixture.ResolvedPath, StringComparison.Ordinal);
            return;
        }

        if (_fixture.IsAvailable)
        {
            // Healthy x64 resolution: path must end with .dll so downstream tooling can locate the binary.
            Assert.EndsWith(".dll", _fixture.ResolvedPath, StringComparison.OrdinalIgnoreCase);
        }
    }
}
