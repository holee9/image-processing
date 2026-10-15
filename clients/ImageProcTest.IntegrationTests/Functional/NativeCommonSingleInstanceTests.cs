// #225 / #202 (GUI-C-199): the native tests of the Deterministic Baseline load their modules by full path. None of them may leave a second copy of xpe_common.dll in the process.
using System.Diagnostics;
using System.Reflection;

namespace ImageProcTest.IntegrationTests.Functional;

[Trait("Category", "Functional")]
public sealed class NativeCommonSingleInstanceTests
{
    private static readonly (Type Class, string Loader)[] NativeTestClasses =
    [
        (typeof(BaselineDicomNativeTests), "EnsureLoaded"),
        (typeof(BaselineDisplayNativeTests), "EnsureLoaded"),
        (typeof(EnhanceBasicNativeTests), "EnsureResolver"),
    ];

    private static string Read(string relative) => File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile(relative)).Replace("\r\n", "\n");

    [Fact]
    public void TheNativeTests_LoadXpeCommonThroughTheSharedLoader_NotByAPathOfTheirOwn()
    {
        foreach (var file in new[] { "BaselineDicomNativeTests.cs", "BaselineDisplayNativeTests.cs", "EnhanceBasicNativeTests.cs" })
        {
            var source = Read("clients/ImageProcTest.IntegrationTests/Functional/" + file);
            Assert.Contains("SharedCommonModule.Load(NativeDirectory!)", source, StringComparison.Ordinal);
            Assert.DoesNotContain("Path.Combine(NativeDirectory!, \"xpe_common.dll\")", source, StringComparison.Ordinal);
        }
    }

    [SkippableFact]
    public void AfterEveryNativeTestClassHasLoadedItsModules_TheProcessHoldsOneXpeCommon()
    {
        var loadedAny = false;
        foreach (var (type, loader) in NativeTestClasses)
        {
            var directory = type.GetField("NativeDirectory", BindingFlags.NonPublic | BindingFlags.Static)?.GetValue(null) as string;
            if (directory is null)
            {
                continue;   // the module is not staged here: that class's own tests skip with the reason
            }

            var method = type.GetMethod(loader, BindingFlags.NonPublic | BindingFlags.Static);
            Assert.True(method is not null, $"{type.Name}.{loader} not found: the test needs updating with the class");
            loadedAny |= (bool)method!.Invoke(null, null)!;
        }

        Skip.If(!loadedAny, "no native test class could load its modules in this environment");

        // What GainPolyClampAlertTests does when it runs: it asks for xpe_common.dll BY NAME, which returns the copy already in the process.
        System.Runtime.InteropServices.NativeLibrary.Load("xpe_common.dll", typeof(NativeCommonSingleInstanceTests).Assembly, null);

        var modules = Process.GetCurrentProcess().Modules.Cast<ProcessModule>().ToArray();
        var copies = modules
            .Where(m => string.Equals(m.ModuleName, "xpe_common.dll", StringComparison.OrdinalIgnoreCase))
            .Select(m => m.FileName)
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .ToArray();

        // Diagnostics for a red run: every module name that is loaded from more than one path (the same hazard for any xpe_* module).
        var doubled = modules.Where(m => m.ModuleName.StartsWith("xpe_", StringComparison.OrdinalIgnoreCase)
                || string.Equals(m.ModuleName, "spdlog.dll", StringComparison.OrdinalIgnoreCase)
                || string.Equals(m.ModuleName, "fmt.dll", StringComparison.OrdinalIgnoreCase))   // GUI-C-233b: the shared runtime libraries count too
            .GroupBy(m => m.ModuleName, StringComparer.OrdinalIgnoreCase)
            .Where(g => g.Select(m => m.FileName).Distinct(StringComparer.OrdinalIgnoreCase).Count() > 1)
            .Select(g => g.Key + " at " + string.Join(" and ", g.Select(m => m.FileName).Distinct(StringComparer.OrdinalIgnoreCase)))
            .ToArray();
        Assert.True(doubled.Length == 0, "a module is loaded from two paths: " + string.Join(" ; ", doubled)
            + ". Usual cause (GUI-C-233b): copies of xpe_common/xpe_preprocess/spdlog/fmt left in the test output folder by a build WITHOUT XPE_NATIVE_DIR, next to a run WITH it. The build now removes them when XPE_NATIVE_DIR is set; rebuild with the variable set.");

        Assert.True(copies.Length == 1, "more than one copy of xpe_common.dll is loaded, so the alert queue is split: " + string.Join(" ; ", copies));
    }
}
