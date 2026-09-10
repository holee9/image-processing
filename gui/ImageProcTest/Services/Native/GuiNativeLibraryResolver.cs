// #129 / #136: the gui app resolves native DLLs through the same policy clients uses.
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

namespace ImageProcTest.Services.Native;

/// <summary>
/// Routes this assembly's <c>DllImport</c>s through the shared search policy.
///
/// Before GUI-C-32 the gui app had plain <c>[DllImport("xpe_common.dll")]</c> declarations, so the
/// Windows loader resolved them from the application directory and nothing else. That made
/// <c>XPE_NATIVE_DIR</c> / <c>XPE_NATIVE_DIR_EXCLUSIVE</c> — the policy clients follows (#129) —
/// silently inert here: measured in GUI-C-31, where a Native E2E run with an empty XPE_NATIVE_DIR
/// still loaded DLLs from the app folder and passed. Two apps answering the same environment
/// variable differently is the trap this closes.
///
/// The candidate lists are NOT duplicated: <see cref="XpeCommonLibraryLocator"/> and
/// <see cref="NativeModuleLibraryLocator"/> are linked from clients, so the order
/// (injected directory → app directory → opt-in developer fallbacks) has one definition.
/// </summary>
internal static class GuiNativeLibraryResolver
{
    private const string CommonDll = "xpe_common.dll";
    private const string DisplayDll = "xpe_display.dll";

    private static readonly object Gate = new();
    private static bool _installed;

    /// <summary>Where each DLL was actually loaded from, for diagnostics.</summary>
    private static readonly Dictionary<string, string> Resolved =
        new(StringComparer.OrdinalIgnoreCase);

    /// <summary>
    /// Installs the resolver once. A second call is a no-op rather than an exception: the runtime
    /// allows only one resolver per assembly and throws on a re-registration
    /// (InvalidOperationException, measured in GUI-C-13).
    /// </summary>
    public static void Install()
    {
        lock (Gate)
        {
            if (_installed)
            {
                return;
            }

            NativeLibrary.SetDllImportResolver(typeof(GuiNativeLibraryResolver).Assembly, Resolve);
            _installed = true;
        }
    }

    /// <summary>The path a DLL resolved to, or null when it was never loaded through this resolver.</summary>
    public static string? ResolvedPath(string dllName)
    {
        lock (Gate)
        {
            return Resolved.TryGetValue(dllName, out var path) ? path : null;
        }
    }

    private static IntPtr Resolve(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
    {
        var candidates = CandidatesFor(libraryName);
        if (candidates is null)
        {
            return IntPtr.Zero;   // Not ours — let the runtime handle it.
        }

        foreach (var candidate in candidates)
        {
            if (!File.Exists(candidate))
            {
                continue;
            }

            NativeDependencyLoader.TryLoadFor(candidate);
            if (NativeLibrary.TryLoad(candidate, out var handle))
            {
                lock (Gate)
                {
                    Resolved[libraryName] = candidate;
                }

                return handle;
            }
        }

        // No candidate loaded. Returning Zero lets the default loader try, which keeps a missing
        // DLL a normal DllNotFoundException rather than a resolver-shaped failure — the app already
        // degrades to Mock on that path.
        return IntPtr.Zero;
    }

    /// <summary>Candidate paths for a DLL this app owns, or null for anything else.</summary>
    private static IEnumerable<string>? CandidatesFor(string libraryName) =>
        libraryName switch
        {
            _ when Is(libraryName, CommonDll) => XpeCommonLibraryLocator.GetDllCandidates(),
            _ when Is(libraryName, DisplayDll) =>
                NativeModuleLibraryLocator.GetDllCandidates(DisplayDll, "image-processing"),
            _ => null,
        };

    private static bool Is(string libraryName, string dllName) =>
        string.Equals(libraryName, dllName, StringComparison.OrdinalIgnoreCase);
}
