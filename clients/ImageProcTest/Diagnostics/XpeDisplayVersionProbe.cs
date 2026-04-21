using System;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using ImageProcTest.PInvokeWrappers;

namespace ImageProcTest
{
    internal static class XpeDisplayVersionProbe
    {
        private static readonly string[] RequiredExports =
        [
            "xpe_display_version",
            "xpe_apply_modality_lut",
            "xpe_apply_voi_lut",
            "xpe_voi_preset_create",
            "xpe_apply_presentation_lut",
            "xpe_gsdf_calibrate"
        ];

        public static DisplayHealthResult Check()
        {
            foreach (var candidate in NativeModuleLibraryLocator.GetDllCandidates(
                         XpeDisplayWrapper.DllName,
                         "image-processing",
                         "xpe-post"))
            {
                if (!File.Exists(candidate))
                {
                    continue;
                }

                NativeDependencyLoader.TryLoadFor(candidate);
                if (!NativeLibrary.TryLoad(candidate, out var handle))
                {
                    continue;
                }

                try
                {
                    var present = RequiredExports
                        .Where(name => NativeLibrary.TryGetExport(handle, name, out _))
                        .ToArray();
                    var missing = RequiredExports.Except(present).ToArray();
                    var version = "Unavailable";

                    if (NativeLibrary.TryGetExport(handle, "xpe_display_version", out var symbol))
                    {
                        var versionPtr = Marshal.GetDelegateForFunctionPointer<XpeDisplayWrapper.VersionDelegate>(symbol)();
                        version = Marshal.PtrToStringAnsi(versionPtr) ?? "<empty>";
                    }

                    if (missing.Length > 0)
                    {
                        return new DisplayHealthResult(
                            Status: "Export checklist incomplete",
                            Version: version,
                            DllPath: candidate,
                            Details: "xpe_display.dll exists but mandatory display exports are missing.",
                            PresentExports: present,
                            MissingExports: missing,
                            Smoke: DisplaySmokeResult.NotRun("Export checklist is incomplete."),
                            IsVersionReady: version != "Unavailable",
                            IsExportReady: false,
                            IsSmokeReady: false);
                    }

                    var smoke = RunSmoke(handle);
                    return new DisplayHealthResult(
                        Status: smoke.Passed ? "ABI smoke ready" : "Export checklist ready",
                        Version: version,
                        DllPath: candidate,
                        Details: smoke.Passed
                            ? "Display exports are discoverable and null-guard ABI smoke passed."
                            : "Display exports are discoverable, but ABI smoke failed.",
                        PresentExports: present,
                        MissingExports: missing,
                        Smoke: smoke,
                        IsVersionReady: true,
                        IsExportReady: true,
                        IsSmokeReady: smoke.Passed);
                }
                finally
                {
                    NativeLibrary.Free(handle);
                }
            }

            return new DisplayHealthResult(
                Status: "DLL not found",
                Version: "Unavailable",
                DllPath: XpeDisplayWrapper.DllName,
                Details: "xpe_display.dll is not available in known GUI/build output locations.",
                PresentExports: [],
                MissingExports: RequiredExports,
                Smoke: DisplaySmokeResult.NotRun("DLL not found."),
                IsVersionReady: false,
                IsExportReady: false,
                IsSmokeReady: false);
        }

        private static DisplaySmokeResult RunSmoke(IntPtr handle)
        {
            try
            {
                var applyModality = XpeDisplayWrapper.GetRequiredDelegate<XpeDisplayWrapper.RawPointerDelegate>(
                    handle,
                    "xpe_apply_modality_lut");
                var applyVoi = XpeDisplayWrapper.GetRequiredDelegate<XpeDisplayWrapper.RawPointerDelegate>(
                    handle,
                    "xpe_apply_voi_lut");
                var applyPresentation = XpeDisplayWrapper.GetRequiredDelegate<XpeDisplayWrapper.RawPointerDelegate>(
                    handle,
                    "xpe_apply_presentation_lut");
                var voiPreset = XpeDisplayWrapper.GetRequiredDelegate<XpeDisplayWrapper.VoiPresetCreateDelegate>(
                    handle,
                    "xpe_voi_preset_create");
                var gsdf = XpeDisplayWrapper.GetRequiredDelegate<XpeDisplayWrapper.RawGsdfCalibrateDelegate>(
                    handle,
                    "xpe_gsdf_calibrate");

                var modalityNull = applyModality(IntPtr.Zero, IntPtr.Zero);
                var voiNull = applyVoi(IntPtr.Zero, IntPtr.Zero);
                var presentationNull = applyPresentation(IntPtr.Zero, IntPtr.Zero);
                var gsdfNull = gsdf(IntPtr.Zero, 0, IntPtr.Zero);
                var preset = XpeVoiLutParams.UnitWindow;
                var presetCode = voiPreset(ref preset, XpeBodyPart.Lung);
                var presetFinite = float.IsFinite(preset.Center) &&
                    float.IsFinite(preset.Width) &&
                    preset.Width > 0.0f;
                var passed =
                    modalityNull == XpeCommonApi.XpeErrorCode.INVALID_INPUT &&
                    voiNull == XpeCommonApi.XpeErrorCode.INVALID_INPUT &&
                    presentationNull == XpeCommonApi.XpeErrorCode.INVALID_INPUT &&
                    gsdfNull == XpeCommonApi.XpeErrorCode.INVALID_INPUT &&
                    presetCode == XpeCommonApi.XpeErrorCode.OK &&
                    presetFinite;

                return new DisplaySmokeResult(
                    passed ? "Pass" : "Fail",
                    passed,
                    $"nullGuards={modalityNull}/{voiNull}/{presentationNull}/{gsdfNull}; preset={presetCode}; presetFinite={presetFinite}");
            }
            catch (Exception ex)
            {
                return new DisplaySmokeResult("Exception", false, ex.Message);
            }
        }
    }
}
