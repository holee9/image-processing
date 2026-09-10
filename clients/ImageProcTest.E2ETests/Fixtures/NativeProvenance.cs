// #98 (GUI-C-41): a Native run must say WHICH native binaries it exercised.
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace ImageProcTest.E2ETests.Fixtures;

/// <summary>
/// The provenance record staged beside the native DLLs — which CI run produced them, at which
/// commit, and the md5 of each file.
///
/// GUI-C-40 is why this exists. That run reported "Native 9/9" while the staged
/// <c>xpe_preprocess.dll</c> predated the six-argument <c>xpe_calib_generate_offset</c> it was
/// supposed to exercise. The run was not wrong; the WORD was. "Native" does not say which native,
/// and the reader — the lane, and the lead reading its report — filled in "the one for this commit".
///
/// #98 keeps this lane from building native binaries, so running against an older artifact stays
/// unavoidable. What changes here is that it is no longer invisible.
///
/// Written by <c>Staging/Stage-NativeArtifacts.ps1</c> (source "lane") and by ci.yml (source "ci").
/// </summary>
public sealed record NativeProvenance(
    [property: JsonPropertyName("source")] string? Source,
    [property: JsonPropertyName("runId")] string? RunId,
    [property: JsonPropertyName("headSha")] string? HeadSha,
    [property: JsonPropertyName("files")] IReadOnlyList<NativeProvenanceFile>? Files)
{
    /// <summary>File name the producers agree on, inside the pinned native directory.</summary>
    public const string FileName = "provenance.json";

    /// <summary>
    /// Reads the record from a pinned native directory.
    ///
    /// A missing or unreadable file returns null WITH a reason. The caller turns that into a
    /// failure — passing a Native run whose binaries have no stated origin is the state GUI-C-40
    /// measured, and it reads as a stronger result than it is.
    /// </summary>
    public static NativeProvenance? Read(string nativeDirectory, out string reason)
    {
        var path = Path.Combine(nativeDirectory, FileName);
        if (!File.Exists(path))
        {
            reason =
                $"{path} is missing. A Native run has to name the binaries it exercised: stage them " +
                "with clients/ImageProcTest.E2ETests/Staging/Stage-NativeArtifacts.ps1 " +
                "-RunId <id> -Destination <dir>, which writes this file.";
            return null;
        }

        NativeProvenance? record;
        try
        {
            record = JsonSerializer.Deserialize<NativeProvenance>(File.ReadAllText(path));
        }
        catch (JsonException ex)
        {
            reason = $"{path} is not valid JSON: {ex.Message}";
            return null;
        }

        if (record is null || string.IsNullOrWhiteSpace(record.RunId) || string.IsNullOrWhiteSpace(record.HeadSha))
        {
            reason = $"{path} carries no runId/headSha — it cannot answer which binaries these are.";
            return null;
        }

        reason = string.Empty;
        return record;
    }

    /// <summary>
    /// Renders the provenance for the test output, plus a warning line when the artifacts were built
    /// at a different commit than the one under test.
    ///
    /// The mismatch is a WARNING, never a failure. Under #98 the newest artifact is often older than
    /// the working commit, and failing there would only teach the lane to delete the file. The
    /// judgement belongs to whoever reads the report; this makes it possible to make.
    /// </summary>
    public string Describe(string nativeDirectory, string? currentHeadSha)
    {
        var lines = new List<string>
        {
            $"native provenance: source={Source} run={RunId} head={HeadSha}",
            $"  directory: {nativeDirectory}",
        };

        foreach (var file in Files ?? [])
        {
            var path = Path.Combine(nativeDirectory, file.Name ?? string.Empty);
            var onDisk = TryMd5(path);
            var drift = onDisk is null ? "  (file not present)"
                : string.Equals(onDisk, file.Md5, StringComparison.OrdinalIgnoreCase) ? string.Empty
                : $"  (ON DISK {onDisk} — the file changed after it was staged)";

            lines.Add($"  {file.Md5}  {file.Name}{drift}");
        }

        if (!string.IsNullOrWhiteSpace(currentHeadSha) &&
            !string.Equals(currentHeadSha, HeadSha, StringComparison.OrdinalIgnoreCase))
        {
            lines.Add(
                $"WARNING: these binaries were built at {HeadSha}, not at the commit under test " +
                $"({currentHeadSha}). Native results describe THAT code, not necessarily this " +
                "working tree (#98 — this lane does not build native binaries).");
        }

        return string.Join(Environment.NewLine, lines);
    }

    private static string? TryMd5(string path)
    {
        if (!File.Exists(path)) return null;

        using var stream = File.OpenRead(path);
        return Convert.ToHexString(MD5.HashData(stream)).ToLowerInvariant();
    }
}

/// <summary>One staged file's identity.</summary>
public sealed record NativeProvenanceFile(
    [property: JsonPropertyName("name")] string? Name,
    [property: JsonPropertyName("md5")] string? Md5,
    [property: JsonPropertyName("length")] long Length);
