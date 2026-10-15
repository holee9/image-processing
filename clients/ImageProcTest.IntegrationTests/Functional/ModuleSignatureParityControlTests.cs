// GUI-C-213 (#249): the signature checker must say "red" for each kind of drift it claims to detect.
using static ImageProcTest.IntegrationTests.Functional.ModuleSignatureParity;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// A comparison that has never been seen to fail proves nothing (the oracle that nothing called, GUI-C-212). These controls run the engine of <see cref="NativeModuleSignatureParityTests"/>
/// on a small made-up header and C# binding that agree (the baseline must be clean), then break ONE thing at a time and require the matching finding. The real sources are checked by
/// the other class; this one checks the checker.
/// </summary>
public sealed class ModuleSignatureParityControlTests
{
    private const string Header = """
        typedef enum XpeMode { XPE_MODE_A = 0, XPE_MODE_B = 1 } XpeMode;
        typedef struct XpeParams { XpeMode mode; float sigma; int32_t size; } XpeParams;
        typedef struct XpeBlob { char name[16]; uint16_t table[4]; const uint16_t* data; uint32_t n; } XpeBlob;
        XPE_API int32_t xpe_t_add(int32_t a, uint32_t b);
        XPE_API int32_t xpe_t_str(const char* src, char* out, size_t outLen);
        XPE_API int32_t xpe_t_ptr(const float* v, float* result, const XpeParams* p, XpeBlob* blob);
        XPE_API int32_t xpe_t_flag(bool on, XpeMode m);
        """;

    private const string Cs = """
        using System;
        using System.Runtime.InteropServices;
        using System.Text;
        enum XpeMode { A = 0, B = 1 }
        [StructLayout(LayoutKind.Sequential, Pack = 8)]
        struct XpeParams { public XpeMode Mode; public float Sigma; public int Size; }
        [StructLayout(LayoutKind.Sequential, Pack = 8)]
        struct XpeBlob { [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 16)] public string Name; [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)] public ushort[] Table; public IntPtr Data; public uint N; }
        static class N
        {
            const string DllName = "xpe_t.dll";
            [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)] public static extern int xpe_t_add(int a, uint b);
            [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)] public static extern int xpe_t_str([MarshalAs(UnmanagedType.LPStr)] string src, StringBuilder output, UIntPtr outLen);
            [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)] public static extern int xpe_t_ptr(float[] v, out float result, in XpeParams p, ref XpeBlob blob);
            [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)] public static extern int xpe_t_flag([MarshalAs(UnmanagedType.U1)] bool on, XpeMode m);
        }
        static class D
        {
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int AddFn(int a, uint b);
            static void Bind(IntPtr h) { var f = G<AddFn>(h, "xpe_t_add"); }
            static T G<T>(IntPtr h, string n) where T : Delegate => throw new NotImplementedException();
        }
        """;

    private static readonly IReadOnlySet<string> Own = new HashSet<string> { "h.h" };
    private static readonly IReadOnlySet<string> NoForeign = new HashSet<string>();
    private static readonly IReadOnlyDictionary<string, string> Empty = new Dictionary<string, string>();

    private static List<string> Run(string header, string cs, IReadOnlyDictionary<string, string>? csOnly = null, IReadOnlyDictionary<string, string>? unresolved = null)
    {
        var native = ParseHeaders([("h.h", header)]);
        var model = new CsModel();
        AddSource(model, "c.cs", cs);
        ResolveBindings(model, [("c.cs", cs)]);
        var problems = new List<string>(native.Problems);
        problems.AddRange(model.Problems);
        problems.AddRange(CheckExterns(native, model, dll => dll == "xpe_t.dll" ? Own : null, NoForeign, csOnly ?? Empty));
        problems.AddRange(CheckDelegates(native, model, unresolved ?? Empty));
        return problems;
    }

    /// <summary>
    /// GUI-C-220: the sources below are raw string literals, so they carry whatever line ending THIS file was checked out with (CI checks <c>.cs</c> out as CRLF; a local worktree is LF), while
    /// the <c>from</c> text of a mutation names a line break as a backslash-n. The source is normalized to LF before the target is looked for, so the controls behave the same on both checkouts.
    /// Whether the ENGINE reads CRLF input correctly is a separate question, asked directly by <see cref="TheEngine_ReadsCrlfSourcesLikeLfOnes"/>.
    /// </summary>
    private static string Mutate(string source, string from, string to)
    {
        source = source.Replace("\r\n", "\n", StringComparison.Ordinal);
        Assert.Contains(from, source, StringComparison.Ordinal);   // the control itself must hit its target, or it proves nothing
        return source.Replace(from, to, StringComparison.Ordinal);
    }

    private static string Crlf(string text) => text.Replace("\r\n", "\n", StringComparison.Ordinal).Replace("\n", "\r\n", StringComparison.Ordinal);

    /// <summary>
    /// The checkout the engine reads on CI has CRLF in every header and every C# file. A clean pair must stay clean, and a broken pair must report exactly what it reports with LF: an engine that
    /// read nothing from a CRLF file would pass the first and fail the second, which is why both are asked.
    /// </summary>
    [Fact]
    public void TheEngine_ReadsCrlfSourcesLikeLfOnes()
    {
        var plainHeader = Header.Replace("\r\n", "\n", StringComparison.Ordinal);
        var plainCs = Cs.Replace("\r\n", "\n", StringComparison.Ordinal);
        Assert.Empty(Run(Crlf(plainHeader), Crlf(plainCs)));

        var broken = Mutate(plainCs, "public static extern int xpe_t_add", "public static extern uint xpe_t_add");
        var lf = Run(plainHeader, broken);
        var crlf = Run(Crlf(plainHeader), Crlf(broken));
        Assert.NotEmpty(lf);
        Assert.Equal(lf, crlf);
    }

    [Fact]
    public void Baseline_AMatchingHeaderAndBinding_IsClean_AndTheCheckerSawWhatItShould()
    {
        var native = ParseHeaders([("h.h", Header)]);
        Assert.Empty(native.Problems);
        Assert.Equal(4, native.Functions.Count);
        Assert.Equal(3, native.Structs.Count + native.Enums.Count);
        var model = new CsModel();
        AddSource(model, "c.cs", Cs);
        ResolveBindings(model, [("c.cs", Cs)]);
        Assert.Equal(4, model.Externs.Count);
        Assert.Single(model.Bindings);
        Assert.Empty(Run(Header, Cs));
    }

    [Theory]
    [InlineData("a parameter type changed", "xpe_t_add(int a, uint b)", "xpe_t_add(int a, int b)", "parameter 2")]
    [InlineData("a parameter dropped", "xpe_t_add(int a, uint b)", "xpe_t_add(int a)", "parameter(s)")]
    [InlineData("a return type changed", "public static extern int xpe_t_add", "public static extern uint xpe_t_add", "return")]
    [InlineData("a pointer passed by value", "out float result", "float result", "by value")]
    [InlineData("an out on a const pointer", "in XpeParams p", "out XpeParams p", "'out'")]
    [InlineData("an in on a writable pointer", "ref XpeBlob blob", "in XpeBlob blob", "'in'")]
    [InlineData("an ANSI string marshalled wide", "UnmanagedType.LPStr", "UnmanagedType.LPWStr", "marshals as")]
    [InlineData("a bool without a one-byte marshal", "[MarshalAs(UnmanagedType.U1)] bool on", "bool on", "bool")]
    [InlineData("a function name the header lacks", "xpe_t_flag", "xpe_t_flagged", "no such function")]
    [InlineData("a struct field of another type", "public float Sigma;", "public double Sigma;", "field 2")]
    [InlineData("a struct field renamed", "public int Size;", "public int Count;", "'Count'")]
    [InlineData("a struct field dropped", "public float Sigma; ", "", "field(s)")]
    [InlineData("a by-value array of another length", "SizeConst = 4", "SizeConst = 8", "XpeBlob")]
    [InlineData("a struct packed to 1", "[StructLayout(LayoutKind.Sequential, Pack = 8)]\nstruct XpeBlob", "[StructLayout(LayoutKind.Sequential, Pack = 1)]\nstruct XpeBlob", "size")]
    [InlineData("an enum with other values", "enum XpeMode { A = 0, B = 1 }", "enum XpeMode { A = 0, B = 2 }", "values")]
    [InlineData("an unknown C# type", "float[] v", "Quaternion v", "unknown C# type")]
    public void Control_EachKindOfDriftInTheCSharp_IsReported(string what, string from, string to, string expectedFragment)
    {
        var problems = Run(Header, Mutate(Cs.Replace("\r\n", "\n", StringComparison.Ordinal), from, to));
        Assert.True(problems.Count > 0, $"{what}: the checker found nothing");
        Assert.True(problems.Any(p => p.Contains(expectedFragment, StringComparison.OrdinalIgnoreCase)), $"{what}: findings were [{string.Join(" | ", problems)}], none mentions '{expectedFragment}'");
    }

    // GUI-C-212c (Codex #98): the two ABI differences the checker used to let through.

    /// <summary>CharSet.Auto is UTF-16 on Windows. An 8-bit <c>const char*</c> bound as a string with no [MarshalAs] and CharSet.Auto marshals the wrong width; it used to be accepted.</summary>
    [Fact]
    public void Control_AConstCharPointerBoundWithCharSetAuto_IsReported_ButAnsiWithoutMarshalAsIsNot()
    {
        var plain = Cs.Replace("\r\n", "\n", StringComparison.Ordinal);
        var auto = Mutate(plain, "CharSet = CharSet.Ansi)] public static extern int xpe_t_str([MarshalAs(UnmanagedType.LPStr)] string src", "CharSet = CharSet.Auto)] public static extern int xpe_t_str(string src");
        Assert.Contains(Run(Header, auto), p => p.Contains("CharSet.Auto", StringComparison.Ordinal) && p.Contains("UTF-16", StringComparison.Ordinal));

        // the other side of the control: the same binding with Ansi and no [MarshalAs] is a valid 8-bit marshal and must stay clean
        var ansi = Mutate(plain, "CharSet = CharSet.Ansi)] public static extern int xpe_t_str([MarshalAs(UnmanagedType.LPStr)] string src", "CharSet = CharSet.Ansi)] public static extern int xpe_t_str(string src");
        Assert.Empty(Run(Header, ansi));
    }

    [Theory]
    [InlineData("a uint16_t[4] bound as an 8-element byte[] (same 8 bytes)", "[MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)] public ushort[] Table;", "[MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)] public byte[] Table;", "elements of")]
    [InlineData("a uint16_t[4] bound as a 4-element short[] (same size, other signedness)", "public ushort[] Table;", "public short[] Table;", "elements of")]
    [InlineData("an array bound with another element count", "SizeConst = 4)] public ushort[] Table;", "SizeConst = 8)] public ushort[] Table;", "SizeConst 8")]
    [InlineData("an array bound with no marshalling attribute", "[MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)] public ushort[] Table;", "public ushort[] Table;", "without [MarshalAs")]
    [InlineData("an array bound as a ByValTStr string", "[MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)] public ushort[] Table;", "[MarshalAs(UnmanagedType.ByValTStr, SizeConst = 4)] public string Table;", "ByValTStr")]
    [InlineData("a char[16] bound with ByValArray on a non-array", "[MarshalAs(UnmanagedType.ByValTStr, SizeConst = 16)] public string Name;", "[MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)] public string Name;", "which is for an array")]
    [InlineData("a char[16] bound as a ByValTStr of another length", "ByValTStr, SizeConst = 16", "ByValTStr, SizeConst = 32", "SizeConst 32")]
    public void Control_AByValueArray_IsComparedByShapeElementKindAndCount_NotByTotalSizeAlone(string what, string from, string to, string expectedFragment)
    {
        var problems = Run(Header, Mutate(Cs.Replace("\r\n", "\n", StringComparison.Ordinal), from, to));
        Assert.True(problems.Count > 0, $"{what}: the checker found nothing");
        Assert.True(problems.Any(p => p.Contains(expectedFragment, StringComparison.OrdinalIgnoreCase)), $"{what}: findings were [{string.Join(" | ", problems)}], none mentions '{expectedFragment}'");
    }

    // GUI-C-212d (Codex #100): a ByValTStr character is 1 byte under CharSet.Ansi and 2 under Unicode and Auto, and the struct's own CharSet decides which.

    [Theory]
    [InlineData("Unicode")]
    [InlineData("Auto")]
    public void Control_AByValTStrInAStructWithAWideCharSet_IsReported_ForANativeCharArray(string charSet)
    {
        var wide = Mutate(Cs.Replace("\r\n", "\n", StringComparison.Ordinal), "[StructLayout(LayoutKind.Sequential, Pack = 8)]\nstruct XpeBlob", $"[StructLayout(LayoutKind.Sequential, Pack = 8, CharSet = CharSet.{charSet})]\nstruct XpeBlob");

        var problems = Run(Header, wide);

        Assert.Contains(problems, p => p.Contains($"CharSet.{charSet}", StringComparison.Ordinal) && p.Contains("2-byte", StringComparison.Ordinal) && p.Contains("CharSet.Ansi", StringComparison.Ordinal));
        Assert.Contains(problems, p => p.Contains("byte(s) at offset", StringComparison.Ordinal) || p.Contains("size", StringComparison.Ordinal));   // and the layout itself no longer matches the header
    }

    /// <summary>The other side of the control: an explicit Ansi struct, and a struct with no CharSet at all (Ansi by default), both stay clean.</summary>
    [Fact]
    public void Control_AByValTStrInAnAnsiStruct_IsClean_WithOrWithoutTheExplicitCharSet()
    {
        var plain = Cs.Replace("\r\n", "\n", StringComparison.Ordinal);
        var explicitAnsi = Mutate(plain, "[StructLayout(LayoutKind.Sequential, Pack = 8)]\nstruct XpeBlob", "[StructLayout(LayoutKind.Sequential, Pack = 8, CharSet = CharSet.Ansi)]\nstruct XpeBlob");

        Assert.Empty(Run(Header, explicitAnsi));
        Assert.Empty(Run(Header, plain));
    }

    /// <summary>The checker must not flag a correct alternative: a native char array may be bound as a ByValArray of bytes.</summary>
    [Fact]
    public void Control_ACharArrayBoundAsAByteArray_IsStillClean()
    {
        var bytes = Mutate(Cs.Replace("\r\n", "\n", StringComparison.Ordinal), "[MarshalAs(UnmanagedType.ByValTStr, SizeConst = 16)] public string Name;", "[MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)] public byte[] Name;");
        Assert.Empty(Run(Header, bytes));
    }

    [Fact]
    public void Control_ACallingConventionThatIsNotCdecl_IsReported_ForExternsAndDelegates()
    {
        var stdcall = Run(Header, Mutate(Cs, "xpe_t_add(int a, uint b);", "xpe_t_add(int a, uint b);").Replace("[DllImport(DllName, CallingConvention = CallingConvention.Cdecl)] public static extern int xpe_t_add", "[DllImport(DllName, CallingConvention = CallingConvention.StdCall)] public static extern int xpe_t_add", StringComparison.Ordinal));
        Assert.Contains(stdcall, p => p.Contains("calling convention is StdCall", StringComparison.Ordinal));

        var noAttribute = Run(Header, Mutate(Cs, "[UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int AddFn", "delegate int AddFn"));
        Assert.Contains(noAttribute, p => p.Contains("calling convention is (default)", StringComparison.Ordinal) && p.Contains("delegate AddFn", StringComparison.Ordinal));
    }

    [Fact]
    public void Control_ADelegateWithTheWrongShape_OrBoundToAnExportNoHeaderHas_IsReported()
    {
        var wrongShape = Run(Header, Mutate(Cs, "delegate int AddFn(int a, uint b);", "delegate int AddFn(int a);"));
        Assert.Contains(wrongShape, p => p.Contains("delegate AddFn", StringComparison.Ordinal) && p.Contains("parameter(s)", StringComparison.Ordinal));

        var noSuchExport = Run(Header, Mutate(Cs, "\"xpe_t_add\"", "\"xpe_t_addd\""));
        Assert.Contains(noSuchExport, p => p.Contains("no module header declares that export", StringComparison.Ordinal));
    }

    [Fact]
    public void Control_AnUnknownNativeType_IsAFindingNotASkip()
    {
        var problems = Run(Mutate(Header, "const XpeParams* p", "const XpeUnknown* p"), Cs);
        Assert.Contains(problems, p => p.Contains("unknown native type 'XpeUnknown'", StringComparison.Ordinal));
    }

    [Fact]
    public void Control_AHeaderStructWithAnotherFieldType_IsReportedAgainstTheUnchangedCSharp()
    {
        var problems = Run(Mutate(Header, "float sigma;", "double sigma;"), Cs);
        Assert.Contains(problems, p => p.Contains("XpeParams", StringComparison.Ordinal));
    }

    [Fact]
    public void Control_AnAllowListEntry_IsRequiredForOneSidedDeclarations_AndStaleEntriesFail()
    {
        var extra = Mutate(Cs, "public static extern int xpe_t_flag(", "public static extern int xpe_t_extra(int z);\n    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)] public static extern int xpe_t_flag(");
        Assert.Contains(Run(Header, extra), p => p.Contains("xpe_t_extra", StringComparison.Ordinal) && p.Contains("no such function", StringComparison.Ordinal));

        var allowed = new Dictionary<string, string> { ["c.cs|xpe_t_extra"] = "a test-only export" };
        Assert.DoesNotContain(Run(Header, extra, allowed), p => p.Contains("xpe_t_extra", StringComparison.Ordinal));

        Assert.Contains(Run(Header, Cs, allowed), p => p.Contains("no longer declared", StringComparison.Ordinal));   // the same entry against code that does not have it
    }

    [Fact]
    public void Control_ACommentThatMentionsADllImport_IsNotABinding()
    {
        var withComment = Mutate(Cs, "static class N\n", "/// <summary>Declared with [DllImport(\"xpe_t.dll\")] in the old days.</summary>\nstatic class N\n".Replace("\n", "\n"));
        Assert.Empty(Run(Header, withComment.Replace("\r\n", "\n", StringComparison.Ordinal)));
    }
}
