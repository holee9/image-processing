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
    [SkippableFact]
    public void XpeVersion_WhenDllLoaded_ReturnsSemverString()
    {
        SkipHelper.SkipIf(!_fixture.IsAvailable, _fixture.SkipReason);

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
    [SkippableFact]
    public void ResolvedDll_IsX64Architecture()
    {
        // If fixture failed due to architecture mismatch, report it.
        if (_fixture.ResolvedPath.Contains("Architecture mismatch", StringComparison.Ordinal))
        {
            Assert.Fail($"DLL architecture mismatch: {_fixture.ResolvedPath}");
        }

        // GUI-C-208 (D6): with no DLL this used to `return`, which xUnit counts as a PASS. It is a skip, with the fixture's reason.
        SkipHelper.SkipIf(!_fixture.IsAvailable, _fixture.SkipReason);

        // Process itself must be x64.
        Assert.Equal(Architecture.X64, RuntimeInformation.ProcessArchitecture);

        // IntPtr.Size == 8 on 64-bit
        Assert.Equal(8, IntPtr.Size);
    }

    // GUI-C-208 (D7): ProcessArchitecture_IsRecordedForDiagnostics lived here. It recorded nothing and failed any architecture but X64 or Arm64. REQ-GUI-IT-043 asks for a diagnostic
    // that RECORDS the architecture and does not fail other tests; that is PlatformDetectionTests.ProcessArchitecture_IsRecordedForDiagnostics now.
    /// <summary>
    /// REQ-GUI-IT-041: Missing DLL must raise DllNotFoundException deterministically (not crash).
    /// Verified structurally: if DLL is absent, fixture.IsAvailable is false and SkipReason is set.
    /// This test verifies that the fixture correctly diagnosed absence.
    /// </summary>
    /// <summary>
    /// REQ-GUI-IT-042 (the half the fixture can check without a second DLL): the bootstrap's architecture guard accepts an x64 binary and rejects anything else instead of crashing.
    /// GUI-C-208 (D5) replaced <c>WhenDllAbsent_FixtureReportsUnavailable_NotCrash</c>, whose assertion <c>IsAvailable || SkipReason non-empty</c> was true in every state the fixture
    /// can be in (an unavailable fixture always has a reason) and so could never fail. The PE headers here are built by the test, so the verdicts do not depend on the real DLL.
    /// </summary>
    [Fact]
    public void ArchitectureGuard_RejectsABinaryThatIsNotX64_AndAcceptsOneThatIs()
    {
        var dir = Path.Combine(Path.GetTempPath(), $"xpe_pe_{Guid.NewGuid():N}");
        Directory.CreateDirectory(dir);
        try
        {
            string Write(string name, byte[] bytes) { var p = Path.Combine(dir, name); File.WriteAllBytes(p, bytes); return p; }

            static byte[] Pe(ushort machine)
            {
                var b = new byte[0x80];
                b[0] = (byte)'M'; b[1] = (byte)'Z';
                BitConverter.GetBytes(0x40).CopyTo(b, 0x3C);          // e_lfanew
                b[0x40] = (byte)'P'; b[0x41] = (byte)'E';              // the PE signature
                BitConverter.GetBytes(machine).CopyTo(b, 0x44);       // IMAGE_FILE_HEADER.Machine
                return b;
            }

            Assert.True(NativeLibraryFixture.VerifyX64Pe(Write("x64.dll", Pe(0x8664))), "a PE with machine AMD64 must be accepted");
            Assert.False(NativeLibraryFixture.VerifyX64Pe(Write("x86.dll", Pe(0x014C))), "a PE with machine i386 must be rejected");
            Assert.False(NativeLibraryFixture.VerifyX64Pe(Write("arm64.dll", Pe(0xAA64))), "a PE with machine ARM64 must be rejected");
            Assert.False(NativeLibraryFixture.VerifyX64Pe(Write("mz_only.dll", new byte[] { 0x4D, 0x5A })), "a two-byte MZ stub must be rejected, not crash");
            Assert.False(NativeLibraryFixture.VerifyX64Pe(Write("not_pe.dll", System.Text.Encoding.ASCII.GetBytes(new string('x', 0x80)))), "a file without the MZ signature must be rejected");
            Assert.False(NativeLibraryFixture.VerifyX64Pe(Path.Combine(dir, "missing.dll")), "a missing file must be rejected, not throw");
        }
        finally
        {
            Directory.Delete(dir, recursive: true);
        }
    }

    /// <summary>
    /// REQ-GUI-IT-005: xpe_version() returns a static pointer; consecutive calls return the same pointer.
    /// </summary>
    [SkippableFact]
    public void XpeVersion_CalledTwice_ReturnsSamePointer()
    {
        SkipHelper.SkipIf(!_fixture.IsAvailable, _fixture.SkipReason);

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
