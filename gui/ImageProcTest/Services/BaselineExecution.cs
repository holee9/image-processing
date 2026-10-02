// #225 row 9 (GUI-C-196 M4, reworked in M6 after Codex #73): the whole Deterministic Baseline command apart from the window: two runs, the verdict, the DICOM export,
// the evidence file, the one-line status. Free of WPF and of native code, so the integration tests link it and the view model only has to schedule it.
using System.Diagnostics;
using System.IO;
using System.Text.Json;

namespace ImageProcTest.Services;

/// <summary>Everything one baseline command produced. <see cref="Passed"/> needs the verdict, the DICOM export AND the evidence file to be in order.</summary>
public sealed record BaselineExecutionResult(
    bool Passed,
    string Status,
    BaselineVerdict Verdict,
    string EvidenceFolder,
    string JsonPath,
    string StageTimes,
    string ExposureIndex,
    double TotalMs,
    string? EvidenceWriteProblem,
    bool DicomValid,
    bool DicomRoundTripIdentical,
    string DicomSummary);

public static class BaselineExecution
{
    /// <summary>The budget of product.md. MEASURED against, never asserted: it has not been calibrated on a CI runner (design §5, leader's ruling).</summary>
    public const int BudgetMs = 3000;

    /// <summary>The lock file a run holds in its evidence folder while it works. It is not an output: it is deleted when the run ends (never left behind, see <see cref="FolderLock"/>).</summary>
    internal const string LockFileName = "baseline.lock";

    internal static BaselineExecutionResult Run(
        ushort[] raw,
        int width,
        int height,
        Func<BaselineSingleRun> runOnce,
        IDicomSession? dicom,
        BaselineDicomMetadata metadata,
        string evidenceFolder,
        Func<DateTimeOffset>? clock = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(evidenceFolder);

        // Codex #78 finding 1 (the leader's ruling, M9): the folder is the run's alone from before it removes anything until it has judged. Without this, run B's start-up
        // removal could delete the file run A had just moved into place, and A would report Pass with no DICOM file on disk. A folder that is already taken fails THIS run at
        // once and touches nothing of the other run's.
        using var folderLock = FolderLock.TryAcquire(evidenceFolder, out var lockProblem);
        if (folderLock is null)
        {
            var reason = "the evidence folder is being used by another run, or its lock could not be taken: " + lockProblem;
            var notRun = new BaselineVerdict(BaselineStatus.Fail, reason, 0, false, false, null, 0, string.Empty, string.Empty, null, [], []);
            return new BaselineExecutionResult(false, "Deterministic Baseline FAIL: " + reason, notRun, evidenceFolder, Path.Combine(evidenceFolder, "baseline.json"),
                string.Empty, string.Empty, 0, null, false, false, string.Empty);
        }

        return RunLocked(raw, width, height, runOnce, dicom, metadata, evidenceFolder, clock);
    }

    private static BaselineExecutionResult RunLocked(
        ushort[] raw,
        int width,
        int height,
        Func<BaselineSingleRun> runOnce,
        IDicomSession? dicom,
        BaselineDicomMetadata metadata,
        string evidenceFolder,
        Func<DateTimeOffset>? clock = null)
    {
        ArgumentNullException.ThrowIfNull(raw);
        ArgumentNullException.ThrowIfNull(runOnce);
        ArgumentNullException.ThrowIfNull(metadata);
        ArgumentException.ThrowIfNullOrWhiteSpace(evidenceFolder);

        var now = clock ?? (() => DateTimeOffset.Now);
        var startedAt = now();

        // Codex #76 finding 1 (the leader's ruling, M8): a new run starts by removing the final-named files an earlier run left in the same folder. Without it a Fail run
        // sits beside the previous run's passing baseline.dcm under its proper name. A file that cannot be removed fails THIS run, with the reason.
        var staleProblem = ClearPreviousOutputs(evidenceFolder);
        var total = Stopwatch.StartNew();
        var runs = new List<BaselineSingleRun>();
        var runMs = new List<double>();

        var verdict = BaselineRunner.Run(raw, () =>
        {
            var watch = Stopwatch.StartNew();
            var run = runOnce();
            runMs.Add(watch.Elapsed.TotalMilliseconds);
            runs.Add(run);
            return run;
        });

        // D5: a failed verdict writes no DICOM file, so nothing that looks like an output exists for a baseline that did not pass.
        BaselineDicomResult? dicomResult = null;
        string? failure = staleProblem ?? (verdict.Status == BaselineStatus.Fail ? verdict.FailureReason : null);
        if (verdict.Status == BaselineStatus.Pass && staleProblem is null)
        {
            if (dicom is null)
            {
                failure = "the backend offered no DICOM session, so the output was not written";
            }
            else
            {
                // The export leaves a PASSING file under its partial name and a failing one deleted (Codex #73 finding 3): the final name is given below, only
                // after the evidence file has been written too.
                dicomResult = BaselineDicomExport.Export(Path.Combine(evidenceFolder, "baseline.dcm"), runs[0].Output, width, height, metadata, dicom);
                if (!dicomResult.Passed)
                {
                    failure = "DICOM export: " + dicomResult.Summary;
                }
            }
        }

        var totalMs = total.Elapsed.TotalMilliseconds;
        var stageTimes = string.Join(" | ", runs.Select((r, i) => $"run{i + 1}: {r.Chain.Timings.Replace("times: ", string.Empty)}, total {runMs[i]:0} ms"));
        var exposure = ExposureOf(runs);
        var jsonPath = Path.Combine(evidenceFolder, "baseline.json");

        string WriteEvidence(string? currentFailure, bool finalFile)
        {
            Directory.CreateDirectory(evidenceFolder);
            var document = new
            {
                status = currentFailure is null ? "Pass" : "Fail",
                failureReason = currentFailure ?? string.Empty,
                startedAt = startedAt.ToString("o", System.Globalization.CultureInfo.InvariantCulture),
                width,
                height,
                runsExecuted = verdict.RunsExecuted,
                inputPreserved = verdict.InputPreserved,
                inputSha256Before = verdict.InputSha256Before,
                inputSha256After = verdict.InputSha256After,
                bitIdentical = verdict.BitIdentical,
                difference = verdict.Difference is null ? null : new
                {
                    identical = verdict.Difference.Identical,
                    firstIndex = verdict.Difference.FirstIndex,
                    differentCount = verdict.Difference.DifferentCount,
                    maxAbsDifference = verdict.Difference.MaxAbsDifference,
                },
                nanInfCount = verdict.NaNInfCount,
                nonFiniteByStageRun1 = runs.Count == 0 ? [] : runs[0].Chain.Stages.Select(s => $"{s.StageId}={s.NonFiniteCount}").Append($"display={runs[0].NaNInfCount}").ToArray(),
                outputSha256 = verdict.OutputSha256,
                stageHashesRun1 = verdict.StageHashesRun1,
                stageHashesRun2 = verdict.StageHashesRun2,
                stageTimes,
                runTotalsMs = runMs.Select(ms => Math.Round(ms, 1)).ToArray(),
                totalMs = Math.Round(totalMs, 1),
                budgetMs = BudgetMs,
                budgetNote = "measured against, not asserted",
                exposureIndex = exposure,
                dicom = dicomResult is null ? null : new
                {
                    path = dicomResult.Path,
                    finalFileWritten = finalFile,
                    passed = dicomResult.Passed,
                    valid = dicomResult.Valid,
                    report = dicomResult.ReportJson,
                    pixelsIdentical = dicomResult.Pixels is { Identical: true },
                    metadataAgrees = dicomResult.MetadataAgrees,
                    summary = dicomResult.Summary,
                    cleanupProblem = dicomResult.CleanupProblem,
                },
            };
            File.WriteAllText(jsonPath, JsonSerializer.Serialize(document, new JsonSerializerOptions { WriteIndented = true }));
            return jsonPath;
        }

        // The evidence file is REQUIRED (Codex #73 finding 2, the leader's ruling): a baseline whose record cannot be written is a Fail, and no success is announced.
        // `status` inside the file is decided before the write, from everything judged so far; if the write or the promotion below fails, the result becomes Fail and
        // the file is rewritten to say so where that is still possible.
        string? writeProblem = null;
        try
        {
            WriteEvidence(failure, finalFile: false);
        }
        catch (Exception ex)
        {
            writeProblem = $"{ex.GetType().Name}: {ex.Message}";
            failure ??= $"the evidence file {jsonPath} could not be written ({writeProblem})";
        }

        if (dicomResult is { Passed: true })
        {
            if (failure is null)
            {
                var promoteProblem = BaselineDicomExport.Promote(dicomResult);
                if (promoteProblem is not null)
                {
                    failure = $"the DICOM file could not be given its final name ({promoteProblem})";
                    dicomResult = BaselineDicomExport.Discard(dicomResult);
                    try
                    {
                        WriteEvidence(failure, finalFile: false);
                    }
                    catch (Exception ex)
                    {
                        writeProblem ??= $"{ex.GetType().Name}: {ex.Message}";
                    }
                }
                else
                {
                    try
                    {
                        WriteEvidence(null, finalFile: true);
                    }
                    catch (Exception ex)
                    {
                        // The final file exists but its record cannot say so: not a baseline output that anyone can find in order. Removed, and the result is a Fail.
                        writeProblem = $"{ex.GetType().Name}: {ex.Message}";
                        failure = $"the evidence file {jsonPath} could not be updated ({writeProblem})";
                        try
                        {
                            File.Delete(dicomResult.Path);
                        }
                        catch (Exception deleteEx)
                        {
                            failure += $"; and {dicomResult.Path} could not be removed ({deleteEx.GetType().Name}: {deleteEx.Message})";
                        }
                    }
                }
            }
            else
            {
                // Something else failed (the evidence file): the passing DICOM is not kept under any name.
                dicomResult = BaselineDicomExport.Discard(dicomResult);
                if (dicomResult.CleanupProblem is not null)
                {
                    failure += $"; the partial DICOM file could not be removed ({dicomResult.CleanupProblem})";
                }
            }
        }

        var passed = failure is null && writeProblem is null;
        var status = passed
            ? $"Deterministic Baseline PASS: two runs bit-identical ({width}x{height}, {totalMs:0} ms; DICOM valid)"
            : $"Deterministic Baseline FAIL: {failure ?? writeProblem}";

        return new BaselineExecutionResult(
            passed,
            status,
            verdict,
            evidenceFolder,
            jsonPath,
            stageTimes,
            exposure,
            totalMs,
            writeProblem,
            dicomResult?.Valid ?? false,
            dicomResult?.Pixels is { Identical: true },
            dicomResult?.Summary ?? string.Empty);
    }

    /// <summary>
    /// The exclusive hold on an evidence folder: <c>baseline.lock</c> opened with <c>FileShare.None</c> and <c>DeleteOnClose</c>. A second run that tries to open it fails at
    /// once (a sharing violation, or access denied while the first handle is closing). Because the operating system deletes the file when the handle closes, the lock file does
    /// not outlive the run, not even one that ends by an exception, so the folder holds exactly the two outputs afterwards.
    /// </summary>
    private sealed class FolderLock : IDisposable
    {
        private readonly FileStream _stream;

        private FolderLock(FileStream stream) => _stream = stream;

        public static FolderLock? TryAcquire(string folder, out string? problem)
        {
            try
            {
                Directory.CreateDirectory(folder);
                var stream = new FileStream(Path.Combine(folder, LockFileName), FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None, 1, FileOptions.DeleteOnClose);
                problem = null;
                return new FolderLock(stream);
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
            {
                problem = $"{ex.GetType().Name}: {ex.Message}";
                return null;
            }
        }

        public void Dispose() => _stream.Dispose();
    }

    /// <summary>
    /// Removes <c>baseline.dcm</c>, its partial file and <c>baseline.json</c> from the evidence folder when they exist. Returns null when none is left, otherwise the reason one
    /// could not be removed (a missing folder is not a problem: nothing is there to be stale).
    /// </summary>
    private static string? ClearPreviousOutputs(string evidenceFolder)
    {
        if (!Directory.Exists(evidenceFolder))
        {
            return null;   // File.Delete throws on a missing directory, and nothing in one can be stale
        }

        var problems = new List<string>();
        foreach (var name in new[] { "baseline.dcm", "baseline.dcm" + BaselineDicomExport.PartialSuffix, "baseline.json" })
        {
            var path = Path.Combine(evidenceFolder, name);
            try
            {
                File.Delete(path);
            }
            catch (Exception ex)
            {
                problems.Add($"{path} ({ex.GetType().Name}: {ex.Message})");
            }
        }

        return problems.Count == 0 ? null : "the previous run's output could not be removed, so a stale file could be mistaken for this run's: " + string.Join("; ", problems);
    }

    /// <summary>The uncalibrated EI sentence the preprocess stage put in its summary (first run), or empty when it did not measure.</summary>
    private static string ExposureOf(IReadOnlyList<BaselineSingleRun> runs)
    {
        const string Marker = "uncalibrated EI";
        var reason = runs.Count == 0 ? null : runs[0].Chain.Stages.FirstOrDefault(s => s.StageId == Models.StageIds.Preprocess)?.Reason;
        var at = reason?.IndexOf(Marker, StringComparison.Ordinal) ?? -1;
        return at < 0 ? string.Empty : reason![at..].Trim();
    }
}
