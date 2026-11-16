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
        var json = JsonNode.Parse(bytes) ?? throw new IOException("Manifest is empty.");
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
        if (string.IsNullOrWhiteSpace(scriptPath)) return (null, "Contract script path is empty. Enter a path; there is no default.");
        if (string.IsNullOrWhiteSpace(outRoot)) return (null, "Output directory is empty.");
        var script = Path.GetFullPath(scriptPath);
        if (!File.Exists(script)) return (null, "Contract script file not found: " + script);
        var baseDir = manifest.BaseDir.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        var outFull = Path.GetFullPath(outRoot).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        // the script only READS the input folder; its products must not be written next to the original data
        if (string.Equals(outFull, baseDir, StringComparison.OrdinalIgnoreCase) || outFull.StartsWith(baseDir + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
            return (null, "Output directory is inside the manifest/source-data directory. Choose a separate development output directory.");

        // the bytes that are hashed are the bytes that are copied: one read, no second look at the original
        byte[] bytes;
        try { bytes = await File.ReadAllBytesAsync(script, cancel); }
        catch (IOException e) { return (null, "Could not read the contract script: " + e.Message); }
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
        if (pinnedShaBefore != originalSha) return (null, "Pinned SHA-256 differs from the original bytes read. Execution refused.");
        var id = new ScriptIdentity(script, originalSha, pinned, pinnedShaBefore, null, null, python.Env, requestDir);
        return (new Prepared(id, manifestCopy, Path.Combine(requestDir, "runs"), baseDir), null);
    }

    private async Task<(PythonEnvironment? Env, string? Failure)> ResolvePythonAsync(CancellationToken cancel)
    {
        var exe = string.IsNullOrWhiteSpace(pythonPath) ? "python" : pythonPath.Trim();
        var probe = await RunAsync(exe, [.. Isolated, "-c", "import sys, numpy; print(sys.executable); print(sys.version.split()[0]); print(numpy.__version__)"], cancel);
        if (probe.StartFailure is not null) return (null, "Could not start Python (" + exe + "): " + probe.StartFailure);
        if (probe.ExitCode != 0) return (null, "Python exited with code " + probe.ExitCode + " (NumPy may be missing): " + Tail(probe.Stderr));
        var lines = probe.Stdout.Split('\n', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        if (lines.Length < 3 || !Path.IsPathFullyQualified(lines[0])) return (null, "Could not read Python path/version and NumPy version: " + Tail(probe.Stdout));
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
            if (r.StartFailure is not null) return new ValidateResponse(false, "Could not start the script: " + r.StartFailure, null, id, -1, "", "");
            if (r.ExitCode is not (0 or 2)) return new ValidateResponse(false, $"Script returned unexpected exit code {r.ExitCode}: {Tail(r.Stderr)}", null, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr));
            JsonObject? obj;
            try { obj = JsonNode.Parse(r.Stdout) as JsonObject; }
            catch (JsonException e) { return new ValidateResponse(false, "Validation output is not JSON: " + e.Message, null, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr)); }
            if (obj is null || obj["comparable"] is not JsonValue c || c.GetValueKind() is not (JsonValueKind.True or JsonValueKind.False) || obj["reasons"] is not JsonArray)
                return new ValidateResponse(false, "Validation output lacks comparable (boolean) and reasons (list).", null, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr));
            if (c.GetValue<bool>() != (r.ExitCode == 0)) return new ValidateResponse(false, "The comparable value and exit code disagree.", null, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr));
            return new ValidateResponse(true, null, obj, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr));
        }
        catch (OperationCanceledException) { return new ValidateResponse(false, "Cancelled. Recorded as failure, not a pass.", null, id, -1, "", ""); }
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
            if (r.StartFailure is not null) { WriteRecord(id, "compare", r, null); return new CompareResponse(false, "Could not start the script: " + r.StartFailure, null, null, id, -1, "", ""); }
            var last = r.Stdout.Split('\n', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries).LastOrDefault() ?? "";
            var space = last.IndexOf(' ');
            if (space <= 0) { WriteRecord(id, "compare", r, null); return new CompareResponse(false, $"Could not read 'status run-directory' from script output (exit {r.ExitCode}): {Tail(r.Stdout)} {Tail(r.Stderr)}", null, null, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr)); }
            var statusWord = last[..space];
            var runDir = last[(space + 1)..].Trim();
            var runFull = Path.GetFullPath(runDir);
            var runsFull = Path.GetFullPath(p.RunsDir).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            CompareResponse Reject(string why, JsonObject? res = null) { WriteRecord(id!, "compare", r, runFull); return new CompareResponse(false, why, runFull, res, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr)); }
            if (!string.Equals(Path.GetDirectoryName(runFull.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)), runsFull, StringComparison.OrdinalIgnoreCase))
                return Reject("Reported run directory is not directly under this request's runs directory: " + runFull);
            var resultPath = Path.Combine(runFull, "result.json");
            if (!File.Exists(resultPath)) return Reject("result.json not found: " + resultPath);
            JsonObject? result;
            try { result = JsonNode.Parse(File.ReadAllText(resultPath)) as JsonObject; }
            catch (JsonException e) { return Reject("Could not read result.json: " + e.Message); }
            if (result is null) return Reject("result.json is not a JSON object.");
            var schema = (result["schema"] as JsonValue)?.GetValueKind() == JsonValueKind.String ? result["schema"]!.GetValue<string>() : null;
            if (schema != ComparisonContractReview.Schema)
                return Reject($"result.json schema is not {ComparisonContractReview.Schema} ({schema ?? "missing"}). Treated as failure.", result);
            var status = (result["status"] as JsonValue)?.GetValueKind() == JsonValueKind.String ? result["status"]!.GetValue<string>() : null;
            if (status is not ("computed" or "refused" or "failed" or "error") || status != statusWord)
                return Reject($"result.json status ({status ?? "missing"}) is unknown or differs from script status ({statusWord}).", result);
            var runId = (result["run_id"] as JsonValue)?.GetValueKind() == JsonValueKind.String ? result["run_id"]!.GetValue<string>() : null;
            if (runId != Path.GetFileName(runFull.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)))
                return Reject($"result.json run_id ({runId ?? "missing"}) differs from the run directory name. Rejected as possibly belonging to another run.", result);
            if (!JsonNode.DeepEquals(result["manifest"], manifest.Json))
                return Reject("The result.json manifest differs from the user-validated snapshot. Rejected.", result);
            if ((status == "computed") != (r.ExitCode == 0))
                return Reject($"status={status} and exit code {r.ExitCode} disagree.", result);
            WriteRecord(id, "compare", r, runFull);
            return new CompareResponse(true, null, runFull, result, id, r.ExitCode, Tail(r.Stdout), Tail(r.Stderr));
        }
        catch (OperationCanceledException) { return new CompareResponse(false, "Cancelled or timed out (10 minutes). Child process stopped; recorded as failure, not a pass. Retry requires a new run.", null, null, id, -1, "", ""); }
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
