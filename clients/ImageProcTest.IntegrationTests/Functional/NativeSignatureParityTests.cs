// #249 (GUI-C-208 M1): every C# copy of the xpe_common P/Invoke declarations says what the native header says.
using System.Text.RegularExpressions;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// SPEC-XPE-GUI-IT §9 says the tests call the app's own <c>XpeCommonApi</c> declarations, "no shim". They cannot (GUI-C-13: one assembly cannot hold two DllImport resolvers), so the test project
/// carries a MIRROR (<c>PInvoke/XpeCommonNative.cs</c>), and the gui app a third copy (<c>XpeDisplayInterop.cs</c>). <see cref="DllNameParityTests"/> compares the DLL name and
/// <see cref="ErrorCodeHeaderParityTests"/> the error codes; nothing compared the FUNCTIONS. This does, from source, against the native headers:
/// for every <c>XPE_API</c> declaration — name, parameter count, each parameter's type (pointer or value, integer width and sign, struct, string), return type, calling convention, char set —
/// and the two structs and the pixel-format enum the declarations use.
///
/// <para><b>Types are mapped, not pattern-matched.</b> A native type or a C# type this class has no row for is a FAILURE listing the type, never a skip: a new type needs one table row on
/// purpose (<see cref="NativeKinds"/>, <see cref="CSharpKinds"/>).</para>
///
/// <para><b>One-sided declarations are asserted as lists.</b> A header declaration a copy does not bind, or a copy's declaration the header does not have, must be named in that copy's
/// allow-list with its reason; a stale entry fails too.</para>
/// </summary>
public sealed class NativeSignatureParityTests
{
    private static readonly string[] Headers =
    [
        "modules/common/include/xpe/common/xpe_common_api.h",
        "modules/common/include/xpe/common/xpe_error.h",
        "modules/common/include/xpe/common/xpe_memory.h",
    ];

    private const string TypesHeader = "modules/common/include/xpe/common/xpe_types.h";

    /// <summary>A copy of the declarations. <c>HeaderOnly</c> are the header declarations it deliberately does not bind (name → reason); a copy with <c>Complete = false</c> binds a subset.</summary>
    private sealed record Copy(string Path, bool Complete, Dictionary<string, string> HeaderOnly, Dictionary<string, string> StructAndEnumSubsets);

    private static readonly Dictionary<string, Copy> Copies = new()
    {
        ["mirror"] = new("clients/ImageProcTest.IntegrationTests/PInvoke/XpeCommonNative.cs", true, [], []),
        ["app"] = new("clients/ImageProcTest/PInvokeWrapper.cs", true, new()
        {
            ["xpe_alert_push"] = "the producer side of the alert queue: the app only reads alerts; only tests push one (AlertQueueJunctionTests drives overflow with it)",
        }, []),
        ["gui"] = new("gui/ImageProcTest/Services/Native/XpeDisplayInterop.cs", false, [], new()
        {
            ["XpePixelFormatNative"] = "gui passes only UInt16 and Float32 to xpe_alloc_image; the header's XPE_PIXEL_UINT8 = 2 is not bound there",
        }),
    };

    public static IEnumerable<object[]> CopyNames() => Copies.Keys.Select(k => new object[] { k });

    // ----------------------------------------------------------------------------------------------------------------------------- the tests

    [Theory]
    [MemberData(nameof(CopyNames))]
    public void EveryDeclaration_SaysWhatTheHeaderSays(string copyName)
    {
        var problems = Compare(copyName, ReadHeaders(), Source(Copies[copyName].Path));
        Assert.True(problems.Count == 0, $"{copyName} ({Copies[copyName].Path}) differs from the native headers:{Environment.NewLine}  " + string.Join(Environment.NewLine + "  ", problems));
    }

    /// <summary>The header has 16 declarations; the count itself is held so a parser that silently finds none cannot make the rest pass.</summary>
    [Fact]
    public void TheHeaders_YieldTheDeclarationsTheyHold()
    {
        var decls = ParseHeaderFunctions(ReadHeaders(), out var unknown);
        Assert.Empty(unknown);
        Assert.Equal(16, decls.Count);
        Assert.Contains("xpe_get_pending_alert", decls.Keys);   // control: a multi-parameter declaration with a char* and a size_t
        Assert.Contains("xpe_alert_push", decls.Keys);          // control: the one the app wrapper does not bind
        Assert.Equal(4, decls["xpe_get_pending_alert"].Params.Count);
    }

    // ----------------------------------------------------------------------------------------------------------------------------- controls: the checker detects what it claims to

    [Fact]
    public void Control_AChangedParameterType_IsReported()
    {
        var source = Source(Copies["mirror"].Path);
        var changed = source.Replace("public static extern XpeErrorCode xpe_log_set_level(int level);", "public static extern XpeErrorCode xpe_log_set_level(uint level);", StringComparison.Ordinal);
        Assert.NotEqual(source, changed);
        var problems = Compare("mirror", ReadHeaders(), changed);
        Assert.Contains(problems, p => p.Contains("xpe_log_set_level", StringComparison.Ordinal) && p.Contains("level", StringComparison.Ordinal));
    }

    [Fact]
    public void Control_ADroppedParameter_AndAChangedReturnType_AreReported()
    {
        var source = Source(Copies["mirror"].Path);
        var dropped = source.Replace("out float defaultValue);", ");", StringComparison.Ordinal).Replace("out float maxValue,", "out float maxValue", StringComparison.Ordinal);
        Assert.NotEqual(source, dropped);
        Assert.Contains(Compare("mirror", ReadHeaders(), dropped), p => p.Contains("xpe_get_param_range", StringComparison.Ordinal) && p.Contains("parameter", StringComparison.Ordinal));

        var returns = source.Replace("public static extern int xpe_get_pending_alert_count();", "public static extern uint xpe_get_pending_alert_count();", StringComparison.Ordinal);
        Assert.Contains(Compare("mirror", ReadHeaders(), returns), p => p.Contains("xpe_get_pending_alert_count", StringComparison.Ordinal) && p.Contains("return", StringComparison.Ordinal));
    }

    [Fact]
    public void Control_ACallingConventionAndAWrongModifier_AreReported()
    {
        var source = Source(Copies["mirror"].Path);
        var cdecl = source.Replace("[DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]\n    public static extern void xpe_clear_alerts();",
            "[DllImport(DllName, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi)]\n    public static extern void xpe_clear_alerts();", StringComparison.Ordinal)
            .Replace("[DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]\r\n    public static extern void xpe_clear_alerts();",
            "[DllImport(DllName, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi)]\r\n    public static extern void xpe_clear_alerts();", StringComparison.Ordinal);
        Assert.NotEqual(source, cdecl);
        Assert.Contains(Compare("mirror", ReadHeaders(), cdecl), p => p.Contains("xpe_clear_alerts", StringComparison.Ordinal) && p.Contains("calling convention", StringComparison.Ordinal));

        var modifier = source.Replace("xpe_free_image(ref XpeImageBuffer buffer)", "xpe_free_image(XpeImageBuffer buffer)", StringComparison.Ordinal);
        Assert.NotEqual(source, modifier);
        Assert.Contains(Compare("mirror", ReadHeaders(), modifier), p => p.Contains("xpe_free_image", StringComparison.Ordinal));
    }

    [Fact]
    public void Control_AnUnknownNativeType_AndAnUnknownCSharpType_AreFailuresNotSkips()
    {
        ParseHeaderFunctions("XPE_API FooHandle* xpe_made_up(BarKind kind, int32_t n);", out var unknownNative);
        Assert.Contains(unknownNative, u => u.Contains("FooHandle", StringComparison.Ordinal));
        Assert.Contains(unknownNative, u => u.Contains("BarKind", StringComparison.Ordinal));

        var source = Source(Copies["mirror"].Path);
        var odd = source.Replace("xpe_log_set_level(int level)", "xpe_log_set_level(Int128 level)", StringComparison.Ordinal);
        Assert.Contains(Compare("mirror", ReadHeaders(), odd), p => p.Contains("Int128", StringComparison.Ordinal) && p.Contains("unknown", StringComparison.OrdinalIgnoreCase));
    }

    [Fact]
    public void Control_AOneSidedDeclaration_MustBeNamedInTheAllowList()
    {
        var source = Source(Copies["mirror"].Path);
        var extra = source.Replace("    #endregion\n\n}", "    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]\n    public static extern void xpe_not_in_the_header();\n    #endregion\n\n}", StringComparison.Ordinal)
            .Replace("    #endregion\r\n\r\n}", "    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]\r\n    public static extern void xpe_not_in_the_header();\r\n    #endregion\r\n\r\n}", StringComparison.Ordinal);
        Assert.NotEqual(source, extra);
        Assert.Contains(Compare("mirror", ReadHeaders(), extra), p => p.Contains("xpe_not_in_the_header", StringComparison.Ordinal));

        // The app wrapper's allowance for xpe_alert_push is real: with the header's declaration removed from the allow-list's view, the app copy would report it.
        var withoutAllowance = Compare("app", ReadHeaders(), Source(Copies["app"].Path), ignoreAllowList: true);
        Assert.Contains(withoutAllowance, p => p.Contains("xpe_alert_push", StringComparison.Ordinal));
    }

    // ----------------------------------------------------------------------------------------------------------------------------- the comparison

    private static List<string> Compare(string copyName, string headerText, string csSource, bool ignoreAllowList = false)
    {
        var copy = Copies[copyName];
        var problems = new List<string>();
        var native = ParseHeaderFunctions(headerText, out var unknownNative);
        problems.AddRange(unknownNative.Select(u => "unknown native type: " + u));

        var cs = ParseExterns(csSource, out var unknownCs).Where(e => e.Dll.Equals("xpe_common.dll", StringComparison.OrdinalIgnoreCase)).ToList();
        problems.AddRange(unknownCs.Select(u => "unknown C# type: " + u));
        var types = ParseCSharpTypes(csSource);

        // One-sided lists.
        var csNames = cs.Select(e => e.Name).ToList();
        foreach (var dup in csNames.GroupBy(n => n).Where(g => g.Count() > 1)) problems.Add($"{dup.Key} is declared {dup.Count()} times");
        foreach (var name in csNames.Except(native.Keys)) problems.Add($"{name} is declared here but the header has no such function");
        var headerOnly = native.Keys.Except(csNames).OrderBy(n => n, StringComparer.Ordinal).ToList();
        if (copy.Complete)
        {
            var allowed = ignoreAllowList ? new Dictionary<string, string>() : copy.HeaderOnly;
            foreach (var name in headerOnly.Where(n => !allowed.ContainsKey(n))) problems.Add($"{name} is in the header but this copy does not bind it (and it is not in the allow-list)");
            foreach (var name in allowed.Keys.Where(n => !headerOnly.Contains(n))) problems.Add($"the allow-list names {name} but this copy binds it or the header dropped it: remove the entry");
        }

        foreach (var e in cs.Where(e => native.ContainsKey(e.Name)))
        {
            problems.AddRange(CompareFunction(native[e.Name], e, types));
        }

        problems.AddRange(CompareStructsAndEnums(copy, headerText: ReadTypesHeader(), types));
        return problems;
    }

    private static IEnumerable<string> CompareFunction(NativeFunction n, CsExtern c, CsTypes types)
    {
        var who = n.Name;
        if (!c.CallingConvention.Equals("Cdecl", StringComparison.Ordinal)) yield return $"{who}: calling convention is {c.CallingConvention}, the header's functions are cdecl (no __stdcall)";
        if (!c.CharSet.Equals("Ansi", StringComparison.Ordinal) && (c.Params.Any(p => p.Type is "string" or "StringBuilder") || c.Params.Count > 0 && n.Params.Any(p => p.Type.Base == "char")))
        {
            yield return $"{who}: char set is {c.CharSet}, the header's strings are char* (ANSI)";
        }

        foreach (var problem in MatchReturn(n.Ret, c.Ret, types)) yield return $"{who}: return {problem}";
        if (n.Params.Count != c.Params.Count)
        {
            yield return $"{who}: the header has {n.Params.Count} parameter(s) ({string.Join(", ", n.Params.Select(p => p.Text))}), this copy has {c.Params.Count} ({string.Join(", ", c.Params.Select(p => p.Text))})";
            yield break;
        }

        for (var i = 0; i < n.Params.Count; i++)
        {
            foreach (var problem in MatchParam(n.Params[i], c.Params[i], c, types)) yield return $"{who}: parameter {i + 1} '{c.Params[i].Name}' (header '{n.Params[i].Name}'): {problem}";
        }
    }

    private static IEnumerable<string> MatchReturn(NativeType n, string cRet, CsTypes types)
    {
        var kind = CsKind(cRet, types);
        if (kind is null) { yield return $"unknown C# type '{cRet}'"; yield break; }
        if (n.Base == "void" && n.Pointers == 0) { if (kind != "void") yield return $"is {cRet}, the header returns void"; yield break; }
        if (n.Base == "char" && n.Pointers == 1 && n.IsConst) { if (kind != "ptr") yield return $"is {cRet}, the header returns const char* (an IntPtr the caller must not free)"; yield break; }
        if (n.Pointers == 0 && ValueKinds.Contains(NativeKinds[n.Base])) { if (!ValueMatches(NativeKinds[n.Base], kind)) yield return $"is {cRet} ({kind}), the header returns {n} ({NativeKinds[n.Base]})"; yield break; }
        yield return $"the header's return type {n} has no C# counterpart in this checker";
    }

    private static IEnumerable<string> MatchParam(NativeParam n, CsParam c, CsExtern owner, CsTypes types)
    {
        var kind = CsKind(c.Type, types);
        if (kind is null) { yield return $"unknown C# type '{c.Type}'"; yield break; }
        var t = n.Type;
        var nativeKind = NativeKinds[t.Base];

        if (t.Pointers == 0)
        {
            if (c.Modifier.Length > 0) yield return $"has the modifier '{c.Modifier}', the header passes by value";
            if (!ValueKinds.Contains(nativeKind)) yield return $"the header passes {t} by value; this checker has no C# counterpart for that";
            else if (!ValueMatches(nativeKind, kind)) yield return $"is {c.Type} ({kind}), the header has {t} ({nativeKind})";
            yield break;
        }

        if (t.Pointers == 1 && t.Base == "char")
        {
            if (t.IsConst)
            {
                if (c.Type != "string") yield return $"is {c.Type}, the header has const char* (a string)";
                else if (c.MarshalAs is not null && !c.MarshalAs.Contains("LPStr", StringComparison.Ordinal)) yield return $"marshals as {c.MarshalAs}, the header's const char* is an ANSI string (LPStr)";
                else if (c.MarshalAs is null && owner.CharSet != "Ansi") yield return "is a string without [MarshalAs(LPStr)] and the DllImport is not CharSet.Ansi";
                if (c.Modifier.Length > 0) yield return $"has the modifier '{c.Modifier}', the header passes a const char*";
            }
            else if (c.Type != "StringBuilder") yield return $"is {c.Type}, the header has a writable char* buffer (StringBuilder)";
            yield break;
        }

        if (t.Pointers == 1 && t.Base == "void") { if (kind != "ptr") yield return $"is {c.Type}, the header has void*"; yield break; }

        if (t.Pointers == 1 && ValueKinds.Contains(nativeKind))
        {
            if (c.Modifier is not ("out" or "ref")) yield return $"has no out/ref modifier, the header has {t} (a pointer to a scalar)";
            if (!ValueMatches(nativeKind, kind)) yield return $"points to {c.Type} ({kind}), the header points to {t} ({nativeKind})";
            if (t.IsConst && c.Modifier == "out") yield return "is 'out', the header's pointer is const";
            yield break;
        }

        if (t.Pointers == 1 && nativeKind.StartsWith("struct:", StringComparison.Ordinal))
        {
            var structName = nativeKind["struct:".Length..];
            if (!StructAliases(structName).Contains(c.Type)) yield return $"is {c.Type}, the header has {t} (struct {structName}; accepted C# names: {string.Join(", ", StructAliases(structName))})";
            var allowed = t.IsConst ? new[] { "ref", "in" } : new[] { "ref", "out" };
            if (!allowed.Contains(c.Modifier)) yield return $"has the modifier '{(c.Modifier.Length == 0 ? "(none)" : c.Modifier)}', the header's {(t.IsConst ? "const " : "")}pointer needs {string.Join(" or ", allowed)}";
            yield break;
        }

        yield return $"the header's parameter type {t} has no C# counterpart in this checker";
    }

    private static IEnumerable<string> CompareStructsAndEnums(Copy copy, string headerText, CsTypes types)
    {
        var stripped = StripComments(headerText);

        // structs
        foreach (Match m in Regex.Matches(stripped, @"typedef\s+struct\s+(?<tag>\w+)\s*\{(?<body>.*?)\}\s*(?<name>\w+)\s*;", RegexOptions.Singleline))
        {
            var name = m.Groups["name"].Value;
            var fields = m.Groups["body"].Value.Split(';', StringSplitOptions.RemoveEmptyEntries).Select(f => f.Trim()).Where(f => f.Length > 0).ToList();
            foreach (var alias in StructAliases(name))
            {
                if (!types.Structs.TryGetValue(alias, out var cs)) continue;
                if (!cs.Layout.Contains("Sequential", StringComparison.Ordinal) || !Regex.IsMatch(cs.Layout, @"Pack\s*=\s*8\b")) yield return $"struct {alias}: [StructLayout] is '{cs.Layout}', the header is #pragma pack(8) sequential";
                if (fields.Count != cs.Fields.Count) { yield return $"struct {alias}: the header's {name} has {fields.Count} field(s), this has {cs.Fields.Count}"; continue; }
                for (var i = 0; i < fields.Count; i++)
                {
                    foreach (var p in MatchField(fields[i], cs.Fields[i], types, cs.Layout)) yield return $"struct {alias} field {i + 1} '{cs.Fields[i].Name}': {p}";
                }
            }
        }

        // enums
        foreach (Match m in Regex.Matches(stripped, @"typedef\s+enum\s+(?<tag>\w+)\s*\{(?<body>.*?)\}\s*(?<name>\w+)\s*;", RegexOptions.Singleline))
        {
            var name = m.Groups["name"].Value;
            if (name != "XpePixelFormat") continue;   // the other header enum (XpeAlertSeverity) is not part of any declaration
            var header = m.Groups["body"].Value.Split(',', StringSplitOptions.RemoveEmptyEntries).Select(x => Regex.Match(x, @"(?<n>\w+)\s*=\s*(?<v>-?\d+)")).Where(x => x.Success).Select(x => long.Parse(x.Groups["v"].Value, System.Globalization.CultureInfo.InvariantCulture)).ToList();
            foreach (var alias in new[] { "XpePixelFormat", "XpePixelFormatNative" })
            {
                if (!types.Enums.TryGetValue(alias, out var cs)) continue;
                var subsetAllowed = copy.StructAndEnumSubsets.ContainsKey(alias);
                var missing = header.Except(cs.Values).ToList();
                var extra = cs.Values.Except(header).ToList();
                if (extra.Count > 0) yield return $"enum {alias}: has value(s) {string.Join(", ", extra)} the header's XpePixelFormat does not";
                if (missing.Count > 0 && !subsetAllowed) yield return $"enum {alias}: lacks value(s) {string.Join(", ", missing)} the header's XpePixelFormat has";
                if (missing.Count == 0 && subsetAllowed) yield return $"enum {alias}: is complete now: remove its subset allowance";
                if (cs.Underlying is not ("int" or "uint")) yield return $"enum {alias}: underlying type {cs.Underlying} is not 4 bytes (the header's enum is 4 bytes)";
            }
        }
    }

    private static IEnumerable<string> MatchField(string nativeField, CsField c, CsTypes types, string layout)
    {
        var arr = Regex.Match(nativeField, @"^(?<type>.+?)\s+(?<name>\w+)\s*\[(?<n>\d+)\]$");
        string typeText, name;
        int? length = null;
        if (arr.Success) { typeText = arr.Groups["type"].Value; name = arr.Groups["name"].Value; length = int.Parse(arr.Groups["n"].Value, System.Globalization.CultureInfo.InvariantCulture); }
        else
        {
            var m = Regex.Match(nativeField, @"^(?<type>.+?)\s*(?<name>\w+)$");
            typeText = m.Groups["type"].Value.Trim(); name = m.Groups["name"].Value;
        }

        var t = ParseNativeType(typeText);
        if (!NativeKinds.TryGetValue(t.Base, out var nativeKind)) { yield return $"unknown native type '{t.Base}' (field '{name}')"; yield break; }
        if (!Norm(name).Equals(Norm(c.Name), StringComparison.Ordinal)) yield return $"is named differently from the header's '{name}'";
        var kind = CsKind(c.Type, types);
        if (kind is null) { yield return $"unknown C# type '{c.Type}'"; yield break; }

        if (length is not null)
        {
            if (t.Base != "char" || c.Type != "string" || c.MarshalAs is null || !c.MarshalAs.Contains("ByValTStr", StringComparison.Ordinal) || !Regex.IsMatch(c.MarshalAs, $@"SizeConst\s*=\s*{length}\b")) yield return $"the header's char[{length}] needs [MarshalAs(ByValTStr, SizeConst = {length})] string; this is {c.Type} {c.MarshalAs}";
            if (!layout.Contains("Ansi", StringComparison.Ordinal)) yield return "the struct is not CharSet.Ansi but holds a char[] string";
            yield break;
        }

        if (t.Pointers == 1 && t.Base == "void") { if (kind != "ptr") yield return $"is {c.Type}, the header has void*"; yield break; }
        if (t.Pointers == 0 && ValueKinds.Contains(nativeKind)) { if (!ValueMatches(nativeKind, kind)) yield return $"is {c.Type} ({kind}), the header has {t} ({nativeKind})"; yield break; }
        yield return $"the header's field type {t} has no C# counterpart in this checker";
    }

    // ----------------------------------------------------------------------------------------------------------------------------- the type tables (a type without a row is a failure)

    private static readonly HashSet<string> ValueKinds = ["i32", "u32", "u64", "usize", "f32", "enum4"];

    /// <summary>Native base type → kind. <c>XpeErrorCode</c> is <c>typedef int32_t</c> (xpe_error.h:36), so it is an i32.</summary>
    private static readonly Dictionary<string, string> NativeKinds = new(StringComparer.Ordinal)
    {
        ["void"] = "void", ["char"] = "char", ["int32_t"] = "i32", ["uint32_t"] = "u32", ["uint64_t"] = "u64", ["size_t"] = "usize", ["float"] = "f32",
        ["XpeErrorCode"] = "i32", ["XpePixelFormat"] = "enum4", ["XpeImageBuffer"] = "struct:XpeImageBuffer",
    };

    /// <summary>C# type name → kind. Types declared in the file (enums and structs) are resolved from the file itself.</summary>
    private static readonly Dictionary<string, string> CSharpKinds = new(StringComparer.Ordinal)
    {
        ["void"] = "void", ["int"] = "i32", ["uint"] = "u32", ["ulong"] = "u64", ["float"] = "f32", ["IntPtr"] = "ptr", ["UIntPtr"] = "usize", ["nuint"] = "usize",
        ["string"] = "string", ["StringBuilder"] = "stringbuilder",
    };

    private static string[] StructAliases(string nativeName) => nativeName == "XpeImageBuffer" ? ["XpeImageBuffer", "XpeImageBufferNative"] : [nativeName];

    private static string? CsKind(string type, CsTypes types)
    {
        if (CSharpKinds.TryGetValue(type, out var k)) return k;
        if (types.Enums.TryGetValue(type, out var e)) return e.Underlying == "int" ? "enum:i32" : e.Underlying == "uint" ? "enum:u32" : null;
        if (types.Structs.ContainsKey(type)) return "struct:" + type;
        return null;
    }

    /// <summary>A native value kind against a C# kind. An enum matches its 4-byte underlying integer as well as itself (a C# copy that spells the typedef'd int32_t as int is the same ABI).</summary>
    private static bool ValueMatches(string nativeKind, string csKind) => nativeKind switch
    {
        "i32" => csKind is "i32" or "enum:i32",
        "u32" => csKind is "u32" or "enum:u32",
        "u64" => csKind == "u64",
        "usize" => csKind == "usize",
        "f32" => csKind == "f32",
        "enum4" => csKind is "enum:i32" or "enum:u32" or "i32" or "u32",
        _ => false,
    };

    // ----------------------------------------------------------------------------------------------------------------------------- parsing

    private sealed record NativeType(string Base, int Pointers, bool IsConst)
    {
        public override string ToString() => (IsConst ? "const " : "") + Base + new string('*', Pointers);
    }

    private sealed record NativeParam(NativeType Type, string Name) { public string Text => $"{Type} {Name}"; }
    private sealed record NativeFunction(string Name, NativeType Ret, List<NativeParam> Params);
    private sealed record CsParam(string Modifier, string Type, string Name, string? MarshalAs) { public string Text => $"{(Modifier.Length > 0 ? Modifier + " " : "")}{Type} {Name}"; }
    private sealed record CsExtern(string Name, string Ret, string Dll, string CallingConvention, string CharSet, List<CsParam> Params);
    private sealed record CsField(string Type, string Name, string? MarshalAs);
    private sealed record CsStruct(string Layout, List<CsField> Fields);
    private sealed record CsEnum(string Underlying, List<long> Values);
    private sealed record CsTypes(Dictionary<string, CsStruct> Structs, Dictionary<string, CsEnum> Enums);

    private static string Norm(string s) => s.Replace("_", string.Empty, StringComparison.Ordinal).ToLowerInvariant();

    private static string StripComments(string text) => Regex.Replace(Regex.Replace(text, @"/\*.*?\*/", string.Empty, RegexOptions.Singleline), @"//[^\r\n]*", string.Empty);

    private static NativeType ParseNativeType(string text)
    {
        var pointers = text.Count(ch => ch == '*');
        var tokens = text.Replace("*", " ", StringComparison.Ordinal).Split([' ', '\t', '\r', '\n'], StringSplitOptions.RemoveEmptyEntries).ToList();
        var isConst = tokens.Remove("const");
        return new NativeType(string.Join(" ", tokens), pointers, isConst);
    }

    private static Dictionary<string, NativeFunction> ParseHeaderFunctions(string headerText, out List<string> unknown)
    {
        unknown = [];
        var result = new Dictionary<string, NativeFunction>(StringComparer.Ordinal);
        var stripped = StripComments(headerText);
        foreach (Match m in Regex.Matches(stripped, @"\bXPE_API\b\s+(?<decl>[^;{}]+);"))
        {
            var decl = Regex.Replace(m.Groups["decl"].Value, @"\s+", " ").Trim();
            var open = decl.IndexOf('(');
            var close = decl.LastIndexOf(')');
            if (open < 0 || close < open) { unknown.Add($"unparseable declaration '{decl}'"); continue; }
            var head = decl[..open].Trim();
            var nameMatch = Regex.Match(head, @"^(?<ret>.+?)(?<name>\w+)$");
            if (!nameMatch.Success) { unknown.Add($"unparseable declaration head '{head}'"); continue; }
            var name = nameMatch.Groups["name"].Value;
            var ret = ParseNativeType(nameMatch.Groups["ret"].Value);
            var args = decl[(open + 1)..close].Trim();
            var parameters = new List<NativeParam>();
            if (args.Length > 0 && args != "void")
            {
                foreach (var part in args.Split(','))
                {
                    var pm = Regex.Match(part.Trim(), @"^(?<type>.+?)\s*(?<name>\w+)$");
                    if (!pm.Success) { unknown.Add($"unparseable parameter '{part.Trim()}' of {name}"); continue; }
                    parameters.Add(new NativeParam(ParseNativeType(pm.Groups["type"].Value), pm.Groups["name"].Value));
                }
            }

            foreach (var t in parameters.Select(p => p.Type).Append(ret).Where(t => !NativeKinds.ContainsKey(t.Base)).Select(t => t.Base).Distinct())
            {
                unknown.Add($"'{t}' in {name}");
            }

            result[name] = new NativeFunction(name, ret, parameters);
        }

        return result;
    }

    private static List<CsExtern> ParseExterns(string source, out List<string> unknown)
    {
        unknown = [];
        var result = new List<CsExtern>();
        var constDll = Regex.Match(source, @"const\s+string\s+DllName\s*=\s*""(?<v>[^""]+)""").Groups["v"].Value;
        var types = ParseCSharpTypes(source);
        foreach (Match m in Regex.Matches(source, @"\[DllImport\("))
        {
            var attrStart = m.Index + "[DllImport(".Length;
            var attrEnd = MatchingClose(source, attrStart - 1, '(', ')');
            var attrArgs = source[attrStart..attrEnd];
            var after = source[attrEnd..];
            var ext = Regex.Match(after, @"^\)\]\s*(?:\[[^\]]*\]\s*)*(?:public|internal|private)?\s*static\s+extern\s+(?<ret>[\w.<>?]+)\s+(?<name>\w+)\s*\(");
            if (!ext.Success) { unknown.Add($"a [DllImport] whose declaration this checker cannot read, near: {Regex.Replace(after[..Math.Min(80, after.Length)], @"\s+", " ")}"); continue; }
            var paramsStart = attrEnd + ext.Length;
            var paramsEnd = MatchingClose(source, paramsStart - 1, '(', ')');
            var dll = Regex.Match(attrArgs, @"^\s*""(?<v>[^""]+)""").Success ? Regex.Match(attrArgs, @"^\s*""(?<v>[^""]+)""").Groups["v"].Value : constDll;
            var cc = Regex.Match(attrArgs, @"CallingConvention\s*=\s*CallingConvention\.(?<v>\w+)").Groups["v"].Value;
            var cs = Regex.Match(attrArgs, @"CharSet\s*=\s*CharSet\.(?<v>\w+)").Groups["v"].Value;
            var parameters = SplitTopLevel(source[paramsStart..paramsEnd]).Select(ParseCsParam).ToList();
            var ret = Strip(ext.Groups["ret"].Value);
            foreach (var t in parameters.Select(p => p.Type).Append(ret).Where(t => CsKind(t, types) is null).Distinct()) unknown.Add($"'{t}' in {ext.Groups["name"].Value}");
            result.Add(new CsExtern(ext.Groups["name"].Value, ret, dll, cc.Length == 0 ? "(default)" : cc, cs.Length == 0 ? "(default)" : cs, parameters));
        }

        return result;
    }

    private static string Strip(string type) => type.Replace("?", string.Empty, StringComparison.Ordinal).Split('.')[^1];

    private static CsParam ParseCsParam(string text)
    {
        var marshal = Regex.Match(text, @"\[MarshalAs\((?<v>[^\]]*)\)\]");
        var rest = Regex.Replace(text, @"\[[^\]]*\]", string.Empty).Trim();
        var tokens = rest.Split([' ', '\t', '\r', '\n'], StringSplitOptions.RemoveEmptyEntries).ToList();
        var modifier = tokens.Count > 0 && tokens[0] is "ref" or "out" or "in" ? tokens[0] : string.Empty;
        if (modifier.Length > 0) tokens.RemoveAt(0);
        return new CsParam(modifier, tokens.Count >= 2 ? Strip(tokens[0]) : "?", tokens.Count >= 2 ? tokens[^1] : "?", marshal.Success ? marshal.Groups["v"].Value : null);
    }

    private static CsTypes ParseCSharpTypes(string source)
    {
        var structs = new Dictionary<string, CsStruct>(StringComparer.Ordinal);
        foreach (Match m in Regex.Matches(source, @"(?<layout>\[StructLayout\([^\]]*\)\])\s*(?:public|internal)\s+struct\s+(?<name>\w+)\s*\{"))
        {
            var open = m.Index + m.Length - 1;
            var body = source[(open + 1)..MatchingClose(source, open, '{', '}')];
            var fields = new List<CsField>();
            foreach (Match f in Regex.Matches(body, @"(?<attr>(?:\[[^\]]*\]\s*)*)public\s+(?<type>[\w.?]+)\s+(?<name>\w+)\s*;"))
            {
                var marshal = Regex.Match(f.Groups["attr"].Value, @"\[MarshalAs\((?<v>[^\]]*)\)\]");
                fields.Add(new CsField(Strip(f.Groups["type"].Value), f.Groups["name"].Value, marshal.Success ? marshal.Groups["v"].Value : null));
            }

            structs[m.Groups["name"].Value] = new CsStruct(m.Groups["layout"].Value, fields);
        }

        var enums = new Dictionary<string, CsEnum>(StringComparer.Ordinal);
        foreach (Match m in Regex.Matches(source, @"(?:public|internal)\s+enum\s+(?<name>\w+)\s*(?::\s*(?<u>\w+))?\s*\{"))
        {
            var open = m.Index + m.Length - 1;
            var body = StripComments(source[(open + 1)..MatchingClose(source, open, '{', '}')]);
            var values = new List<long>();
            var next = 0L;
            foreach (var member in body.Split(',', StringSplitOptions.RemoveEmptyEntries).Select(x => x.Trim()).Where(x => x.Length > 0))
            {
                var eq = Regex.Match(member, @"=\s*(?<v>-?\d+)");
                next = eq.Success ? long.Parse(eq.Groups["v"].Value, System.Globalization.CultureInfo.InvariantCulture) : next;
                values.Add(next);
                next++;
            }

            enums[m.Groups["name"].Value] = new CsEnum(m.Groups["u"].Success ? m.Groups["u"].Value : "int", values);
        }

        return new CsTypes(structs, enums);
    }

    private static int MatchingClose(string text, int openIndex, char open, char close)
    {
        var depth = 0;
        for (var i = openIndex; i < text.Length; i++)
        {
            if (text[i] == open) depth++;
            else if (text[i] == close && --depth == 0) return i;
        }

        throw new InvalidOperationException($"no matching '{close}' for the '{open}' at {openIndex}");
    }

    private static IEnumerable<string> SplitTopLevel(string args)
    {
        if (string.IsNullOrWhiteSpace(args)) yield break;
        var depth = 0;
        var start = 0;
        for (var i = 0; i < args.Length; i++)
        {
            if (args[i] is '(' or '[' or '<') depth++;
            else if (args[i] is ')' or ']' or '>') depth--;
            else if (args[i] == ',' && depth == 0) { yield return args[start..i].Trim(); start = i + 1; }
        }

        yield return args[start..].Trim();
    }

    // ----------------------------------------------------------------------------------------------------------------------------- files

    private static string ReadHeaders() => string.Join("\n", Headers.Select(Source));
    private static string ReadTypesHeader() => Source(TypesHeader);

    private static string Source(string relative) =>
        File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile(relative));
}
