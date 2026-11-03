using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;

namespace ImageProcTest.Services.Native;

/// <summary>Separate-process reload. Never clears the operator GUI's calibration store.</summary>
internal static class CalibrationSetInspector
{
    internal static int Execute(string directory, string report)
    {
        if (File.Exists(report)) return 2;
        var loads = new List<object>();
        var passed = false;
        string? error = null;
        try
        {
            if (Marshal.SizeOf<XpeImageMetadataNative>() != 96 || Marshal.SizeOf<XpeImageBufferNative>() != 40)
                throw new InvalidOperationException("PRE ABI size mismatch (96/40).");
            var init = XpePreprocessNative.xpe_preprocess_init(null);
            if (init != 0) throw new InvalidOperationException($"PRE init {init}");
            try
            {
                foreach (var (name, load) in new (string, Func<string, int>)[] {
                    ("offset.xcal", XpePreprocessNative.xpe_calib_load_offset),
                    ("gain.xcal", XpePreprocessNative.xpe_calib_load_gain),
                    ("defect.xcal", XpePreprocessNative.xpe_calib_load_defect_map) })
                {
                    var path = Path.GetFullPath(Path.Combine(directory, name));
                    var sha = CalibrationWorkflowService.Hash(path);
                    var code = load(path);
                    loads.Add(new { path, sha256 = sha, code });
                    if (code != 0) throw new InvalidOperationException($"{name} load {code}");
                    if (CalibrationWorkflowService.Hash(path) != sha) throw new IOException("Map changed during reload.");
                }
                passed = true;
            }
            finally { XpePreprocessNative.xpe_preprocess_shutdown(); }
        }
        catch (Exception ex) { error = ex.Message; }
        var modules = new List<object>();
        foreach (ProcessModule module in Process.GetCurrentProcess().Modules)
            if (module.ModuleName.StartsWith("xpe_", StringComparison.OrdinalIgnoreCase))
                modules.Add(new { module.ModuleName, path = module.FileName, sha256 = CalibrationWorkflowService.Hash(module.FileName) });
        CalibrationWorkflowService.WriteNew(report, new { passed, error, loads, modules,
            limitation = "공개 loader의 SHA/형식/만료/session 검사는 통과했어도 생성기 session generated는 detector UUID 검증이 아님. 실영상 품질 미검증." });
        return passed ? 0 : 1;
    }
}
