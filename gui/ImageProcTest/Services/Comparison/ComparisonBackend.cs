using System.Diagnostics;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Text.Encodings.Web;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace ImageProcTest.Services.Comparison;

/// <summary>
/// QA-A-273 / 273b (Refs #278). The GUI does NOT re-implement the comparison contract (Reviewer #249/#250, leader decision 2026-10-21): the contract is the script tools/calibration/comparison_manifest.py
/// (schema xpe-2g-pre-comparison/4, main-local 47d932e3 at the time of writing), run as a CHILD PROCESS; the GUI reads its validate JSON and its result.json and shows them. A failure to run it is a failure, never a pass.
///
/// Every request (validate or compare) is self-contained (Reviewer #252): the script bytes are read once, hashed, copied into the request folder and THAT copy is executed; the manifest the user confirmed is
/// a snapshot (bytes + SHA-256) whose own copy is what the script reads; the interpreter is resolved to an absolute path and its version and NumPy version are recorded. Nothing is cached between requests.
/// </summary>
public sealed record PythonEnvironment(string ExePath, string Version, string NumpyVersion);

/// <summary>What was actually executed for ONE request. Reviewed means: the SHA-256 of the pinned copy, before and after the run, is a reviewed value.</summary>
public sealed record ScriptIdentity(string OriginalPath, string OriginalSha256, string PinnedPath, string PinnedShaBefore, string? PinnedShaAfter, string? OriginalShaAfter, PythonEnvironment Python, string RequestDir)
{
    public bool Reviewed => PinnedShaAfter is not null && PinnedShaBefore == PinnedShaAfter && ComparisonContractReview.IsReviewedSha(PinnedShaBefore);
}

/// <summary>The manifest as the user saw it when opening: its bytes, SHA-256 and parsed content. Validation, preview and computation are all bound to this snapshot.</summary>
public sealed record ManifestSnapshot(string Path, string BaseDir, byte[] Bytes, string Sha256, JsonNode Json)
{
    public static ManifestSnapshot Create(string path)
    {
        var full = System.IO.Path.GetFullPath(path);
        var bytes = File.ReadAllBytes(full);
        var json = JsonNode.Parse(bytes) ?? throw new IOException("매니페스트가 비어 있습니다.");
        return new ManifestSnapshot(full, System.IO.Path.GetDirectoryName(full)!, bytes, Convert.ToHexString(SHA256.HashData(bytes)).ToLowerInvariant(), json);
    }

    /// <summary>True when the file on disk still has exactly the snapshot's bytes.</summary>
    public bool StillMatchesDisk(out string? currentSha)
    {
        try { currentSha = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path))).ToLowerInvariant(); }
        catch (IOException) { currentSha = null; return false; }
        return currentSha == Sha256;
    }
}

public sealed record ValidateResponse(bool Ok, string? Failure, JsonObject? Validation, ScriptIdentity? Script, int ExitCode, string StdoutTail, string StderrTail);

public sealed record CompareResponse(bool Ok, string? Failure, string? RunDirectory, JsonObject? Result, ScriptIdentity? Script, int ExitCode, string StdoutTail, string StderrTail);

public interface IComparisonBackend
{
    Task<ValidateResponse> ValidateAsync(ManifestSnapshot manifest, CancellationToken cancel);
    Task<CompareResponse> CompareAsync(ManifestSnapshot manifest, CancellationToken cancel);
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

    public static bool IsReviewedSha(string? sha) => ReviewedScriptSha256.Any(h => string.Equals(h, sha, StringComparison.OrdinalIgnoreCase));
}

public sealed class PythonScriptComparisonBackend(string scriptPath, string pythonPath, string outRoot) : IComparisonBackend
{
    private static readonly JsonSerializerOptions Indented = new() { WriteIndented = true, Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping };
    private static readonly string[] Isolated = ["-I", "-X", "utf8"];   // -I: no user site / no env / no script-dir import surprises; utf8: the JSON is read as UTF-8

    private sealed record Prepared(ScriptIdentity Identity, string ManifestCopy, string RunsDir, string BaseDir);

    private async Task<(Prepared? P, string? Failure)> PrepareAsync(ManifestSnapshot manifest, CancellationToken cancel)
    {
        if (string.IsNullOrWhiteSpace(scriptPath)) return (null, "계약 스크립트 경로가 비어 있습니다. 경로를 입력하십시오(기본값은 없습니다).");
        if (string.IsNullOrWhiteSpace(outRoot)) return (null, "결과 폴더가 비어 있습니다.");
        var script = Path.GetFullPath(scriptPath);
        if (!File.Exists(script)) return (null, "계약 스크립트 파일이 없습니다: " + script);
        var baseDir = manifest.BaseDir.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        var outFull = Path.GetFullPath(outRoot).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        // the script only READS the input folder; its products must not be written next to the original data
        if (string.Equals(outFull, baseDir, StringComparison.OrdinalIgnoreCase) || outFull.StartsWith(baseDir + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
            return (null, "결과 폴더가 매니페스트(원데이터) 폴더 안에 있습니다. 개발 산출물 폴더를 따로 지정하십시오.");

        // the bytes that are hashed are the bytes that are copied: one read, no second look at the original
        byte[] bytes;
        try { bytes = await File.ReadAllBytesAsync(script, cancel); }
        catch (IOException e) { return (null, "계약 스크립트를 읽지 못했습니다: " + e.Message); }
        var originalSha = Hex(bytes);

        var python = await ResolvePythonAsync(cancel);
        if (python.Env is null) return (null, python.Failure);

        var requestDir = Path.Combine(outFull, $"req-{DateTime.UtcNow:yyyyMMddTHHmmssfffZ}-{Guid.NewGuid().ToString("N")[..8]}");
        var pinnedDir = Path.Combine(requestDir, "pinned");
        Directory.CreateDirectory(pinnedDir);   // throws when it exists: a request folder is never reused
        var pinned = Path.Combine(pinnedDir, "comparison_manifest.py");
        await File.WriteAllBytesAsync(pinned, bytes, cancel);
        var manifestCopy = Path.Combine(requestDir, "manifest.snapshot.json");
        await File.WriteAllBytesAsync(manifestCopy, manifest.Bytes, cancel);
        var pinnedShaBefore = Hex(await File.ReadAllBytesAsync(pinned, cancel));
        if (pinnedShaBefore != originalSha) return (null, "고정 사본의 SHA-256 이 원본을 읽은 바이트와 다릅니다. 실행하지 않습니다.");
        var id = new ScriptIdentity(script, originalSha, pinned, pinnedShaBefore, null, null, python.Env, requestDir);
        return (new Prepared(id, manifestCopy, Path.Combine(requestDir, "runs"), baseDir), null);
    }

    private async Task<(PythonEnvironment? Env, string? Failure)> ResolvePythonAsync(CancellationToken cancel)
    {
        var exe = string.IsNullOrWhiteSpace(pythonPath) ? "python" : pythonPath.Trim();
        var probe = await RunAsync(exe, [.. Isolated, "-c", "import sys, numpy; print(sys.executable); print(sys.version.split()[0]); print(numpy.__version__)"], cancel);
        if (probe.StartFailure is not null) return (null, "python 을 실행하지 못했습니다(" + exe + "): " + probe.StartFailure);
        if (probe.ExitCode != 0) return (null, "python 이 종료 코드 " + probe.ExitCode + " 로 끝났습니다(NumPy 가 없을 수 있습니다): " + Tail(probe.Stderr));
        var lines = probe.Stdout.Split('\n', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        if (lines.Length < 3 || !Path.IsPathFullyQualified(lines[0])) return (null, "python 의 경로·버전·NumPy 버전을 읽지 못했습니다: " + Tail(probe.Stdout));
        return (new PythonEnvironment(lines[0], lines[1], lines[2]), null);
    }

    private static string Hex(byte[] b) => Convert.ToHexString(SHA256.HashData(b)).ToLowerInvariant();

    /// <summary>Hashes the pinned copy and the original again after the run. A short gap between "hashed" and "executed" remains (see the class note): this narrows it to a recorded fact, it does not remove it.</summary>
    private static async Task<ScriptIdentity> SealAsync(ScriptIdentity id)
    {
        string? after = null, orig = null;
        try { after = Hex(await File.ReadAllBytesAsync(id.PinnedPath)); } catch (IOException) { }
        try { orig = Hex(await File.ReadAllBytesAsync(id.OriginalPath)); } catch (IOException) { }
        return id with { PinnedShaAfter = after, OriginalShaAfter = orig };
    }

    public async Task<ValidateResponse> ValidateAsync(ManifestSnapshot manifest, CancellationToken cancel)
    {
        ScriptIdentity? id = null;
        try
        {
            var (p, failure) = await PrepareAsync(manifest, cancel);
            if (p is null) return new ValidateResponse(false, failure, null, null, -1, "", "");
            id = p.Identity;
            var r = await RunAsync(id.Python.ExePath, [.. Isolated, id.PinnedPath, "validate", p.ManifestCopy, "--base-dir", p.BaseDir], cancel);
            id = await SealAsync(id);
            WriteRecord(id, "validate", r, null);
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
        catch (OperationCanceledException) { return new ValidateResponse(false, "취소했습니다. 실패로 기록하며 통과가 아닙니다.", null, id, -1, "", ""); }
    }

    public async Task<CompareResponse> CompareAsync(ManifestSnapshot manifest, CancellationToken cancel)
    {
        ScriptIdentity? id = null;
        try
        {
            var (p, failure) = await PrepareAsync(manifest, cancel);
            if (p is null) return new CompareResponse(false, failure, null, null, null, -1, "", "");
            id = p.Identity;
            // Reviewer #250 item 3/5: only a run folder that did not exist before THIS request, directly inside THIS request's runs folder, bound to THIS manifest snapshot, is accepted
            using var timeout = CancellationTokenSource.CreateLinkedTokenSource(cancel);
            timeout.CancelAfter(TimeSpan.FromMinutes(10));
            var r = await RunAsync(id.Python.ExePath, [.. Isolated, id.PinnedPath, "compare", p.ManifestCopy, "--base-dir", p.BaseDir, "--out", p.RunsDir], timeout.Token);
            id = await SealAsync(id);
            if (r.StartFailure is not null) { WriteRecord(id, "compare", r, null); return new CompareResponse(false, "스크립트를 실행하지 못했습니다: " + r.StartFailure, null, null, id, -1, "", ""); }
            var last = r.Stdout.Split('\n', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries).LastOrDefault() ?? "";
            var space = last.IndexOf(' ');
            if (space <= 0) { WriteRecord(id, "compare", r, null); return new CompareResponse(false, $"스크립트 출력에서 '상태 실행폴더'를 읽지 못했습니다(종료 코드 {r.ExitCode}): {Tail(r.Stdout)} {Tail(r.Stderr)}", null, null, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr)); }
            var statusWord = last[..space];
            var runDir = last[(space + 1)..].Trim();
            var runFull = Path.GetFullPath(runDir);
            var runsFull = Path.GetFullPath(p.RunsDir).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            CompareResponse Reject(string why, JsonObject? res = null) { WriteRecord(id!, "compare", r, runFull); return new CompareResponse(false, why, runFull, res, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr)); }
            if (!string.Equals(Path.GetDirectoryName(runFull.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)), runsFull, StringComparison.OrdinalIgnoreCase))
                return Reject("스크립트가 알려 준 실행 폴더가 이번 요청의 runs 폴더 바로 아래가 아닙니다: " + runFull);
            var resultPath = Path.Combine(runFull, "result.json");
            if (!File.Exists(resultPath)) return Reject("result.json 이 없습니다: " + resultPath);
            JsonObject? result;
            try { result = JsonNode.Parse(File.ReadAllText(resultPath)) as JsonObject; }
            catch (JsonException e) { return Reject("result.json 을 읽지 못했습니다: " + e.Message); }
            if (result is null) return Reject("result.json 이 JSON 객체가 아닙니다.");
            var schema = (result["schema"] as JsonValue)?.GetValueKind() == JsonValueKind.String ? result["schema"]!.GetValue<string>() : null;
            if (schema != ComparisonContractReview.Schema)
                return Reject($"result.json 의 schema 가 {ComparisonContractReview.Schema} 가 아닙니다({schema ?? "없음"}). 실패로 처리합니다.", result);
            var status = (result["status"] as JsonValue)?.GetValueKind() == JsonValueKind.String ? result["status"]!.GetValue<string>() : null;
            if (status is not ("computed" or "refused" or "failed" or "error") || status != statusWord)
                return Reject($"result.json 의 status({status ?? "없음"})가 스크립트가 말한 상태({statusWord})와 맞지 않거나 알 수 없는 값입니다.", result);
            var runId = (result["run_id"] as JsonValue)?.GetValueKind() == JsonValueKind.String ? result["run_id"]!.GetValue<string>() : null;
            if (runId != Path.GetFileName(runFull.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)))
                return Reject($"result.json 의 run_id({runId ?? "없음"})가 실행 폴더 이름과 다릅니다. 다른 실행의 결과일 수 있어 거부합니다.", result);
            if (!JsonNode.DeepEquals(result["manifest"], manifest.Json))
                return Reject("result.json 에 기록된 매니페스트가 사용자가 확인한 매니페스트 스냅샷과 다릅니다. 거부합니다.", result);
            if ((status == "computed") != (r.ExitCode == 0))
                return Reject($"status={status} 와 종료 코드 {r.ExitCode} 가 서로 맞지 않습니다.", result);
            WriteRecord(id, "compare", r, runFull);
            return new CompareResponse(true, null, runFull, result, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr));
        }
        catch (OperationCanceledException) { return new CompareResponse(false, "취소했거나 시간 제한(10분)에 걸렸습니다. 자식 프로세스를 멈췄고 실패로 기록하며 통과가 아닙니다. 다시 하려면 새 실행입니다.", null, null, id, -1, "", ""); }
    }

    /// <summary>A NEW file in the request folder: who ran what, with which interpreter, on which bytes (the script's own files are never edited).</summary>
    private static void WriteRecord(ScriptIdentity id, string command, ProcessResult r, string? runDir)
    {
        try
        {
            var record = new JsonObject
            {
                ["written_by"] = "ImageProcTest comparison import window (QA-A-273b)",
                ["utc"] = DateTime.UtcNow.ToString("O"),
                ["command"] = command,
                ["original_script_path"] = id.OriginalPath,
                ["original_script_sha256_read"] = id.OriginalSha256,
                ["original_script_sha256_after_run"] = id.OriginalShaAfter,
                ["pinned_script_path"] = id.PinnedPath,
                ["pinned_script_sha256_before_run"] = id.PinnedShaBefore,
                ["pinned_script_sha256_after_run"] = id.PinnedShaAfter,
                ["python_exe"] = id.Python.ExePath,
                ["python_version"] = id.Python.Version,
                ["numpy_version"] = id.Python.NumpyVersion,
                ["exit_code"] = r.ExitCode,
                ["run_directory"] = runDir,
                ["reviewed_script_sha256"] = new JsonArray(ComparisonContractReview.ReviewedScriptSha256.Select(h => (JsonNode?)JsonValue.Create(h)).ToArray()),
                ["contract_reviewed"] = id.Reviewed,
                ["stdout_tail"] = Tail(r.Stdout),
                ["stderr_tail"] = Tail(r.Stderr),
                ["limit"] = "Between hashing the bytes and the child opening the pinned copy a short gap remains: another process could replace the pinned copy in it. The after-run hash records whether that happened; it does not prevent it.",
            };
            AtomicFile.WriteAllBytes(Path.Combine(id.RequestDir, $"gui-record-{command}.json"), Encoding.UTF8.GetBytes(record.ToJsonString(Indented)));
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException) { /* a record that cannot be written must not turn a result into a different result */ }
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
