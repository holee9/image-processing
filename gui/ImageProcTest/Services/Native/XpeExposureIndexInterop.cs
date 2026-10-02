// #225 row 9 (GUI-C-196 M4): the exposure-index call the Deterministic Baseline reports (EI-0, design D1). Declared apart from XpeEnhanceBasicNative so the
// enhance stage's file stays as reviewed.
using System.Runtime.InteropServices;

namespace ImageProcTest.Services.Native;

/// <summary><c>xpe_calc_exposure_index</c> in <c>xpe_enhance_basic.dll</c>: EI and DI of a float32 detector-domain image (read-only).</summary>
internal static class XpeExposureIndexNative
{
    private const string DllName = "xpe_enhance_basic.dll";

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_calc_exposure_index(ref XpeImageBufferNative img, ref XpeImageMetadataNative meta, out float exposureIndex, out float deviationIndex);
}
