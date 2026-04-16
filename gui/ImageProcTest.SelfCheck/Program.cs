using System.IO;
using System.Text.Json;
using ImageProcTest.Services;

var repoRoot = GuiFixtureManifestService.FindRepositoryRoot(AppContext.BaseDirectory);
var fixtureRoot = GuiFixtureManifestService.GetRepositoryFixtureRoot(repoRoot);
var manifest = GuiFixtureManifestService.LoadFromRepository(repoRoot);

var manifestPath = Path.Combine(fixtureRoot, "fixture-manifest.json");
var templatePath = GuiFixtureManifestService.ResolveFixturePath(fixtureRoot, manifest.Runtime.SettingsTemplateRelativePath);
var rawPath = GuiFixtureManifestService.ResolveFixturePath(fixtureRoot, manifest.RawSample.RelativePath);
var rawDirectory = Path.GetDirectoryName(rawPath) ?? string.Empty;

Assert(File.Exists(manifestPath), $"Fixture manifest not found: {manifestPath}");
Assert(File.Exists(templatePath), $"Fixture settings template not found: {templatePath}");
Assert(File.Exists(rawPath), $"Fixture raw sample not found: {rawPath}");

var rawHash = GuiFixtureManifestService.ComputeSha256(rawPath);
Assert(
    string.Equals(rawHash, manifest.RawSample.Sha256, StringComparison.OrdinalIgnoreCase),
    $"Fixture raw hash mismatch. Expected {manifest.RawSample.Sha256}, got {rawHash}.");

Assert(Directory.Exists(GuiFixtureManifestService.ResolveFixturePath(fixtureRoot, manifest.CalibrationDirectories.Offset)), "Offset calibration fixture directory missing.");
Assert(Directory.Exists(GuiFixtureManifestService.ResolveFixturePath(fixtureRoot, manifest.CalibrationDirectories.Gain)), "Gain calibration fixture directory missing.");
Assert(Directory.Exists(GuiFixtureManifestService.ResolveFixturePath(fixtureRoot, manifest.CalibrationDirectories.Defect)), "Defect calibration fixture directory missing.");

using (var templateDocument = JsonDocument.Parse(File.ReadAllText(templatePath)))
{
    Assert(
        templateDocument.RootElement.GetProperty("backendMode").GetString() == manifest.BackendMode,
        "Template backendMode should match fixture manifest.");
    Assert(
        templateDocument.RootElement.GetProperty("rawWidth").GetInt32() == manifest.RawSample.Width,
        "Template rawWidth should match fixture manifest.");
    Assert(
        templateDocument.RootElement.GetProperty("rawHeight").GetInt32() == manifest.RawSample.Height,
        "Template rawHeight should match fixture manifest.");
}

var settingsPath = Path.Combine(AppContext.BaseDirectory, "appsettings.selfcheck.json");
var settingsService = new AppSettingsService(settingsPath);
var fixtureSettings = GuiFixtureManifestService.CreateFixtureSettings(manifest, fixtureRoot, rawDirectory);

settingsService.Save(fixtureSettings);
var loadedSettings = settingsService.Load();

Assert(loadedSettings.BackendMode == manifest.BackendMode, "BackendMode should persist.");
Assert(loadedSettings.RawWidth == manifest.RawSample.Width, "RawWidth should persist.");
Assert(loadedSettings.RawHeight == manifest.RawSample.Height, "RawHeight should persist.");
Assert(loadedSettings.RawPixelFormat == manifest.RawSample.PixelFormat, "RawPixelFormat should persist.");
Assert(loadedSettings.VoiWindowCenter == 40.0f, "VOI window center should default to Abdomen preset.");
Assert(loadedSettings.VoiWindowWidth == 400.0f, "VOI window width should default to Abdomen preset.");
Assert(loadedSettings.VoiLutMode == "Linear", "VOI LUT mode should default to Linear.");
Assert(loadedSettings.SelectedBodyPart == "Abdomen", "Selected body part should default to Abdomen.");
Assert(!loadedSettings.GsdfEnabled, "GSDF should default to disabled until PS3.14 validation is complete.");
Assert(loadedSettings.ModalityRescaleSlope == 1.0f, "Modality slope should default to 1.0.");
Assert(loadedSettings.ModalityRescaleIntercept == -1024.0f, "Modality intercept should default to -1024.");
Assert(
    loadedSettings.OffsetCalibrationDirectory == GuiFixtureManifestService.ResolveFixturePath(fixtureRoot, manifest.CalibrationDirectories.Offset),
    "Offset path should persist.");
Assert(
    loadedSettings.GainCalibrationDirectory == GuiFixtureManifestService.ResolveFixturePath(fixtureRoot, manifest.CalibrationDirectories.Gain),
    "Gain path should persist.");
Assert(
    loadedSettings.DefectCalibrationDirectory == GuiFixtureManifestService.ResolveFixturePath(fixtureRoot, manifest.CalibrationDirectories.Defect),
    "Defect path should persist.");
Assert(loadedSettings.LastRawDirectory == rawDirectory, "LastRawDirectory should persist.");

var backend = new MockXpeBackend(
    new RawImageLoader(),
    Path.Combine(AppContext.BaseDirectory, "xpe_common.dll"),
    false,
    Path.Combine(AppContext.BaseDirectory, "xpe_display.dll"),
    false);
var runtime = backend.Initialize(loadedSettings);

Assert(runtime.Version == manifest.ExpectedTelemetry.BackendVersion, "Mock version should match.");
Assert(!string.IsNullOrWhiteSpace(backend.GetVersion()), "GetVersion should be non-empty.");
Assert(backend.GetLogCount() == manifest.ExpectedTelemetry.InitialLogCount, "Mock backend log count should match fixture manifest.");
Assert(backend.GetAlertCount() == manifest.ExpectedTelemetry.InitialAlertCount, "Mock backend alert count should match fixture manifest.");
Assert(backend.GetDisplayVersion() == "v0.0.0-mock-display", "Mock display version should match.");
Assert(backend.GetAlert(0)?.Severity == "INFO", "First alert should be INFO.");
Assert(backend.GetAlert(1)?.Severity == "WARN", "Second alert should be WARN.");
Assert(backend.GetAlert(2)?.Severity == "ERROR", "Third alert should be ERROR.");

var frame = backend.LoadRawImage(rawPath, loadedSettings);
Assert(frame.Preview.PixelWidth == manifest.RawSample.Width, "Preview width should match fixture.");
Assert(frame.Preview.PixelHeight == manifest.RawSample.Height, "Preview height should match fixture.");
Assert(frame.Summary.Contains($"RAW {manifest.RawSample.Width}x{manifest.RawSample.Height}"), "Summary should include fixture dimensions.");
Assert(frame.RawPixels?.Length == manifest.RawSample.Width * manifest.RawSample.Height, "Raw pixel payload should be retained for display integration.");

var preset = backend.CreateVoiPreset(ImageProcTest.Models.XpeBodyPartEnum.Abdomen);
Assert(preset.Center == 40.0f && preset.Width == 400.0f, "Abdomen VOI preset should match display integration guide.");

var displayFrame = backend.ApplyDisplayPipeline(frame, loadedSettings);
Assert(displayFrame.DisplayPipelineApplied, "Mock display pipeline should mark the frame as applied.");
Assert(displayFrame.ProcessedPreview is not null, "Mock display pipeline should provide a processed preview.");
Assert(displayFrame.DisplayPipelineSummary.Contains("VOI", StringComparison.Ordinal), "Display summary should include VOI settings.");

Console.WriteLine("GUI-S0 self-check passed.");
Console.WriteLine($"Fixture manifest: {manifestPath}");
Console.WriteLine($"Fixture raw: {rawPath}");
Console.WriteLine($"Settings file: {settingsPath}");

static void Assert(bool condition, string message)
{
    if (!condition)
    {
        throw new InvalidOperationException(message);
    }
}
