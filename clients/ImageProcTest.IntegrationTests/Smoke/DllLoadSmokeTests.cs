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

    /// <summary>The time REQ-GUI-IT-020 allows for locating the DLL and getting the first answer from it.</summary>
    private static readonly TimeSpan LocateBudget = TimeSpan.FromSeconds(5);

    /// <summary>
    /// GUI-C-225 (REQ-GUI-IT-020): the resolver locates xpe_common.dll "within 5 seconds" and the call after it answers. The requirement has a number in it, so the number is asserted: the locator
    /// and the first <c>xpe_version</c> call are timed together against <see cref="LocateBudget"/>. (The semver pattern itself is <see cref="XpeVersion_WhenDllLoaded_ReturnsSemverString"/>.)
    /// </summary>
    [SkippableFact]
    public void TheDll_IsLocatedAndAnswersXpeVersion_WithinTheRequirementsFiveSeconds()
    {
        SkipHelper.SkipIf(!_fixture.IsAvailable, _fixture.SkipReason);

        var watch = System.Diagnostics.Stopwatch.StartNew();
        var (found, path) = NativeLibraryFixture.TryLocateDll();
        var version = Marshal.PtrToStringAnsi(XpeCommonNative.xpe_version());
        watch.Stop();

        Assert.True(found, $"the locator did not find the DLL the fixture loaded: {path}");
        Assert.False(string.IsNullOrEmpty(version));
        Assert.Matches(@"^\d+\.\d+\.\d+", version!);
        Assert.True(watch.Elapsed <= LocateBudget, $"locating the DLL and the first xpe_version call took {watch.Elapsed.TotalMilliseconds:0.###} ms; REQ-GUI-IT-020 allows {LocateBudget.TotalSeconds:0.###} s");
    }

    /// <summary>
    /// GUI-C-225 (REQ-GUI-IT-041, the locator half): with no search path that holds the DLL the locator says "not found" and says WHERE it looked, instead of returning a bare failure. The
    /// search path is emptied by giving the locator an output directory with no DLL and no repository above it, and an environment folder that does not exist. The control is the same call
    /// with the DLL present, so the "not found" is about the missing file and not about the inputs.
    /// </summary>
    [Fact]
    public void TheLocator_WithNothingToFind_ReportsNotFound_AndNamesTheFoldersItSearched()
    {
        var root = Path.Combine(Path.GetTempPath(), $"xpe_c225_loc_{Guid.NewGuid():N}");
        var empty = Path.Combine(root, "empty_output");
        var withDll = Path.Combine(root, "with_dll");
        var absentEnv = Path.Combine(root, "no_such_native_dir");
        Directory.CreateDirectory(empty);
        Directory.CreateDirectory(withDll);
        File.WriteAllBytes(Path.Combine(withDll, "xpe_common.dll"), [0x4D, 0x5A]);
        try
        {
            var (control, controlPath) = NativeLibraryFixture.Locate(absentEnv, withDll);
            Assert.True(control, "the control (a folder that holds the file) must be found: " + controlPath);
            Assert.Equal(Path.Combine(withDll, "xpe_common.dll"), controlPath, StringComparer.OrdinalIgnoreCase);

            var (found, message) = NativeLibraryFixture.Locate(absentEnv, empty);

            Assert.False(found);
            Assert.Contains("xpe_common.dll", message, StringComparison.Ordinal);
            Assert.Contains(empty, message, StringComparison.OrdinalIgnoreCase);     // the output directory that was searched
            Assert.Contains(absentEnv, message, StringComparison.OrdinalIgnoreCase); // and the environment folder
        }
        finally
        {
            Directory.Delete(root, recursive: true);
        }
    }

    private const string AbsentLibraryName = "xpe_c225_no_such_library.dll";

    /// <summary>
    /// GUI-C-225 (REQ-GUI-IT-041, the loader half): asking the runtime to load a DLL that cannot be found raises <see cref="DllNotFoundException"/> that names the DLL, and the test host carries on
    /// (this test is still running afterwards). The name in the message is what lets an operator see WHICH library is missing. It goes through <c>NativeLibrary.Load</c>, the call the fixture itself
    /// makes, rather than a declared <c>[DllImport]</c>: the signature-parity test reads every declared extern and a made-up one would be a finding there.
    /// </summary>
    [Fact]
    public void LoadingAMissingDll_RaisesDllNotFoundException_ThatNamesIt()
    {
        var thrown = Assert.Throws<DllNotFoundException>(() => NativeLibrary.Load(AbsentLibraryName));

        Assert.Contains(AbsentLibraryName, thrown.Message, StringComparison.OrdinalIgnoreCase);
        Assert.Equal(8, IntPtr.Size);   // the host is alive and still running its own code
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
