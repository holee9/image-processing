using System.Text.Json.Serialization;

namespace ImageProcTest.Models;

public sealed class GuiFixtureManifest
{
    [JsonPropertyName("fixtureId")]
    public string FixtureId { get; set; } = string.Empty;

    [JsonPropertyName("fixtureVersion")]
    public string FixtureVersion { get; set; } = string.Empty;

    [JsonPropertyName("backendMode")]
    public string BackendMode { get; set; } = "Mock";

    [JsonPropertyName("rawSample")]
    public GuiFixtureRawSample RawSample { get; set; } = new();

    [JsonPropertyName("runtime")]
    public GuiFixtureRuntime Runtime { get; set; } = new();

    [JsonPropertyName("calibrationDirectories")]
    public GuiFixtureCalibrationDirectories CalibrationDirectories { get; set; } = new();

    [JsonPropertyName("expectedTelemetry")]
    public GuiFixtureExpectedTelemetry ExpectedTelemetry { get; set; } = new();
}

public sealed class GuiFixtureRawSample
{
    [JsonPropertyName("relativePath")]
    public string RelativePath { get; set; } = string.Empty;

    [JsonPropertyName("width")]
    public int Width { get; set; }

    [JsonPropertyName("height")]
    public int Height { get; set; }

    [JsonPropertyName("pixelFormat")]
    public string PixelFormat { get; set; } = "UInt16LE";

    [JsonPropertyName("sha256")]
    public string Sha256 { get; set; } = string.Empty;
}

public sealed class GuiFixtureRuntime
{
    [JsonPropertyName("settingsTemplateRelativePath")]
    public string SettingsTemplateRelativePath { get; set; } = string.Empty;

    [JsonPropertyName("preparedSettingsFileName")]
    public string PreparedSettingsFileName { get; set; } = "appsettings.json";

    [JsonPropertyName("automationReportFileName")]
    public string AutomationReportFileName { get; set; } = "automation-report.json";

    [JsonPropertyName("prepReportFileName")]
    public string PrepReportFileName { get; set; } = "fixture-prep-report.json";
}

public sealed class GuiFixtureCalibrationDirectories
{
    [JsonPropertyName("offset")]
    public string Offset { get; set; } = string.Empty;

    [JsonPropertyName("gain")]
    public string Gain { get; set; } = string.Empty;

    [JsonPropertyName("defect")]
    public string Defect { get; set; } = string.Empty;
}

public sealed class GuiFixtureExpectedTelemetry
{
    [JsonPropertyName("backendVersion")]
    public string BackendVersion { get; set; } = string.Empty;

    [JsonPropertyName("initialLogCount")]
    public int InitialLogCount { get; set; }

    [JsonPropertyName("initialAlertCount")]
    public int InitialAlertCount { get; set; }
}
