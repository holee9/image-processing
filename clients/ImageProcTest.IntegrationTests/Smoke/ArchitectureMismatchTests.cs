// REQ-GUI-IT-042 (GUI-C-209 M1, D9): a DLL of the wrong architecture is refused with BadImageFormatException, and the bootstrap names the file.
using System.Runtime.InteropServices;
using ImageProcTest.IntegrationTests.Fixtures;

namespace ImageProcTest.IntegrationTests.Smoke;

/// <summary>
/// REQ-GUI-IT-042: "While the resolved DLL is x86 and the test host is x64 (or vice versa), the first P/Invoke call <b>shall raise <c>BadImageFormatException</c></b> and the test <b>shall surface this as
/// a test failure with the resolved path</b>."
///
/// <para>No x86 <c>xpe_common.dll</c> is built here, and a hand-built PE image proves nothing (measured: a minimal PE32, a minimal ARM64 image and a minimal AMD64 image are ALL refused with
/// <c>BadImageFormatException</c>, because a header with no sections is malformed whatever its machine field says). So the wrong-architecture image is MADE from a real x64 system DLL
/// (<c>version.dll</c>) by changing one field, the PE header's Machine; the unchanged copy is the control that loads. The only difference between "loads" and "BadImageFormatException" is then the
/// machine type. Nothing is stored in the repository: the files are created in the test output folder at run time and removed afterwards.</para>
/// </summary>
[Trait("Category", "Smoke")]
public sealed class ArchitectureMismatchTests
{
    private const ushort MachineX86 = 0x014C;
    private const ushort MachineArm64 = 0xAA64;
    private const int BadExeFormat = unchecked((int)0x8007000B);

    private static string? SystemDll()
    {
        if (!RuntimeInformation.IsOSPlatform(OSPlatform.Windows) || RuntimeInformation.ProcessArchitecture != Architecture.X64) return null;
        var path = Path.Combine(Environment.SystemDirectory, "version.dll");
        return File.Exists(path) ? path : null;
    }

    /// <summary>A copy of <paramref name="source"/> with the PE header's Machine field replaced (e_lfanew + 4).</summary>
    private static string CopyWithMachine(string source, string destination, ushort? machine)
    {
        var bytes = File.ReadAllBytes(source);
        if (machine is { } m)
        {
            var peOffset = BitConverter.ToInt32(bytes, 0x3C);
            BitConverter.GetBytes(m).CopyTo(bytes, peOffset + 4);
        }

        File.WriteAllBytes(destination, bytes);
        return destination;
    }

    private static string TempDir()
    {
        var dir = Path.Combine(Path.GetTempPath(), $"xpe_arch_{Guid.NewGuid():N}");
        Directory.CreateDirectory(dir);
        return dir;
    }

    /// <summary>
    /// GUI-C-228 (REQ-GUI-IT-042, "the test shall surface this as a failure with the resolved path"): the REAL fixture, built over a native folder whose <c>xpe_common.dll</c> is an x86 image, reports
    /// itself unavailable and its resolved path (what the smoke test prints when it fails) is the architecture diagnostic that names the file. The pieces (the guard, the diagnostic wording) are
    /// held by the test above; this holds that the fixture puts them together. The control is the same construction over the unchanged x64 DLL, which is available.
    /// </summary>
    [SkippableFact]
    public void TheFixtureBootstrap_WithAnX86XpeCommon_IsUnavailable_AndItsResolvedPathNamesTheFile()
    {
        var source = SystemDll();
        Skip.If(source is null, "Needs an x64 Windows host with System32\\version.dll.");
        var dir = TempDir();
        try
        {
            var x86 = CopyWithMachine(source!, Path.Combine(dir, "xpe_common.dll"), MachineX86);

            var fixture = new NativeLibraryFixture(dir, Path.Combine(dir, "no_output_folder"), registerResolver: false);

            Assert.False(fixture.IsAvailable, "an x86 xpe_common.dll must not be reported as available to an x64 host");
            Assert.Equal(NativeLibraryFixture.ArchitectureMismatchDiagnostic(x86), fixture.ResolvedPath);
            Assert.Contains(x86, fixture.ResolvedPath, StringComparison.OrdinalIgnoreCase);
            Assert.Contains(x86, fixture.SkipReason, StringComparison.OrdinalIgnoreCase);
        }
        finally
        {
            Directory.Delete(dir, recursive: true);
        }
    }

    /// <summary>
    /// The loader's verdict. Control: the unchanged copy of the x64 DLL loads. Test: the same bytes with Machine = i386 (and = ARM64) are refused with <see cref="BadImageFormatException"/> (HRESULT 0x8007000B,
    /// ERROR_BAD_EXE_FORMAT) — and the bootstrap's own guard (<see cref="NativeLibraryFixture.VerifyX64Pe"/>) agrees with the loader on all three files.
    /// </summary>
    [SkippableFact]
    public void AnImageOfAnotherArchitecture_IsRefusedByTheLoader_WithBadImageFormatException_WhileTheSameBytesAsX64Load()
    {
        var source = SystemDll();
        Skip.If(source is null, "Needs an x64 Windows host with System32\\version.dll: REQ-GUI-IT-042 is about an architecture mismatch on such a host.");
        var dir = TempDir();
        try
        {
            var control = CopyWithMachine(source!, Path.Combine(dir, "control_x64.dll"), null);
            var x86 = CopyWithMachine(source!, Path.Combine(dir, "image_x86.dll"), MachineX86);
            var arm64 = CopyWithMachine(source!, Path.Combine(dir, "image_arm64.dll"), MachineArm64);

            var handle = NativeLibrary.Load(control);   // control: it loads, so a refusal below is about the Machine field
            NativeLibrary.Free(handle);

            foreach (var wrong in new[] { x86, arm64 })
            {
                var ex = Assert.Throws<BadImageFormatException>(() => NativeLibrary.Load(wrong));
                Assert.Equal(BadExeFormat, ex.HResult);
            }

            Assert.True(NativeLibraryFixture.VerifyX64Pe(control), "the bootstrap guard must accept the unchanged x64 DLL");
            Assert.False(NativeLibraryFixture.VerifyX64Pe(x86), "the bootstrap guard must reject the i386 image the loader refuses");
            Assert.False(NativeLibraryFixture.VerifyX64Pe(arm64), "the bootstrap guard must reject the ARM64 image the loader refuses");
        }
        finally
        {
            Directory.Delete(dir, recursive: true);
        }
    }

    /// <summary>
    /// "The first P/Invoke call shall raise BadImageFormatException": a real <c>[DllImport]</c> whose library is the i386 image, found by the runtime's own probing in the application folder
    /// (the fixture's resolver answers only for xpe_common.dll, so this name goes to the default probing). The exception is raised by the call, not by a load the test made itself.
    /// </summary>
    [SkippableFact]
    public void TheFirstPInvokeCallIntoAnX86Image_RaisesBadImageFormatException()
    {
        var source = SystemDll();
        Skip.If(source is null, "Needs an x64 Windows host with System32\\version.dll.");
        var target = Path.Combine(AppContext.BaseDirectory, Decoy.LibraryName);
        CopyWithMachine(source!, target, MachineX86);
        try
        {
            var ex = Assert.Throws<BadImageFormatException>(() => Decoy.Call());
            Assert.Equal(BadExeFormat, ex.HResult);
        }
        finally
        {
            File.Delete(target);
        }
    }

    /// <summary>
    /// "…surface this as a test failure with the resolved path": the fixture's diagnostic for a DLL that is not x64 names the file and the reason, and an x86 <c>xpe_common.dll</c> placed in the native
    /// directory (<c>XPE_NATIVE_DIR</c>, the highest-priority location) is the one the locator returns and the guard rejects. <c>ResolvedDll_IsX64Architecture</c> turns that diagnostic into
    /// <c>Assert.Fail</c>; the evidence run in the GUI-C-209 report runs the whole suite that way.
    /// </summary>
    [SkippableFact]
    public void AnX86XpeCommonInTheNativeDirectory_IsLocated_RejectedByTheGuard_AndNamedInTheDiagnostic()
    {
        var source = SystemDll();
        Skip.If(source is null, "Needs an x64 Windows host with System32\\version.dll.");
        var dir = TempDir();
        var previous = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
        try
        {
            var x86 = CopyWithMachine(source!, Path.Combine(dir, "xpe_common.dll"), MachineX86);
            Environment.SetEnvironmentVariable("XPE_NATIVE_DIR", dir);

            var (found, located) = NativeLibraryFixture.TryLocateDll();
            Assert.True(found);
            Assert.Equal(x86, located, StringComparer.OrdinalIgnoreCase);
            Assert.False(NativeLibraryFixture.VerifyX64Pe(located));

            var diagnostic = NativeLibraryFixture.ArchitectureMismatchDiagnostic(located);
            Assert.Contains(located, diagnostic, StringComparison.OrdinalIgnoreCase);
            Assert.Contains("is not x64", diagnostic, StringComparison.Ordinal);
            Assert.Contains("Architecture mismatch", diagnostic, StringComparison.Ordinal);
        }
        finally
        {
            Environment.SetEnvironmentVariable("XPE_NATIVE_DIR", previous);
            Directory.Delete(dir, recursive: true);
        }
    }

    private static class Decoy
    {
        public const string LibraryName = "xpe_x86_decoy.dll";

        [DllImport(LibraryName, EntryPoint = "GetFileVersionInfoSizeW", ExactSpelling = true)]
        private static extern int Entry(IntPtr path, IntPtr handle);

        public static void Call() => Entry(IntPtr.Zero, IntPtr.Zero);
    }
}
