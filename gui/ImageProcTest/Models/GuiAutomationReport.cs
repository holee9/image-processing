namespace ImageProcTest.Models;

public sealed class GuiAutomationReport
{
    public bool Passed { get; set; }

    public string BackendVersion { get; set; } = string.Empty;

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

    public bool DisplayPanelVisible { get; set; }

    public string DisplayVersion { get; set; } = string.Empty;

    public bool VoiPresetApplied { get; set; }

    public int DisabledFutureCommandCount { get; set; }

    public bool MenuCommandReportCreated { get; set; }

    public string? Error { get; set; }
}
