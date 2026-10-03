// GUI-C-219d (#249, Codex #116 finding 1): the oracle worker loads the DLLs the preprocess DLL imports from the SNAPSHOT folder, not from the folder of the executable.
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Legacy;

/// <summary>
/// Windows resolves the DLLs a loaded DLL imports BY NAME, and the folder of the process's executable comes first in that search. The oracle gives the worker a private copy of the preprocess DLL
/// and its dependencies (what was hashed is what is judged), so a same-named DLL beside the executable must not be the one that gets loaded.
///
/// Here the executable is a copy of the legacy app with the run's native DLLs placed BESIDE it (the decoys), and the snapshot is a separate folder holding the same set. The worker is the real
/// one, started as the app starts it. It passes only if every module that shares a name with a file of the snapshot was loaded from the snapshot folder: the worker enumerates its loaded modules
/// and reports a failed verdict ("module audit failed") naming any that came from elsewhere. Falsification (kept in the GUI-C-219d report): with the confined load switched off, the same run
/// fails the audit and names the decoys.
/// </summary>
[Trait("Category", "LegacyE2E")]
public sealed class LegacyOracleConfinementScenarios(ITestOutputHelper output)
{
    [SkippableFact]
    public void TheWorker_LoadsItsDependenciesFromTheSnapshotFolder_NotFromTheFolderOfItsExecutable()
    {
        var dll = LegacyOracleIsolationScenarios.Dll();
        Skip.If(dll is null, "XPE_NATIVE_DIR does not name a folder containing xpe_preprocess.dll.");
        var exe = LegacyOracleIsolationScenarios.FindExecutable();
        Skip.If(exe is null, "clients/ImageProcTest/bin/Debug/net8.0-windows/ImageProcTest.exe was not found: build clients/ImageProcTest first.");
        LegacyOracleIsolationScenarios.AssertFresh(exe!);

        var nativeDir = Path.GetDirectoryName(dll!)!;
        var root = Path.Combine(Path.GetTempPath(), $"xpe-c219d-confine-{Guid.NewGuid():N}");
        var appDir = Path.Combine(root, "app");
        var snapDir = Path.Combine(root, "snap");
        try
        {
            Directory.CreateDirectory(appDir);
            Directory.CreateDirectory(snapDir);
            foreach (var file in Directory.GetFiles(Path.GetDirectoryName(exe!)!))
            {
                File.Copy(file, Path.Combine(appDir, Path.GetFileName(file)));
            }

            var natives = Directory.GetFiles(nativeDir, "*.dll");
            foreach (var native in natives)
            {
                File.Copy(native, Path.Combine(appDir, Path.GetFileName(native)), overwrite: true);   // the decoys, beside the executable
                File.Copy(native, Path.Combine(snapDir, Path.GetFileName(native)));                  // the snapshot
            }

            Assert.True(File.Exists(Path.Combine(appDir, "xpe_common.dll")) && File.Exists(Path.Combine(snapDir, "xpe_common.dll")),
                "the set-up has no xpe_common.dll in both places, so it cannot tell the two apart");

            var verdict = XpePreprocessOracleProcess.Run(Path.Combine(appDir, Path.GetFileName(exe!)), [XpePreprocessOracleProcess.ModeArgument, Path.Combine(snapDir, "xpe_preprocess.dll")], TimeSpan.FromSeconds(90));

            output.WriteLine($"verdict {verdict.Status}: {verdict.Details}");
            Assert.DoesNotContain("module audit", verdict.Status, StringComparison.OrdinalIgnoreCase);
            Assert.True(verdict.Passed, $"{verdict.Status}: {verdict.Details}");
        }
        finally
        {
            try { Directory.Delete(root, recursive: true); } catch (IOException) { /* temp folder */ }
        }
    }
}
