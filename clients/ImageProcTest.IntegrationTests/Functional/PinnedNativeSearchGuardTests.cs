// GUI-C-243 (Codex #195, #267): with the native search pinned (XPE_NATIVE_DIR_EXCLUSIVE=1) the modules that load LATE (DICOM export, AI start) and the DLLs a module imports are held to the pinned folder too.
// The behaviour that can be driven without the DICOM / AI modules is tested by the launch-path E2E and the SelfCheck runner; what cannot (those two features need real data and a model) is pinned here by the
// shape of the source, the way NativeCallWrapperGuardTests pins the alert drain: removing the call turns this red.
using System.Text.RegularExpressions;

namespace ImageProcTest.IntegrationTests.Functional;

[Trait("Category", "Functional")]
public sealed class PinnedNativeSearchGuardTests
{
    private static string Read(string relativePath)
    {
        var native = relativePath.Replace('/', Path.DirectorySeparatorChar);
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null)
        {
            var candidate = Path.Combine(dir.FullName, native);
            if (File.Exists(candidate)) return File.ReadAllText(candidate);
            dir = dir.Parent;
        }

        Assert.Fail($"Could not locate {relativePath} by walking up from {AppContext.BaseDirectory}.");
        return string.Empty;
    }

    [Fact]
    public void TheDicomExport_FailsOnAModuleOutsideThePinnedFolder_RightAfterTheCallsThatLoadTheModule()
    {
        // GUI-C-245: the audit is IN FRONT of every call into the module (the behaviour is run by SelfCheck scenario 11 with a stray module; this pins the order in the source), and after the calls too
        var source = Read("gui/ImageProcTest/Services/Native/GuiDicomNative.cs");
        foreach (var (feature, call) in new[] { ("xpe_dicom_write", "XpeDicomNative.xpe_dicom_write("), ("xpe_dicom_validate", "XpeDicomNative.xpe_dicom_validate("), ("xpe_dicom_open", "XpeDicomNative.xpe_dicom_open(") })
        {
            var preflight = source.IndexOf($"Preflight(\"{feature}\")", StringComparison.Ordinal);
            var first = source.IndexOf(call, StringComparison.Ordinal);
            Assert.True(preflight > 0 && first > preflight, $"{feature} must be preceded by Preflight(\"{feature}\"): the modules are checked BEFORE the call, not only after it");
        }

        var write = source.IndexOf("XpeDicomNative.xpe_dicom_write(", StringComparison.Ordinal);
        Assert.True(source.IndexOf("_auditLoadedModules(\"xpe_dicom_write\")", write, StringComparison.Ordinal) > write, "xpe_dicom_write must also be followed by the audit of the loaded modules");
        var open = source.IndexOf("XpeDicomNative.xpe_dicom_open(", StringComparison.Ordinal);
        var tryAt = source.IndexOf("try", open, StringComparison.Ordinal);
        var auditAfterOpen = source.IndexOf("_auditLoadedModules(\"xpe_dicom_open\")", tryAt, StringComparison.Ordinal);
        var finallyAt = auditAfterOpen < 0 ? -1 : source.IndexOf("finally", auditAfterOpen, StringComparison.Ordinal);
        Assert.True(tryAt > open && auditAfterOpen > tryAt && finallyAt > auditAfterOpen, "the audit after xpe_dicom_open must be inside the try whose finally gives the handle back");
    }

    [Fact]
    public void TheAiStart_FailsOnAModuleOrWorkerOutsideThePinnedFolder_BeforeXpeAiInitIsCalled()
    {
        var source = Read("gui/ImageProcTest/Services/Native/GuiAiRunner.cs");
        var probe = source.IndexOf("XpeAiNative.xpe_ai_worker_state(out var probeState", StringComparison.Ordinal);
        var moduleAudit = source.IndexOf("NativeModuleAudit.ThrowIfOutsidePinnedFolder(\"AI start\")", StringComparison.Ordinal);
        var workerAudit = source.IndexOf("NativeModuleAudit.DescribeAndCheckAiWorker(", StringComparison.Ordinal);
        var init = source.IndexOf("XpeAiNative.xpe_ai_init(directory, config)", StringComparison.Ordinal);
        Assert.True(probe > 0 && moduleAudit > probe && workerAudit > moduleAudit && init > workerAudit,
            "the AI start must audit the loaded modules and the worker's path after the call that loads xpe_ai.dll and BEFORE xpe_ai_init (order: worker_state probe, module audit, worker check, init)");
    }

    [Fact]
    public void TheDllsAModuleImports_AreLoadedWithTheSameRestrictedSearch_WhenPinned()
    {
        var loader = Read("clients/ImageProcTest/Diagnostics/NativeDependencyLoader.cs");
        Assert.Matches(new Regex(@"StopAtInjectedDirectory\s*\?\s*OracleModuleConfinement\.TryLoad\(dependencyPath", RegexOptions.Singleline), loader);

        var backend = Read("gui/ImageProcTest/Services/RealXpeBackend.cs");
        Assert.Matches(new Regex(@"StopAtInjectedDirectory\s*\?\s*OracleModuleConfinement\.TryLoad\(dllPath", RegexOptions.Singleline), backend);

        var resolver = Read("gui/ImageProcTest/Services/Native/GuiNativeLibraryResolver.cs");
        Assert.Matches(new Regex(@"pinned\s*\?\s*OracleModuleConfinement\.TryLoad\(candidate", RegexOptions.Singleline), resolver);
    }
}
