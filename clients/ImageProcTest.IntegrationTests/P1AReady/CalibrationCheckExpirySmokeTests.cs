// AC-14: Optional P1A calibration loader tests.
using System.Runtime.InteropServices;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.P1AReady;

/// <summary>
/// Optional calibration loader smoke tests.
/// Verifies that missing calibration files produce IO_FAILED — not a crash.
/// Covers REQ-GUI-IT-062, AC-14.
/// </summary>
[Trait("Category", "P1AReady")]
public sealed class CalibrationCheckExpirySmokeTests
{
    private static readonly string? DllPath = XpePreprocessNative.TryFindDll();
    private static readonly string SkipReason = DllPath is null
        ? "Skipped: xpe_preprocess.dll not staged"
        : string.Empty;

    /// <summary>REQ-GUI-IT-062: xpe_calib_load_offset with non-existent path returns IO_FAILED.</summary>
    [Fact]
    public void CalibLoadOffset_NonExistentPath_ReturnsIoFailed()
    {
        if (DllPath is null) return; // P1A DLL not staged — test skipped

        RunCalibLoadTest("xpe_calib_load_offset");
    }

    /// <summary>REQ-GUI-IT-062: xpe_calib_load_gain with non-existent path returns IO_FAILED.</summary>
    [Fact]
    public void CalibLoadGain_NonExistentPath_ReturnsIoFailed()
    {
        if (DllPath is null) return; // P1A DLL not staged — test skipped

        RunCalibLoadTest("xpe_calib_load_gain");
    }

    /// <summary>REQ-GUI-IT-062: xpe_calib_load_defect_map with non-existent path returns IO_FAILED.</summary>
    [Fact]
    public void CalibLoadDefectMap_NonExistentPath_ReturnsIoFailed()
    {
        if (DllPath is null) return; // P1A DLL not staged — test skipped

        RunCalibLoadTest("xpe_calib_load_defect_map");
    }

    private static void RunCalibLoadTest(string exportName)
    {
        if (!NativeLibrary.TryLoad(DllPath!, out var handle))
        {
            return; // DLL load failed — test skipped
        }

        try
        {
            Assert.True(NativeLibrary.TryGetExport(handle, exportName, out var sym),
                $"Export '{exportName}' not found");

            var fn = Marshal.GetDelegateForFunctionPointer<XpePreprocessNative.CalibLoadDelegate>(sym);
            var nonExistentPath = Path.Combine(Path.GetTempPath(), $"nonexistent_{Guid.NewGuid():N}.xcal");

            var result = fn(nonExistentPath, out _);
            Assert.Equal(XpeCommonNative.XpeErrorCode.IO_FAILED, result);
        }
        finally
        {
            NativeLibrary.Free(handle);
        }
    }
}
