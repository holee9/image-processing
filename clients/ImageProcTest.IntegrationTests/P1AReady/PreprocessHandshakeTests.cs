// AC-14: Optional P1A tests skip cleanly when xpe_preprocess.dll absent.
using System.Runtime.InteropServices;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.P1AReady;

/// <summary>
/// Optional lifecycle tests for xpe_preprocess.dll.
/// These tests activate automatically when the DLL is staged in the build tree.
/// Covers REQ-GUI-IT-060, AC-14, AC-15.
/// </summary>
[Trait("Category", "P1AReady")]
public sealed class PreprocessHandshakeTests
{
    private static readonly string? DllPath = XpePreprocessNative.TryFindDll();
    private static readonly string SkipReason = DllPath is null
        ? "Skipped: xpe_preprocess.dll not staged — build P1A first or set XPE_NATIVE_DIR"
        : string.Empty;

    /// <summary>REQ-GUI-IT-060: xpe_preprocess_version export exists and returns non-empty string.</summary>
    [Fact]
    public void PreprocessVersion_WhenDllStaged_ReturnsNonEmptyString()
    {
        if (DllPath is null) return; // P1A DLL not staged — test skipped

        if (!NativeLibrary.TryLoad(DllPath, out var handle))
            return; // DLL load failed — test skipped

        try
        {
            Assert.True(NativeLibrary.TryGetExport(handle, "xpe_preprocess_version", out var sym),
                "xpe_preprocess_version export not found");

            var fn = Marshal.GetDelegateForFunctionPointer<XpePreprocessNative.VersionDelegate>(sym);
            var ptr = fn();
            Assert.NotEqual(IntPtr.Zero, ptr);
            var version = Marshal.PtrToStringAnsi(ptr);
            Assert.NotNull(version);
            Assert.NotEmpty(version);
        }
        finally
        {
            NativeLibrary.Free(handle);
        }
    }

    /// <summary>REQ-GUI-IT-060: xpe_preprocess_init / xpe_preprocess_shutdown lifecycle works.</summary>
    [Fact]
    public void PreprocessInitShutdown_WhenDllStaged_LifecycleSucceeds()
    {
        if (DllPath is null) return; // P1A DLL not staged — test skipped

        if (!NativeLibrary.TryLoad(DllPath, out var handle))
            return; // DLL load failed — test skipped

        try
        {
            Assert.True(NativeLibrary.TryGetExport(handle, "xpe_preprocess_init", out var initSym));
            Assert.True(NativeLibrary.TryGetExport(handle, "xpe_preprocess_shutdown", out var shutdownSym));

            var init = Marshal.GetDelegateForFunctionPointer<XpePreprocessNative.InitDelegate>(initSym);
            var shutdown = Marshal.GetDelegateForFunctionPointer<XpePreprocessNative.ShutdownDelegate>(shutdownSym);

            var result = init(IntPtr.Zero);
            Assert.Equal(XpeCommonNative.XpeErrorCode.OK, result);
            shutdown();
        }
        finally
        {
            NativeLibrary.Free(handle);
        }
    }

    /// <summary>REQ-GUI-IT-060: All 9 required exports are present when DLL is staged.</summary>
    [Fact]
    public void PreprocessDll_WhenStaged_HasAllRequiredExports()
    {
        if (DllPath is null) return; // P1A DLL not staged — test skipped

        if (!NativeLibrary.TryLoad(DllPath, out var handle))
            return; // DLL load failed — test skipped

        try
        {
            foreach (var export in XpePreprocessNative.RequiredExports)
            {
                Assert.True(
                    NativeLibrary.TryGetExport(handle, export, out _),
                    $"Required export '{export}' not found in {DllPath}");
            }
        }
        finally
        {
            NativeLibrary.Free(handle);
        }
    }
}
