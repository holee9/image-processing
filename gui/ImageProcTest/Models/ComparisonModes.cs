using System.Diagnostics.CodeAnalysis;

namespace ImageProcTest.Models;

/// <summary>
/// The comparison modes the viewer supports, and the one rule for turning a stored string into one
/// of them.
///
/// <para><b>Why this type exists (#161, GUI-C-60).</b> The mode was written down in three places —
/// the view model's option array, the renderer's own <c>switch</c>, and the settings property — and
/// each of the three readouts a user can see resolved it separately. With a valid mode they agreed,
/// so nothing showed; with <c>"NotAMode"</c> in <c>appsettings.json</c> they disagreed out loud: the
/// viewport's HUD said <c>SwipeVertical</c>, the automation <c>HelpText</c> said <c>NotAMode</c>, and
/// every menu check was off. <b>The bad value revealed the structure; it was not the structure.</b>
/// Everything now resolves through here.</para>
///
/// <para>It follows <see cref="CalibrationStageMode"/>, which already solved the same problem for
/// the calibration stages — a canonical list plus a <c>Normalize</c>. Same shape on purpose.</para>
///
/// <para><b>Order is not arbitrary</b>: it is the order of <c>XPE-GUI-COMPARE-001</c> §4, the
/// document that owns the mode list. <c>XPE-GUI-MENU-001</c> §9.3 names six of them, but that is a
/// shortcut table rather than an inventory — reading it as the list is the mistake GUI-C-59
/// corrected.</para>
/// </summary>
public static class ComparisonModes
{
    public const string SwipeVertical = "SwipeVertical";
    public const string SwipeHorizontal = "SwipeHorizontal";
    public const string SplitLocked = "SplitLocked";
    public const string OverlayOpacity = "OverlayOpacity";
    public const string DifferenceHeatmap = "DifferenceHeatmap";
    public const string SourceOnly = "SourceOnly";
    public const string ProcessedOnly = "ProcessedOnly";

    /// <summary>The mode used when nothing valid was asked for.</summary>
    public const string Default = SwipeVertical;

    /// <summary>Every supported mode, in XPE-GUI-COMPARE-001 §4 order.</summary>
    public static readonly string[] All =
    [
        SwipeVertical,
        SwipeHorizontal,
        SplitLocked,
        OverlayOpacity,
        DifferenceHeatmap,
        SourceOnly,
        ProcessedOnly,
    ];

    /// <summary>
    /// True when the string is one of the supported modes.
    ///
    /// Ordinal and case-sensitive: these strings are written to <c>appsettings.json</c>, compared in
    /// XAML converter parameters, and matched in the renderer's switch. A case-insensitive match here
    /// would accept <c>"sourceonly"</c>, which the XAML comparison would then fail to check — the
    /// divergence this type exists to remove, reintroduced one layer down.
    /// </summary>
    public static bool IsKnown([NotNullWhen(true)] string? mode) =>
        mode is not null && Array.IndexOf(All, mode) >= 0;

    /// <summary>The mode itself when it is known, otherwise <see cref="Default"/>.</summary>
    public static string Normalize(string? mode) =>
        IsKnown(mode) ? mode! : Default;
}
