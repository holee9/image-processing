using System.IO;
using System.Text.Json;

namespace ImageProcTest.Services;

/// <summary>Publishes a new snapshot without opening an existing evidence file for writing.</summary>
public static class EvidenceSnapshotStore
{
    public static string Write(string applicationDirectory, object report)
    {
        var directory = Path.Combine(applicationDirectory, "evidence", "snapshots");
        Directory.CreateDirectory(directory);
        var path = Path.Combine(directory, $"menu-command-report-{DateTime.UtcNow:yyyyMMddTHHmmssfffZ}-{Guid.NewGuid():N}.json");
        using var stream = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read);
        JsonSerializer.Serialize(stream, report, new JsonSerializerOptions { WriteIndented = true });
        return path;
    }
}
