using System.Text.Json.Serialization;
using ImageProcTest.ViewModels;

namespace ImageProcTest.Models;

/// <summary>
/// Stores the persisted GUI-S0 runtime settings used by the WPF shell and test automation.
/// </summary>
public sealed class AppSettings : ObservableObject
{
    private string _backendMode = "Mock";
    private int _rawWidth = 3072;
    private int _rawHeight = 3072;
    private string _rawPixelFormat = "UInt16LE";
    private string _offsetCalibrationDirectory = "data/calibration/offset";
    private string _gainCalibrationDirectory = "data/calibration/gain";
    private string _defectCalibrationDirectory = "data/calibration/defect";
    private string _offsetCorrectionMode = CalibrationStageMode.Auto;
    private string _gainCorrectionMode = CalibrationStageMode.Auto;
    private string _defectCorrectionMode = CalibrationStageMode.Auto;
    private string _ghostCorrectionMode = CalibrationStageMode.Auto;
    private string _temperatureCompensationMode = CalibrationStageMode.Auto;
    private string _nonlinearityCorrectionMode = CalibrationStageMode.Auto;
    private string _binningCorrectionMode = CalibrationStageMode.Auto;
    private string _lastOpenedPath = string.Empty;
    private float _voiWindowCenter = 40.0f;
    private float _voiWindowWidth = 400.0f;
    private string _voiLutMode = "Linear";
    private string _selectedBodyPart = "Abdomen";
    private bool _gsdfEnabled;
    private float _modalityRescaleSlope = 1.0f;
    private float _modalityRescaleIntercept = -1024.0f;
    private bool _showDisplayPanel = true;
    private string _comparisonMode = "SwipeVertical";
    private double _comparisonZoomScale;
    private double _comparisonPanX;
    private double _comparisonPanY;
    private double _comparisonSwipePosition = 0.5;
    private double _comparisonOverlayOpacity = 0.5;
    private string _laneAAlgorithm = "Production v1.2";
    private string _laneBAlgorithm = "Candidate v1.4";
    private bool _focusMode;
    private bool _leftPanelOpen = true;
    private bool _rightPanelOpen = true;
    private string _analysisTab = "metrics";
    private double _laneBSharpeningSigma = 0.85;
    private double _laneBDenoiseStrength = 0.42;
    private string _lastRunSetId = string.Empty;

    /// <summary>
    /// Gets or sets the requested backend mode. GUI-S0 currently supports Mock and prepares for Native.
    /// </summary>
    [JsonPropertyName("backendMode")]
    public string BackendMode
    {
        get => _backendMode;
        set => SetProperty(ref _backendMode, value);
    }

    /// <summary>
    /// Gets or sets the expected raw width used by the raw loader.
    /// </summary>
    [JsonPropertyName("rawWidth")]
    public int RawWidth
    {
        get => _rawWidth;
        set => SetProperty(ref _rawWidth, value);
    }

    /// <summary>
    /// Gets or sets the expected raw height used by the raw loader.
    /// </summary>
    [JsonPropertyName("rawHeight")]
    public int RawHeight
    {
        get => _rawHeight;
        set => SetProperty(ref _rawHeight, value);
    }

    /// <summary>
    /// Gets or sets the raw pixel format identifier consumed by the raw loader.
    /// </summary>
    [JsonPropertyName("rawPixelFormat")]
    public string RawPixelFormat
    {
        get => _rawPixelFormat;
        set => SetProperty(ref _rawPixelFormat, value);
    }

    /// <summary>
    /// Gets or sets the preferred offset calibration directory.
    /// </summary>
    [JsonPropertyName("calibOffsetDir")]
    public string OffsetCalibrationDirectory
    {
        get => _offsetCalibrationDirectory;
        set => SetProperty(ref _offsetCalibrationDirectory, value);
    }

    /// <summary>
    /// Gets or sets the preferred gain calibration directory.
    /// </summary>
    [JsonPropertyName("calibGainDir")]
    public string GainCalibrationDirectory
    {
        get => _gainCalibrationDirectory;
        set => SetProperty(ref _gainCalibrationDirectory, value);
    }

    /// <summary>
    /// Gets or sets the preferred defect calibration directory.
    /// </summary>
    [JsonPropertyName("calibDefectDir")]
    public string DefectCalibrationDirectory
    {
        get => _defectCalibrationDirectory;
        set => SetProperty(ref _defectCalibrationDirectory, value);
    }

    /// <summary>
    /// Gets or sets the Test GUI evaluation mode for offset correction: Auto, On, or Off.
    /// </summary>
    [JsonPropertyName("calibOffsetMode")]
    public string OffsetCorrectionMode
    {
        get => _offsetCorrectionMode;
        set => SetProperty(ref _offsetCorrectionMode, CalibrationStageMode.Normalize(value));
    }

    /// <summary>
    /// Gets or sets the Test GUI evaluation mode for gain correction: Auto, On, or Off.
    /// </summary>
    [JsonPropertyName("calibGainMode")]
    public string GainCorrectionMode
    {
        get => _gainCorrectionMode;
        set => SetProperty(ref _gainCorrectionMode, CalibrationStageMode.Normalize(value));
    }

    /// <summary>
    /// Gets or sets the Test GUI evaluation mode for defect correction: Auto, On, or Off.
    /// </summary>
    [JsonPropertyName("calibDefectMode")]
    public string DefectCorrectionMode
    {
        get => _defectCorrectionMode;
        set => SetProperty(ref _defectCorrectionMode, CalibrationStageMode.Normalize(value));
    }

    /// <summary>
    /// Gets or sets the Test GUI evaluation mode for ghost correction: Auto, On, or Off.
    /// </summary>
    [JsonPropertyName("calibGhostMode")]
    public string GhostCorrectionMode
    {
        get => _ghostCorrectionMode;
        set => SetProperty(ref _ghostCorrectionMode, CalibrationStageMode.Normalize(value));
    }

    /// <summary>
    /// Gets or sets the Test GUI evaluation mode for temperature compensation: Auto, On, or Off.
    /// </summary>
    [JsonPropertyName("calibTemperatureMode")]
    public string TemperatureCompensationMode
    {
        get => _temperatureCompensationMode;
        set => SetProperty(ref _temperatureCompensationMode, CalibrationStageMode.Normalize(value));
    }

    /// <summary>
    /// Gets or sets the Test GUI evaluation mode for nonlinearity correction: Auto, On, or Off.
    /// </summary>
    [JsonPropertyName("calibNonlinearityMode")]
    public string NonlinearityCorrectionMode
    {
        get => _nonlinearityCorrectionMode;
        set => SetProperty(ref _nonlinearityCorrectionMode, CalibrationStageMode.Normalize(value));
    }

    /// <summary>
    /// Gets or sets the Test GUI evaluation mode for binning correction: Auto, On, or Off.
    /// </summary>
    [JsonPropertyName("calibBinningMode")]
    public string BinningCorrectionMode
    {
        get => _binningCorrectionMode;
        set => SetProperty(ref _binningCorrectionMode, CalibrationStageMode.Normalize(value));
    }

    /// <summary>
    /// Gets or sets the last raw directory selected by the operator or automation run.
    /// </summary>
    [JsonPropertyName("lastRawDir")]
    public string LastRawDirectory
    {
        get => _lastOpenedPath;
        set => SetProperty(ref _lastOpenedPath, value);
    }

    /// <summary>
    /// Gets or sets the VOI LUT window center used by the display pipeline.
    /// </summary>
    [JsonPropertyName("voiWindowCenter")]
    public float VoiWindowCenter
    {
        get => _voiWindowCenter;
        set => SetProperty(ref _voiWindowCenter, value);
    }

    /// <summary>
    /// Gets or sets the VOI LUT window width used by the display pipeline.
    /// </summary>
    [JsonPropertyName("voiWindowWidth")]
    public float VoiWindowWidth
    {
        get => _voiWindowWidth;
        set => SetProperty(ref _voiWindowWidth, Math.Max(1.0f, value));
    }

    /// <summary>
    /// Gets or sets the VOI LUT mode: Linear, LinearExact, or Sigmoid.
    /// </summary>
    [JsonPropertyName("voiLutMode")]
    public string VoiLutMode
    {
        get => _voiLutMode;
        set => SetProperty(ref _voiLutMode, string.IsNullOrWhiteSpace(value) ? "Linear" : value);
    }

    /// <summary>
    /// Gets or sets the selected body part preset for VOI LUT initialization.
    /// </summary>
    [JsonPropertyName("selectedBodyPart")]
    public string SelectedBodyPart
    {
        get => _selectedBodyPart;
        set => SetProperty(ref _selectedBodyPart, string.IsNullOrWhiteSpace(value) ? "Abdomen" : value);
    }

    /// <summary>
    /// Gets or sets whether GSDF presentation LUT calibration is requested.
    /// </summary>
    [JsonPropertyName("gsdfEnabled")]
    public bool GsdfEnabled
    {
        get => _gsdfEnabled;
        set => SetProperty(ref _gsdfEnabled, value);
    }

    /// <summary>
    /// Gets or sets the modality LUT rescale slope.
    /// </summary>
    [JsonPropertyName("modalityRescaleSlope")]
    public float ModalityRescaleSlope
    {
        get => _modalityRescaleSlope;
        set => SetProperty(ref _modalityRescaleSlope, value == 0.0f ? 1.0f : value);
    }

    /// <summary>
    /// Gets or sets the modality LUT rescale intercept.
    /// </summary>
    [JsonPropertyName("modalityRescaleIntercept")]
    public float ModalityRescaleIntercept
    {
        get => _modalityRescaleIntercept;
        set => SetProperty(ref _modalityRescaleIntercept, value);
    }

    /// <summary>
    /// Gets or sets whether the display settings panel is visible.
    /// </summary>
    [JsonPropertyName("showDisplayPanel")]
    public bool ShowDisplayPanel
    {
        get => _showDisplayPanel;
        set => SetProperty(ref _showDisplayPanel, value);
    }

    /// <summary>
    /// Gets or sets the active source-vs-processed comparison mode.
    /// </summary>
    [JsonPropertyName("comparisonMode")]
    public string ComparisonMode
    {
        get => _comparisonMode;
        set => SetProperty(ref _comparisonMode, string.IsNullOrWhiteSpace(value) ? "SwipeVertical" : value);
    }

    /// <summary>
    /// Gets or sets the absolute viewport zoom scale. A value of 0 means fit-to-view.
    /// </summary>
    [JsonPropertyName("comparisonZoomScale")]
    public double ComparisonZoomScale
    {
        get => _comparisonZoomScale;
        set => SetProperty(ref _comparisonZoomScale, Math.Clamp(value, 0.0, 16.0));
    }

    /// <summary>
    /// Gets or sets the comparison viewport horizontal pan offset in device-independent pixels.
    /// </summary>
    [JsonPropertyName("comparisonPanX")]
    public double ComparisonPanX
    {
        get => _comparisonPanX;
        set => SetProperty(ref _comparisonPanX, value);
    }

    /// <summary>
    /// Gets or sets the comparison viewport vertical pan offset in device-independent pixels.
    /// </summary>
    [JsonPropertyName("comparisonPanY")]
    public double ComparisonPanY
    {
        get => _comparisonPanY;
        set => SetProperty(ref _comparisonPanY, value);
    }

    /// <summary>
    /// Gets or sets the swipe divider position as a normalized 0..1 fraction.
    /// </summary>
    [JsonPropertyName("comparisonSwipePosition")]
    public double ComparisonSwipePosition
    {
        get => _comparisonSwipePosition;
        set => SetProperty(ref _comparisonSwipePosition, Math.Clamp(value, 0.0, 1.0));
    }

    /// <summary>
    /// Gets or sets the processed-layer opacity used by overlay mode.
    /// </summary>
    [JsonPropertyName("comparisonOverlayOpacity")]
    public double ComparisonOverlayOpacity
    {
        get => _comparisonOverlayOpacity;
        set => SetProperty(ref _comparisonOverlayOpacity, Math.Clamp(value, 0.0, 1.0));
    }

    [JsonPropertyName("laneAAlgorithm")]
    public string LaneAAlgorithm
    {
        get => _laneAAlgorithm;
        set => SetProperty(ref _laneAAlgorithm, string.IsNullOrWhiteSpace(value) ? "Production v1.2" : value);
    }

    [JsonPropertyName("laneBAlgorithm")]
    public string LaneBAlgorithm
    {
        get => _laneBAlgorithm;
        set => SetProperty(ref _laneBAlgorithm, string.IsNullOrWhiteSpace(value) ? "Candidate v1.4" : value);
    }

    [JsonPropertyName("focusMode")]
    public bool FocusMode
    {
        get => _focusMode;
        set => SetProperty(ref _focusMode, value);
    }

    [JsonPropertyName("leftPanelOpen")]
    public bool LeftPanelOpen
    {
        get => _leftPanelOpen;
        set => SetProperty(ref _leftPanelOpen, value);
    }

    [JsonPropertyName("rightPanelOpen")]
    public bool RightPanelOpen
    {
        get => _rightPanelOpen;
        set => SetProperty(ref _rightPanelOpen, value);
    }

    [JsonPropertyName("analysisTab")]
    public string AnalysisTab
    {
        get => _analysisTab;
        set => SetProperty(ref _analysisTab, string.IsNullOrWhiteSpace(value) ? "metrics" : value);
    }

    [JsonPropertyName("laneBSharpeningSigma")]
    public double LaneBSharpeningSigma
    {
        get => _laneBSharpeningSigma;
        set => SetProperty(ref _laneBSharpeningSigma, value);
    }

    [JsonPropertyName("laneBDenoiseStrength")]
    public double LaneBDenoiseStrength
    {
        get => _laneBDenoiseStrength;
        set => SetProperty(ref _laneBDenoiseStrength, value);
    }

    [JsonPropertyName("lastRunSetId")]
    public string LastRunSetId
    {
        get => _lastRunSetId;
        set => SetProperty(ref _lastRunSetId, value ?? string.Empty);
    }
}
