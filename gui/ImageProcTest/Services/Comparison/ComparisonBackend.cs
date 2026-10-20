using System.Diagnostics;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Text.Encodings.Web;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace ImageProcTest.Services.Comparison;

/// <summary>
/// QA-A-273 (Refs #278). The GUI does NOT re-implement the comparison contract (Reviewer #249, leader decision 2026-10-21): the contract is the script tools/calibration/comparison_manifest.py
/// (schema xpe-2g-pre-comparison/4, main-local 99113f2d at the time of writing), run as a CHILD PROCESS; the GUI reads its validate JSON and its result.json and shows them. A failure to run it
/// is a failure, never a pass.
/// </summary>
public sealed record ScriptIdentity(string ScriptPath, string ScriptSha256, string PythonPath, string PythonVersion);

public sealed record ValidateResponse(bool Ok, string? Failure, JsonObject? Validation, ScriptIdentity? Script, int ExitCode, string StdoutTail, string StderrTail);

public sealed record CompareResponse(bool Ok, string? Failure, string? RunDirectory, JsonObject? Result, ScriptIdentity? Script, int ExitCode, string StdoutTail, string StderrTail);

public interface IComparisonBackend
{
    Task<ValidateResponse> ValidateAsync(string manifestPath, CancellationToken cancel);
    Task<CompareResponse> CompareAsync(string manifestPath, string outRoot, CancellationToken cancel);
}

/// <summary>
/// The script SHA-256 values that a review accepted: Reviewer #251 accepted contract revision 4 (main-local 47d932e3, schema xpe-2g-pre-comparison/4) as a TOOL only (not G-IMPORT, not the GUI, not 2G, not quality).
/// Two values name the same content in the two line-ending forms: the commit content (LF) and the leader working copy (CRLF). A script whose own computed SHA-256 is neither of them is labelled "검토 안 된 계약"
/// and is never shown as a passing comparison.
/// </summary>
public static class ComparisonContractReview
{
    public static readonly string[] ReviewedScriptSha256 =
    [
        "86104cc6c11fd43a010a15abe696075bdc751db3aa0b66cf5c5925d2280ddf0c",   // git show 47d932e3:tools/calibration/comparison_manifest.py (LF)
        "e9e3850c27cf2bb56521cb945439320afea4f0952c50d5a099aca4d1dc26baa6",   // leader working copy of the same content (CRLF)
    ];
    public const string Schema = "xpe-2g-pre-comparison/4";

    public static string State(string? scriptSha256) =>
        ReviewedScriptSha256.Any(h => string.Equals(h, scriptSha256, StringComparison.OrdinalIgnoreCase)) ? "reviewed" : "unreviewed-sha-differs";

    public static bool IsReviewed(string? scriptSha256) => State(scriptSha256) == "reviewed";
}

public sealed class PythonScriptComparisonBackend(string scriptPath, string pythonPath) : IComparisonBackend
{
    private static readonly JsonSerializerOptions Indented = new() { WriteIndented = true, Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping };
    private ScriptIdentity? _identity;

    private async Task<(ScriptIdentity? Identity, string? Failure)> IdentifyAsync(CancellationToken cancel)
    {
        if (_identity is not null) return (_identity, null);
        if (string.IsNullOrWhiteSpace(scriptPath)) return (null, "계약 스크립트 경로가 비어 있습니다. 경로를 입력하십시오(기본값은 없습니다).");
        var full = Path.GetFullPath(scriptPath);
        if (!File.Exists(full)) return (null, "계약 스크립트 파일이 없습니다: " + full);
        var python = string.IsNullOrWhiteSpace(pythonPath) ? "python" : pythonPath.Trim();
        var version = await RunAsync(python, ["--version"], cancel);
        if (version.StartFailure is not null) return (null, "python 을 실행하지 못했습니다(" + python + "): " + version.StartFailure);
        if (version.ExitCode != 0) return (null, "python --version 이 종료 코드 " + version.ExitCode + " 로 끝났습니다: " + Tail(version.Stderr));
        string sha;
        await using (var s = new FileStream(full, FileMode.Open, FileAccess.Read, FileShare.Read)) sha = Convert.ToHexString(await SHA256.HashDataAsync(s, cancel)).ToLowerInvariant();
        _identity = new ScriptIdentity(full, sha, python, (version.Stdout + " " + version.Stderr).Trim());
        return (_identity, null);
    }

    public async Task<ValidateResponse> ValidateAsync(string manifestPath, CancellationToken cancel)
    {
        try
        {
            var (id, failure) = await IdentifyAsync(cancel);
            if (id is null) return new ValidateResponse(false, failure, null, null, -1, "", "");
            var r = await RunAsync(id.PythonPath, [id.ScriptPath, "validate", Path.GetFullPath(manifestPath)], cancel);
            if (r.StartFailure is not null) return new ValidateResponse(false, "스크립트를 실행하지 못했습니다: " + r.StartFailure, null, id, -1, "", "");
            if (r.ExitCode is not (0 or 2)) return new ValidateResponse(false, $"스크립트가 예상 밖의 종료 코드 {r.ExitCode} 로 끝났습니다: {Tail(r.Stderr)}", null, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr));
            JsonObject? obj;
            try { obj = JsonNode.Parse(r.Stdout) as JsonObject; }
            catch (JsonException e) { return new ValidateResponse(false, "검증 출력이 JSON 이 아닙니다: " + e.Message, null, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr)); }
            if (obj is null || obj["comparable"] is not JsonValue c || c.GetValueKind() is not (JsonValueKind.True or JsonValueKind.False) || obj["reasons"] is not JsonArray)
                return new ValidateResponse(false, "검증 출력에 comparable(true/false)·reasons(목록)가 없습니다.", null, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr));
            if (c.GetValue<bool>() != (r.ExitCode == 0)) return new ValidateResponse(false, "comparable 값과 종료 코드가 서로 맞지 않습니다.", null, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr));
            return new ValidateResponse(true, null, obj, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr));
        }
        catch (OperationCanceledException) { return new ValidateResponse(false, "취소했습니다. 실패로 기록하며 통과가 아닙니다.", null, _identity, -1, "", ""); }
    }

    public async Task<CompareResponse> CompareAsync(string manifestPath, string outRoot, CancellationToken cancel)
    {
        try
        {
            var (id, failure) = await IdentifyAsync(cancel);
            if (id is null) return new CompareResponse(false, failure, null, null, null, -1, "", "");
            var manifestFull = Path.GetFullPath(manifestPath);
            var baseDir = Path.GetDirectoryName(manifestFull)!.TrimEnd('\\', '/');
            var outFull = Path.GetFullPath(outRoot).TrimEnd('\\', '/');
            // the script only READS the input folder; its products must not be written next to the original data
            if (string.Equals(outFull, baseDir, StringComparison.OrdinalIgnoreCase) || outFull.StartsWith(baseDir + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
                return new CompareResponse(false, "결과 폴더가 매니페스트(원데이터) 폴더 안에 있습니다. 개발 산출물 폴더를 따로 지정하십시오.", null, null, id, -1, "", "");
            var args = new[] { id.ScriptPath, "compare", manifestFull, "--out", outFull };
            // Reviewer #250 item 3/5: only a run folder that did not exist before THIS request, inside THIS output root, bound to THIS manifest, is accepted
            var before = Directory.Exists(outFull) ? new HashSet<string>(Directory.GetDirectories(outFull), StringComparer.OrdinalIgnoreCase) : [];
            JsonNode? sent;
            try { sent = JsonNode.Parse(File.ReadAllText(manifestFull)); }
            catch (Exception e) when (e is IOException or JsonException) { return new CompareResponse(false, "매니페스트를 읽지 못했습니다: " + e.Message, null, null, id, -1, "", ""); }
            using var timeout = CancellationTokenSource.CreateLinkedTokenSource(cancel);
            timeout.CancelAfter(TimeSpan.FromMinutes(10));
            var r = await RunAsync(id.PythonPath, args, timeout.Token);
            if (r.StartFailure is not null) return new CompareResponse(false, "스크립트를 실행하지 못했습니다: " + r.StartFailure, null, null, id, -1, "", "");
            var last = r.Stdout.Split('\n', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries).LastOrDefault() ?? "";
            var space = last.IndexOf(' ');
            if (space <= 0) return new CompareResponse(false, $"스크립트 출력에서 '상태 실행폴더'를 읽지 못했습니다(종료 코드 {r.ExitCode}): {Tail(r.Stdout)} {Tail(r.Stderr)}", null, null, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr));
            var statusWord = last[..space];
            var runDir = last[(space + 1)..].Trim();
            var runFull = Path.GetFullPath(runDir);
            if (!string.Equals(Path.GetDirectoryName(runFull.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)), outFull, StringComparison.OrdinalIgnoreCase) || before.Contains(runFull))
                return new CompareResponse(false, "스크립트가 알려 준 실행 폴더가 이번 요청의 새 폴더가 아닙니다(출력 폴더 밖이거나 이미 있던 폴더): " + runFull, runFull, null, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr));
            var resultPath = Path.Combine(runDir, "result.json");
            if (!File.Exists(resultPath)) return new CompareResponse(false, "result.json 이 없습니다: " + resultPath, runDir, null, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr));
            JsonObject? result;
            try { result = JsonNode.Parse(File.ReadAllText(resultPath)) as JsonObject; }
            catch (JsonException e) { return new CompareResponse(false, "result.json 을 읽지 못했습니다: " + e.Message, runDir, null, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr)); }
            if (result is null) return new CompareResponse(false, "result.json 이 JSON 객체가 아닙니다.", runDir, null, id, r.ExitCode, "", "");
            var schema = (result["schema"] as JsonValue)?.GetValueKind() == JsonValueKind.String ? result["schema"]!.GetValue<string>() : null;
            if (schema != ComparisonContractReview.Schema)
                return new CompareResponse(false, $"result.json 의 schema 가 {ComparisonContractReview.Schema} 가 아닙니다({schema ?? "없음"}). 실패로 처리합니다.", runDir, result, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr));
            var status = (result["status"] as JsonValue)?.GetValueKind() == JsonValueKind.String ? result["status"]!.GetValue<string>() : null;
            if (status is not ("computed" or "refused" or "failed" or "error") || status != statusWord)
                return new CompareResponse(false, $"result.json 의 status({status ?? "없음"})가 스크립트가 말한 상태({statusWord})와 맞지 않거나 알 수 없는 값입니다.", runDir, result, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr));
            var runId = (result["run_id"] as JsonValue)?.GetValueKind() == JsonValueKind.String ? result["run_id"]!.GetValue<string>() : null;
            if (runId != Path.GetFileName(runFull.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)))
                return new CompareResponse(false, $"result.json 의 run_id({runId ?? "없음"})가 실행 폴더 이름과 다릅니다. 다른 실행의 결과일 수 있어 거부합니다.", runDir, result, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr));
            if (!JsonNode.DeepEquals(result["manifest"], sent))
                return new CompareResponse(false, "result.json 에 기록된 매니페스트가 이번 요청에서 보낸 매니페스트와 다릅니다. 거부합니다.", runDir, result, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr));
            if ((status == "computed") != (r.ExitCode == 0))
                return new CompareResponse(false, $"status={status} 와 종료 코드 {r.ExitCode} 가 서로 맞지 않습니다.", runDir, result, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr));
            WriteSidecar(runDir, id, args, r);
            return new CompareResponse(true, null, runDir, result, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr));
        }
        catch (OperationCanceledException) { return new CompareResponse(false, "취소했거나 시간 제한(10분)에 걸렸습니다. 자식 프로세스를 멈췄고 실패로 기록하며 통과가 아닙니다. 다시 하려면 새 실행입니다.", null, null, _identity, -1, "", ""); }
    }

    /// <summary>A NEW file next to the script's result.json: who ran what (the script's own file is never edited).</summary>
    private static void WriteSidecar(string runDir, ScriptIdentity id, string[] args, ProcessResult r)
    {
        var record = new JsonObject
        {
            ["written_by"] = "ImageProcTest comparison import window (QA-A-273)",
            ["utc"] = DateTime.UtcNow.ToString("O"),
            ["script_path"] = id.ScriptPath,
            ["script_sha256"] = id.ScriptSha256,
            ["python_path"] = id.PythonPath,
            ["python_version"] = id.PythonVersion,
            ["command"] = new JsonArray(new[] { id.PythonPath }.Concat(args).Select(a => (JsonNode?)JsonValue.Create(a)).ToArray()),
            ["exit_code"] = r.ExitCode,
            ["reviewed_script_sha256"] = new JsonArray(ComparisonContractReview.ReviewedScriptSha256.Select(h => (JsonNode?)JsonValue.Create(h)).ToArray()),
            ["computed_script_sha256"] = id.ScriptSha256,
            ["contract_review_state"] = ComparisonContractReview.State(id.ScriptSha256),
            ["stdout_tail"] = Tail(r.Stdout),
            ["stderr_tail"] = Tail(r.Stderr),
        };
        AtomicFile.WriteAllBytes(Path.Combine(runDir, "gui-run-record.json"), Encoding.UTF8.GetBytes(record.ToJsonString(Indented)));
    }

    private static string Tail(string s) => s.Length <= 1200 ? s.Trim() : "…" + s[^1200..].Trim();

    private sealed record ProcessResult(int ExitCode, string Stdout, string Stderr, string? StartFailure);

    private static async Task<ProcessResult> RunAsync(string exe, IEnumerable<string> args, CancellationToken cancel)
    {
        var psi = new ProcessStartInfo(exe)
        {
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            StandardOutputEncoding = Encoding.UTF8,
            StandardErrorEncoding = Encoding.UTF8,
            CreateNoWindow = true,
            WorkingDirectory = Path.GetTempPath(),
        };
        foreach (var a in args) psi.ArgumentList.Add(a);
        psi.Environment["PYTHONIOENCODING"] = "utf-8";
        Process? p;
        try { p = Process.Start(psi); }
        catch (Exception e) when (e is System.ComponentModel.Win32Exception or InvalidOperationException or FileNotFoundException) { return new ProcessResult(-1, "", "", e.Message); }
        if (p is null) return new ProcessResult(-1, "", "", "process did not start");
        using (p)
        {
            var outTask = p.StandardOutput.ReadToEndAsync(cancel);
            var errTask = p.StandardError.ReadToEndAsync(cancel);
            try { await p.WaitForExitAsync(cancel); }
            catch (OperationCanceledException)
            {
                try { p.Kill(entireProcessTree: true); } catch (InvalidOperationException) { /* already gone */ }
                throw;
            }
            return new ProcessResult(p.ExitCode, await outTask, await errTask, null);
        }
    }
}
