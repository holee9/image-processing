// #225 row 9 (GUI-C-196 M4): the whole Deterministic Baseline command apart from the window: two runs, the verdict, the DICOM export, the evidence file, the
// one-line status. Free of WPF and of native code, so the integration tests link it and the view model only has to schedule it.
using System.Diagnostics;
using System.IO;
using System.Text.Json;

namespace ImageProcTest.Services;

/// <summary>Everything one baseline command produced. <see cref="Passed"/> needs the verdict AND the DICOM export to pass.</summary>
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
        ArgumentNullException.ThrowIfNull(raw);
        ArgumentNullException.ThrowIfNull(runOnce);
        ArgumentNullException.ThrowIfNull(metadata);
        ArgumentException.ThrowIfNullOrWhiteSpace(evidenceFolder);

        var now = clock ?? (() => DateTimeOffset.Now);
        var startedAt = now();
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
        string? failure = verdict.Status == BaselineStatus.Fail ? verdict.FailureReason : null;
        if (verdict.Status == BaselineStatus.Pass)
        {
            if (dicom is null)
            {
                failure = "the backend offered no DICOM session, so the output was not written";
            }
            else
            {
                dicomResult = BaselineDicomExport.Export(Path.Combine(evidenceFolder, "baseline.dcm"), runs[0].Output, width, height, metadata, dicom);
                if (!dicomResult.Passed)
                {
                    failure = "DICOM export: " + dicomResult.Summary;
                }
            }
        }

        var passed = failure is null;
        var totalMs = total.Elapsed.TotalMilliseconds;
        var stageTimes = string.Join(" | ", runs.Select((r, i) => $"run{i + 1}: {r.Chain.Timings.Replace("times: ", string.Empty)}, total {runMs[i]:0} ms"));
        var exposure = ExposureOf(runs);
        var status = passed
            ? $"Deterministic Baseline PASS: two runs bit-identical ({width}x{height}, {totalMs:0} ms; DICOM valid)"
            : $"Deterministic Baseline FAIL: {failure}";

        var jsonPath = Path.Combine(evidenceFolder, "baseline.json");
        string? writeProblem = null;
        try
        {
            Directory.CreateDirectory(evidenceFolder);
            var document = new
            {
                status = passed ? "Pass" : "Fail",
                failureReason = failure ?? string.Empty,
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
                    passed = dicomResult.Passed,
                    valid = dicomResult.Valid,
                    report = dicomResult.ReportJson,
                    pixelsIdentical = dicomResult.Pixels is { Identical: true },
                    metadataAgrees = dicomResult.MetadataAgrees,
                    summary = dicomResult.Summary,
                },
            };
            File.WriteAllText(jsonPath, JsonSerializer.Serialize(document, new JsonSerializerOptions { WriteIndented = true }));
        }
        catch (Exception ex)
        {
            writeProblem = $"{ex.GetType().Name}: {ex.Message}";
        }

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

    /// <summary>The uncalibrated EI sentence the preprocess stage put in its summary (first run), or empty when it did not measure.</summary>
    private static string ExposureOf(IReadOnlyList<BaselineSingleRun> runs)
    {
        const string Marker = "uncalibrated EI";
        var reason = runs.Count == 0 ? null : runs[0].Chain.Stages.FirstOrDefault(s => s.StageId == Models.StageIds.Preprocess)?.Reason;
        var at = reason?.IndexOf(Marker, StringComparison.Ordinal) ?? -1;
        return at < 0 ? string.Empty : reason![at..].Trim();
    }
}
