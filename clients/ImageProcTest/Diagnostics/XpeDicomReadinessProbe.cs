using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using ImageProcTest.PInvokeWrappers;

namespace ImageProcTest
{
    internal static class XpeDicomReadinessProbe
    {
        private static readonly string[] RequiredExports =
        [
            "xpe_dicom_open",
            "xpe_dicom_read_image",
            "xpe_dicom_get_metadata",
            "xpe_dicom_close",
            "xpe_dicom_write",
            "xpe_dicom_write_j2k",
            "xpe_dicom_validate",
            "xpe_dicom_cstore",
            "xpe_dicom_cfind_mwl",
            "xpe_dicom_cancel"
        ];

        public static DicomHealthResult Check()
        {
            foreach (var candidate in NativeModuleLibraryLocator.GetDllCandidates(
                         XpeDicomWrapper.DllName,
                         "dicom",
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

                    if (missing.Length > 0)
                    {
                        return new DicomHealthResult(
                            "Export checklist incomplete",
                            candidate,
                            "xpe_dicom.dll exists but mandatory DICOM I/O exports are missing.",
                            present,
                            missing,
                            DicomSmokeResult.NotRun("Export checklist is incomplete."),
                            IsExportReady: false,
                            IsSmokeReady: false);
                    }

                    var smoke = RunSmoke(handle);
                    return new DicomHealthResult(
                        smoke.Passed ? "ABI smoke ready" : "Export checklist ready",
                        candidate,
                        smoke.Passed
                            ? "DICOM exports are discoverable and invalid-input validation ABI smoke passed."
                            : "DICOM exports are discoverable, but invalid-input validation smoke did not pass.",
                        present,
                        missing,
                        smoke,
                        IsExportReady: true,
                        IsSmokeReady: smoke.Passed);
                }
                finally
                {
                    NativeLibrary.Free(handle);
                }
            }

            return new DicomHealthResult(
                "DLL not found",
                XpeDicomWrapper.DllName,
                "xpe_dicom.dll is not available in known GUI/build output locations.",
                [],
                RequiredExports,
                DicomSmokeResult.NotRun("DLL not found."),
                IsExportReady: false,
                IsSmokeReady: false);
        }

        private static DicomSmokeResult RunSmoke(IntPtr handle)
        {
            try
            {
                var open = XpeDicomWrapper.GetRequiredDelegate<XpeDicomWrapper.OpenDelegate>(
                    handle,
                    "xpe_dicom_open");
                var validate = XpeDicomWrapper.GetRequiredDelegate<XpeDicomWrapper.ValidateDelegate>(
                    handle,
                    "xpe_dicom_validate");
                var cancel = XpeDicomWrapper.GetRequiredDelegate<XpeDicomWrapper.CancelDelegate>(
                    handle,
                    "xpe_dicom_cancel");

                var openCode = open(null, out var dicomHandle);
                if (dicomHandle != IntPtr.Zero)
                {
                    var close = XpeDicomWrapper.GetRequiredDelegate<XpeDicomWrapper.CloseDelegate>(
                        handle,
                        "xpe_dicom_close");
                    close(dicomHandle);
                }

                var report = new StringBuilder(4096);
                var validateCode = validate(null, report, checked((uint)report.Capacity));
                cancel();
                var passed =
                    openCode == XpeCommonApi.XpeErrorCode.INVALID_INPUT &&
                    validateCode == XpeCommonApi.XpeErrorCode.INVALID_INPUT;

                return new DicomSmokeResult(
                    passed ? "Pass" : "Fail",
                    passed,
                    $"open(empty)={openCode}; validate(empty)={validateCode}; reportLength={report.Length}");
            }
            catch (Exception ex)
            {
                return new DicomSmokeResult("Exception", false, ex.Message);
            }
        }
    }
}
