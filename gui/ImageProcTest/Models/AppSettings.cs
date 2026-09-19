using System.Text.Json.Serialization;
using ImageProcTest.ViewModels;

namespace ImageProcTest.Models;

/// <summary>
/// Stores the persisted GUI-S0 runtime settings used by the WPF shell and test automation.
/// </summary>
public sealed class AppSettings : ObservableObject
{
    /// <summary>
    /// An independent copy of the persisted values (#171 ②).
    ///
    /// <para>The display pipeline runs on a worker thread. Handing it the live object meant a value typed
    /// during the run could be read half-way through, and the view model could not say which values
    /// produced the image it then displayed. The pipeline now gets a snapshot, and the snapshot is what
    /// the HUD reports.</para>
    /// </summary>
    public AppSettings Snapshot() =>
        System.Text.Json.JsonSerializer.Deserialize<AppSettings>(System.Text.Json.JsonSerializer.Serialize(this))
        ?? throw new InvalidOperationException("AppSettings could not be copied.");

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
    private float _voiWindowCenter = 32768.0f;
    private float _voiWindowWidth = 65535.0f;
    private string _voiLutMode = "Linear";
    private string _selectedBodyPart = "Abdomen";
    private bool _gsdfEnabled;
    private float _modalityRescaleSlope = 1.0f;
    private float _modalityRescaleIntercept = 0.0f;
    private bool _showDisplayPanel = true;
    private string _comparisonMode = ComparisonModes.Default;
    private double _comparisonZoomScale;
    private double _comparisonPanX;
    private double _comparisonPanY;
    private double _comparisonSwipePosition = 0.5;
    private double _comparisonOverlayOpacity = 0.5;
    private string _laneAAlgorithm = "Grid suppression";
    private string _laneBAlgorithm = "Grid suppression";
    private bool _focusMode;
    private bool _leftPanelOpen = true;
    private bool _rightPanelOpen = true;
    private string _analysisTab = "metrics";
    private double _laneBSharpeningSigma = 0.85;
    private double _laneBDenoiseStrength = 0.42;
    private float _laneBVoiWindowWidth;
    private string _lastRunSetId = string.Empty;
    private bool _preprocessInChain;
    private float _exposureKvp = 70.0f;
    private float _pixelPitchMm = 0.14f;
    private string _gsvgMode = GsvgModes.None;
    private string _gsvgTablePath = string.Empty;
    private double _gsvgGridRatio = 10.0;
    private double _gsvgGridFrequencyPerCm = 60.0;
    private double _gsvgAirSignal = 60000.0;
    private int _gsvgIterations = 3;
    private int _gsvgPyramidLevels = 4;
    private double _gsvgPyramidGain = 1.3;
    private double _gsvgDenoiseK = 2.0;

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
    ///
    /// <para><b>Only a supported mode can be stored (#161, GUI-C-60).</b> The setter used to reject
    /// the empty string and accept everything else, so <c>"NotAMode"</c> in <c>appsettings.json</c>
    /// survived into the running app — and the three places a user can read the mode then disagreed,
    /// because only one of them resolved it. Normalising here makes the property's value the single
    /// answer: every reader sees a supported mode or the default, never a third thing.</para>
    ///
    /// <para>The rejected text is kept in <see cref="RejectedComparisonMode"/> rather than dropped.
    /// A setting that is silently replaced is a setting the user will write again; the shell reports
    /// it by name at start-up, which is what this repository decided in #150 (refuse rather than
    /// truncate) and QA-B-60 (name the key you could not read).</para>
    /// </summary>
    [JsonPropertyName("comparisonMode")]
    public string ComparisonMode
    {
        get => _comparisonMode;
        set
        {
            RejectedComparisonMode = ComparisonModes.IsKnown(value) ? null : value;
            SetProperty(ref _comparisonMode, ComparisonModes.Normalize(value));
        }
    }

    /// <summary>
    /// The last value handed to <see cref="ComparisonMode"/> that was not a supported mode, or null.
    ///
    /// <para>Not persisted: it describes THIS load, not the settings. It is deliberately not cleared
    /// by a later valid write either — the shell reads it once at start-up, and a mode the user then
    /// picks from the menu must not erase the reason the file was wrong.</para>
    /// </summary>
    [JsonIgnore]
    public string? RejectedComparisonMode { get; private set; }

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
        set => SetProperty(ref _laneAAlgorithm, string.IsNullOrWhiteSpace(value) ? "Grid suppression" : value);
    }

    [JsonPropertyName("laneBAlgorithm")]
    public string LaneBAlgorithm
    {
        get => _laneBAlgorithm;
        set => SetProperty(ref _laneBAlgorithm, string.IsNullOrWhiteSpace(value) ? "Grid suppression" : value);
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

    /// <summary>
    /// Whether the preprocess stage (offset → gain → defect) runs in the pixel chain before the display
    /// pipeline (#180, GUI-C-99). Off by default: the display then starts from the raw frame, as before.
    /// </summary>
    [JsonPropertyName("preprocessInChain")]
    public bool PreprocessInChain
    {
        get => _preprocessInChain;
        set => SetProperty(ref _preprocessInChain, value);
    }

    /// <summary>
    /// Tube voltage of the exposure [kVp], one value for every chain stage that needs it (#180, GUI-C-99).
    /// Replaces the 70 kVp that GuiPreprocessRunner used to hard-code; 70 stays the default. Values at or
    /// below zero fall back to the default rather than reaching the native metadata.
    /// </summary>
    [JsonPropertyName("exposureKvp")]
    public float ExposureKvp
    {
        get => _exposureKvp;
        set => SetProperty(ref _exposureKvp, value > 0.0f && float.IsFinite(value) ? value : 70.0f);
    }

    /// <summary>
    /// Detector pixel pitch [mm], one value for every chain stage that needs it (GUI-C-100, user decision:
    /// 140 µm). It used to be a literal in GuiPreprocessRunner (0.14) and in the clients-side probes (0.143);
    /// a future GSVG stage passes this same value as <c>vg_pixel_pitch_mm</c>. Values outside the range the
    /// preprocess module reports for <c>pixelPitch_mm</c> (0.1–0.5 mm) fall back to the default.
    /// </summary>
    [JsonPropertyName("pixelPitchMm")]
    public float PixelPitchMm
    {
        get => _pixelPitchMm;
        set => SetProperty(ref _pixelPitchMm, value is >= 0.1f and <= 0.5f ? value : 0.14f);
    }

    /// <summary>
    /// Which GSVG correction the chain runs: None, GridSuppression or VirtualGrid (#180, GUI-C-101).
    /// The module refuses both corrections at once, so this is one choice of three rather than two switches.
    /// </summary>
    [JsonPropertyName("gsvgMode")]
    public string GsvgMode
    {
        get => _gsvgMode;
        set => SetProperty(ref _gsvgMode, GsvgModes.Normalize(value));
    }

    /// <summary>
    /// The virtual-grid parameter table. Empty means "the product table beside gsvg.dll"
    /// (GUI-C-101 decision: setting, default next to the DLL).
    /// </summary>
    [JsonPropertyName("gsvgTablePath")]
    public string GsvgTablePath
    {
        get => _gsvgTablePath;
        set => SetProperty(ref _gsvgTablePath, value ?? string.Empty);
    }

    /// <summary>Virtual-grid ratio; must be a row of the table's [grid] section (default 10).</summary>
    [JsonPropertyName("gsvgGridRatio")]
    public double GsvgGridRatio
    {
        get => _gsvgGridRatio;
        set => SetProperty(ref _gsvgGridRatio, value > 0.0 ? value : 10.0);
    }

    /// <summary>
    /// Grid line density [1/cm]; required when the table's [grid] section has a freq_per_cm column.
    /// Default 60 — the lead's assumption, with no source behind it (GUI-C-101 report).
    /// </summary>
    [JsonPropertyName("gsvgGridFrequencyPerCm")]
    public double GsvgGridFrequencyPerCm
    {
        get => _gsvgGridFrequencyPerCm;
        set => SetProperty(ref _gsvgGridFrequencyPerCm, value > 0.0 ? value : 60.0);
    }

    /// <summary>Detector signal without an object [DN] (default 60000, the module header's example).</summary>
    [JsonPropertyName("gsvgAirSignal")]
    public double GsvgAirSignal
    {
        get => _gsvgAirSignal;
        set => SetProperty(ref _gsvgAirSignal, value > 0.0 ? value : 60000.0);
    }

    /// <summary>Thickness/scatter iterations, 1..100 (default 3).</summary>
    [JsonPropertyName("gsvgIterations")]
    public int GsvgIterations
    {
        get => _gsvgIterations;
        set => SetProperty(ref _gsvgIterations, value is >= 1 and <= 100 ? value : 3);
    }

    /// <summary>
    /// Laplacian pyramid levels for the virtual grid (#180, GUI-C-104). 0 turns the pyramid and the
    /// de-noise step off entirely; 4..8 is the range REQ-GSVG-013 states and the module validates.
    /// Anything else falls back to 4.
    ///
    /// The GUI sends this key explicitly rather than relying on the module's default, so the two do not
    /// have to be changed in step — GUI-C-103 measured that omitting it left the pyramid off.
    /// </summary>
    [JsonPropertyName("gsvgPyramidLevels")]
    public int GsvgPyramidLevels
    {
        get => _gsvgPyramidLevels;
        set => SetProperty(ref _gsvgPyramidLevels, value == 0 || value is >= 4 and <= 8 ? value : 4);
    }

    /// <summary>
    /// Detail gain of the virtual grid's pyramid (#180, GUI-C-104). 1.0 leaves the detail bands as they
    /// are, and the pyramid then decomposes and rebuilds the image unchanged — measured: with levels 4
    /// and gain 1.0 the drawn pixels are byte-identical to levels 0, at ~10 ms extra cost. The levels
    /// setting is therefore only observable together with this one.
    ///
    /// Default 1.3 — the module's own default, matched on the lead's decision so the two do not drift.
    /// Range 0.1..4.0; the module requires gain 1.0 exactly when levels is 0, which is why the pyramid
    /// keys are all omitted in that case.
    /// </summary>
    [JsonPropertyName("gsvgPyramidGain")]
    public double GsvgPyramidGain
    {
        get => _gsvgPyramidGain;
        set => SetProperty(ref _gsvgPyramidGain, value is >= 0.1 and <= 4.0 ? value : 1.0);
    }

    /// <summary>
    /// Soft-threshold strength on the pyramid's finest band (#180). 0 turns the de-noise step off;
    /// larger values cut more of the finest detail. Default 2.0, the module's own default.
    ///
    /// The module rejects a config carrying a non-zero k with levels 0, so this key travels with the
    /// other two or not at all.
    /// </summary>
    [JsonPropertyName("gsvgDenoiseK")]
    public double GsvgDenoiseK
    {
        get => _gsvgDenoiseK;
        set => SetProperty(ref _gsvgDenoiseK, value is >= 0.0 and <= 10.0 ? value : 2.0);
    }

    /// <summary>
    /// The Candidate lane's VOI window width, or 0 to follow the Reference (#173, GUI-C-113).
    ///
    /// The workbench compares SETTINGS over one original, so exactly one value has to be able to differ
    /// between the lanes for the comparison to mean anything. This is that value. Zero means "no
    /// override", which is how both lanes are made to draw identically — the control case of L-01.
    /// </summary>
    [JsonPropertyName("laneBVoiWindowWidth")]
    public float LaneBVoiWindowWidth
    {
        get => _laneBVoiWindowWidth;
        set => SetProperty(ref _laneBVoiWindowWidth, value >= 0.0f ? value : 0.0f);
    }

    [JsonPropertyName("lastRunSetId")]
    public string LastRunSetId
    {
        get => _lastRunSetId;
        set => SetProperty(ref _lastRunSetId, value ?? string.Empty);
    }
}
