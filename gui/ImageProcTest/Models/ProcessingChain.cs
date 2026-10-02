namespace ImageProcTest.Models;

// #180 / #173 (GUI-C-99): the GUI's pixel chain. Contract B from GUI-C-97 — the backend runs an ordered
// list of stages on the loaded raw frame, keeps the raw frame untouched, and returns what each stage did.

/// <summary>What a requested stage did. The four values keep "asked for" apart from "done" (HAZ-GUI-005).</summary>
public enum StageStatus
{
    /// <summary>The stage was in the list but switched off.</summary>
    NotRequested,

    /// <summary>The stage ran and produced pixels that differ from its input.</summary>
    Applied,

    /// <summary>The stage ran and returned pixels equal to its input.</summary>
    AppliedNoChange,

    /// <summary>The stage was asked for and did not run, or failed; the chain fell back to its input.</summary>
    RequestedNotApplied,
}

/// <summary>Stage identifiers. Strings, so a list can be written down and compared in reports.</summary>
public static class StageIds
{
    /// <summary>Phase-1a preprocess: offset → gain → defect (xpe_preprocess.dll).</summary>
    public const string Preprocess = "preprocess";

    /// <summary>Grid shadow suppression or the virtual grid (gsvg.dll), after preprocess (#180, GUI-C-101).</summary>
    public const string Gsvg = "gsvg";

    /// <summary>AI bone suppression (xpe_ai.dll, worker path), after gsvg and before the display pipeline (#225 row 10, GUI-C-184).</summary>
    public const string AiBoneSuppression = "ai_bone_suppress";

    /// <summary>
    /// Phase-1b basic enhancement as ONE stage (#225 row 9, GUI-C-196): log transform, noise reduction, contrast (CLAHE) and edge enhancement (USM) run
    /// in float inside the stage and are converted to 16 bits once, at its end (xpe_enhance_basic.dll). Part of the Deterministic Baseline's fixed stage
    /// list only; the ordinary Apply does not run it.
    /// </summary>
    public const string EnhanceBasic = "enhance_basic";
}

/// <summary>
/// The GSVG correction the chain runs. The module refuses grid suppression and the virtual grid
/// together (gsvg_api.h §init), so the GUI offers one of three (#180, GUI-C-101).
/// </summary>
public static class GsvgModes
{
    public const string None = "None";
    public const string GridSuppression = "GridSuppression";
    public const string VirtualGrid = "VirtualGrid";

    public static readonly string[] All = [None, GridSuppression, VirtualGrid];

    /// <summary>An unknown or empty value becomes <see cref="None"/> — the pass-through the module defaults to.</summary>
    public static string Normalize(string? mode) =>
        All.FirstOrDefault(m => string.Equals(m, mode, StringComparison.Ordinal)) ?? None;

    public static bool IsKnown(string? mode) => All.Contains(mode, StringComparer.Ordinal);
}

/// <summary>One entry of the chain, in order.</summary>
public sealed record StageRequest(string StageId, bool Enabled);

/// <summary>What one stage did.</summary>
/// <param name="StageId">The stage, see <see cref="StageIds"/>.</param>
/// <param name="Status">What the stage did.</param>
/// <param name="Pixels">The stage's output, a NEW array, when it ran; null otherwise.</param>
/// <param name="Reason">Why a requested stage did not apply, or the stage's own summary when it did.</param>
/// <param name="ElapsedMs">
/// Wall time the stage itself took, measured around the backend call (#180, GUI-C-102). Zero for a
/// stage that was not requested. It is the module's time plus this app's marshalling, and NOT the time
/// to get the result on screen — the display pipeline and the render follow it.
/// </param>
/// <param name="NonFiniteCount">NaN/Inf values the stage counted in its float intermediates (#225 row 9, GUI-C-196 M6); 0 when it counted none.</param>
public sealed record StageOutcome(string StageId, StageStatus Status, ushort[]? Pixels, string Reason, double ElapsedMs = 0.0, long NonFiniteCount = 0);

/// <summary>The chain's result. <see cref="Raw"/> is the loaded frame's array, never written to.</summary>
public sealed record ChainResult(ushort[] Raw, IReadOnlyList<StageOutcome> Stages)
{
    /// <summary>What the display pipeline starts from: the last stage that produced pixels, else the raw frame.</summary>
    public ushort[] DisplayInput => Stages.LastOrDefault(s => s.Pixels is not null)?.Pixels ?? Raw;

    /// <summary>True when the display input is the raw frame itself.</summary>
    public bool DisplaysRaw => ReferenceEquals(DisplayInput, Raw);

    /// <summary>One line for the status bar and the reports, e.g. <c>chain: preprocess=RequestedNotApplied</c>.</summary>
    public string Summary =>
        Stages.Count == 0
            ? "chain: (empty)"
            : "chain: " + string.Join(", ", Stages.Select(s => $"{s.StageId}={s.Status}"));

    /// <summary>Per-stage timing, e.g. <c>times: preprocess=0 ms, gsvg=512 ms</c> (#180, GUI-C-102).</summary>
    public string Timings =>
        Stages.Count == 0
            ? "times: (empty)"
            : "times: " + string.Join(", ", Stages.Select(s => $"{s.StageId}={s.ElapsedMs:0} ms"));

    /// <summary>An empty chain over <paramref name="raw"/> — the display starts from the raw frame.</summary>
    public static ChainResult Empty(ushort[] raw) => new(raw, Array.Empty<StageOutcome>());
}
