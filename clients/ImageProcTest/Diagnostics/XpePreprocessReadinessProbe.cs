using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;

namespace ImageProcTest
{
    internal static class XpePreprocessReadinessProbe
    {
        private static readonly string[] RequiredExports =
        [
            "xpe_preprocess_version",
            "xpe_preprocess_init",
            "xpe_preprocess_shutdown",
            "xpe_calib_load_offset",
            "xpe_calib_load_gain",
            "xpe_calib_load_defect_map",
            "xpe_offset_correct",
            "xpe_gain_correct",
            "xpe_defect_correct",
            "xpe_calib_generate_offset",
            "xpe_calib_check_expiry",
            "xpe_calib_save",
            "xpe_validate_readout_artifact",
            "xpe_defect_detect_runtime",
            "xpe_preprocess_get_param_range"
        ];

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr VersionDelegate();

        public static PreprocessHealthResult Check()
        {
            foreach (var candidate in XpePreprocessLibraryLocator.GetDllCandidates())
            {
                if (!File.Exists(candidate))
                {
                    continue;
                }

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
                    var missingExecution = Array.Empty<string>();

                    var version = "Unavailable";
                    if (NativeLibrary.TryGetExport(handle, "xpe_preprocess_version", out var versionSymbol))
                    {
                        var versionPtr = Marshal.GetDelegateForFunctionPointer<VersionDelegate>(versionSymbol)();
                        version = Marshal.PtrToStringAnsi(versionPtr) ?? "<empty>";
                    }

                    if (missing.Length > 0)
                    {
                        return new PreprocessHealthResult(
                            Status: "Export checklist incomplete",
                            Version: version,
                            DllPath: candidate,
                            Details: "xpe_preprocess.dll exists but mandatory readiness exports are missing.",
                            PresentExports: present,
                            MissingExports: missing,
                            MissingExecutionExports: missingExecution,
                            SyntheticOracle: PreprocessSyntheticOracleResult.NotRun("Export checklist is incomplete."),
                            IsVersionReady: version != "Unavailable",
                            IsExportReady: false,
                            IsSyntheticOracleReady: false);
                    }

                    var synthetic = XpePreprocessSyntheticOracle.Run(candidate);
                    return new PreprocessHealthResult(
                        Status: synthetic.Passed ? "Synthetic oracle ready" : "Export checklist ready",
                        Version: version,
                        DllPath: candidate,
                        Details: synthetic.Passed
                            ? "Preprocess exports are discoverable and the 16x16 synthetic adapter-chain oracle passed. Real fixture execution remains gated until R4 fixture E2E passes."
                            : "Preprocess exports are discoverable. Native real-image execution remains gated until ABI smoke, synthetic oracle, and fixture E2E pass.",
                        PresentExports: present,
                        MissingExports: missing,
                        MissingExecutionExports: missingExecution,
                        SyntheticOracle: synthetic,
                        IsVersionReady: true,
                        IsExportReady: true,
                        IsSyntheticOracleReady: synthetic.Passed);
                }
                finally
                {
                    NativeLibrary.Free(handle);
                }
            }

            return new PreprocessHealthResult(
                Status: "DLL not found",
                Version: "Unavailable",
                DllPath: XpePreprocessLibraryLocator.DllName,
                Details: "xpe_preprocess.dll is not available in known GUI/build output locations.",
                PresentExports: [],
                MissingExports: RequiredExports,
                MissingExecutionExports: [],
                SyntheticOracle: PreprocessSyntheticOracleResult.NotRun("DLL not found."),
                IsVersionReady: false,
                IsExportReady: false,
                IsSyntheticOracleReady: false);
        }
    }
}
