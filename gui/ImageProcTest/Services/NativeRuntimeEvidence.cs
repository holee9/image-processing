using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using ImageProcTest.Services.Native;

namespace ImageProcTest.Services;

/// <summary>Actual module paths and SHA; semantic versions alone cannot certify a mixed build.</summary>
public static class NativeRuntimeEvidence
{
    public static void VerifyPreCombination(string toolDirectory)
    {
        foreach (var name in new[] { "xpe_common.dll", "xpe_preprocess.dll" })
        {
            NativeModuleAudit.PreflightModule(name, "PRE calibration apply");
            var loaded = Process.GetCurrentProcess().Modules.Cast<ProcessModule>().SingleOrDefault(m => string.Equals(m.ModuleName, name, StringComparison.OrdinalIgnoreCase))
                ?? throw new InvalidOperationException($"실제 {name} 로드 증거 없음. Native backend와 고정 native 폴더로 실행하십시오.");
            if (CalibrationWorkflowService.Hash(loaded.FileName) != CalibrationWorkflowService.Hash(Path.Combine(toolDirectory, name)))
                throw new InvalidOperationException($"GUI 실제 {name}와 검사 도구의 DLL SHA가 다릅니다. 조합을 검증하기 전 적용하지 않습니다.");
            var handle = NativeLibrary.Load(loaded.FileName);
            try
            {
                var exports = name == "xpe_preprocess.dll"
                    ? new[] { "xpe_preprocess_init", "xpe_preprocess_shutdown", "xpe_calib_load_offset", "xpe_calib_load_gain", "xpe_calib_load_defect_map", "xpe_preprocess_pipeline_out" }
                    : new[] { "xpe_version", "xpe_alloc_image", "xpe_free_image" };
                foreach (var export in exports)
                    if (!NativeLibrary.TryGetExport(handle, export, out _)) throw new InvalidOperationException($"{name} export 없음: {export}");
            }
            finally { NativeLibrary.Free(handle); }
        }
    }
    public static object Capture()
    {
        var modules = new List<object>();
        foreach (ProcessModule module in Process.GetCurrentProcess().Modules)
            if (NativeModuleAudit.IsNativeModuleOfTheProduct(module.ModuleName))
                modules.Add(new { name = module.ModuleName, path = module.FileName, sha256 = CalibrationWorkflowService.Hash(module.FileName) });
        var assembly = typeof(NativeRuntimeEvidence).Assembly.Location;
        return new { guiAssembly = assembly, guiSha256 = CalibrationWorkflowService.Hash(assembly), modules,
            abi = "x64 Cdecl Pack=8; buffer40 metadata96; public loader/pipeline calls validated separately",
            limitation = "로드 경로/해시는 식별 증거. 전체 조합의 제품·실영상 품질 승인을 의미하지 않음" };
    }
}
