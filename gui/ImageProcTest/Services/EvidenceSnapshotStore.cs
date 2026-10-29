using System.IO;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace ImageProcTest.Services;

/// <summary>
/// Publishes ONE automation report twice from the same bytes: as a new unique snapshot (the evidence of record, never opened for writing again) and as the fixed
/// <c>menu-command-report.json</c> beside the executable that older consumers (Lane C end-to-end scenarios) still read. QA-A-274 section 7-1 (c).
/// </summary>
public static class EvidenceSnapshotStore
{
    public const string FixedAliasName = "menu-command-report.json";

    /// <summary>
    /// What happened to each of the two outputs. <see cref="SnapshotPath"/> is set only when the snapshot is fully written and closed; <see cref="AliasError"/> is
    /// set when the snapshot exists but the fixed copy could not be replaced (the previous fixed file, if any, is then still in place and still names an OLDER snapshot).
    /// </summary>
    public sealed record PublishResult(string? SnapshotPath, string? AliasPath, string? ReplacedAliasCopyPath, Exception? SnapshotError, Exception? AliasError);

    /// <summary>Publishes a new snapshot without opening an existing evidence file for writing (snapshot only; kept for callers that need no fixed copy).</summary>
    public static string Write(string applicationDirectory, object report)
    {
        var directory = Path.Combine(applicationDirectory, "evidence", "snapshots");
        Directory.CreateDirectory(directory);
        var path = Path.Combine(directory, $"menu-command-report-{DateTime.UtcNow:yyyyMMddTHHmmssfffZ}-{Guid.NewGuid():N}.json");
        using var stream = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read);
        JsonSerializer.Serialize(stream, report, new JsonSerializerOptions { WriteIndented = true });
        return path;
    }

    /// <summary>
    /// Order and failure rules:
    /// 1. The report is serialized once; a top-level <c>evidence</c> object (<c>runId</c>, <c>snapshotFile</c>) is the only addition to the existing schema, so a fixed file that
    ///    does not name the snapshot of THIS run is recognisably stale.
    /// 2. The snapshot is written first (CreateNew). If that fails, its own partial file is removed and the fixed copy is NOT touched: a fixed file must never name a snapshot that does not exist.
    /// 3. A previous fixed file that is not already kept byte for byte as one of our own snapshots (the normal case: the last export wrote both) is first copied to
    ///    <c>evidence/snapshots/replaced-fixed-report-*.json</c>; only if nothing is left to keep, or that copy exists, is the fixed file replaced (temporary file in the same folder,
    ///    then move over), so a reader sees the old or the new file, never half of one. If the copy fails the fixed file is left alone.
    /// 4. A failed fixed copy does not undo the snapshot: the snapshot is the evidence of record. The failure is returned in <see cref="PublishResult.AliasError"/>.
    /// </summary>
    public static PublishResult Publish(string snapshotRoot, string aliasDirectory, object report, string runId)
    {
        var directory = Path.Combine(snapshotRoot, "evidence", "snapshots");
        var stamp = $"{DateTime.UtcNow:yyyyMMddTHHmmssfffZ}-{Guid.NewGuid():N}";
        var snapshotName = $"menu-command-report-{stamp}.json";
        var snapshotPath = Path.Combine(directory, snapshotName);
        var aliasPath = Path.Combine(aliasDirectory, FixedAliasName);

        var node = JsonSerializer.SerializeToNode(report)!.AsObject();
        node["evidence"] = new JsonObject { ["runId"] = runId, ["snapshotFile"] = snapshotName };
        var bytes = new UTF8Encoding(false).GetBytes(node.ToJsonString(new JsonSerializerOptions { WriteIndented = true }));

        var created = false;
        try
        {
            Directory.CreateDirectory(directory);
            using (var stream = new FileStream(snapshotPath, FileMode.CreateNew, FileAccess.Write, FileShare.Read))
            {
                created = true;
                stream.Write(bytes, 0, bytes.Length);
            }
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException)
        {
            if (created) TryDelete(snapshotPath);
            return new PublishResult(null, aliasPath, null, error, null);
        }

        string? replacedCopy = null;
        var temp = aliasPath + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            if (File.Exists(aliasPath) && !IsKeptAsSnapshot(aliasPath, directory))
            {
                replacedCopy = Path.Combine(directory, $"replaced-fixed-report-{stamp}.json");
                File.Copy(aliasPath, replacedCopy, overwrite: false);
            }
            File.WriteAllBytes(temp, bytes);
            File.Move(temp, aliasPath, overwrite: true);
            return new PublishResult(snapshotPath, aliasPath, replacedCopy, null, null);
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException)
        {
            TryDelete(temp);
            return new PublishResult(snapshotPath, aliasPath, replacedCopy, null, error);
        }
    }

    /// <summary>True when the fixed file names one of our snapshots in its <c>evidence</c> object and that snapshot holds exactly the same bytes (nothing would be lost by replacing it).</summary>
    private static bool IsKeptAsSnapshot(string aliasPath, string snapshotDirectory)
    {
        try
        {
            var previous = File.ReadAllBytes(aliasPath);
            using var document = JsonDocument.Parse(previous);
            var name = document.RootElement.GetProperty("evidence").GetProperty("snapshotFile").GetString();
            if (string.IsNullOrEmpty(name) || name != Path.GetFileName(name)) return false;
            var kept = Path.Combine(snapshotDirectory, name);
            return File.Exists(kept) && File.ReadAllBytes(kept).AsSpan().SequenceEqual(previous);
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or JsonException or KeyNotFoundException or InvalidOperationException)
        {
            return false;
        }
    }

    private static void TryDelete(string path)
    {
        try { File.Delete(path); }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException) { }
    }
}
