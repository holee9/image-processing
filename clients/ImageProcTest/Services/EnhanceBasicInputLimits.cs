using System.Globalization;

namespace ImageProcTest
{
    /// <summary>
    /// The limits of the post-processing (enhance_basic) parameter boxes, in one place. The window reads its boxes through
    /// <see cref="ParseClamped"/> with these numbers, and the integration tests check them against the module (GUI-C-230, #251).
    ///
    /// <para><b>SigmaSpaceMax.</b> The bilateral filter's spatial sigma is limited to 7.5: the module truncates the kernel at 2 sigma with a
    /// radius cap of 15, so 7.5 is the largest value that cap reflects (REQ-ENH-007/020, user decision 2026-10-03 "set a sigma limit and
    /// reject above it", #251; post commit 22056f80, QA-B-210 E7). From that module version on, a larger <c>sigma_space</c> is answered with
    /// <c>XPE_ERR_INVALID_INPUT</c>. This is the ONE place the app states the number: the window and the tests read it from here, and
    /// <c>EnhanceBasicInputLimitsTests</c> compares it with the module's own statement of the bound (its header and source) so the two cannot
    /// drift apart unnoticed.</para>
    ///
    /// <para>Before GUI-C-230 the box accepted up to 100. A value above 7.5 never did what it said (the radius cap made it behave like 7.5),
    /// so limiting the box to 7.5 keeps what such an input did and stops it from reaching the module as an invalid value.</para>
    /// </summary>
    internal static class EnhanceBasicInputLimits
    {
        /// <summary>The smallest bilateral spatial sigma the box passes on. The module only requires a positive value.</summary>
        public const float SigmaSpaceMin = 0.1f;

        /// <summary>The largest bilateral spatial sigma the box passes on (see the type's remarks for where the number comes from).</summary>
        public const float SigmaSpaceMax = 7.5f;

        /// <summary>
        /// Reads a number from a box's text: the fallback when the text is not a number, otherwise the number limited to
        /// <paramref name="min"/>..<paramref name="max"/>. The window's <c>ReadFloat</c> is this method, so its behaviour can be tested without a window.
        /// </summary>
        public static float ParseClamped(string? text, float fallback, float min, float max)
        {
            if (!float.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out var value))
            {
                return fallback;
            }

            return Math.Clamp(value, min, max);
        }
    }
}
