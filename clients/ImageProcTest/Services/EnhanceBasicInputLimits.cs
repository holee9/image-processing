using System.Globalization;

namespace ImageProcTest
{
    /// <summary>
    /// The limits of the post-processing (enhance_basic) parameter boxes, in one place. The window checks its sigma_space box with
    /// <see cref="SigmaSpaceError"/> before it runs anything, and the integration tests check these numbers against the module (GUI-C-230, GUI-C-230b, #251).
    ///
    /// <para><b>SigmaSpaceMax.</b> The bilateral filter's spatial sigma is limited to 7.5: the module truncates the kernel at 2 sigma and caps its radius at 15,
    /// so 7.5 is the largest sigma whose 2-sigma range still fits whole inside the radius cap (REQ-ENH-007/020, user decision 2026-10-03 and 2026-10-04, #251; post commit
    /// 22056f80, QA-B-210 E7). From that module version on, a larger <c>sigma_space</c> is answered with <c>XPE_ERR_INVALID_INPUT</c>. This is the ONE place the app states the
    /// number: the window and the tests read it from here, and <c>EnhanceBasicInputLimitsTests</c> compares it with the module's own statement of the bound (its header and
    /// source) so the two cannot drift apart unnoticed.</para>
    ///
    /// <para><b>What a larger value used to do.</b> Not "the same as 7.5": above 7.5 only the radius stays at 15, while the weight <c>exp(-0.5 d^2 / sigma^2)</c> keeps following
    /// sigma (at distance 10: sigma 7.5 gives 0.411, sigma 8 gives 0.458), so the results differ. That is why the app does not cut such a value to 7.5 and run: it tells the user and
    /// does not run.</para>
    /// </summary>
    internal static class EnhanceBasicInputLimits
    {
        /// <summary>The smallest bilateral spatial sigma the box passes on. The module only requires a positive value.</summary>
        public const float SigmaSpaceMin = 0.1f;

        /// <summary>The largest bilateral spatial sigma the box passes on (see the type's remarks for where the number comes from).</summary>
        public const float SigmaSpaceMax = 7.5f;

        /// <summary>
        /// The message to show when the sigma_space box holds a number outside <see cref="SigmaSpaceMin"/>..<see cref="SigmaSpaceMax"/> (a non-finite number counts as outside), otherwise null.
        /// A text that is not a number is not an error here: the box falls back to the default (see <see cref="ParseClamped"/>). The box keeps what the user typed; nothing is cut.
        /// </summary>
        public static string? SigmaSpaceError(string? text)
        {
            if (!float.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out var value))
            {
                return null;
            }

            if (value >= SigmaSpaceMin && value <= SigmaSpaceMax)
            {
                return null;
            }

            return string.Create(CultureInfo.InvariantCulture, $"Noise sigma_space must be between {SigmaSpaceMin:0.0#} and {SigmaSpaceMax:0.0#} (got {text!.Trim()}). Nothing was run; change the value and run again.");
        }

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
