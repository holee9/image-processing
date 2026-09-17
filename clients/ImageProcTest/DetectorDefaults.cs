namespace ImageProcTest
{
    /// <summary>
    /// Detector values this app fills into <c>XpeImageMetadata</c> when it has none from a file.
    ///
    /// <para>GUI-C-100 (#180): the pixel pitch is 140 µm by user decision. Four call sites here carried
    /// 0.143 mm with no source behind it, and the repository also held 0.139, 0.14 and 0.148; the value now
    /// comes from one place. The gui app takes the same value from its settings (<c>AppSettings.PixelPitchMm</c>),
    /// which this project cannot reference — the two are kept equal by this comment and the GUI-C-100 report.</para>
    /// </summary>
    internal static class DetectorDefaults
    {
        /// <summary>Detector pixel pitch in millimetres (140 µm).</summary>
        public const float PixelPitchMm = 0.14f;
    }
}
