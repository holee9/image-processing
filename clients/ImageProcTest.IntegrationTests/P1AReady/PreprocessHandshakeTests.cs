// AC-14: Optional P1A tests skip cleanly when xpe_preprocess.dll absent.
using System.Runtime.InteropServices;
using ImageProcTest.IntegrationTests.Fixtures;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.P1AReady;

/// <summary>
/// Optional lifecycle tests for xpe_preprocess.dll.
/// These tests activate automatically when the DLL is staged in the build tree.
/// Covers REQ-GUI-IT-060, AC-14, AC-15.
/// </summary>
[Trait("Category", "P1AReady")]
[Collection(ImageProcTest.IntegrationTests.Fixtures.PreprocessModuleCollection.Name)]
public sealed class PreprocessHandshakeTests
{
    private static readonly string? DllPath = XpePreprocessNative.TryFindDll();
    private static readonly string SkipReason = DllPath is null
        ? "Skipped: xpe_preprocess.dll not staged — build P1A first or set XPE_NATIVE_DIR"
        : string.Empty;

    /// <summary>REQ-GUI-IT-060: xpe_preprocess_version export exists and returns non-empty string.</summary>
    [SkippableFact]
    public void PreprocessVersion_WhenDllStaged_ReturnsNonEmptyString()
    {
        SkipHelper.SkipIf(DllPath is null, SkipReason);

        SkipHelper.SkipIf(!NativeLibrary.TryLoad(DllPath!, out var handle), $"Skipped: xpe_preprocess.dll load failed: {DllPath}");

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
    [SkippableFact]
    public void PreprocessInitShutdown_WhenDllStaged_LifecycleSucceeds()
    {
        SkipHelper.SkipIf(DllPath is null, SkipReason);

        SkipHelper.SkipIf(!NativeLibrary.TryLoad(DllPath!, out var handle), $"Skipped: xpe_preprocess.dll load failed: {DllPath}");

        try
        {
            Assert.True(NativeLibrary.TryGetExport(handle, "xpe_preprocess_init", out var initSym));
            Assert.True(NativeLibrary.TryGetExport(handle, "xpe_preprocess_shutdown", out var shutdownSym));

            var init = Marshal.GetDelegateForFunctionPointer<XpePreprocessNative.InitDelegate>(initSym);
            var shutdown = Marshal.GetDelegateForFunctionPointer<XpePreprocessNative.ShutdownDelegate>(shutdownSym);

            // GUI-C-233h (Codex #177): the module must be UNINITIALISED here. No shutdown() first - that would hide the test that left it initialised; if one did, init is refused and this test is
            // red, which is how the culprit shows (PreprocessModuleLeakTests names it; DataSizeContractTests was one). It ends uninitialised on every path, including a failed assertion.
            try
            {
                var result = init(IntPtr.Zero);
                Assert.True(result == XpeCommonNative.XpeErrorCode.OK,
                    $"xpe_preprocess_init answered {result}: the module was already initialised, i.e. an earlier test left it initialised (see PreprocessModuleLeakTests)");
            }
            finally
            {
                shutdown();
            }
        }
        finally
        {
            NativeLibrary.Free(handle);
        }
    }

    /// <summary>
    /// GUI-C-233g/h (Codex #176, #177): the mechanism that makes the test above fail, made deterministic. The module has one process-wide state; while it is initialised (by a test that did not shut
    /// it down), a second <c>xpe_preprocess_init</c> is refused (INVALID_INPUT), which is exactly the assertion that failed. This pins both halves: init without a prior shutdown is refused, init after
    /// one succeeds. Which earlier test actually left it initialised in the failing runs was never captured; PreprocessModuleLeakTests found and fixed one that could (DataSizeContractTests).
    /// </summary>
    [SkippableFact]
    public void PreprocessInit_WhileAnotherCallerHoldsTheModuleInitialised_IsRefused_AndSucceedsAfterAShutdown()
    {
        SkipHelper.SkipIf(DllPath is null, SkipReason);

        SkipHelper.SkipIf(!NativeLibrary.TryLoad(DllPath!, out var handle), $"Skipped: xpe_preprocess.dll load failed: {DllPath}");

        try
        {
            Assert.True(NativeLibrary.TryGetExport(handle, "xpe_preprocess_init", out var initSym));
            Assert.True(NativeLibrary.TryGetExport(handle, "xpe_preprocess_shutdown", out var shutdownSym));
            var init = Marshal.GetDelegateForFunctionPointer<XpePreprocessNative.InitDelegate>(initSym);
            var shutdown = Marshal.GetDelegateForFunctionPointer<XpePreprocessNative.ShutdownDelegate>(shutdownSym);

            try
            {
                Assert.Equal(XpeCommonNative.XpeErrorCode.OK, init(IntPtr.Zero));                    // the "other caller" now holds the module (no shutdown first: an earlier leak must show here too)
                Assert.NotEqual(XpeCommonNative.XpeErrorCode.OK, init(IntPtr.Zero));                 // a second caller without a shutdown: refused - the failure observed in the full runs
                shutdown();
                Assert.Equal(XpeCommonNative.XpeErrorCode.OK, init(IntPtr.Zero));                    // with the start-of-test shutdown the same call succeeds
            }
            finally
            {
                shutdown();
            }
        }
        finally
        {
            NativeLibrary.Free(handle);
        }
    }

    /// <summary>REQ-GUI-IT-060: All 9 required exports are present when DLL is staged.</summary>
    [SkippableFact]
    public void PreprocessDll_WhenStaged_HasAllRequiredExports()
    {
        SkipHelper.SkipIf(DllPath is null, SkipReason);

        SkipHelper.SkipIf(!NativeLibrary.TryLoad(DllPath!, out var handle), $"Skipped: xpe_preprocess.dll load failed: {DllPath}");

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
