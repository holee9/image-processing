// GUI-C-213 (#249): every C# binding of a native module is compared with that module's public header.
using Xunit.Abstractions;
using static ImageProcTest.IntegrationTests.Functional.ModuleSignatureParity;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// GUI-C-208 compared the three C# copies of xpe_common with xpe_common's headers. GUI-C-212 then found the legacy oracle calling xpe_preprocess's correction functions with a shape the header
/// never had, and the comparison could not have caught it: it read one module and only static <c>[DllImport]</c> externs. This one reads every module header
/// (<c>modules/*/include</c>) and every C# source of the app, the legacy diagnostic app and the test mirrors, and checks two kinds of binding:
/// <list type="bullet">
/// <item>static <c>[DllImport]</c> externs, against the header of the DLL they name;</item>
/// <item>delegates that are bound to an export by name (<c>GetRequiredDelegate&lt;T&gt;(handle, "export")</c>, <c>TryGetDelegate(handle, "export", out T v)</c>, <c>TryGetExport</c> +
/// <c>GetDelegateForFunctionPointer&lt;T&gt;</c>) — the style the diagnostics, the wrappers and most test mirrors use, and the style of the oracle.</item>
/// </list>
/// What is compared and how (names, counts, kinds, ref/out/in, const, string marshalling, bool width, calling convention, struct layout by computed offsets and size, enum values) is in
/// <see cref="ModuleSignatureParity"/>. A header or a C# declaration the engine cannot read is a finding. A declaration present on one side only must be in the allow-list below WITH a reason.
/// </summary>
public sealed class NativeModuleSignatureParityTests(ITestOutputHelper output)
{
    private static readonly (string Header, string Dll)[] Headers =
    [
        ("modules/common/include/xpe/common/xpe_common_api.h", "xpe_common.dll"),
        ("modules/common/include/xpe/common/xpe_error.h", "xpe_common.dll"),
        ("modules/common/include/xpe/common/xpe_memory.h", "xpe_common.dll"),
        ("modules/common/include/xpe/common/xpe_types.h", "xpe_common.dll"),
        ("modules/preprocess/include/xpe/preprocess_api.h", "xpe_preprocess.dll"),
        ("modules/enhance_basic/include/xpe/enhance_basic/enhance_basic_api.h", "xpe_enhance_basic.dll"),
        ("modules/enhance_advanced/include/xpe/enhance_advanced/xpe_enhance_advanced_api.h", "xpe_enhance_advanced.dll"),
        ("modules/display/include/xpe/display/display_api.h", "xpe_display.dll"),
        ("modules/dicom/include/xpe/dicom/dicom_api.h", "xpe_dicom.dll"),
        ("modules/gsvg/include/xpe/gsvg/gsvg_api.h", "gsvg.dll"),
        ("modules/ai/include/xpe/ai/ai_api.h", "xpe_ai.dll"),
    ];

    /// <summary>Sources scanned. The parity tests and the engine are excluded: they contain C# and header text as DATA (controls), which is not a binding.</summary>
    private static readonly string[] SourceRoots = ["gui/ImageProcTest", "clients/ImageProcTest", "clients/ImageProcTest.IntegrationTests"];

    private static readonly string[] ExcludedFiles =
    [
        "clients/ImageProcTest.IntegrationTests/Functional/ModuleSignatureParity.cs",
        "clients/ImageProcTest.IntegrationTests/Functional/NativeModuleSignatureParityTests.cs",
        "clients/ImageProcTest.IntegrationTests/Functional/ModuleSignatureParityControlTests.cs",
        "clients/ImageProcTest.IntegrationTests/Functional/NativeSignatureParityTests.cs",
        // Imports GetFileVersionInfoSizeW from a copy of version.dll renamed xpe_x86_decoy.dll on purpose (GUI-C-209): a loader experiment, not a binding of a module.
        "clients/ImageProcTest.IntegrationTests/Smoke/ArchitectureMismatchTests.cs",
    ];

    /// <summary>DLLs a <c>[DllImport]</c> may name that are not ours (Win32). Anything else not in <see cref="Headers"/> is a finding.</summary>
    private static readonly HashSet<string> ForeignDlls = new(StringComparer.OrdinalIgnoreCase) { "user32.dll", "kernel32.dll", "gdi32.dll", "dwmapi.dll", "shell32.dll", "ntdll.dll" };

    /// <summary>A C# extern whose name the module's header does not have: (file, name) → why that is fine. Empty on purpose; entries are added with a reason or the finding is fixed.</summary>
    private static readonly Dictionary<string, string> CsOnlyExterns = new(StringComparer.Ordinal);

    /// <summary>A delegate nothing binds by name, or an export literal no header has: (file|name) → why that is fine.</summary>
    private static readonly Dictionary<string, string> Unresolved = new(StringComparer.Ordinal)
    {
        ["gui/ImageProcTest/Services/NativeAlertDrain.cs|ReadAlert"] = "a managed seam for tests (reads one alert), not a native function pointer",
        ["gui/ImageProcTest/Services/ProcessingChainRunner.cs|StageExecutor"] = "a managed callback the chain runner is given, not a native function pointer",
        ["clients/ImageProcTest/PInvokeWrappers/XpeDicomWrapper.cs|ReadImageDelegate"] = "declared and referenced nowhere in the scanned sources (dead declaration)",
        ["clients/ImageProcTest/PInvokeWrappers/XpeDicomWrapper.cs|GetMetadataDelegate"] = "declared and referenced nowhere in the scanned sources (dead declaration)",
        ["clients/ImageProcTest/PInvokeWrappers/XpeDicomWrapper.cs|WriteJ2kDelegate"] = "declared and referenced nowhere in the scanned sources (dead declaration)",
        ["clients/ImageProcTest/PInvokeWrappers/XpeDicomWrapper.cs|CStoreDelegate"] = "declared and referenced nowhere in the scanned sources (dead declaration)",
        ["clients/ImageProcTest/PInvokeWrappers/XpeDicomWrapper.cs|CFindMwlDelegate"] = "declared and referenced nowhere in the scanned sources (dead declaration)",
        ["clients/ImageProcTest/PInvokeWrappers/XpeDicomWrapper.cs|RawPointer3Delegate"] = "declared and referenced nowhere in the scanned sources (dead declaration)",
        ["clients/ImageProcTest/PInvokeWrappers/XpeDicomWrapper.cs|RawPointerUIntDelegate"] = "declared and referenced nowhere in the scanned sources (dead declaration)",
        ["clients/ImageProcTest/PInvokeWrappers/XpeDicomWrapper.cs|RawCStoreDelegate"] = "declared and referenced nowhere in the scanned sources (dead declaration)",
        ["clients/ImageProcTest/PInvokeWrappers/XpeDicomWrapper.cs|RawCFindMwlDelegate"] = "declared and referenced nowhere in the scanned sources (dead declaration)",
        ["clients/ImageProcTest/PInvokeWrappers/XpeEnhanceBasicWrapper.cs|LogInverseDelegate"] = "declared and referenced nowhere in the scanned sources (dead declaration)",
    };

    /// <summary>A C# enum that deliberately carries fewer values than the header's: (name|file) → why. gui passes only UInt16 and Float32 to xpe_alloc_image (the same allowance GUI-C-208 made for xpe_common).</summary>
    private static readonly Dictionary<string, string> EnumSubsets = new(StringComparer.Ordinal)
    {
        ["XpePixelFormatNative|gui/ImageProcTest/Services/Native/XpeDisplayInterop.cs"] = "gui hands xpe_alloc_image only UInt16 and Float32; the header's XPE_PIXEL_UINT8 = 2 is not bound there",
    };

    private sealed record Scan(NativeModel Native, CsModel Cs, List<(string File, string Source)> Sources, Dictionary<string, string> DllOfHeader);

    private static readonly Lazy<Scan> Cached = new(Load);

    private static string Root()
    {
        for (var dir = new DirectoryInfo(AppContext.BaseDirectory); dir is not null; dir = dir.Parent)
        {
            if (Directory.Exists(Path.Combine(dir.FullName, "modules", "common"))) return dir.FullName;
        }

        Assert.Fail("the repository root (modules/common) was not found above " + AppContext.BaseDirectory);
        return string.Empty;
    }

    private static Scan Load()
    {
        var root = Root();
        var native = ParseHeaders(Headers.Select(h => (h.Header, File.ReadAllText(Path.Combine(root, h.Header)))));
        var cs = new CsModel();
        var sources = new List<(string, string)>();
        foreach (var rel in SourceRoots)
        {
            foreach (var file in Directory.EnumerateFiles(Path.Combine(root, rel), "*.cs", SearchOption.AllDirectories))
            {
                var relative = Path.GetRelativePath(root, file).Replace('\\', '/');
                if (relative.Contains("/obj/", StringComparison.Ordinal) || relative.Contains("/bin/", StringComparison.Ordinal) || ExcludedFiles.Contains(relative)) continue;
                sources.Add((relative, File.ReadAllText(file)));
            }
        }

        foreach (var (file, text) in sources) AddSource(cs, file, text);
        ResolveBindings(cs, sources);
        return new Scan(native, cs, sources, Headers.ToDictionary(h => h.Header, h => h.Dll));
    }

    /// <summary>
    /// The exported function names a header's text declares: for each export marker, the identifier just before the first opening parenthesis after it. Deliberately not the engine's code (its own
    /// comment stripping and declaration pattern are what is being checked against).
    /// </summary>
    private static HashSet<string> ExportedNamesIn(string text)
    {
        var plain = System.Text.RegularExpressions.Regex.Replace(text, @"/\*[\s\S]*?\*/", " ");
        plain = System.Text.RegularExpressions.Regex.Replace(plain, @"//[^\n]*", " ");
        plain = System.Text.RegularExpressions.Regex.Replace(plain, @"^[ \t]*#[^\n]*(\\\r?\n[^\n]*)*", " ", System.Text.RegularExpressions.RegexOptions.Multiline);   // the marker's own macro definition (with its continuation lines) is not a declaration
        var names = new HashSet<string>(StringComparer.Ordinal);
        foreach (System.Text.RegularExpressions.Match m in System.Text.RegularExpressions.Regex.Matches(plain, @"\bXPE_API\b[^;(]*?(\w+)\s*\("))
        {
            names.Add(m.Groups[1].Value);
        }

        return names;
    }

    // ----------------------------------------------------------------------------------------------------------------------------- the tests

    [Fact]
    public void TheHeaders_AreReadable_AndHoldTheDeclarationsTheyHold()
    {
        var scan = Cached.Value;
        Assert.True(scan.Native.Problems.Count == 0, "the headers contain something this checker cannot read:" + Environment.NewLine + "  " + string.Join(Environment.NewLine + "  ", scan.Native.Problems));
        var perHeader = scan.Native.Functions.Values.GroupBy(f => f.Header).ToDictionary(g => g.Key, g => g.Count());
        foreach (var (header, count) in perHeader.OrderBy(p => p.Key)) output.WriteLine($"{count,3} functions  {header}");

        // What this guards (GUI-C-223): that the parser READ each header, because "a parser that silently found nothing would otherwise make every comparison below pass". It used to pin the number of
        // functions per header, which is a different thing: the number moves whenever a module's owner adds a declaration (xpe_dicom_version made dicom 11 and turned main red from another lane),
        // and a number written next to the parser's own output is only as independent as the person who last copied it. The independent derivation is the header text itself: the names that follow
        // each export marker, found with a regular expression that shares nothing with the parser's. The two must be the same set, header by header, and a difference names the functions.
        foreach (var (header, _) in Headers)
        {
            var text = File.ReadAllText(Path.Combine(Root(), header));
            var declared = ExportedNamesIn(text);
            var parsed = scan.Native.Functions.Values.Where(f => f.Header == header).Select(f => f.Name).ToHashSet(StringComparer.Ordinal);
            output.WriteLine($"{declared.Count,3} exported names in the text  {header}");
            var unread = declared.Except(parsed).OrderBy(n => n, StringComparer.Ordinal).ToList();
            var invented = parsed.Except(declared).OrderBy(n => n, StringComparer.Ordinal).ToList();
            Assert.True(unread.Count == 0 && invented.Count == 0,
                $"{header}: the text declares {declared.Count} exported function(s), the parser read {parsed.Count}. " +
                $"Declared but NOT read: [{string.Join(", ", unread)}]. Read but not declared in the text: [{string.Join(", ", invented)}].");
        }

        // Controls: the independent derivation itself must see something, or "both empty" would be an agreement. These modules have always had exports.
        foreach (var header in new[] { "modules/preprocess/include/xpe/preprocess_api.h", "modules/dicom/include/xpe/dicom/dicom_api.h", "modules/ai/include/xpe/ai/ai_api.h" })
        {
            Assert.True(perHeader.GetValueOrDefault(header) > 0, $"{header}: no function was read at all.");
        }

        Assert.NotEmpty(scan.Native.Functions.Values.Where(f => scan.DllOfHeader[f.Header] == "xpe_common.dll"));
        Assert.Contains("XpeCalibQualityMeta", scan.Native.Structs.Keys);   // control: a struct with arrays and pointers is read
        Assert.Contains("XpeGainSemantics", scan.Native.Enums.Keys);
        Assert.Contains("XpeDicomHandle", scan.Native.Handles);
    }

    [Fact]
    public void Census_OfTheBindings_IsNotEmpty()
    {
        var scan = Cached.Value;
        var ours = scan.Cs.Externs.Where(e => !ForeignDlls.Contains(e.Dll)).ToList();
        foreach (var g in ours.GroupBy(e => e.Dll, StringComparer.OrdinalIgnoreCase).OrderBy(g => g.Key)) output.WriteLine($"{g.Count(),3} static extern  {g.Key}  ({string.Join(", ", g.Select(e => Path.GetFileName(e.File)).Distinct())})");
        output.WriteLine($"{scan.Cs.Externs.Count - ours.Count,3} static extern  foreign (Win32)");
        foreach (var g in scan.Cs.Bindings.Select(b => (b, f: scan.Native.Functions.GetValueOrDefault(b.Export))).GroupBy(x => x.f is null ? "(no header function)" : scan.DllOfHeader[x.f.Header]).OrderBy(g => g.Key))
        {
            output.WriteLine($"{g.Select(x => x.b.Export).Distinct().Count(),3} exports bound through delegates  {g.Key}");
        }

        var boundNames = scan.Cs.Externs.Select(e => e.Name).Concat(scan.Cs.Bindings.Select(b => b.Export)).ToHashSet(StringComparer.Ordinal);
        foreach (var g in scan.Native.Functions.Values.GroupBy(f => f.Header).OrderBy(g => g.Key))
        {
            var unbound = g.Where(f => !boundNames.Contains(f.Name)).Select(f => f.Name).OrderBy(n => n, StringComparer.Ordinal).ToList();
            output.WriteLine($"{g.Count() - unbound.Count,3}/{g.Count(),-3} header functions have a C# binding  {g.Key}{(unbound.Count == 0 ? "" : "  unbound: " + string.Join(", ", unbound))}");
        }

        output.WriteLine($"{scan.Cs.Delegates.Count,3} delegate types, {scan.Cs.Bindings.Count} binding sites, {scan.Cs.Structs.Values.Sum(l => l.Count)} struct declarations, {scan.Cs.Enums.Values.Sum(l => l.Count)} enum declarations, {scan.Sources.Count} files scanned");
        Assert.True(ours.Count >= 30, $"expected the app's static externs, found {ours.Count}");
        Assert.True(scan.Cs.Bindings.Count >= 40, $"expected the delegate bindings, found {scan.Cs.Bindings.Count}");
        Assert.True(scan.Cs.Problems.Count == 0, "the C# sources contain a binding this checker cannot read:" + Environment.NewLine + "  " + string.Join(Environment.NewLine + "  ", scan.Cs.Problems));
    }

    [Fact]
    public void EveryStaticExtern_SaysWhatItsModulesHeaderSays()
    {
        var scan = Cached.Value;
        var problems = CheckExterns(scan.Native, scan.Cs, DllHeaders, ForeignDlls, CsOnlyExterns);
        Report(WithoutAllowedEnumSubsets(problems), "static [DllImport] externs");
    }

    [Fact]
    public void EveryDelegateBoundToAnExport_SaysWhatThatExportsHeaderSays()
    {
        var scan = Cached.Value;
        Report(CheckDelegates(scan.Native, scan.Cs, Unresolved), "delegates bound to exports");
    }

    private static IReadOnlySet<string>? DllHeaders(string dll)
    {
        var own = Headers.Where(h => h.Dll.Equals(dll, StringComparison.OrdinalIgnoreCase)).Select(h => h.Header).ToHashSet();
        return own.Count == 0 ? null : own;
    }

    private static List<string> WithoutAllowedEnumSubsets(List<string> problems)
    {
        var kept = new List<string>(problems);
        foreach (var key in EnumSubsets.Keys)
        {
            var (name, file) = (key.Split('|')[0], key.Split('|')[1]);
            var removed = kept.RemoveAll(p => p.Contains($"enum {name} ({file}) has values", StringComparison.Ordinal));
            if (removed == 0) kept.Add($"the enum allow-list names {key}, which no longer differs from the header: remove the entry");
        }

        return kept;
    }

    private void Report(List<string> problems, string what)
    {
        foreach (var p in problems) output.WriteLine(p);
        Assert.True(problems.Count == 0, $"{what} differ from the native headers ({problems.Count}):{Environment.NewLine}  " + string.Join(Environment.NewLine + "  ", problems));
    }
}
