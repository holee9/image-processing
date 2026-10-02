// #225 row 9 (GUI-C-196 M4): what a backend must offer for the Deterministic Baseline. Separate from IXpeBackend on purpose: the DICOM session type is internal,
// and a backend that does not implement this (the Mock) simply cannot run the command, with no change to the other implementers.
using ImageProcTest.Models;

namespace ImageProcTest.Services;

internal interface IBaselineBackend
{
    /// <summary>True when every module the baseline needs is available to this backend (native only).</summary>
    bool SupportsDeterministicBaseline { get; }

    /// <summary>
    /// One execution of the baseline: the fixed chain (<see cref="ProcessingChainPlan.BuildBaselineStages"/>) with the exposure index measured, then the fixed
    /// display pipeline. Reads no user display setting. When the chain does not apply every stage the display is not run and <c>Output</c> is empty.
    /// </summary>
    BaselineSingleRun RunBaselineOnce(LoadedImageFrame rawFrame, AppSettings settings);

    /// <summary>The DICOM module's session for the baseline's export.</summary>
    IDicomSession CreateBaselineDicomSession();
}
