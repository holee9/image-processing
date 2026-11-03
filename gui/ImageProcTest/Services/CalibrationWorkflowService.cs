using System.Diagnostics;
using System.IO;
using System.Security.Cryptography;
using System.Text.Json;

namespace ImageProcTest.Services;

/// <summary>Operator workflow over the existing PRE tools, with immutable per-run evidence.</summary>
public sealed record CalibrationInputs(int Width, int Height, string[] Dark, string[] Gain,
    string[] Defect, bool KeepFirstDark, ulong ExpiryMs);

public sealed class CalibrationWorkflowService
{
    public string RunDirectory { get; }
    public string SetDirectory => Path.Combine(RunDirectory, "calibration-set");
    public string NativeDirectory { get; }
    public CalibrationInputs Inputs { get; }
    private readonly Dictionary<string, string> _inputHashes;
    private bool _validated;
    private bool _generated;
    private Dictionary<string, string>? _reloadedHashes;

    public CalibrationWorkflowService(string outputRoot, string nativeDirectory, CalibrationInputs inputs)
    {
        Inputs = inputs with { Dark = (string[])inputs.Dark.Clone(), Gain = (string[])inputs.Gain.Clone(), Defect = (string[])inputs.Defect.Clone() };
        NativeDirectory = Path.GetFullPath(nativeDirectory);
        RunDirectory = Path.Combine(Path.GetFullPath(outputRoot), $"pre-{DateTime.UtcNow:yyyyMMddTHHmmssfffZ}-{Guid.NewGuid():N}");
        if (inputs.Width <= 0 || inputs.Height <= 0) throw new InvalidOperationException("영상 크기는 양수여야 합니다.");
        _inputHashes = new(StringComparer.OrdinalIgnoreCase);
        foreach (var path in inputs.Dark.Concat(inputs.Gain).Concat(inputs.Defect))
        {
            var full = Path.GetFullPath(path);
            if (new FileInfo(full).Length != checked((long)inputs.Width * inputs.Height * 2))
                throw new InvalidOperationException($"UINT16LE 크기 불일치: {full}");
            _inputHashes[full] = Hash(full);
        }
        Directory.CreateDirectory(RunDirectory);
        WriteNew(Path.Combine(RunDirectory, "inputs.json"), new
        {
            inputs = Inputs, files = _inputHashes,
            acquisitionMetadata = "사용자 촬영기록과 대조 필요; kVp/mAs/SID/온도/PREP 미확인",
            offsetGeneratorTemplate = "기존 생성기 100 ms / 25 C: 취득 실측값 아님",
            quality = "미검증: 생성/재로드/적용 성공은 실영상 품질 합격이 아님"
        });
    }

    public async Task<string> ValidateAsync()
    {
        VerifyInputs();
        _validated = false;
        var result = await ToolAsync("validate", Path.Combine(NativeDirectory, "xpe_calib_fixture_gen.exe"), GeneratorArguments(true));
        VerifyInputs();
        _validated = result.ExitCode == 0;
        if (!_validated) throw new InvalidOperationException($"입력 검사 실패 ({result.ExitCode}). {result.Evidence}");
        return result.Evidence;
    }

    public async Task<string> GenerateAsync()
    {
        if (!_validated) throw new InvalidOperationException("먼저 입력 검사를 통과하십시오.");
        VerifyInputs();
        _generated = false;
        var result = await ToolAsync("generate", Path.Combine(NativeDirectory, "xpe_calib_fixture_gen.exe"), GeneratorArguments(false));
        VerifyInputs();
        if (result.ExitCode != 0 || !MapPaths().All(File.Exists) || !File.Exists(Path.Combine(SetDirectory, "manifest.json")))
            throw new InvalidOperationException($"생성 실패/불완전 ({result.ExitCode}); 경로를 재사용하지 마십시오. {result.Evidence}");
        VerifyManifest();
        _generated = true;
        return result.Evidence;
    }

    public async Task<string> ReloadAsync()
    {
        if (!_generated) throw new InvalidOperationException("이 실행의 생성 완료 후 재로드하십시오.");
        VerifyManifest();
        var report = Path.Combine(RunDirectory, $"reload-{Guid.NewGuid():N}.json");
        var arguments = new List<string>();
        var executable = Path.Combine(AppContext.BaseDirectory, "ImageProcTest.exe");
        if (!File.Exists(executable))
        {
            executable = "dotnet";
            arguments.Add(typeof(CalibrationWorkflowService).Assembly.Location);
        }
        arguments.AddRange(["--inspect-calibration-set", SetDirectory, report]);
        _reloadedHashes = null;
        var result = await ToolAsync("reload", executable, arguments);
        if (result.ExitCode != 0 || !File.Exists(report))
            throw new InvalidOperationException($"PRE 공개 API 재로드 실패 ({result.ExitCode}). {result.Evidence}");
        using var document = JsonDocument.Parse(File.ReadAllText(report));
        _reloadedHashes = ValidateReloadReport(document.RootElement, MapPaths().ToDictionary(p => p, Hash, StringComparer.OrdinalIgnoreCase));
        return report;
    }

    public static Dictionary<string, string> ValidateReloadReport(JsonElement root, IReadOnlyDictionary<string, string> current)
    {
        if (!root.GetProperty("passed").GetBoolean()) throw new InvalidOperationException("재로드 보고서가 실패입니다.");
        var checkedHashes = root.GetProperty("loads").EnumerateArray().ToDictionary(
            item => item.GetProperty("path").GetString()!, item => item.GetProperty("sha256").GetString()!, StringComparer.OrdinalIgnoreCase);
        if (checkedHashes.Count != 3 || current.Count != 3 || root.GetProperty("loads").EnumerateArray().Any(item => item.GetProperty("code").GetInt32() != 0))
            throw new IOException("재로드 보고서가 정확한 3개 map 성공을 입증하지 않습니다.");
        foreach (var (path, sha) in current)
            if (!checkedHashes.TryGetValue(path, out var inspected) || inspected != sha)
                throw new IOException("재로드 시점과 현재 map SHA가 다릅니다. 새 실행으로 검사하십시오.");
        return checkedHashes;
    }

    public void VerifyReadyToApply()
    {
        if (_reloadedHashes is null) throw new InvalidOperationException("생성한 세트를 별도 PRE 프로세스에서 재로드한 뒤 적용하십시오.");
        VerifyManifest();
        foreach (var (path, hash) in _reloadedHashes)
            if (Hash(path) != hash) throw new InvalidOperationException($"재로드 후 파일 변경: {path}. 다시 검사하십시오.");
    }

    public async Task<string> EvaluateAsync(string holdout, string inputKind, string oracle, string reference, string gainSemantics = "unknown")
    {
        VerifyReadyToApply();
        VerifyInputs();
        if (inputKind is not ("image" or "dark" or "flat")) throw new InvalidOperationException("영상 종류를 선택하십시오.");
        if (gainSemantics is not ("unknown" or "normalized_gain" or "reciprocal_gain")) throw new InvalidOperationException("Gain 의미를 명시하십시오.");
        var raw = Path.GetFullPath(holdout);
        if (new FileInfo(raw).Length != checked((long)Inputs.Width * Inputs.Height * 2))
            throw new InvalidOperationException("독립 평가 RAW 크기 불일치.");
        var before = Hash(raw);
        if (_inputHashes.Values.Contains(before, StringComparer.OrdinalIgnoreCase))
            throw new InvalidOperationException("학습/생성 목록과 같은 내용의 영상은 독립 holdout으로 사용할 수 없습니다 (제외한 첫 Dark 포함).");
        var output = Path.Combine(RunDirectory, "evaluation-" + Guid.NewGuid().ToString("N"));
        var args = new List<string> { "--raw", raw, "--calib", SetDirectory, "--width", Inputs.Width.ToString(),
            "--height", Inputs.Height.ToString(), "--input-kind", inputKind, "--gain-semantics", gainSemantics, "--out", output };
        if (!string.IsNullOrWhiteSpace(oracle)) { args.Add("--oracle-bpm"); args.Add(Path.GetFullPath(oracle)); args.Add("--oracle-kind"); args.Add("reference"); }
        if (!string.IsNullOrWhiteSpace(reference)) { args.Add("--reference-f32"); args.Add(Path.GetFullPath(reference)); }
        var result = await ToolAsync("evaluate", Path.Combine(NativeDirectory, "xpe_pre_e2e.exe"), args);
        var after = Hash(raw);
        WriteNew(Path.Combine(RunDirectory, "holdout-" + Guid.NewGuid().ToString("N") + ".json"), new
        { path = raw, before, after, unchanged = before == after, output, result.ExitCode,
            oracle = string.IsNullOrWhiteSpace(oracle) ? "未提供 / 외부 결함 정답 미검증" : Path.GetFullPath(oracle),
            reference = string.IsNullOrWhiteSpace(reference) ? "미제공 / 외부 수치 동등성 미검증" : Path.GetFullPath(reference),
            independence = "생성 입력 SHA와 불일치 확인; oracle/reference 출처 독립성은 사용자 증거 필요",
            quality = "exit 0 또는 Applied는 합격 아님. evaluation report의 phase1/release/CES 및 미평가 지표를 확인" });
        if (before != after) throw new InvalidOperationException("평가 전후 원본 SHA가 다릅니다.");
        if (result.ExitCode != 0) throw new InvalidOperationException($"평가 실행 실패 ({result.ExitCode}). {result.Evidence}");
        VerifyReadyToApply();
        return output;
    }

    public static string QualitySummary(string directory)
    {
        using var report = JsonDocument.Parse(File.ReadAllText(Path.Combine(directory, "report.json")));
        var root = report.RootElement;
        return $"phase1={root.GetProperty("phase1_pass")}; release={root.GetProperty("release_pass")}; score={root.GetProperty("score").GetRawText()}\n"
            + string.Join("\n", root.GetProperty("gates").EnumerateArray().Select(g => $"{g.GetProperty("id").GetString()}: {g.GetProperty("result").GetString()}"));
    }

    public static string Hash(string path)
    {
        using var stream = File.OpenRead(path);
        return Convert.ToHexString(SHA256.HashData(stream)).ToLowerInvariant();
    }

    public static void WriteNew(string path, object value)
    {
        using var stream = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read);
        JsonSerializer.Serialize(stream, value, new JsonSerializerOptions { WriteIndented = true });
    }

    private string[] MapPaths() => [Path.Combine(SetDirectory, "offset.xcal"), Path.Combine(SetDirectory, "gain.xcal"), Path.Combine(SetDirectory, "defect.xcal")];
    private void VerifyManifest()
    {
        using var manifest = JsonDocument.Parse(File.ReadAllText(Path.Combine(SetDirectory, "manifest.json")));
        var consumed = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var item in manifest.RootElement.GetProperty("inputs").EnumerateArray())
        {
            var path = Path.GetFullPath(item.GetProperty("path").GetString()!);
            if (!_inputHashes.TryGetValue(path, out var baseline) || item.GetProperty("sha256").GetString() != baseline)
                throw new IOException("생성기가 실제 소비한 입력 SHA가 등록 baseline과 다릅니다: " + path);
            consumed.Add(path);
        }
        if (!consumed.SetEquals(_inputHashes.Keys)) throw new IOException("생성 manifest의 입력 목록이 등록 목록과 다릅니다.");
        foreach (var path in MapPaths())
            if (manifest.RootElement.GetProperty("files").GetProperty(Path.GetFileName(path)).GetString() != Hash(path))
                throw new IOException($"생성 manifest와 map SHA 불일치: {path}");
    }
    private void VerifyInputs()
    {
        foreach (var (path, hash) in _inputHashes)
            if (Hash(path) != hash) throw new InvalidOperationException($"검사 후 입력 변경: {path}. 새 실행을 만드십시오.");
    }
    private List<string> GeneratorArguments(bool validate)
    {
        var args = new List<string> { "--out", SetDirectory, "--width", Inputs.Width.ToString(), "--height", Inputs.Height.ToString(),
            "--expiry-ms", Inputs.ExpiryMs.ToString(), "--bpm-v2" };
        if (validate) args.Add("--validate-only");
        if (Inputs.KeepFirstDark) args.Add("--keep-first-dark");
        foreach (var p in Inputs.Dark) { args.Add("--dark"); args.Add(Path.GetFullPath(p)); }
        foreach (var p in Inputs.Gain) { args.Add("--bright"); args.Add(Path.GetFullPath(p)); }
        foreach (var p in Inputs.Defect) { args.Add("--bpm-bright"); args.Add(Path.GetFullPath(p)); }
        return args;
    }
    private async Task<(int ExitCode, string Evidence)> ToolAsync(string operation, string executable, IEnumerable<string> arguments)
    {
        var args = arguments.ToArray();
        var info = new ProcessStartInfo(executable) { UseShellExecute = false, CreateNoWindow = true, WorkingDirectory = RunDirectory };
        foreach (var arg in args) info.ArgumentList.Add(arg);
        info.Environment["XPE_NATIVE_DIR"] = NativeDirectory;
        info.Environment["XPE_NATIVE_DIR_EXCLUSIVE"] = "1";
        info.Environment.Remove("DOTNET_STARTUP_HOOKS");
        info.RedirectStandardOutput = info.RedirectStandardError = true;
        using var process = Process.Start(info) ?? throw new IOException("로컬 도구를 시작하지 못했습니다.");
        var stdout = process.StandardOutput.ReadToEndAsync();
        var stderr = process.StandardError.ReadToEndAsync();
        await process.WaitForExitAsync();
        var evidence = Path.Combine(RunDirectory, operation + "-" + Guid.NewGuid().ToString("N") + ".json");
        WriteNew(evidence, new { operation, executable, executableSha256 = File.Exists(executable) ? Hash(executable) : null,
            arguments = args, nativeDirectory = NativeDirectory, exitCode = process.ExitCode,
            stdout = await stdout, stderr = await stderr, utc = DateTime.UtcNow });
        return (process.ExitCode, evidence);
    }
}
