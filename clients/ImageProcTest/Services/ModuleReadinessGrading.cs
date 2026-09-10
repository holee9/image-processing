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
    }
}
