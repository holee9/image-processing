// #178 (GUI-C-86): the evidence bundle can be exported, and two run sets keep their evidence apart.
using System.IO.Compression;
using System.Text.Json;
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;
using static ImageProcTest.E2ETests.Scenarios.Workflows.WorkbenchObservation;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// GUI-C-85 measured "Export bundle" failing on every launch: <c>RunSetState.RunId</c> was never set, so
/// the export zipped <c>evidence/</c> into <c>evidence/bundles/.zip</c> — a file inside the folder being
/// zipped. Each case launches its own app(s), because the evidence folder is shared by every launch of the
/// same build and a run set is one launch.
/// </summary>
[Collection(EvidenceBundleCollection.Name)]
public sealed class EvidenceBundleScenarios(ITestOutputHelper output)
{
    private const string RawRelativePath = @"fixtures\gui-s0\raw\synthetic_1024x1024.raw";
    private const string ExportedPrefix = "Evidence bundle exported: ";

    /// <summary>EB-01: a verdict then an export produces a zip whose backend.json names the actual backend.</summary>
    [SkippableFact]
    public void EB01_ExportAfterAVerdict_ProducesABundleNamingTheBackend()
    {
        using var app = new BundleApp();
        var run = RecordAndExport("EB01", app);

        Assert.True(run.ZipPath is not null, $"Export failed: '{run.Status}' (#178).");
        using var zip = ZipFile.OpenRead(run.ZipPath!);
        var names = zip.Entries.Select(e => e.FullName).ToArray();
        output.WriteLine($"EB01 zip='{run.ZipPath}' entries=[{string.Join(", ", names)}]");
        var entry = zip.GetEntry("backend.json");
        Assert.True(entry is not null, $"The bundle has no backend.json: [{string.Join(", ", names)}].");

        using var reader = new StreamReader(entry!.Open());
        var backend = JsonDocument.Parse(reader.ReadToEnd()).RootElement;
        output.WriteLine($"EB01 backend.json actual={backend.GetProperty("actualBackendMode")} requested={backend.GetProperty("requestedBackendMode")}");
        Assert.Equal(app.ExpectedActualMode, backend.GetProperty("actualBackendMode").GetString());
    }

    /// <summary>
    /// EB-02: two run sets, one after the other, leave two evidence folders, each naming its own backend.
    /// The second launch requests Native with an empty DLL directory, so its (requested, actual) pair is
    /// (Native, Mock) whatever the suite's own backend is — the two folders cannot agree by accident.
    /// </summary>
    [SkippableFact]
    public void EB02_TwoRunSets_KeepSeparateEvidence()
    {
        Recorded first, second;
        string firstExpected, secondExpected;
        using (var app = new BundleApp())
        {
            first = RecordAndExport("EB02-1", app);
            firstExpected = app.ExpectedActualMode;
        }

        Thread.Sleep(1100);   // the run id carries the start time to the second
        using (var app = new FallbackBundleApp())
        {
            second = RecordAndExport("EB02-2", app);
            secondExpected = "Mock";
        }

        Assert.False(string.IsNullOrEmpty(first.RunId), $"The first run set has no run id (status '{first.Status}').");
        Assert.False(string.IsNullOrEmpty(second.RunId), $"The second run set has no run id (status '{second.Status}').");
        Assert.NotEqual(first.RunId, second.RunId);

        var a = ReadBackend(first.EvidenceDirectory!);
        var b = ReadBackend(second.EvidenceDirectory!);
        output.WriteLine($"EB02 first={first.RunId} actual={a.Actual} requested={a.Requested}; second={second.RunId} actual={b.Actual} requested={b.Requested}");
        Assert.Equal(firstExpected, a.Actual);
        Assert.Equal(secondExpected, b.Actual);
        Assert.Equal("Native", b.Requested);
    }

    /// <summary>EB-03: nothing is written straight into <c>evidence/</c> — every file sits in a run folder.</summary>
    [SkippableFact]
    public void EB03_NothingIsWrittenIntoTheEvidenceRoot()
    {
        using var app = new BundleApp();
        var root = EvidenceRoot(app);
        var before = Directory.Exists(root) ? Directory.GetFiles(root).Length : 0;
        RecordAndExport("EB03", app);
        var loose = Directory.Exists(root) ? Directory.GetFiles(root) : [];
        output.WriteLine($"EB03 files directly under evidence/: before={before} after=[{string.Join(", ", loose.Select(Path.GetFileName))}]");
        Assert.Empty(loose);
    }

    private sealed record Recorded(string Status, string? ZipPath, string? RunId, string? EvidenceDirectory);

    private Recorded RecordAndExport(string scenario, ApplicationFixture app)
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        var window = app.MainWindow!;

        Buttons(window).First(b => (b.Name ?? string.Empty).Contains("Pass", StringComparison.Ordinal)).AsButton().Invoke();
        Thread.Sleep(600);
        Buttons(window).First(b => b.Name == "Export bundle").AsButton().Invoke();
        Thread.Sleep(1200);

        var status = window.FindFirstDescendant(cf => cf.ByAutomationId("StatusBarText"))?.Name ?? string.Empty;
        output.WriteLine($"{scenario} status-bar='{RuntimeSummary(window)}' export='{status}'");
        if (!status.StartsWith(ExportedPrefix, StringComparison.Ordinal))
        {
            return new Recorded(status, null, null, null);
        }

        var zip = status[ExportedPrefix.Length..];
        var runId = Path.GetFileNameWithoutExtension(zip);
        var keep = Path.Combine(Path.GetTempPath(), $"xpe-e2e-bundle-{scenario}-{Environment.ProcessId}.zip");
        File.Copy(zip, keep, overwrite: true);
        return new Recorded(status, keep, runId, Path.Combine(EvidenceRoot(app), runId));
    }

    private static (string? Actual, string? Requested) ReadBackend(string directory)
    {
        var path = Path.Combine(directory, "backend.json");
        Assert.True(File.Exists(path), $"No backend.json in {directory}.");
        var json = JsonDocument.Parse(File.ReadAllText(path)).RootElement;
        return (json.GetProperty("actualBackendMode").GetString(), json.GetProperty("requestedBackendMode").GetString());
    }

    private static string EvidenceRoot(ApplicationFixture app) =>
        Path.Combine(Path.GetDirectoryName(ApplicationFixture.ResolveApplicationExecutable()!)!, "evidence");

    private static AutomationElement[] Buttons(Window window) =>
        window.FindAllDescendants(cf => cf.ByControlType(FlaUI.Core.Definitions.ControlType.Button));

    /// <summary>The suite's own backend (XPE_E2E_BACKEND); its actual mode is the requested one.</summary>
    private sealed class BundleApp() : ApplicationFixture(RawRelativePath, Array.Empty<string>())
    {
        public string ExpectedActualMode => BackendMode ?? "Mock";
    }

    /// <summary>Native requested with an empty DLL directory: the actual mode is Mock.</summary>
    private sealed class FallbackBundleApp() : ApplicationFixture(RawRelativePath, EmptyDirectory());

    private static DirectoryInfo EmptyDirectory()
    {
        var dir = Directory.CreateDirectory(Path.Combine(Path.GetTempPath(), $"xpe-e2e-empty-native-eb-{Environment.ProcessId}"));
        foreach (var file in dir.GetFiles()) file.Delete();
        return dir;
    }
}

/// <summary>Its own collection: these cases launch and close their own apps.</summary>
[CollectionDefinition(Name)]
public sealed class EvidenceBundleCollection
{
    public const string Name = "EvidenceBundle";
}
