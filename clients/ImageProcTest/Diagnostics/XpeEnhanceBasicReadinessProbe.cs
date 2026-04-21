using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using ImageProcTest.PInvokeWrappers;

namespace ImageProcTest
{
    internal sealed record EnhanceBasicSmokeResult(
        string Status,
        bool Passed,
        double LatencyMs,
        float SigmaBefore,
        float SigmaAfter,
        float ExposureIndex,
        float DeviationIndex,
        string Details)
    {
        public static EnhanceBasicSmokeResult NotRun(string reason) =>
            new("Not run", false, 0, 0, 0, 0, 0, reason);
    }

    internal sealed record EnhanceBasicHealthResult(
        string Status,
        string Version,
        string DllPath,
        string Details,
        IReadOnlyList<string> PresentExports,
        IReadOnlyList<string> MissingExports,
        EnhanceBasicSmokeResult Smoke,
        bool IsVersionReady,
        bool IsExportReady,
        bool IsSmokeReady);

    internal static class XpeEnhanceBasicReadinessProbe
    {
        private static readonly string[] RequiredExports =
        [
            "xpe_enhance_basic_version",
            "xpe_log_transform",
            "xpe_log_inverse",
            "xpe_noise_reduce",
            "xpe_noise_estimate_sigma",
            "xpe_contrast_enhance",
            "xpe_edge_enhance",
            "xpe_calc_exposure_index"
        ];

        public static EnhanceBasicHealthResult Check()
        {
            foreach (var candidate in XpeEnhanceBasicLibraryLocator.GetDllCandidates())
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
                    if (NativeLibrary.TryGetExport(handle, "xpe_enhance_basic_version", out var versionSymbol))
                    {
                        var versionPtr = Marshal.GetDelegateForFunctionPointer<XpeEnhanceBasicWrapper.VersionDelegate>(versionSymbol)();
                        version = Marshal.PtrToStringAnsi(versionPtr) ?? "<empty>";
                    }

                    if (missing.Length > 0)
                    {
                        return new EnhanceBasicHealthResult(
                            "Export checklist incomplete",
                            version,
                            candidate,
                            "xpe_enhance_basic.dll exists but mandatory post-basic exports are missing.",
                            present,
                            missing,
                            EnhanceBasicSmokeResult.NotRun("Export checklist is incomplete."),
                            version != "Unavailable",
                            IsExportReady: false,
                            IsSmokeReady: false);
                    }

                    var smoke = RunSmoke(handle);
                    return new EnhanceBasicHealthResult(
                        smoke.Passed ? "ABI smoke ready" : "Export checklist ready",
                        version,
                        candidate,
                        smoke.Passed
                            ? "Enhance basic exports are discoverable and float32 ABI smoke passed."
                            : "Enhance basic exports are discoverable, but ABI smoke failed.",
                        present,
                        missing,
                        smoke,
                        IsVersionReady: true,
                        IsExportReady: true,
                        IsSmokeReady: smoke.Passed);
                }
                finally
                {
                    NativeLibrary.Free(handle);
                }
            }

            return new EnhanceBasicHealthResult(
                "DLL not found",
                "Unavailable",
                XpeEnhanceBasicLibraryLocator.DllName,
                "xpe_enhance_basic.dll is not available in known GUI/build output locations.",
                [],
                RequiredExports,
                EnhanceBasicSmokeResult.NotRun("DLL not found."),
                IsVersionReady: false,
                IsExportReady: false,
                IsSmokeReady: false);
        }

        private static EnhanceBasicSmokeResult RunSmoke(IntPtr handle)
        {
            var stopwatch = System.Diagnostics.Stopwatch.StartNew();
            var pixels = CreateSyntheticFloatImage(32, 32);
            var imageHandle = GCHandle.Alloc(pixels, GCHandleType.Pinned);
            try
            {
                var buffer = CreateFloatBuffer(32, 32, pixels.Length);
                buffer.Data = imageHandle.AddrOfPinnedObject();

                var log = XpeEnhanceBasicWrapper.GetRequiredDelegate<XpeEnhanceBasicWrapper.LogTransformDelegate>(
                    handle,
                    "xpe_log_transform");
                var noise = XpeEnhanceBasicWrapper.GetRequiredDelegate<XpeEnhanceBasicWrapper.NoiseReduceDelegate>(
                    handle,
                    "xpe_noise_reduce");
                var sigma = XpeEnhanceBasicWrapper.GetRequiredDelegate<XpeEnhanceBasicWrapper.NoiseEstimateSigmaDelegate>(
                    handle,
                    "xpe_noise_estimate_sigma");
                var contrast = XpeEnhanceBasicWrapper.GetRequiredDelegate<XpeEnhanceBasicWrapper.ContrastEnhanceDelegate>(
                    handle,
                    "xpe_contrast_enhance");
                var edge = XpeEnhanceBasicWrapper.GetRequiredDelegate<XpeEnhanceBasicWrapper.EdgeEnhanceDelegate>(
                    handle,
                    "xpe_edge_enhance");
                var ei = XpeEnhanceBasicWrapper.GetRequiredDelegate<XpeEnhanceBasicWrapper.CalcExposureIndexDelegate>(
                    handle,
                    "xpe_calc_exposure_index");

                var sigmaBeforeCode = sigma(ref buffer, out var sigmaBefore);
                var metadata = CreateMetadata();
                var eiCode = ei(ref buffer, ref metadata, out var exposureIndex, out var deviationIndex);
                var logCode = log(ref buffer, 1000.0f);
                var noiseParams = XpeNoiseReduceParams.DefaultBilateral;
                var noiseCode = noise(ref buffer, ref noiseParams);
                var claheParams = XpeClaheParams.Default;
                var contrastCode = contrast(ref buffer, ref claheParams);
                var usmParams = XpeUsmParams.Default;
                var edgeCode = edge(ref buffer, ref usmParams);
                var sigmaAfterCode = sigma(ref buffer, out var sigmaAfter);
                stopwatch.Stop();

                var codes = new[]
                {
                    sigmaBeforeCode,
                    eiCode,
                    logCode,
                    noiseCode,
                    contrastCode,
                    edgeCode,
                    sigmaAfterCode
                };
                var finite = pixels.All(float.IsFinite) &&
                    float.IsFinite(sigmaBefore) &&
                    float.IsFinite(sigmaAfter) &&
                    float.IsFinite(exposureIndex) &&
                    float.IsFinite(deviationIndex);
                var passed = codes.All(code => code == XpeCommonApi.XpeErrorCode.OK) && finite;

                return new EnhanceBasicSmokeResult(
                    passed ? "Pass" : "Fail",
                    passed,
                    stopwatch.Elapsed.TotalMilliseconds,
                    sigmaBefore,
                    sigmaAfter,
                    exposureIndex,
                    deviationIndex,
                    $"codes={string.Join("/", codes.Select(code => code.ToString()))}; finite={finite}");
            }
            catch (Exception ex)
            {
                stopwatch.Stop();
                return new EnhanceBasicSmokeResult(
                    "Exception",
                    false,
                    stopwatch.Elapsed.TotalMilliseconds,
                    0,
                    0,
                    0,
                    0,
                    ex.Message);
            }
            finally
            {
                imageHandle.Free();
            }
        }

        private static float[] CreateSyntheticFloatImage(int width, int height)
        {
            var pixels = new float[width * height];
            for (var y = 0; y < height; y++)
            {
                for (var x = 0; x < width; x++)
                {
                    pixels[y * width + x] = 800.0f + x * 7.0f + y * 5.0f + ((x + y) % 5) * 11.0f;
                }
            }

            return pixels;
        }

        private static XpeCommonApi.XpeImageBuffer CreateFloatBuffer(int width, int height, int pixelCount)
        {
            return new XpeCommonApi.XpeImageBuffer
            {
                Width = checked((uint)width),
                Height = checked((uint)height),
                BitsAllocated = 32,
                BitsStored = 32,
                Format = XpeCommonApi.XpePixelFormat.Float32,
                DataSize = (nuint)checked(pixelCount * sizeof(float))
            };
        }

        private static XpeCommonApi.XpeImageMetadata CreateMetadata()
        {
            return new XpeCommonApi.XpeImageMetadata
            {
                BodyPart = "CHEST",
                KVp = 120.0f,
                MAs = 10.0f,
                SID_mm = 1200.0f,
                PixelPitch_mm = 0.143f,
                AcquisitionTime = 0,
                Flags = 0
            };
        }
    }
}
