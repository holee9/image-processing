using System.Globalization;
using System.Text.RegularExpressions;

namespace ImageProcTest
{
    /// <summary>
    /// Turns a native alert into the text a user sees (#110, api-spec §5.17 / SRS-ALERT-007).
    ///
    /// The alert queue drops entries when it is full and reports the loss as one synthetic alert:
    /// severity <c>XPE_ALERT_ERROR</c>, message <c>"alert queue overflow: &lt;N&gt; alert(s) dropped"</c>.
    /// §5.17 requires consumers to render it like any other Error — the prefix distinguishes it, the
    /// severity does not change. This type therefore never rewrites severity; it only rewrites the
    /// message body when the prefix is present.
    ///
    /// Pure string in, string out: no native calls, no WPF, nothing to initialise. That is what lets
    /// the regression run without xpe_common.dll (the native side is QA-A-28, in flight separately).
    /// </summary>
    internal static class AlertDisplayFormatter
    {
        /// <summary>Stable message prefix of the loss alert (§5.17). Matching is ordinal and case-sensitive.</summary>
        public const string OverflowPrefix = "alert queue overflow:";

        /// <summary>First run of ASCII digits after the prefix — the dropped count.</summary>
        private static readonly Regex DroppedCount = new(@"\d+", RegexOptions.Compiled);

        /// <summary>
        /// Returns the message to display. A loss alert becomes a counted sentence; anything else is
        /// returned unchanged. Never throws: an unparseable loss alert falls back to its own raw text
        /// rather than losing the fact that alerts were dropped.
        /// </summary>
        public static string FormatMessage(string? message)
        {
            if (message is null)
            {
                return string.Empty;
            }

            if (!message.StartsWith(OverflowPrefix, StringComparison.Ordinal))
            {
                return message;
            }

            var match = DroppedCount.Match(message, OverflowPrefix.Length);
            if (!match.Success ||
                !int.TryParse(match.Value, NumberStyles.None, CultureInfo.InvariantCulture, out var dropped))
            {
                // Prefix without a usable count: the queue still lost alerts, so say so with the
                // text we were given instead of inventing a number or dropping the alert.
                return message;
            }

            return dropped == 1
                ? "1 alert was dropped because the alert queue overflowed."
                : $"{dropped} alerts were dropped because the alert queue overflowed.";
        }

        /// <summary>
        /// True when the message is the queue-loss alert. Offered so a consumer can badge it, NOT so
        /// it can be filtered out or shown at a lower severity — §5.17 requires Error rendering.
        /// </summary>
        public static bool IsOverflowAlert(string? message) =>
            message is not null && message.StartsWith(OverflowPrefix, StringComparison.Ordinal);
    }
}
