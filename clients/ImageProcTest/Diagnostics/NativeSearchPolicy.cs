using System;

namespace ImageProcTest
{
    /// <summary>
    /// Where the app is allowed to look for native DLLs (#129).
    ///
    /// Four search implementations own their own candidate lists (xpe_common, preprocess,
    /// enhance_basic, and the generic module locator). They keep those lists — the paths differ per
    /// module — but they all ask THIS type the two policy questions, so the answer cannot drift
    /// between modules.
    ///
    /// The default search is deliberately narrow: the directory the app runs from, plus an
    /// explicitly named directory. Repository build directories and sibling checkouts
    /// (../image-processing, ../xpe-post, ../xpe-pre) are a developer convenience that made it
    /// impossible to say which build a loaded DLL came from — unacceptable for a medical-device
    /// app — so they are now opt-in.
    /// </summary>
    internal static class NativeSearchPolicy
    {
        /// <summary>Directory to search after the app's own; unset means "skip this step".</summary>
        public const string DirectoryVariable = "XPE_NATIVE_DIR";

        /// <summary>Set to 1 to search ONLY <see cref="DirectoryVariable"/> — test isolation (#128).</summary>
        public const string ExclusiveVariable = "XPE_NATIVE_DIR_EXCLUSIVE";

        /// <summary>Set to 1 to re-enable the build-directory and sibling-checkout fallbacks (#129).</summary>
        public const string DevSearchVariable = "XPE_NATIVE_DEV_SEARCH";

        /// <summary>The directory named by <see cref="DirectoryVariable"/>, or null when unset.</summary>
        public static string? InjectedDirectory
        {
            get
            {
                var value = Environment.GetEnvironmentVariable(DirectoryVariable);
                return string.IsNullOrWhiteSpace(value) ? null : value;
            }
        }

        /// <summary>
        /// True when the search must stop right after the injected directory. Used by tests to
        /// produce a "module DLL absent" state that the fallbacks would otherwise hide.
        /// </summary>
        public static bool StopAtInjectedDirectory => IsSet(ExclusiveVariable);

        /// <summary>
        /// True when repository build directories and sibling checkouts may be searched.
        /// Off by default: a DLL found there has no recorded provenance.
        /// </summary>
        public static bool DeveloperSearchEnabled => IsSet(DevSearchVariable);

        private static bool IsSet(string variable) =>
            string.Equals(Environment.GetEnvironmentVariable(variable), "1", StringComparison.Ordinal);
    }
}
