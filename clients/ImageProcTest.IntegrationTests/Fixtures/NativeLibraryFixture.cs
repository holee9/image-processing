// @MX:ANCHOR NativeLibraryFixture — single one-time native DLL resolver; all test collections depend on this.
using System.Reflection;
using System.Runtime.InteropServices;

namespace ImageProcTest.IntegrationTests.Fixtures;

/// <summary>
/// xUnit collection fixture that resolves xpe_common.dll once per test session.
/// Tests that require the native DLL must join the <see cref="NativeLibraryCollection"/>.
/// If the DLL cannot be located, <see cref="IsAvailable"/> is false and callers
/// should skip native-required assertions.
/// </summary>
public sealed class NativeLibraryFixture : IDisposable
{
    private static readonly string DllName = "xpe_common.dll";

    /// <summary>
    /// Gets a value indicating whether xpe_common.dll was successfully LOADED.
    /// File existence is not enough: the DLL links against spdlog.dll and fmt.dll, so a
    /// present-but-unloadable binary must report false — otherwise dependent tests run and
    /// fail with DllNotFoundException instead of skipping (observed in CI, #98).
    /// </summary>
    public bool IsAvailable { get; }

    /// <summary>Gets the resolved path of xpe_common.dll, or a diagnostic message when unavailable.</summary>
    public string ResolvedPath { get; }

    /// <summary>Gets the loader error message when the DLL was located but could not be loaded.</summary>
    private string LoadError { get; } = string.Empty;

    /// <summary>Gets the skip reason naming which DLL is missing, or why loading it failed.</summary>
    public string SkipReason => IsAvailable
        ? string.Empty
        : LoadError.Length > 0
            ? $"xpe_common.dll found at {ResolvedPath} but failed to load (missing dependency such as spdlog.dll or fmt.dll?): {LoadError}"
            : $"xpe_common.dll not found. Set XPE_NATIVE_DIR or build the native project first. Searched: {ResolvedPath}";

    /// <summary>Process-wide handle of the loaded DLL, reused by <see cref="Resolver"/>.</summary>
    private readonly IntPtr _handle;

    public NativeLibraryFixture()
    {
        NativeLibrary.SetDllImportResolver(typeof(NativeLibraryFixture).Assembly, Resolver);

        var (found, path) = TryLocateDll();
        ResolvedPath = path;

        if (!found)
        {
            IsAvailable = false;
            return;
        }

        // Verify it is truly x64 by checking the PE header.
        if (!VerifyX64Pe(path))
        {
            IsAvailable = false;
            ResolvedPath = $"Architecture mismatch: {path} is not x64";
            return;
        }

        // Existence and architecture are necessary but not sufficient — load it.
        try
        {
            _handle = NativeLibrary.Load(path);
            IsAvailable = true;
        }
        catch (Exception ex)
        {
            IsAvailable = false;
            LoadError = ex.Message;
        }
    }

    private static (bool found, string path) TryLocateDll()
    {
        // Priority 1: Env var override
        var envDir = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
        if (!string.IsNullOrEmpty(envDir))
        {
            var envPath = Path.Combine(envDir, DllName);
            if (File.Exists(envPath)) return (true, envPath);
        }

        // Priority 2: Test output directory (AppContext.BaseDirectory)
        var baseDir = Path.Combine(AppContext.BaseDirectory, DllName);
        if (File.Exists(baseDir)) return (true, baseDir);

        // Priority 3: Scan up to find repo root, then check known build directories
        var repoRoot = FindRepositoryRoot(AppContext.BaseDirectory);
        if (repoRoot is not null)
        {
            var candidates = new[]
            {
                Path.Combine(repoRoot, "build", "ci-common", "bin", "Debug", DllName),
                Path.Combine(repoRoot, "build", "ci-common", "bin", DllName),
                Path.Combine(repoRoot, "build", "default", "bin", "Debug", DllName),
                Path.Combine(repoRoot, "build", "default", "bin", DllName),
                Path.Combine(repoRoot, "modules", "common", "build_test", "Debug", DllName),
                Path.Combine(repoRoot, "modules", "common", "build_test", "Release", DllName),
                Path.Combine(repoRoot, "clients", "ImageProcTest", "bin", "Debug", "net8.0-windows", "x64", DllName),
            };

            foreach (var c in candidates)
            {
                if (File.Exists(c)) return (true, c);
            }

            return (false, $"Repo root found at {repoRoot} but no DLL in any build directory");
        }

        return (false, "Repository root could not be determined");
    }

    private static string? FindRepositoryRoot(string start)
    {
        var dir = new DirectoryInfo(start);
        while (dir is not null)
        {
            if (Directory.Exists(Path.Combine(dir.FullName, ".git")) ||
                Directory.Exists(Path.Combine(dir.FullName, "modules", "common")))
            {
                return dir.FullName;
            }
            dir = dir.Parent;
        }
        return null;
    }

    private static bool VerifyX64Pe(string path)
    {
        try
        {
            using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
            // PE signature: MZ header at offset 0, PE header offset at 0x3C
            var buf = new byte[0x40];
            if (fs.Read(buf, 0, buf.Length) < 0x40) return false;
            if (buf[0] != 'M' || buf[1] != 'Z') return false;
            var peOffset = BitConverter.ToInt32(buf, 0x3C);
            fs.Seek(peOffset + 4, SeekOrigin.Begin); // skip "PE\0\0"
            var machBuf = new byte[2];
            if (fs.Read(machBuf, 0, 2) < 2) return false;
            var machine = BitConverter.ToUInt16(machBuf, 0);
            return machine == 0x8664; // IMAGE_FILE_MACHINE_AMD64
        }
        catch
        {
            return false;
        }
    }

    private IntPtr Resolver(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
    {
        if (!string.Equals(libraryName, DllName, StringComparison.OrdinalIgnoreCase))
            return IntPtr.Zero;

        // Reuse the handle opened in the constructor. Zero when the DLL was never
        // loaded, which keeps IsAvailable and P/Invoke resolution on one decision.
        return _handle;
    }

    public void Dispose() { /* DLL lifetime is process-scoped; the handle is not freed */ }
}

/// <summary>xUnit collection definition that shares a single <see cref="NativeLibraryFixture"/>.</summary>
[CollectionDefinition(NativeLibraryCollection.Name)]
public sealed class NativeLibraryCollection : ICollectionFixture<NativeLibraryFixture>
{
    public const string Name = "NativeLibrary";
}
