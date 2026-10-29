using System.IO;

namespace ImageProcTest.Services;

/// <summary>
/// The outcome of ONE export attempt, successful or not. It is kept apart from <c>MainWindowViewModel.LastEvidenceSnapshotPath</c>, which deliberately keeps pointing at the last
/// SUCCESSFUL snapshot so earlier evidence stays reachable after a failure. A consumer that has to know whether the export it just ran succeeded therefore must not read that path:
/// it takes the result it saw before the command and asks <see cref="NewSuccessPath"/> / <see cref="RequireNewSuccess"/> about the result after it.
/// </summary>
/// <param name="ExportId">Unique to this attempt (a run id cannot tell two exports of the same run apart).</param>
/// <param name="SnapshotPath">The snapshot this attempt wrote, or null when it failed.</param>
/// <param name="SnapshotError">Why this attempt failed, or null when it succeeded.</param>
/// <param name="FixedCopyPath">Where this attempt tried to put the fixed copy <c>menu-command-report.json</c>; set whenever the snapshot step ran, even if the fixed copy then failed.</param>
/// <param name="FixedCopyError">Why the fixed copy was not updated although the snapshot was written, or null.</param>
public sealed record EvidenceExportResult(string ExportId, string? SnapshotPath, string? SnapshotError, string? FixedCopyPath = null, string? FixedCopyError = null)
{
    /// <summary>True only when the snapshot AND the fixed copy of this attempt were both written (a snapshot-only success has a stale fixed copy).</summary>
    public bool FixedCopyUpdated => SnapshotPath is not null && FixedCopyError is null;

    /// <summary>The snapshot of the export that ran after <paramref name="before"/>, or null when no new export ran, that export failed, or its file is gone.</summary>
    public static string? NewSuccessPath(EvidenceExportResult? before, EvidenceExportResult? after)
    {
        if (after is null || after.ExportId == before?.ExportId) return null;
        var path = after.SnapshotPath;
        return path is not null && File.Exists(path) ? path : null;
    }

    /// <summary>As <see cref="NewSuccessPath"/>, but throws with the reason when there is no new success.</summary>
    public static string RequireNewSuccess(EvidenceExportResult? before, EvidenceExportResult? after)
    {
        if (after is null || after.ExportId == before?.ExportId)
            throw new InvalidOperationException("No new export ran: the export result is the one seen before the command.");
        if (after.SnapshotPath is null)
            throw new InvalidOperationException($"This export failed: {after.SnapshotError}");
        return NewSuccessPath(before, after) ?? throw new InvalidOperationException($"The snapshot of this export is missing: {after.SnapshotPath}");
    }
}
