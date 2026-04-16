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
}
