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
}
