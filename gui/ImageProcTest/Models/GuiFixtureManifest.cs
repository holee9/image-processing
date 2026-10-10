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
    // GUI-C-233i: one expected value per backend (a single "backendVersion" was the Mock string and was asserted against a Native run). The Native value is not a constant: it is read from the DLL.
    [JsonPropertyName("backendVersionMock")]
    public string BackendVersionMock { get; set; } = string.Empty;

    [JsonPropertyName("backendVersionNative")]
    public string BackendVersionNative { get; set; } = string.Empty;

    [JsonPropertyName("initialLogCount")]
    public int InitialLogCount { get; set; }

    // GUI-C-233i: the Mock backend raises three scripted alerts at start-up; the Native one raises one (INFO REAL_DISPLAY_BACKEND_ACTIVE). One number for both was wrong for the one not measured.
    [JsonPropertyName("initialAlertCountMock")]
    public int InitialAlertCountMock { get; set; }

    [JsonPropertyName("initialAlertCountNative")]
    public int InitialAlertCountNative { get; set; }

    [JsonPropertyName("initialAlertNative")]
    public string InitialAlertNative { get; set; } = string.Empty;
}
