namespace ImageProcTest.Models;

/// <summary>
/// What an algorithm option in the workbench actually runs (#173, GUI-C-114).
///
/// <para><b>These labels had no processing meaning before this.</b> They were four strings in a picker
/// that was disabled with a tooltip saying so (#182). Connecting them means deciding what each one
/// stands for, and that decision is recorded here rather than buried in a switch: a preset is a named
/// chain configuration, so choosing one changes which processing runs over the frame.</para>
///
/// <para><b>The mapping is a judgement, not a measurement.</b> The names suggest released versions of
/// an algorithm, and no such versions exist as separate code paths — there is one chain, whose stages
/// can be switched. Four presets over that chain is the honest reading available today; if the product
/// means something else by "Production v1.2", this table is where that is fixed.</para>
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
        new("Baseline v1.0", PreprocessInChain: false, GsvgModes.None),
        new("Production v1.2", PreprocessInChain: false, GsvgModes.GridSuppression),
        new("Candidate v1.4", PreprocessInChain: false, GsvgModes.VirtualGrid),
        new("Candidate v1.5-rc", PreprocessInChain: true, GsvgModes.VirtualGrid),
    ];

    /// <summary>
    /// The preset for an option name, or the first one when the name is unknown — an unknown name is a
    /// settings file from another build, and refusing to render would be a worse answer than rendering
    /// the baseline and letting the picker show what is in force.
    /// </summary>
    public static AlgorithmPreset For(string? name) =>
        All.FirstOrDefault(p => string.Equals(p.Name, name, StringComparison.Ordinal)) ?? All[0];

    /// <summary>Writes this preset's stages into a settings object.</summary>
    public void ApplyTo(AppSettings settings)
    {
        settings.PreprocessInChain = PreprocessInChain;
        settings.GsvgMode = GsvgMode;
    }
}
