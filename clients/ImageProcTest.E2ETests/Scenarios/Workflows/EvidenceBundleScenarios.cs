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

    /// <summary>
    /// IB-01 (#178, GUI-C-89): a successful Initialize Backend starts a new run set — the evidence before
    /// and after it lands in two folders under two run ids, and the top bar's "Run #" follows.
    /// </summary>
    [SkippableFact]
    public void IB01_InitializeBackend_StartsANewRunSet()
    {
        using var app = new BundleApp();
        var before = RecordAndExport("IB01-1", app);
        var shownBefore = ShownRunId(app);

        InitializeBackend(app);
        var shownAfter = ShownRunId(app);
        var after = RecordAndExport("IB01-2", app);
        output.WriteLine($"IB01 run ids: exported {before.RunId} -> {after.RunId}; top bar '{shownBefore}' -> '{shownAfter}'");

        Assert.False(string.IsNullOrEmpty(before.RunId), $"The first export failed: '{before.Status}'.");
        Assert.False(string.IsNullOrEmpty(after.RunId), $"The second export failed: '{after.Status}'.");
        Assert.True(
            before.RunId != after.RunId,
            $"Initialize Backend succeeded but the evidence before and after it share run id {before.RunId}, so the later " +
            "backend.json overwrote the earlier one (#178).");
        Assert.True(Directory.Exists(before.EvidenceDirectory) && Directory.Exists(after.EvidenceDirectory), "An evidence folder is missing.");
        Assert.Equal(before.RunId, shownBefore);
        Assert.Equal(after.RunId, shownAfter);
    }

    /// <summary>
    /// IB-02 (#178, GUI-C-89): the actual backend changes between two run sets of one launch. Native is
    /// requested with an EMPTY DLL directory (actual Mock); the native DLLs are then copied in and the
    /// backend is initialised again (actual Native). The reverse order is not possible: a loaded DLL is
    /// locked and cannot be removed from under the running app.
    /// </summary>
    [SkippableFact]
    public void IB02_InitializeAfterTheDllsAppear_RecordsMockThenNative()
    {
        var source = Environment.GetEnvironmentVariable(ApplicationFixture.NativeDirVariable);
        Skip.If(
            Environment.GetEnvironmentVariable(ApplicationFixture.BackendVariable) != "Native" || string.IsNullOrWhiteSpace(source),
            "Needs a Native run with XPE_NATIVE_DIR naming the staged DLLs to copy in.");

        var dir = EmptyDirectory();
        using var app = new FallbackBundleAppIn(dir);
        var first = RecordAndExport("IB02-1", app);

        foreach (var file in Directory.GetFiles(source!, "*.dll"))
        {
            File.Copy(file, Path.Combine(dir.FullName, Path.GetFileName(file)), overwrite: true);
        }

        InitializeBackend(app);
        var second = RecordAndExport("IB02-2", app);
        Assert.False(string.IsNullOrEmpty(first.RunId), $"The first export failed: '{first.Status}'.");
        Assert.False(string.IsNullOrEmpty(second.RunId), $"The second export failed: '{second.Status}'.");
        Assert.NotEqual(first.RunId, second.RunId);

        var a = ReadBackend(first.EvidenceDirectory!);
        var b = ReadBackend(second.EvidenceDirectory!);
        output.WriteLine($"IB02 {first.RunId}: actual={a.Actual} requested={a.Requested}; {second.RunId}: actual={b.Actual} requested={b.Requested}");
        Assert.Equal(("Mock", "Native"), (a.Actual, a.Requested));
        Assert.Equal(("Native", "Native"), (b.Actual, b.Requested));
    }

    private void InitializeBackend(ApplicationFixture app)
    {
        var window = app.MainWindow!;
        var button = window.FindFirstDescendant(cf => cf.ByAutomationId("InitializeBackendButton"));
        Assert.True(button is not null, "InitializeBackendButton was not found.");
        button!.AsButton().Invoke();
        Thread.Sleep(1500);
        output.WriteLine($"after Initialize: status-bar='{RuntimeSummary(window)}' status='{window.FindFirstDescendant(cf => cf.ByAutomationId("StatusBarText"))?.Name}'");
    }

    /// <summary>The run id the top bar shows ("… · Run #&lt;id&gt;").</summary>
    private static string ShownRunId(ApplicationFixture app)
    {
        var text = app.MainWindow!.FindAllDescendants(cf => cf.ByControlType(FlaUI.Core.Definitions.ControlType.Text))
            .Select(t => { try { return t.Name ?? string.Empty; } catch { return string.Empty; } })
            .FirstOrDefault(t => t.Contains("Run #", StringComparison.Ordinal)) ?? string.Empty;
        var at = text.IndexOf("Run #", StringComparison.Ordinal);
        return at < 0 ? string.Empty : text[(at + "Run #".Length)..].Trim();
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

    private sealed class FallbackBundleAppIn(DirectoryInfo dir) : ApplicationFixture(RawRelativePath, dir);

    private static DirectoryInfo EmptyDirectory()
    {
        var dir = Directory.CreateDirectory(Path.Combine(Path.GetTempPath(), $"xpe-e2e-empty-native-eb-{Environment.ProcessId}-{Guid.NewGuid():N}"));
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
