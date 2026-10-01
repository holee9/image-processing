using System.Globalization;

namespace ImageProcTest
{
    /// <summary>
    /// The pass lines of the defect (BPM) metrics, in one place, so a gate's label and its decision cannot disagree.
    /// (#218 side finding, GUI-C-182.)
    ///
    /// <para>The canonical source is <c>docs/project/Preprocessing-E2E-Automated-Evaluation-Protocol.md</c> §5.5
    /// "Defect metrics". Before GUI-C-182 the code used the lines of a second document
    /// (<c>docs/calibration/XPE-GUI-CALIB-001_Calibration_Verification_Test_GUI_Spec.md</c> §5.1.1 table) instead:
    /// recall &gt;= 95% (canonical: 100%) and good-pixel P99 &lt;= 0 ADU (canonical: &lt;= 1 ADU).</para>
    ///
    /// <para>No dependencies, so the integration tests link this file and check the values against the canonical
    /// document's text.</para>
    /// </summary>
    internal static class DefectMetricGates
    {
        /// <summary>Protocol §5.5: "synthetic BPM oracle: DefectRecall = 100%".</summary>
        public const double RecallMinPercent = 100.0;

        /// <summary>Protocol §5.5: "synthetic false-positive rate: DefectFPR &lt; 0.001%" — strictly less than.</summary>
        public const double FprMaxPercent = 0.001;

        /// <summary>Protocol §5.5: "GoodPixelDeltaP99 &lt;= 1 ADU".</summary>
        public const double GoodPixelDeltaP99MaxAdu = 1.0;

        /// <summary>
        /// §5.5 defines <c>DefectResidualADU</c> but gives it NO threshold, so it has no pass line: the row shows the
        /// value and never a PASS or REVIEW. (Before GUI-C-182 it carried a 2 ADU line copied from the GUI spec's table;
        /// that spec has since been corrected to the protocol.)
        /// </summary>
        public const string ResidualGate = "reported, no gate";

        public static string RecallGate => string.Create(CultureInfo.InvariantCulture, $">= {RecallMinPercent:0.###}%");

        public static string FprGate => string.Create(CultureInfo.InvariantCulture, $"< {FprMaxPercent:0.###}%");

        public static string GoodPixelDeltaP99Gate => string.Create(CultureInfo.InvariantCulture, $"<= {GoodPixelDeltaP99MaxAdu:0.###} ADU");

        public static bool RecallPasses(double recallPercent) => recallPercent >= RecallMinPercent;

        public static bool FprPasses(double fprPercent) => fprPercent < FprMaxPercent;

        public static bool GoodPixelDeltaP99Passes(double p99Adu) => p99Adu <= GoodPixelDeltaP99MaxAdu;
    }
}
