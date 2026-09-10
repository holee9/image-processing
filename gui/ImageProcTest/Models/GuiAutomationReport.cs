namespace ImageProcTest.Models;

public sealed class GuiAutomationReport
{
    public bool Passed { get; set; }

    public string BackendVersion { get; set; } = string.Empty;

    /// <summary>#136: the backend mode this run used.</summary>
    public string BackendMode { get; set; } = string.Empty;

    /// <summary>#136: where that mode came from — "arg" (--automation-backend) or "file" (appsettings.json).</summary>
    public string BackendModeSource { get; set; } = string.Empty;

    /// <summary>#129: directory the native library resolved from, or "loader"/empty. See RuntimeInfo.NativeSource.</summary>
    public string NativeSource { get; set; } = string.Empty;

    public int InitialLogCount { get; set; }

    public int InitialAlertCount { get; set; }

    public int LogCountAfterLoad { get; set; }

    public int AlertCountAfterLoad { get; set; }

    public int LogCountAfterClear { get; set; }

    public int AlertCountAfterClear { get; set; }

    public string ActiveImageSummary { get; set; } = string.Empty;

    public string StatusAfterLoad { get; set; } = string.Empty;

    public string RuntimeStateAfterShutdown { get; set; } = string.Empty;

    public string LastRawDirectory { get; set; } = string.Empty;

    public bool LastRawDirPersisted { get; set; }

    public bool HelpWindowOpened { get; set; }

    public bool HelpDocumentLoaded { get; set; }

    public string HelpWindowTitle { get; set; } = string.Empty;

    public string HelpDocumentPath { get; set; } = string.Empty;

    public int TopLevelMenuCount { get; set; }

    public bool CanonicalMenuGroupsDetected { get; set; }

    public bool PlannedMenuPlaceholdersDetected { get; set; }

    public bool ToolbarMenuCommandParity { get; set; }

    public bool ResizableDiagnosticsLayoutDetected { get; set; }

    public bool DisplayPipelineApplied { get; set; }

    public string DisplayPipelineSummary { get; set; } = string.Empty;

    public string CalibrationEvaluationSummary { get; set; } = string.Empty;

    public string OffsetCorrectionMode { get; set; } = string.Empty;

    public string DefectCorrectionMode { get; set; } = string.Empty;

    public bool CalibrationEvaluationEvidenceExported { get; set; }

    public bool DisplayPanelVisible { get; set; }

    public string DisplayVersion { get; set; } = string.Empty;

    public bool VoiPresetApplied { get; set; }

    /// <summary>#135: the center the active backend's preset actually produced (Mock 32768 / native 40).</summary>
    public float VoiPresetCenter { get; set; }

    /// <summary>#135: the width the active backend's preset actually produced (Mock 65535 / native 400).</summary>
    public float VoiPresetWidth { get; set; }

    public bool ComparisonViewportDetected { get; set; }

    public string ComparisonMode { get; set; } = string.Empty;

    public double ComparisonZoomScale { get; set; }

    public double ComparisonSwipePosition { get; set; }

    public bool ComparisonSourcePreserved { get; set; }

    public bool ComparisonEvidenceExported { get; set; }

    public int DisabledFutureCommandCount { get; set; }

    public bool MenuCommandReportCreated { get; set; }

    public string? Error { get; set; }
}
