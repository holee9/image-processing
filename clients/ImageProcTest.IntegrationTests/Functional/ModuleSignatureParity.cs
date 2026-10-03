// GUI-C-213 (#249): the engine that compares C# bindings of the native modules with their public headers.
using System.Globalization;
using System.Text.RegularExpressions;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Reads a module's public headers (functions, structs, enums, opaque handles) and the C# that binds them — static <c>[DllImport]</c> externs AND the delegates that are bound to an
/// export by name — and reports every way the two disagree: name, parameter count, parameter and return kinds, ref/out/in, const, string marshalling, bool width, calling convention,
/// struct LAYOUT (field kinds, offsets and total size, computed for both sides), enum VALUES. A type or declaration the engine cannot read is a finding, never a skip.
///
/// <para>Written for GUI-C-213 after GUI-C-212 found that the legacy oracle's call shape was wrong and nothing noticed: the first parity test (GUI-C-208) read only xpe_common's headers.
/// The engine is deliberately independent of that test so the first one stays as it was.</para>
/// </summary>
internal static class ModuleSignatureParity
{
    // ------------------------------------------------------------------------------------------------------------------------------ native model

    internal sealed record NType(string Base, int Ptr, bool Const)
    {
        public override string ToString() => (Const ? "const " : "") + Base + new string('*', Ptr);
    }

    internal sealed record NParam(NType Type, string Name) { public string Text => $"{Type} {Name}"; }
    internal sealed record NFunc(string Name, NType Ret, List<NParam> Params, string Header);
    internal sealed record NField(NType Type, string Name, int Array);
    internal sealed record NStruct(string Name, List<NField> Fields);
    internal sealed record NEnum(string Name, List<long> Values);

    internal sealed class NativeModel
    {
        public Dictionary<string, NFunc> Functions { get; } = new(StringComparer.Ordinal);
        public Dictionary<string, NStruct> Structs { get; } = new(StringComparer.Ordinal);
        public Dictionary<string, NEnum> Enums { get; } = new(StringComparer.Ordinal);
        public HashSet<string> Handles { get; } = new(StringComparer.Ordinal);
        public List<string> Problems { get; } = [];
    }

    private static readonly Dictionary<string, string> NativeScalar = new(StringComparer.Ordinal)
    {
        ["void"] = "void", ["char"] = "i8", ["bool"] = "bool1", ["int"] = "i32", ["int32_t"] = "i32", ["uint32_t"] = "u32", ["int64_t"] = "i64", ["uint64_t"] = "u64",
        ["uint8_t"] = "u8", ["uint16_t"] = "u16", ["int16_t"] = "i16", ["size_t"] = "usize", ["float"] = "f32", ["double"] = "f64", ["XpeErrorCode"] = "i32",
    };

    internal static string StripComments(string text) =>
        Regex.Replace(Regex.Replace(text, @"/\*.*?\*/", string.Empty, RegexOptions.Singleline), @"//[^\r\n]*", string.Empty);

    internal static NType ParseNType(string text)
    {
        var pointers = text.Count(ch => ch == '*');
        var tokens = text.Replace("*", " ", StringComparison.Ordinal).Split([' ', '\t', '\r', '\n'], StringSplitOptions.RemoveEmptyEntries).ToList();
        var isConst = tokens.Remove("const");
        tokens.RemoveAll(t => t is "struct" or "enum");
        return new NType(string.Join(" ", tokens), pointers, isConst);
    }

    /// <summary>Parses every header text into one model (functions, structs, enums, handles). Anything it cannot read is added to <see cref="NativeModel.Problems"/>.</summary>
    internal static NativeModel ParseHeaders(IEnumerable<(string Name, string Text)> headers)
    {
        var model = new NativeModel();
        foreach (var (name, raw) in headers)
        {
            var text = StripComments(raw);
            foreach (Match pm in Regex.Matches(text, @"#\s*pragma\s+pack\s*\(\s*(?:push\s*,\s*)?(?<n>\d+)\s*\)"))
            {
                // Natural alignment capped at 8 is what this checker models, and what C# does by default on x64; pack(8) changes nothing for members of 8 bytes or less.
                if (pm.Groups["n"].Value != "8") model.Problems.Add($"{name}: #pragma pack({pm.Groups["n"].Value}) is not modelled by this checker (only 8)");
            }

            foreach (Match m in Regex.Matches(text, @"typedef\s+(?<kind>struct|enum)\s*(?<tag>\w*)\s*\{(?<body>.*?)\}\s*(?<name>\w+)\s*;", RegexOptions.Singleline))
            {
                if (m.Groups["kind"].Value == "enum") model.Enums[m.Groups["name"].Value] = ParseEnum(m.Groups["name"].Value, m.Groups["body"].Value, model, name);
                else model.Structs[m.Groups["name"].Value] = ParseStruct(m.Groups["name"].Value, m.Groups["body"].Value, model, name);
            }

            foreach (Match m in Regex.Matches(text, @"typedef\s+struct\s+(?<tag>\w+)\s+(?<name>\w+)\s*;")) model.Handles.Add(m.Groups["name"].Value);
            foreach (Match m in Regex.Matches(text, @"typedef\s+(?:struct\s+\w+|void)\s*\*\s*(?<name>\w+)\s*;")) model.Handles.Add(m.Groups["name"].Value);

            foreach (Match m in Regex.Matches(text, @"\bXPE_API\b\s+(?<decl>[^;{}]+);"))
            {
                var decl = Regex.Replace(m.Groups["decl"].Value, @"\s+", " ").Trim();
                var open = decl.IndexOf('(');
                var close = decl.LastIndexOf(')');
                if (open < 0 || close < open) { model.Problems.Add($"{name}: unparseable declaration '{decl}'"); continue; }
                var head = decl[..open].Trim();
                var nm = Regex.Match(head, @"^(?<ret>.+?)(?<name>\w+)$");
                if (!nm.Success) { model.Problems.Add($"{name}: unparseable declaration head '{head}'"); continue; }
                var fname = nm.Groups["name"].Value;
                var parameters = new List<NParam>();
                var args = decl[(open + 1)..close].Trim();
                if (args.Length > 0 && args != "void")
                {
                    foreach (var part in args.Split(','))
                    {
                        var pm = Regex.Match(part.Trim(), @"^(?<type>.+?)\s*(?<name>\w+)$");
                        if (!pm.Success) { model.Problems.Add($"{name}: unparseable parameter '{part.Trim()}' of {fname}"); continue; }
                        parameters.Add(new NParam(ParseNType(pm.Groups["type"].Value), pm.Groups["name"].Value));
                    }
                }

                model.Functions[fname] = new NFunc(fname, ParseNType(nm.Groups["ret"].Value), parameters, name);
            }
        }

        // Every base type the functions and structs mention must be one the checker knows: unknown is a finding.
        foreach (var f in model.Functions.Values)
        {
            foreach (var t in f.Params.Select(p => p.Type).Append(f.Ret).Select(t => t.Base).Distinct().Where(b => NativeKind(b, model) is null)) model.Problems.Add($"unknown native type '{t}' in {f.Name} ({f.Header})");
        }

        foreach (var s in model.Structs.Values)
        {
            foreach (var t in s.Fields.Select(f => f.Type.Base).Distinct().Where(b => NativeKind(b, model) is null)) model.Problems.Add($"unknown native type '{t}' in struct {s.Name}");
        }

        return model;
    }

    private static NEnum ParseEnum(string name, string body, NativeModel model, string header)
    {
        var values = new List<long>();
        long next = 0;
        foreach (var raw in body.Split(','))
        {
            var item = raw.Trim();
            if (item.Length == 0) continue;
            var eq = item.IndexOf('=');
            if (eq >= 0)
            {
                var expr = item[(eq + 1)..].Trim();
                if (!TryEvalConstant(expr, out next)) { model.Problems.Add($"{header}: enum {name} has a value this checker cannot evaluate: '{expr}'"); next = 0; }
            }

            values.Add(next);
            next++;
        }

        return new NEnum(name, values);
    }

    private static bool TryEvalConstant(string expr, out long value)
    {
        expr = expr.Trim().TrimEnd('u', 'U', 'l', 'L');
        var shift = Regex.Match(expr, @"^(?<a>\d+)[uUlL]*\s*<<\s*(?<b>\d+)$");
        if (shift.Success) { value = long.Parse(shift.Groups["a"].Value, CultureInfo.InvariantCulture) << int.Parse(shift.Groups["b"].Value, CultureInfo.InvariantCulture); return true; }
        if (expr.StartsWith("0x", StringComparison.OrdinalIgnoreCase)) return long.TryParse(expr[2..], NumberStyles.HexNumber, CultureInfo.InvariantCulture, out value);
        return long.TryParse(expr, NumberStyles.Integer, CultureInfo.InvariantCulture, out value);
    }

    private static NStruct ParseStruct(string name, string body, NativeModel model, string header)
    {
        var fields = new List<NField>();
        foreach (var raw in body.Split(';'))
        {
            var item = Regex.Replace(raw, @"\s+", " ").Trim();
            if (item.Length == 0) continue;
            var fm = Regex.Match(item, @"^(?<type>.+?)\s*(?<name>\w+)\s*(?:\[\s*(?<n>\w+)\s*\])?$");
            if (!fm.Success) { model.Problems.Add($"{header}: struct {name}: unparseable field '{item}'"); continue; }
            var array = 0;
            if (fm.Groups["n"].Success && !int.TryParse(fm.Groups["n"].Value, out array)) { model.Problems.Add($"{header}: struct {name}: array length '{fm.Groups["n"].Value}' is not a number"); }
            fields.Add(new NField(ParseNType(fm.Groups["type"].Value), fm.Groups["name"].Value, array));
        }

        return new NStruct(name, fields);
    }

    /// <summary>Native kind of a base type, or null when the checker does not know it.</summary>
    internal static string? NativeKind(string b, NativeModel m)
    {
        if (NativeScalar.TryGetValue(b, out var k)) return k;
        if (m.Enums.ContainsKey(b)) return "enum";
        if (m.Structs.ContainsKey(b)) return "struct:" + b;
        if (m.Handles.Contains(b)) return "handle";
        return null;
    }

    // ------------------------------------------------------------------------------------------------------------------------------ C# model

    internal sealed record CsParam(string Modifier, string Type, string Name, string? MarshalAs);
    internal sealed record CsFunc(string Kind, string Name, string Ret, string Dll, string CallingConvention, string CharSet, List<CsParam> Params, string File);
    internal sealed record CsField(string Type, string Name, string? MarshalAs);
    internal sealed record CsStruct(string Name, int Pack, string Layout, List<CsField> Fields, string File);
    internal sealed record CsEnum(string Name, string Underlying, List<long> Values, string File);
    internal sealed record Binding(string Export, string Delegate, string File);

    internal sealed class CsModel
    {
        public List<CsFunc> Externs { get; } = [];
        public List<CsFunc> Delegates { get; } = [];
        // Several files may declare a type of the same name (each project keeps its own copy of the buffer struct, say); EVERY copy is checked against the header.
        public Dictionary<string, List<CsStruct>> Structs { get; } = new(StringComparer.Ordinal);
        public Dictionary<string, List<CsEnum>> Enums { get; } = new(StringComparer.Ordinal);
        public List<Binding> Bindings { get; } = [];
        public List<string> Problems { get; } = [];
    }

    private static readonly Dictionary<string, string> CsScalar = new(StringComparer.Ordinal)
    {
        ["void"] = "void", ["int"] = "i32", ["uint"] = "u32", ["long"] = "i64", ["ulong"] = "u64", ["byte"] = "u8", ["sbyte"] = "i8", ["ushort"] = "u16", ["short"] = "i16",
        ["float"] = "f32", ["double"] = "f64", ["IntPtr"] = "ptr", ["UIntPtr"] = "usize", ["nuint"] = "usize", ["nint"] = "ptr", ["bool"] = "bool4",
        ["string"] = "string", ["StringBuilder"] = "stringbuilder", ["char"] = "char16",
    };

    private static string Last(string type) => type.Replace("?", string.Empty, StringComparison.Ordinal).Split('.')[^1];

    /// <summary>The C# kind of a type name in the context of the structs and enums the sources declare; a trailing [] makes it "array:&lt;element kind&gt;". Null = unknown to the checker.</summary>
    internal static string? CsKind(string type, CsModel m)
    {
        type = Last(type.Trim());
        if (type.EndsWith("[]", StringComparison.Ordinal))
        {
            var inner = CsKind(type[..^2], m);
            return inner is null ? null : "array:" + inner;
        }

        if (CsScalar.TryGetValue(type, out var k)) return k;
        if (m.Enums.TryGetValue(type, out var e)) return e[0].Underlying switch { "int" => "enum:i32", "uint" => "enum:u32", _ => null };
        if (m.Structs.ContainsKey(type)) return "struct:" + type;
        return null;
    }

    /// <summary>Removes // and /* */ comments from C# source without touching string, verbatim-string or char literals (a doc comment that mentions [DllImport] is not a binding).</summary>
    internal static string StripCsComments(string src)
    {
        var sb = new System.Text.StringBuilder(src.Length);
        for (var i = 0; i < src.Length; i++)
        {
            var c = src[i];
            if (c == '/' && i + 1 < src.Length && src[i + 1] == '/') { while (i < src.Length && src[i] != '\n') i++; sb.Append('\n'); continue; }
            if (c == '/' && i + 1 < src.Length && src[i + 1] == '*') { i += 2; while (i + 1 < src.Length && !(src[i] == '*' && src[i + 1] == '/')) { if (src[i] == '\n') sb.Append('\n'); i++; } i++; continue; }
            if (c == '@' && i + 1 < src.Length && src[i + 1] == '"')
            {
                sb.Append("@\""); i += 2;
                while (i < src.Length) { sb.Append(src[i]); if (src[i] == '"') { if (i + 1 < src.Length && src[i + 1] == '"') { sb.Append('"'); i += 2; continue; } break; } i++; }
                continue;
            }

            if (c == '"' || c == '\'')
            {
                sb.Append(c); i++;
                while (i < src.Length && src[i] != c) { if (src[i] == '\\' && i + 1 < src.Length) { sb.Append(src[i]); i++; } sb.Append(src[i]); i++; }
                if (i < src.Length) sb.Append(src[i]);
                continue;
            }

            sb.Append(c);
        }

        return sb.ToString();
    }

    internal static void AddSource(CsModel m, string file, string source)
    {
        source = StripCsComments(source);
        ParseTypes(m, file, source);
        var constDll = Regex.Match(source, @"const\s+string\s+(?:DllName|LibraryName)\s*=\s*""(?<v>[^""]+)""").Groups["v"].Value;
        ParseExterns(m, file, source, constDll);
        ParseDelegates(m, file, source);
    }

    /// <summary>Resolves the binding sites once every source is added (a delegate is often declared in one file and bound in another).</summary>
    internal static void ResolveBindings(CsModel m, IEnumerable<(string File, string Source)> sources)
    {
        var delegates = new HashSet<string>(m.Delegates.Select(d => d.Name), StringComparer.Ordinal);
        foreach (var (file, rawSource) in sources)
        {
            var source = StripCsComments(rawSource);
            // GetRequiredDelegate<T>(handle, "export")  /  GetDelegate<T>(handle, "export")  /  Export<T>("export")  — any generic call naming a declared delegate type and an export literal.
            foreach (Match g in Regex.Matches(source, @"<(?<t>[\w.]+)>\(\s*(?:[^()""]*?,\s*)?""(?<n>[A-Za-z_]\w*)"""))
            {
                if (delegates.Contains(Last(g.Groups["t"].Value))) m.Bindings.Add(new Binding(g.Groups["n"].Value, Last(g.Groups["t"].Value), file));
            }

            // TryGetDelegate(handle, "export", out T? v)
            foreach (Match g in Regex.Matches(source, @"TryGetDelegate\(\s*[^,""]+,\s*""(?<n>\w+)""\s*,\s*out\s+(?<t>[\w.]+)\??"))
            {
                if (delegates.Contains(Last(g.Groups["t"].Value))) m.Bindings.Add(new Binding(g.Groups["n"].Value, Last(g.Groups["t"].Value), file));
            }

            // TryGetExport(handle, "export", out var sym) ... GetDelegateForFunctionPointer<T>(sym) within the same method
            foreach (Match g in Regex.Matches(source, @"TryGetExport\(\s*[^,""]+,\s*""(?<n>\w+)""\s*,\s*out\s+var\s+(?<s>\w+)\)"))
            {
                var tail = source[g.Index..Math.Min(source.Length, g.Index + 700)];
                var use = Regex.Match(tail, @"GetDelegateForFunctionPointer<(?<t>[\w.]+)>\(\s*" + g.Groups["s"].Value + @"\s*\)");
                if (use.Success && delegates.Contains(Last(use.Groups["t"].Value))) m.Bindings.Add(new Binding(g.Groups["n"].Value, Last(use.Groups["t"].Value), file));
            }
        }
    }

    private static void ParseTypes(CsModel m, string file, string source)
    {
        foreach (Match en in Regex.Matches(source, @"\benum\s+(?<name>\w+)\s*(?::\s*(?<u>\w+))?\s*\{(?<body>[^}]*)\}"))
        {
            var values = new List<long>();
            long next = 0;
            foreach (var raw in en.Groups["body"].Value.Split(','))
            {
                var item = Regex.Replace(StripComments(raw), @"\s+", " ").Trim();
                if (item.Length == 0) continue;
                var eq = item.IndexOf('=');
                if (eq >= 0 && !TryEvalConstant(item[(eq + 1)..].Trim(), out next)) { m.Problems.Add($"{file}: enum {en.Groups["name"].Value} has a value this checker cannot evaluate: '{item}'"); next = 0; }
                values.Add(next);
                next++;
            }

            AddTo(m.Enums, en.Groups["name"].Value, new CsEnum(en.Groups["name"].Value, en.Groups["u"].Success ? en.Groups["u"].Value : "int", values, file));
        }

        foreach (Match st in Regex.Matches(source, @"(?<attr>(?:\[[^\]]*\]\s*)*)(?:public|internal|private)?\s*(?:readonly\s+)?(?:unsafe\s+)?(?:partial\s+)?struct\s+(?<name>\w+)\s*\{"))
        {
            var open = st.Index + st.Length - 1;
            var close = MatchingClose(source, open, '{', '}');
            var pack = Regex.Match(st.Groups["attr"].Value, @"Pack\s*=\s*(?<p>\d+)");
            var layout = Regex.Match(st.Groups["attr"].Value, @"LayoutKind\.(?<l>\w+)").Groups["l"].Value;
            var fields = new List<CsField>();
            foreach (var stmt in TopLevelStatements(source[(open + 1)..close]))
            {
                var marshal = Regex.Match(stmt, @"\[MarshalAs\((?<v>[^\]]*)\)\]");
                var rest = Regex.Replace(stmt, @"\[[^\]\[]+\]", string.Empty).Trim();
                if (rest.Contains('(') || rest.Contains("=>") || rest.Contains('{')) continue;
                var tokens = rest.Split([' ', '\t', '\r', '\n'], StringSplitOptions.RemoveEmptyEntries).Where(t => t is not ("public" or "internal" or "private" or "readonly" or "unsafe")).ToList();
                if (tokens.Count < 2) continue;
                fields.Add(new CsField(Last(tokens[0]), tokens[^1], marshal.Success ? marshal.Groups["v"].Value : null));
            }

            AddTo(m.Structs, st.Groups["name"].Value, new CsStruct(st.Groups["name"].Value, pack.Success ? int.Parse(pack.Groups["p"].Value, CultureInfo.InvariantCulture) : 0, layout, fields, file));
        }
    }

    private static void AddTo<T>(Dictionary<string, List<T>> map, string key, T value)
    {
        if (!map.TryGetValue(key, out var list)) map[key] = list = [];
        list.Add(value);
    }

    private static IEnumerable<string> TopLevelStatements(string body)
    {
        var depth = 0;
        var start = 0;
        for (var i = 0; i < body.Length; i++)
        {
            if (body[i] == '{' || body[i] == '(') depth++;
            else if (body[i] == '}' || body[i] == ')') depth--;
            else if (body[i] == ';' && depth == 0) { yield return StripComments(body[start..i]).Trim(); start = i + 1; }
        }
    }

    private static void ParseExterns(CsModel m, string file, string source, string constDll)
    {
        foreach (Match dm in Regex.Matches(source, @"\[DllImport\("))
        {
            var attrStart = dm.Index + "[DllImport(".Length;
            var attrEnd = MatchingClose(source, attrStart - 1, '(', ')');
            var attrArgs = source[attrStart..attrEnd];
            var after = source[attrEnd..];
            var ext = Regex.Match(after, @"^\)\]\s*(?:\[[^\]]*\]\s*)*(?:public|internal|private)?\s*static\s+extern\s+(?<ret>[\w.<>?\[\]]+)\s+(?<name>\w+)\s*\(");
            if (!ext.Success) { m.Problems.Add($"{file}: a [DllImport] whose declaration this checker cannot read, near: {Regex.Replace(after[..Math.Min(80, after.Length)], @"\s+", " ")}"); continue; }
            var paramsStart = attrEnd + ext.Length;
            var paramsEnd = MatchingClose(source, paramsStart - 1, '(', ')');
            var lit = Regex.Match(attrArgs, @"^\s*""(?<v>[^""]+)""");
            var dll = lit.Success ? lit.Groups["v"].Value : constDll;
            m.Externs.Add(new CsFunc("extern", ext.Groups["name"].Value, Last(ext.Groups["ret"].Value), dll,
                Regex.Match(attrArgs, @"CallingConvention\s*=\s*CallingConvention\.(?<v>\w+)").Groups["v"].Value is { Length: > 0 } cc ? cc : "(default)",
                Regex.Match(attrArgs, @"CharSet\s*=\s*CharSet\.(?<v>\w+)").Groups["v"].Value is { Length: > 0 } cs ? cs : "(default)",
                SplitTopLevel(source[paramsStart..paramsEnd]).Select(ParseCsParam).ToList(), file));
        }
    }

    private static void ParseDelegates(CsModel m, string file, string source)
    {
        foreach (Match dm in Regex.Matches(source, @"\bdelegate\s+(?<ret>[\w.<>?\[\]]+)\s+(?<name>\w+)\s*\("))
        {
            var openParen = dm.Index + dm.Length - 1;
            var closeParen = MatchingClose(source, openParen, '(', ')');
            var before = source[Math.Max(0, dm.Index - 300)..dm.Index];
            var attr = Regex.Matches(before, @"\[UnmanagedFunctionPointer\((?<a>[^\]]*)\)\]").Cast<Match>().LastOrDefault()?.Groups["a"].Value ?? string.Empty;
            m.Delegates.Add(new CsFunc("delegate", dm.Groups["name"].Value, Last(dm.Groups["ret"].Value), "(by binding)",
                Regex.Match(attr, @"CallingConvention\.(?<v>\w+)").Groups["v"].Value is { Length: > 0 } cc ? cc : "(default)",
                Regex.Match(attr, @"CharSet\s*=\s*CharSet\.(?<v>\w+)").Groups["v"].Value is { Length: > 0 } cs ? cs : "(default)",
                SplitTopLevel(source[(openParen + 1)..closeParen]).Select(ParseCsParam).ToList(), file));
        }
    }

    internal static CsParam ParseCsParam(string text)
    {
        var marshal = Regex.Match(text, @"\[(?:In|Out|In,\s*Out)?\s*(?:,\s*)?MarshalAs\((?<v>[^\]]*)\)\]|\[MarshalAs\((?<v2>[^\]]*)\)\]");
        var rest = Regex.Replace(text, @"\[[^\]\[]+\]", string.Empty).Trim();
        var tokens = rest.Split([' ', '\t', '\r', '\n'], StringSplitOptions.RemoveEmptyEntries).ToList();
        var modifier = tokens.Count > 0 && tokens[0] is "ref" or "out" or "in" ? tokens[0] : string.Empty;
        if (modifier.Length > 0) tokens.RemoveAt(0);
        var marshalText = marshal.Success ? (marshal.Groups["v"].Success ? marshal.Groups["v"].Value : marshal.Groups["v2"].Value) : null;
        return new CsParam(modifier, tokens.Count >= 2 ? Last(tokens[0]) : "?", tokens.Count >= 2 ? tokens[^1] : "?", marshalText);
    }

    internal static int MatchingClose(string text, int openIndex, char open, char close)
    {
        var depth = 0;
        for (var i = openIndex; i < text.Length; i++)
        {
            if (text[i] == open) depth++;
            else if (text[i] == close && --depth == 0) return i;
        }

        return text.Length - 1;
    }

    internal static IEnumerable<string> SplitTopLevel(string args)
    {
        var depth = 0;
        var start = 0;
        for (var i = 0; i < args.Length; i++)
        {
            if (args[i] is '(' or '[' or '<') depth++;
            else if (args[i] is ')' or ']' or '>') depth--;
            else if (args[i] == ',' && depth == 0) { yield return args[start..i].Trim(); start = i + 1; }
        }

        var tail = args[start..].Trim();
        if (tail.Length > 0) yield return tail;
    }

    // ------------------------------------------------------------------------------------------------------------------------------ comparison

    /// <summary>All the disagreements between one native function and one C# binding of it (a static extern or a delegate).</summary>
    internal static List<string> CompareFunction(NFunc n, CsFunc c, NativeModel nm, CsModel cm, Dictionary<string, string> structVerdicts)
    {
        var problems = new List<string>();
        var who = $"{c.Kind} {c.Name} ({c.File}) vs {n.Name} ({n.Header})";
        if (!c.CallingConvention.Equals("Cdecl", StringComparison.Ordinal)) problems.Add($"{who}: calling convention is {c.CallingConvention}; the headers' functions are cdecl and the binding must say so");
        if (n.Params.Count != c.Params.Count)
        {
            problems.Add($"{who}: the header has {n.Params.Count} parameter(s) ({string.Join(", ", n.Params.Select(p => p.Text))}), the C# has {c.Params.Count} ({string.Join(", ", c.Params.Select(p => $"{p.Modifier} {p.Type} {p.Name}".Trim()))})");
            return problems;
        }

        problems.AddRange(MatchReturn(n, c, nm, cm).Select(p => $"{who}: return {p}"));
        for (var i = 0; i < n.Params.Count; i++)
        {
            problems.AddRange(MatchParam(n.Params[i], c.Params[i], c, nm, cm, structVerdicts).Select(p => $"{who}: parameter {i + 1} '{c.Params[i].Name}' (header '{n.Params[i].Name}'): {p}"));
        }

        return problems;
    }

    private static IEnumerable<string> MatchReturn(NFunc n, CsFunc c, NativeModel nm, CsModel cm)
    {
        var kind = CsKind(c.Ret, cm);
        if (kind is null) { yield return $"unknown C# type '{c.Ret}'"; yield break; }
        var t = n.Ret;
        var nk = NativeKind(t.Base, nm);
        if (nk is null) yield break;   // an unknown native type is already a finding of ParseHeaders
        if (t.Ptr == 0 && nk == "void") { if (kind != "void") yield return $"is {c.Ret}, the header returns void"; yield break; }
        if (t.Ptr > 0) { if (kind != "ptr") yield return $"is {c.Ret}, the header returns {t} (a pointer: IntPtr)"; yield break; }
        if (nk.StartsWith("struct:", StringComparison.Ordinal)) { yield return $"the header returns {t} by value; this checker does not model that"; yield break; }
        if (!ValueMatches(nk, kind)) yield return $"is {c.Ret} ({kind}), the header returns {t} ({nk})";
    }

    private static IEnumerable<string> MatchParam(NParam n, CsParam c, CsFunc owner, NativeModel nm, CsModel cm, Dictionary<string, string> structVerdicts)
    {
        var kind = CsKind(c.Type, cm);
        if (kind is null) { yield return $"unknown C# type '{c.Type}'"; yield break; }
        var t = n.Type;
        var nk = NativeKind(t.Base, nm);
        if (nk is null) yield break;   // an unknown native type is already a finding of ParseHeaders

        if (t.Ptr == 0)
        {
            if (c.Modifier.Length > 0) yield return $"has the modifier '{c.Modifier}', the header passes {t} by value";
            if (nk.StartsWith("struct:", StringComparison.Ordinal)) { foreach (var p in MatchStruct(nk["struct:".Length..], c.Type, nm, cm, structVerdicts)) yield return p; yield break; }
            if (!ValueMatches(nk, kind) || (nk == "bool1" && c.MarshalAs?.Contains("U1", StringComparison.Ordinal) != true)) yield return $"is {c.Type} ({kind}{(c.MarshalAs is null ? "" : ", " + c.MarshalAs)}), the header has {t} ({nk})";
            if (nk == "enum" && kind.StartsWith("enum:", StringComparison.Ordinal)) { foreach (var p in MatchEnum(t.Base, c.Type, nm, cm)) yield return p; }
            yield break;
        }

        // pointers ----------------------------------------------------------------------------------------------------------------
        if (t.Ptr >= 2) { if (kind != "ptr" && !(c.Modifier is "ref" or "out" && kind == "ptr")) yield return $"is {c.Type}, the header has {t} (a pointer to a pointer: IntPtr or ref/out IntPtr)"; yield break; }

        if (nk == "i8" && t.Base == "char")
        {
            if (t.Const)
            {
                if (kind != "string" && kind != "ptr" && kind != "array:u8") yield return $"is {c.Type}, the header has const char* (string, IntPtr or byte[])";
                if (kind == "string" && c.MarshalAs is not null && !c.MarshalAs.Contains("LPStr", StringComparison.Ordinal) && !c.MarshalAs.Contains("LPUTF8Str", StringComparison.Ordinal)) yield return $"marshals as {c.MarshalAs}, the header's const char* is an 8-bit string (LPStr or LPUTF8Str)";
                if (kind == "string" && c.MarshalAs is null && owner.CharSet is not ("Ansi" or "Auto" or "Unicode")) yield return "is a string without [MarshalAs] and the binding has no CharSet";
                if (kind == "string" && c.MarshalAs is null && owner.CharSet == "Unicode") yield return "is a string marshalled as UTF-16 (CharSet.Unicode), the header's const char* is 8-bit";
                // GUI-C-212c (Codex #98): CharSet.Auto is not "either": on Windows it is UTF-16, so it is no more an 8-bit marshal than Unicode is.
                if (kind == "string" && c.MarshalAs is null && owner.CharSet == "Auto") yield return "is a string marshalled with CharSet.Auto (UTF-16 on Windows), the header's const char* is 8-bit";
                if (c.Modifier.Length > 0 && kind == "string") yield return $"has the modifier '{c.Modifier}', the header passes a const char*";
            }
            else if (kind is not ("stringbuilder" or "ptr" or "array:u8" or "array:i8")) yield return $"is {c.Type}, the header has a writable char* buffer (StringBuilder, byte[] or IntPtr)";
            yield break;
        }

        if (t.Base == "void" || nk == "handle")
        {
            if (kind != "ptr" && !(kind.StartsWith("array:", StringComparison.Ordinal))) yield return $"is {c.Type}, the header has {t} (IntPtr)";
            yield break;
        }

        // typed single pointer: scalar / enum / struct --------------------------------------------------------------------------
        if (kind == "ptr") yield break;   // an opaque pointer is how callers hand buffers over; the element type is not checkable from here
        var isArray = kind.StartsWith("array:", StringComparison.Ordinal);
        var elemKind = isArray ? kind["array:".Length..] : kind;
        var byRef = c.Modifier is "ref" or "out" or "in";
        if (!isArray && !byRef) { yield return $"is {c.Type} passed by value, the header has {t} (a pointer: ref/out/in, an array, or IntPtr)"; yield break; }
        if (t.Const && c.Modifier == "out") yield return "is 'out', the header's pointer is const";
        if (!t.Const && c.Modifier == "in") yield return "is 'in', the header's pointer is not const (the callee may write)";
        if (nk.StartsWith("struct:", StringComparison.Ordinal)) { foreach (var p in MatchStruct(nk["struct:".Length..], c.Type.Replace("[]", string.Empty, StringComparison.Ordinal), nm, cm, structVerdicts)) yield return p; yield break; }
        if (nk == "enum") { if (!ValueMatches("enum", elemKind)) yield return $"points to {c.Type} ({elemKind}), the header points to enum {t.Base}"; yield break; }
        if (!ValueMatches(nk, elemKind) || (nk == "bool1" && c.MarshalAs?.Contains("U1", StringComparison.Ordinal) != true)) yield return $"points to {c.Type} ({elemKind}), the header points to {t} ({nk})";
    }

    private static bool ValueMatches(string nk, string ck) => nk switch
    {
        "i8" => ck is "i8",
        "u8" => ck is "u8",
        "i16" => ck is "i16",
        "u16" => ck is "u16",
        "i32" => ck is "i32" or "enum:i32",
        "u32" => ck is "u32" or "enum:u32",
        "i64" => ck is "i64",
        "u64" => ck is "u64",
        "usize" => ck is "usize",
        "f32" => ck is "f32",
        "f64" => ck is "f64",
        "bool1" => ck is "bool4" or "u8",
        "enum" => ck is "enum:i32" or "enum:u32" or "i32" or "u32",
        "handle" => ck is "ptr",
        _ => false,
    };

    private static IEnumerable<string> MatchEnum(string nativeName, string csName, NativeModel nm, CsModel cm)
    {
        if (!cm.Enums.TryGetValue(Last(csName), out var copies)) yield break;
        var ne = nm.Enums[nativeName];
        foreach (var ce in copies.Where(c => !ne.Values.SequenceEqual(c.Values)))
        {
            yield return $"enum {ce.Name} ({ce.File}) has values [{string.Join(", ", ce.Values)}], the header's {nativeName} has [{string.Join(", ", ne.Values)}]";
        }
    }

    // ------------------------------------------------------------------------------------------------------------------------------ layout

    private static (int Size, int Align) NativeFieldLayout(NField f, NativeModel nm, HashSet<string> visiting)
    {
        var nk = NativeKind(f.Type.Base, nm) ?? "?";
        (int size, int align) one;
        if (f.Type.Ptr > 0) one = (8, 8);
        else if (nk.StartsWith("struct:", StringComparison.Ordinal)) one = NativeStructLayout(nm.Structs[nk["struct:".Length..]], nm, visiting);
        else one = KindSize(nk);
        return f.Array > 0 ? (one.size * f.Array, one.align) : one;
    }

    private static (int Size, int Align) KindSize(string kind) => kind switch
    {
        "i8" or "u8" or "bool1" => (1, 1),
        "i16" or "u16" => (2, 2),
        "i32" or "u32" or "f32" or "enum" => (4, 4),
        "i64" or "u64" or "f64" or "usize" or "handle" or "ptr" => (8, 8),
        _ => (0, 1),
    };

    internal static (int Size, int Align) NativeStructLayout(NStruct s, NativeModel nm, HashSet<string>? visiting = null)
    {
        visiting ??= [];
        if (!visiting.Add(s.Name)) return (0, 1);
        var offset = 0;
        var maxAlign = 1;
        foreach (var f in s.Fields)
        {
            var (size, align) = NativeFieldLayout(f, nm, visiting);
            offset = (offset + align - 1) / align * align;
            offset += size;
            maxAlign = Math.Max(maxAlign, align);
        }

        visiting.Remove(s.Name);
        return ((offset + maxAlign - 1) / maxAlign * maxAlign, maxAlign);
    }

    private static List<(string Kind, int Size, int Align, int Offset)> NativeOffsets(NStruct s, NativeModel nm)
    {
        var result = new List<(string, int, int, int)>();
        var offset = 0;
        foreach (var f in s.Fields)
        {
            var (size, align) = NativeFieldLayout(f, nm, []);
            offset = (offset + align - 1) / align * align;
            result.Add((f.Type.Ptr > 0 ? "ptr" : NativeKind(f.Type.Base, nm) ?? "?", size, align, offset));
            offset += size;
        }

        return result;
    }

    private static (int Size, int Align) CsFieldLayout(CsField f, CsModel cm, int pack, out List<string> problems)
    {
        problems = [];
        var type = f.Type;
        var arrayN = 1;
        var m = f.MarshalAs;
        var sizeConst = Regex.Match(m ?? string.Empty, @"SizeConst\s*=\s*(?<n>\d+)");
        var isArrayMarshal = m is not null && (m.Contains("ByValArray", StringComparison.Ordinal) || m.Contains("ByValTStr", StringComparison.Ordinal));
        if (isArrayMarshal)
        {
            if (!sizeConst.Success) problems.Add($"field {f.Name}: a by-value array/string without SizeConst");
            else arrayN = int.Parse(sizeConst.Groups["n"].Value, CultureInfo.InvariantCulture);
        }

        var elementType = type.EndsWith("[]", StringComparison.Ordinal) ? type[..^2] : type;
        var kind = CsKind(elementType, cm);
        (int size, int align) one;
        if (kind is null) { problems.Add($"field {f.Name}: unknown C# type '{type}'"); return (0, 1); }
        if (type == "string" && m is not null && m.Contains("ByValTStr", StringComparison.Ordinal)) one = (1, 1);   // ANSI char array
        else if (kind.StartsWith("struct:", StringComparison.Ordinal)) one = CsStructLayout(cm.Structs[kind["struct:".Length..]][0], cm, pack, []);
        else if (kind.StartsWith("enum:", StringComparison.Ordinal)) one = (4, 4);
        else if (kind == "bool4") one = m is not null && m.Contains("U1", StringComparison.Ordinal) ? (1, 1) : (4, 4);
        else one = KindSize(kind);
        if (type.EndsWith("[]", StringComparison.Ordinal) && !isArrayMarshal) one = (8, 8);   // a reference to a managed array marshals as a pointer
        return (one.size * arrayN, one.align);
    }

    internal static (int Size, int Align) CsStructLayout(CsStruct s, CsModel cm, int defaultPack, HashSet<string> visiting)
    {
        if (!visiting.Add(s.Name)) return (0, 1);
        var pack = s.Pack > 0 ? s.Pack : defaultPack;
        var offset = 0;
        var maxAlign = 1;
        foreach (var f in s.Fields)
        {
            var (size, align) = CsFieldLayout(f, cm, pack, out _);
            align = Math.Min(align, pack);
            offset = (offset + align - 1) / align * align;
            offset += size;
            maxAlign = Math.Max(maxAlign, align);
        }

        visiting.Remove(s.Name);
        return ((offset + maxAlign - 1) / maxAlign * maxAlign, maxAlign);
    }

    /// <summary>One C# struct against one native struct: same fields in the same order with the same kinds and sizes, the same offsets and total size. Cached per pair.</summary>
    private static IEnumerable<string> MatchStruct(string nativeName, string csName, NativeModel nm, CsModel cm, Dictionary<string, string> verdicts)
    {
        csName = Last(csName);
        if (!cm.Structs.TryGetValue(csName, out var copies)) { yield return $"struct {csName} is not declared in the scanned sources (the header's {nativeName})"; yield break; }
        foreach (var cs in copies)
        {
            var key = nativeName + "|" + cs.File + "|" + cs.Name;
            if (!verdicts.TryGetValue(key, out var cached))
            {
                var found = CompareStructLayout(nm.Structs[nativeName], cs, nm, cm);
                verdicts[key] = cached = string.Join(" ;; ", found);
            }

            if (cached.Length > 0) foreach (var p in cached.Split(" ;; ")) yield return p;
        }
    }

    internal static List<string> CompareStructLayout(NStruct ns, CsStruct cs, NativeModel nm, CsModel cm)
    {
        var problems = new List<string>();
        var prefix = $"struct {cs.Name} ({cs.File}) vs {ns.Name}";
        if (cs.Layout.Length > 0 && cs.Layout != "Sequential") problems.Add($"{prefix}: layout is {cs.Layout}, the header's struct is sequential");
        if (cs.Fields.Count != ns.Fields.Count) { problems.Add($"{prefix}: {cs.Fields.Count} field(s), the header has {ns.Fields.Count}"); return problems; }
        var nativeOffsets = NativeOffsets(ns, nm);
        var pack = cs.Pack > 0 ? cs.Pack : 8;
        var offset = 0;
        for (var i = 0; i < ns.Fields.Count; i++)
        {
            var nf = ns.Fields[i];
            var cf = cs.Fields[i];
            var (size, align) = CsFieldLayout(cf, cm, pack, out var fieldProblems);
            problems.AddRange(fieldProblems.Select(p => $"{prefix}: {p}"));
            align = Math.Min(align, pack);
            offset = (offset + align - 1) / align * align;
            var native = nativeOffsets[i];
            if (Norm(nf.Name) != Norm(cf.Name)) problems.Add($"{prefix}: field {i + 1} is '{cf.Name}', the header's is '{nf.Name}'");
            if (offset != native.Offset || size != native.Size) problems.Add($"{prefix}: field {i + 1} '{cf.Name}' is {size} byte(s) at offset {offset}, the header's '{nf.Name}' is {native.Size} byte(s) at offset {native.Offset}");
            offset += size;
            var nk = nf.Type.Ptr > 0 ? "ptr" : NativeKind(nf.Type.Base, nm) ?? "?";
            var ck = CsKind(cf.Type.EndsWith("[]", StringComparison.Ordinal) ? cf.Type[..^2] : cf.Type, cm) ?? "?";
            if (nk == "enum" && ck.StartsWith("enum:", StringComparison.Ordinal)) problems.AddRange(MatchEnum(nf.Type.Base, cf.Type, nm, cm).Select(p => $"{prefix}: field '{cf.Name}': {p}"));
            else if (nk == "ptr" && ck != "ptr" && !cf.Type.EndsWith("[]", StringComparison.Ordinal)) problems.Add($"{prefix}: field '{cf.Name}' is {cf.Type}, the header's is a pointer (IntPtr)");
            else if (nk.StartsWith("struct:", StringComparison.Ordinal) && !ck.StartsWith("struct:", StringComparison.Ordinal)) problems.Add($"{prefix}: field '{cf.Name}' is {cf.Type}, the header's is struct {nf.Type.Base}");
            else if (nk is not ("ptr" or "enum") && !nk.StartsWith("struct:", StringComparison.Ordinal) && nf.Array == 0 && !ValueMatches(nk, ck)) problems.Add($"{prefix}: field '{cf.Name}' is {cf.Type} ({ck}), the header's is {nf.Type} ({nk})");
        }

        for (var i = 0; i < ns.Fields.Count; i++)
        {
            problems.AddRange(ArrayFieldProblems(ns.Fields[i], cs.Fields[i], nm, cm).Select(p => $"{prefix}: {p}"));
        }

        var nativeTotal = NativeStructLayout(ns, nm).Size;
        var csTotal = CsStructLayout(cs, cm, 8, []).Size;
        if (nativeTotal != csTotal) problems.Add($"{prefix}: size {csTotal} byte(s), the header's is {nativeTotal} byte(s)");
        return problems;
    }

    /// <summary>
    /// GUI-C-212c (Codex #98): a native fixed array was only compared by its total byte size, so <c>uint16_t[1024]</c> and a 2048-element <c>byte[]</c> were "the same". The three things that
    /// make a by-value array an ABI contract are compared one by one: the marshalling shape (ByValArray, or ByValTStr for a char array), the element kind, and the element count.
    /// </summary>
    private static IEnumerable<string> ArrayFieldProblems(NField nf, CsField cf, NativeModel nm, CsModel cm)
    {
        if (nf.Array <= 0 || nf.Type.Ptr > 0) yield break;
        var m = cf.MarshalAs ?? string.Empty;
        var byValArray = m.Contains("ByValArray", StringComparison.Ordinal);
        var byValString = m.Contains("ByValTStr", StringComparison.Ordinal);
        var isChar = nf.Type.Base == "char";
        var nk = NativeKind(nf.Type.Base, nm) ?? "?";
        var declared = $"{nf.Type.Base}[{nf.Array}]";

        if (!byValArray && !byValString) { yield return $"field '{cf.Name}' is {cf.Type} without [MarshalAs(ByValArray or ByValTStr, SizeConst)]; the header's is the by-value array {declared}"; yield break; }

        if (byValString)
        {
            if (!isChar) yield return $"field '{cf.Name}' is a ByValTStr string, the header's {declared} is not a char array";
            else if (cf.Type != "string") yield return $"field '{cf.Name}' is {cf.Type} with ByValTStr, which is for a string";
        }
        else
        {
            if (!cf.Type.EndsWith("[]", StringComparison.Ordinal)) { yield return $"field '{cf.Name}' is {cf.Type} with ByValArray, which is for an array"; yield break; }
            var ck = CsKind(cf.Type[..^2], cm) ?? "?";
            var kindOk = nk.StartsWith("struct:", StringComparison.Ordinal)
                ? ck.StartsWith("struct:", StringComparison.Ordinal)
                : isChar ? ck is "i8" or "u8" : ValueMatches(nk, ck);
            if (!kindOk) yield return $"field '{cf.Name}' is an array of {cf.Type[..^2]} ({ck}), the header's {declared} has elements of {nk}";
        }

        var sizeConst = Regex.Match(m, @"SizeConst\s*=\s*(?<n>\d+)");
        if (!sizeConst.Success) yield return $"field '{cf.Name}' has no SizeConst; the header's {declared} has {nf.Array} element(s)";
        else if (int.Parse(sizeConst.Groups["n"].Value, CultureInfo.InvariantCulture) != nf.Array) yield return $"field '{cf.Name}' has SizeConst {sizeConst.Groups["n"].Value}, the header's {declared} has {nf.Array} element(s)";
    }

    // ------------------------------------------------------------------------------------------------------------------------------ whole-model checks (used by the real tests and by the controls)

    /// <summary>Every static extern of a DLL this repository builds against the header(s) of that DLL; foreign DLLs are skipped, unknown DLLs and one-sided names are findings unless allowed with a reason.</summary>
    internal static List<string> CheckExterns(NativeModel nm, CsModel cm, Func<string, IReadOnlySet<string>?> headersOfDll, IReadOnlySet<string> foreignDlls, IReadOnlyDictionary<string, string> csOnly)
    {
        var verdicts = new Dictionary<string, string>();
        var problems = new List<string>();
        foreach (var e in cm.Externs.Where(e => !foreignDlls.Contains(e.Dll)))
        {
            var own = headersOfDll(e.Dll);
            if (own is null) { problems.Add($"{e.File}: {e.Name} imports from '{e.Dll}', a DLL that is neither a module of this repository nor in the foreign list"); continue; }
            if (!nm.Functions.TryGetValue(e.Name, out var n) || !own.Contains(n.Header))
            {
                if (!csOnly.ContainsKey(e.File + "|" + e.Name)) problems.Add($"{e.File}: {e.Name} is imported from {e.Dll} but that module's header has no such function (and it is not in the allow-list)");
                continue;
            }

            problems.AddRange(CompareFunction(n, e, nm, cm, verdicts));
        }

        foreach (var stale in csOnly.Keys.Where(k => !cm.Externs.Any(e => e.File + "|" + e.Name == k))) problems.Add($"the allow-list names {stale}, which is no longer declared: remove the entry");
        return problems;
    }

    /// <summary>Every delegate bound to an export by name against that export's header declaration; a delegate nothing binds, or an export no header has, is a finding unless allowed with a reason.</summary>
    internal static List<string> CheckDelegates(NativeModel nm, CsModel cm, IReadOnlyDictionary<string, string> unresolved)
    {
        var verdicts = new Dictionary<string, string>();
        var problems = new List<string>();
        var byName = cm.Delegates.GroupBy(d => d.Name).ToDictionary(g => g.Key, g => g.ToList());
        foreach (var b in cm.Bindings.Distinct())
        {
            if (!nm.Functions.TryGetValue(b.Export, out var n))
            {
                if (!unresolved.ContainsKey(b.File + "|" + b.Export)) problems.Add($"{b.File}: \"{b.Export}\" is bound to delegate {b.Delegate} but no module header declares that export");
                continue;
            }

            var candidates = byName[b.Delegate];
            var chosen = candidates.Where(d => d.File == b.File).ToList();
            foreach (var d in chosen.Count > 0 ? chosen : candidates) problems.AddRange(CompareFunction(n, d, nm, cm, verdicts).Select(p => p + $"  [bound in {b.File}]"));
        }

        var bound = cm.Bindings.Select(b => b.Delegate).ToHashSet();
        foreach (var d in cm.Delegates.Where(d => !bound.Contains(d.Name)))
        {
            if (!unresolved.ContainsKey(d.File + "|" + d.Name)) problems.Add($"{d.File}: delegate {d.Name} is bound to no export by name, so it cannot be compared (and it is not in the allow-list)");
        }

        foreach (var stale in unresolved.Keys.Where(k => !cm.Delegates.Any(d => d.File + "|" + d.Name == k) && !cm.Bindings.Any(b => b.File + "|" + b.Export == k))) problems.Add($"the allow-list names {stale}, which no longer exists: remove the entry");
        return problems.Distinct().ToList();
    }

    internal static string Norm(string s) => s.Replace("_", string.Empty, StringComparison.Ordinal).ToLowerInvariant();
}
