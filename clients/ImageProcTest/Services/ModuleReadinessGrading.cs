namespace ImageProcTest
{
    /// <summary>
    /// The "not ready" half of module readiness, in one place (#128).
    ///
    /// Every module grades the same way at the bottom: a DLL that was not resolved — or that was
    /// resolved but failed its probe — is R0. That decision used to be a bare "R0" literal repeated
    /// in each Evaluate* branch of <see cref="ModuleReadinessService"/>, which meant it could only
    /// be checked by running the whole service, and the service reaches P/Invoke code.
    ///
    /// This type holds no state, touches no native code, and references nothing from XpeCommonApi,
    /// so a test project can link it directly and assert the grade a given discovery result yields.
    /// The upper grades (R1/R2/R3) stay in the service: they depend on per-module evidence
    /// (versions, export checklists, smoke results) that has no single shape.
    /// </summary>
    internal static class ModuleReadinessGrading
    {
        /// <summary>Grade of a module whose DLL is missing or whose probe did not pass.</summary>
        public const string NotReady = "R0";

        /// <summary>
        /// Returns <paramref name="readyLevel"/> when the probe reported readiness, otherwise
        /// <see cref="NotReady"/>.
        /// </summary>
        public static string Grade(bool probeReportedReady, string readyLevel) =>
            probeReportedReady ? readyLevel : NotReady;

        /// <summary>
        /// Grade for modules judged on discovery alone: a null path means the DLL was not found in
        /// the search order, which is R0. <paramref name="resolvedPath"/> is the locator's result.
        /// </summary>
        public static string GradeDiscovery(string? resolvedPath, string readyLevel) =>
            Grade(resolvedPath is not null, readyLevel);

        /// <summary>
        /// The ONE answer to "may this module's stages run?", read by every screen that offers or blocks them (GUI-C-212). The Evaluation tab used to ask a different question for
        /// preprocess (<c>IsExportReady</c>, which ignores the synthetic oracle) than the Diagnostics and Calibration tabs asked (<c>ProcessingEnabled</c>), so one screen said
        /// "Preprocess=ready" while another said the same module was blocked (measured on screen in GUI-C-211). A module absent from the list is not enabled.
        /// </summary>
        public static bool IsProcessingEnabled(IEnumerable<ModuleReadinessSnapshot> modules, string moduleName) =>
            modules.Any(module =>
                string.Equals(module.ModuleName, moduleName, StringComparison.OrdinalIgnoreCase) &&
                module.ProcessingEnabled);
    }
}
