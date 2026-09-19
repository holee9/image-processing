namespace ImageProcTest.Models;

/// <summary>
/// What an algorithm option in the workbench actually runs (#173, GUI-C-114).
///
/// <para><b>These labels had no processing meaning before this.</b> They were four strings in a picker
/// that was disabled with a tooltip saying so (#182). Connecting them means deciding what each one
/// stands for, and that decision is recorded here rather than buried in a switch: a preset is a named
/// chain configuration, so choosing one changes which processing runs over the frame.</para>
///
/// <para><b>The names say what runs.</b> They used to be version labels — "Production v1.2",
/// "Candidate v1.4" — and that was the defect, not the mapping: no such versions exist as separate code
/// paths, so the labels asserted something no code could make true. On a medical imaging screen that
/// reads as "the released build against the candidate build" when what is actually on screen is
/// "grid suppression against virtual grid" (lead decision, GUI-C-115). With the names describing the
/// stages, this table stops being a judgement and becomes a definition.</para>
///
/// <para>If versioned pipelines ever exist as separate paths, the version names can come back — pinned
/// to actual versions rather than standing in for them.</para>
///
/// <para>The stages differ only in ways the native backend implements. The Mock backend refuses both
/// chain stages by design, so under Mock every preset draws identical pixels — which is why the case
/// that measures this (L-04) is native-only rather than quietly passing there.</para>
/// </summary>
/// <param name="Name">The option as it appears in the picker.</param>
/// <param name="PreprocessInChain">Whether the preprocess stage runs.</param>
/// <param name="GsvgMode">Which grid correction runs, from <see cref="GsvgModes"/>.</param>
public sealed record AlgorithmPreset(string Name, bool PreprocessInChain, string GsvgMode)
{
    /// <summary>The four options the picker offers, in the order it offers them.</summary>
    public static readonly AlgorithmPreset[] All =
    [
        new("No correction", PreprocessInChain: false, GsvgModes.None),
        new("Grid suppression", PreprocessInChain: false, GsvgModes.GridSuppression),
        new("Virtual grid", PreprocessInChain: false, GsvgModes.VirtualGrid),
        new("Preprocess + virtual grid", PreprocessInChain: true, GsvgModes.VirtualGrid),
    ];

    /// <summary>
    /// The version labels these presets used to carry, and what each one turned out to run.
    ///
    /// <para>Without this, renaming would silently move every stored choice: a settings file written
    /// yesterday names an option that no longer exists, <see cref="For"/> would fall back to the first
    /// preset, and someone who was looking at "Candidate v1.5-rc" would today be looking at no
    /// correction at all — with nothing on screen saying anything changed. The mapping is pinned by a
    /// test so it cannot quietly rot.</para>
    /// </summary>
    public static readonly IReadOnlyDictionary<string, string> RenamedFrom =
        new Dictionary<string, string>(StringComparer.Ordinal)
        {
            ["Baseline v1.0"] = "No correction",
            ["Production v1.2"] = "Grid suppression",
            ["Candidate v1.4"] = "Virtual grid",
            ["Candidate v1.5-rc"] = "Preprocess + virtual grid",
        };

    /// <summary>
    /// The preset for an option name, or the first one when the name is unknown — an unknown name is a
    /// settings file from another build, and refusing to render would be a worse answer than rendering
    /// the baseline and letting the picker show what is in force.
    /// </summary>
    public static AlgorithmPreset For(string? name)
    {
        var current = All.FirstOrDefault(p => string.Equals(p.Name, name, StringComparison.Ordinal));
        if (current is not null) return current;

        // A stored choice from before the rename means the same thing it always did.
        if (name is not null && RenamedFrom.TryGetValue(name, out var renamed))
        {
            return All.First(p => string.Equals(p.Name, renamed, StringComparison.Ordinal));
        }

        return All[0];
    }

    /// <summary>Writes this preset's stages into a settings object.</summary>
    public void ApplyTo(AppSettings settings)
    {
        settings.PreprocessInChain = PreprocessInChain;
        settings.GsvgMode = GsvgMode;
    }
}
