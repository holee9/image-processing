// REQ-GUI-IT-006 / 050 / 052, AC-9 (GUI-C-209 M2, D11): every distinct rejection path of xpe_common, once, with its exact code.
using System.Runtime.InteropServices;
using System.Text;
using ImageProcTest.IntegrationTests.Fixtures;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.ErrorMapping;

/// <summary>
/// AC-9 asks for "20+" negative tests. A count is not what protects anything: ten inputs through the same <c>if</c> are one test of one check. This class is organised by the
/// <b>check the input is meant to reach</b>: one row per distinct validation statement in the native source (<c>xpe_common.cpp</c>, <c>xpe_logging.cpp</c>, <c>xpe_memory.cpp</c>), and
/// each row states the input, the EXACT <see cref="XpeCommonNative.XpeErrorCode"/> (REQ-GUI-IT-052: "document the expected code" — never "this or that"), and the source text of the check.
///
/// <para>Each row has a <b>control</b>: the same call with a valid value must succeed, so a rejection cannot be a side effect of something else (not initialised, wrong state).
/// <see cref="EveryRow_CitesACheckThatExistsInTheNativeSource_AndNoCheckIsCountedTwice"/> reads the native source and requires the cited text to be there, so the table cannot drift away
/// from the code, and requires every cited check to be different, so a path cannot be counted twice. The honest count is <b>18</b> distinct rejection paths in xpe_common; the SPEC's "20+"
/// cannot be reached from this library alone without counting one check twice (see the GUI-C-209 report). Rows whose input is a NULL <c>out</c>/<c>ref</c> argument cannot be produced by the
/// managed mirror (the marshaller never passes NULL for them), so they call the export through a function pointer.</para>
/// </summary>
[Trait("Category", "ErrorMapping")]
[Collection(NativeLibraryCollection.Name)]
public sealed class NegativeInputPathTests : IDisposable
{
    private readonly NativeLibraryFixture _fixture;
    private static readonly XpeCommonNative.XpeErrorCode Ok = XpeCommonNative.XpeErrorCode.OK;
    private const XpeCommonNative.XpeErrorCode Invalid = XpeCommonNative.XpeErrorCode.INVALID_INPUT;

    public NegativeInputPathTests(NativeLibraryFixture fixture)
    {
        _fixture = fixture;
        if (fixture.IsAvailable)
        {
            XpeCommonNative.xpe_init(null);
        }
    }

    public void Dispose()
    {
        if (_fixture.IsAvailable)
        {
            XpeCommonNative.xpe_shutdown();
        }
    }

    /// <summary>One distinct validation statement, one input that reaches it, the exact code, and a valid control.</summary>
    private sealed record Case(
        string Id, string File, string CheckText, string Input,
        XpeCommonNative.XpeErrorCode Expected,
        Func<XpeCommonNative.XpeErrorCode> Negative,
        Func<XpeCommonNative.XpeErrorCode> Control);

    // The table is built per call (it closes over this instance's fixture); ids are static so the theory can enumerate them.
    private static readonly string[] Ids =
    [
        "init-empty-config", "configure-null-or-empty", "configure-malformed-json", "configure-not-an-object",
        "param-range-null-argument", "param-range-before-init", "param-range-unknown-body-part",
        "alert-null-or-bad-argument", "alert-index-out-of-range", "alert-buffer-too-small",
        "log-level-out-of-range", "log-file-parent-missing",
        "alloc-null-out-or-zero-size", "alloc-over-4096", "alloc-unsupported-format",
        "free-null-buffer", "copy-null-or-unallocated", "copy-destination-too-small",
    ];

    public static IEnumerable<object[]> CaseIds() => Ids.Select(i => new object[] { i });

    private IReadOnlyList<Case> Cases() =>
    [
        new("init-empty-config", "xpe_common.cpp", "configJsonOrNull[0] == '\\0'", "xpe_init(\"\")", XpeCommonNative.XpeErrorCode.CONFIG_INVALID,
            () => XpeCommonNative.xpe_init(""), () => XpeCommonNative.xpe_init("{}")),
        new("configure-null-or-empty", "xpe_common.cpp", "if (!jsonConfig || jsonConfig[0] == '\\0')", "xpe_configure(NULL) and xpe_configure(\"\")", Invalid,
            () => { var a = XpeCommonNative.xpe_configure(null!); var b = XpeCommonNative.xpe_configure(""); return a == b ? a : XpeCommonNative.XpeErrorCode.INTERNAL; },
            () => XpeCommonNative.xpe_configure("{}")),
        new("configure-malformed-json", "xpe_common.cpp", "nlohmann::json::accept", "xpe_configure(\"{\\\"a\\\":\")", XpeCommonNative.XpeErrorCode.CONFIG_INVALID,
            () => XpeCommonNative.xpe_configure("{\"a\":"), () => XpeCommonNative.xpe_configure("{\"a\":1}")),
        new("configure-not-an-object", "xpe_common.cpp", "if (*p != '{')", "xpe_configure(\"[1,2]\") (valid JSON, not an object)", XpeCommonNative.XpeErrorCode.CONFIG_INVALID,
            () => XpeCommonNative.xpe_configure("[1,2]"), () => XpeCommonNative.xpe_configure("{}")),
        new("param-range-null-argument", "xpe_common.cpp", "!bodyPart || !paramName", "xpe_get_param_range(NULL, \"gamma\", ...)", Invalid,
            () => XpeCommonNative.xpe_get_param_range(null!, "gamma", out _, out _, out _),
            () => XpeCommonNative.xpe_get_param_range("CHEST", "gamma", out _, out _, out _)),
        new("param-range-before-init", "xpe_common.cpp", "if (!g_initialized) return XPE_ERR_NOT_INITIALIZED;", "xpe_get_param_range(\"CHEST\",\"gamma\") after xpe_shutdown()", XpeCommonNative.XpeErrorCode.NOT_INITIALIZED,
            () => { XpeCommonNative.xpe_shutdown(); try { return XpeCommonNative.xpe_get_param_range("CHEST", "gamma", out _, out _, out _); } finally { XpeCommonNative.xpe_init(null); } },
            () => XpeCommonNative.xpe_get_param_range("CHEST", "gamma", out _, out _, out _)),
        new("param-range-unknown-body-part", "xpe_common.cpp", "if (!validBodyPart)", "xpe_get_param_range(\"KNEE\",\"gamma\")", Invalid,
            () => XpeCommonNative.xpe_get_param_range("KNEE", "gamma", out _, out _, out _),
            () => XpeCommonNative.xpe_get_param_range("EXTREMITY", "gamma", out _, out _, out _)),
        new("alert-null-or-bad-argument", "xpe_common.cpp", "if (!msg || msgLen == 0 || !severity || index < 0)", "xpe_get_pending_alert(0, NULL, 64, ...) with one alert queued", Invalid,
            () => { Queue("a"); return XpeCommonNative.xpe_get_pending_alert(0, null!, (UIntPtr)64, out _); },
            () => { Queue("a"); return XpeCommonNative.xpe_get_pending_alert(0, new StringBuilder(64), (UIntPtr)64, out _); }),
        new("alert-index-out-of-range", "xpe_common.cpp", "static_cast<std::size_t>(index) >= g_alertQueue.size()", "xpe_get_pending_alert(0, buf, 64, ...) on an EMPTY queue", Invalid,
            () => { XpeCommonNative.xpe_clear_alerts(); return XpeCommonNative.xpe_get_pending_alert(0, new StringBuilder(64), (UIntPtr)64, out _); },
            () => { Queue("a"); return XpeCommonNative.xpe_get_pending_alert(0, new StringBuilder(64), (UIntPtr)64, out _); }),
        new("alert-buffer-too-small", "xpe_common.cpp", "e.message.size() + 1 > msgLen", "xpe_get_pending_alert(0, buf, 3, ...) with the 6-character alert \"abcdef\"", XpeCommonNative.XpeErrorCode.BUFFER_TOO_SMALL,
            () => { Queue("abcdef"); return XpeCommonNative.xpe_get_pending_alert(0, new StringBuilder(3), (UIntPtr)3, out _); },
            () => { Queue("abcdef"); return XpeCommonNative.xpe_get_pending_alert(0, new StringBuilder(64), (UIntPtr)64, out _); }),
        new("log-level-out-of-range", "xpe_logging.cpp", "level < 0 || level > 5", "xpe_log_set_level(-1) and xpe_log_set_level(6)", Invalid,
            () => { var a = XpeCommonNative.xpe_log_set_level(-1); var b = XpeCommonNative.xpe_log_set_level(6); return a == b ? a : XpeCommonNative.XpeErrorCode.INTERNAL; },
            () => XpeCommonNative.xpe_log_set_level(2)),
        new("log-file-parent-missing", "xpe_logging.cpp", "!std::filesystem::exists(parent)", "xpe_log_set_file(\"<temp>/no_such_dir_<guid>/x.log\")", XpeCommonNative.XpeErrorCode.IO_FAILED,
            () => XpeCommonNative.xpe_log_set_file(Path.Combine(Path.GetTempPath(), $"no_such_dir_{Guid.NewGuid():N}", "x.log")),
            () =>
            {
                var file = Path.Combine(Path.GetTempPath(), $"xpe_neg_{Guid.NewGuid():N}.log");
                try { return XpeCommonNative.xpe_log_set_file(file); }
                finally { XpeCommonNative.xpe_log_set_file(null!); try { File.Delete(file); } catch (IOException) { /* temp file */ } }
            }),
        new("alloc-null-out-or-zero-size", "xpe_memory.cpp", "!out || width == 0 || height == 0", "xpe_alloc_image(0, 16, UINT16, &buf) and xpe_alloc_image(16, 16, UINT16, NULL)", Invalid,
            () => { var a = XpeCommonNative.xpe_alloc_image(0, 16, XpeCommonNative.XpePixelFormat.UInt16, out _); var b = AllocWithNullOut(); return a == b ? a : XpeCommonNative.XpeErrorCode.INTERNAL; },
            () => AllocAndFree(16, 16, XpeCommonNative.XpePixelFormat.UInt16)),
        new("alloc-over-4096", "xpe_memory.cpp", "width > 4096 || height > 4096", "xpe_alloc_image(4097, 1, UINT16, &buf)", Invalid,
            () => XpeCommonNative.xpe_alloc_image(4097, 1, XpeCommonNative.XpePixelFormat.UInt16, out _),
            () => AllocAndFree(4096, 1, XpeCommonNative.XpePixelFormat.UInt16)),
        new("alloc-unsupported-format", "xpe_memory.cpp", "!bytes_per_pixel(format, &bytesPerPixel)", "xpe_alloc_image(16, 16, (XpePixelFormat)99, &buf)", XpeCommonNative.XpeErrorCode.UNSUPPORTED_FORMAT,
            () => XpeCommonNative.xpe_alloc_image(16, 16, (XpeCommonNative.XpePixelFormat)99, out _),
            () => AllocAndFree(16, 16, XpeCommonNative.XpePixelFormat.UInt8)),
        new("free-null-buffer", "xpe_memory.cpp", "if (!buf)", "xpe_free_image(NULL)", Invalid,
            FreeWithNullBuffer,
            () => AllocAndFree(8, 8, XpeCommonNative.XpePixelFormat.Float32)),
        new("copy-null-or-unallocated", "xpe_memory.cpp", "!src || !dst || !src->data || !dst->data", "xpe_copy_image(src with data == NULL, dst)", Invalid,
            () => { var src = new XpeCommonNative.XpeImageBuffer(); var dst = new XpeCommonNative.XpeImageBuffer(); return XpeCommonNative.xpe_copy_image(ref src, ref dst); },
            () => CopyBetween(8, 8, 8, 8)),
        new("copy-destination-too-small", "xpe_memory.cpp", "dst->dataSize < src->dataSize", "xpe_copy_image(src 8x8, dst 4x4)", XpeCommonNative.XpeErrorCode.BUFFER_TOO_SMALL,
            () => CopyBetween(8, 8, 4, 4),
            () => CopyBetween(8, 8, 8, 8)),
    ];

    private static void Queue(string message)
    {
        XpeCommonNative.xpe_clear_alerts();
        XpeCommonNative.xpe_alert_push(message, 2);
        Assert.Equal(1, XpeCommonNative.xpe_get_pending_alert_count());
    }

    private static XpeCommonNative.XpeErrorCode AllocAndFree(uint w, uint h, XpeCommonNative.XpePixelFormat format)
    {
        var rc = XpeCommonNative.xpe_alloc_image(w, h, format, out var buf);
        if (rc == Ok) XpeCommonNative.xpe_free_image(ref buf);
        return rc;
    }

    private static XpeCommonNative.XpeErrorCode CopyBetween(uint sw, uint sh, uint dw, uint dh)
    {
        Assert.Equal(Ok, XpeCommonNative.xpe_alloc_image(sw, sh, XpeCommonNative.XpePixelFormat.UInt16, out var src));
        Assert.Equal(Ok, XpeCommonNative.xpe_alloc_image(dw, dh, XpeCommonNative.XpePixelFormat.UInt16, out var dst));
        try { return XpeCommonNative.xpe_copy_image(ref src, ref dst); }
        finally { XpeCommonNative.xpe_free_image(ref src); XpeCommonNative.xpe_free_image(ref dst); }
    }

    // ---- NULL out/ref arguments: the marshaller never produces them, so call the export through a function pointer. ----

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int AllocFn(uint width, uint height, int format, IntPtr outBuffer);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int FreeFn(IntPtr buffer);

    private T Export<T>(string name) where T : Delegate =>
        Marshal.GetDelegateForFunctionPointer<T>(NativeLibrary.GetExport(NativeLibrary.Load(_fixture.ResolvedPath), name));

    private XpeCommonNative.XpeErrorCode AllocWithNullOut() =>
        (XpeCommonNative.XpeErrorCode)Export<AllocFn>("xpe_alloc_image")(16, 16, (int)XpeCommonNative.XpePixelFormat.UInt16, IntPtr.Zero);

    private XpeCommonNative.XpeErrorCode FreeWithNullBuffer() =>
        (XpeCommonNative.XpeErrorCode)Export<FreeFn>("xpe_free_image")(IntPtr.Zero);

    // ---- the tests ----

    /// <summary>
    /// One theory row per distinct check: the negative input returns EXACTLY the documented code (and nothing is thrown — REQ-GUI-IT-006/050/052: an access violation would kill the test host,
    /// and a managed exception fails the row), while the control — the same call with a valid value — returns OK.
    /// </summary>
    [SkippableTheory]
    [MemberData(nameof(CaseIds))]
    public void EachDistinctCheck_ReturnsItsExactCode_AndItsValidControlSucceeds(string id)
    {
        SkipHelper.SkipIf(!_fixture.IsAvailable, _fixture.SkipReason);
        var row = Cases().Single(c => c.Id == id);

        XpeCommonNative.XpeErrorCode negative = default;
        var thrown = Record.Exception(() => negative = BoundaryGuard.Invoke(row.Id + " (negative)", row.Negative));   // REQ-GUI-IT-050: a SEHException is recorded and fails the row
        Assert.True(thrown is null, $"{row.Id}: {row.Input} threw {thrown?.GetType().Name}: {thrown?.Message}");
        Assert.True(row.Expected == negative, $"{row.Id}: {row.Input} -> expected {row.Expected} ({(int)row.Expected}), got {negative} ({(int)negative}); check: {row.CheckText}");

        XpeCommonNative.XpeErrorCode control = default;
        thrown = Record.Exception(() => control = BoundaryGuard.Invoke(row.Id + " (control)", row.Control));
        Assert.True(thrown is null, $"{row.Id} control threw {thrown?.GetType().Name}: {thrown?.Message}");
        Assert.True(Ok == control, $"{row.Id} control (the same call with a valid value) returned {control}, so the rejection above may not be about {row.CheckText}");
    }

    /// <summary>
    /// GUI-C-225 (REQ-GUI-IT-050): "every negative test shall run to completion with the host alive". All registered rows are run, one after another, through the boundary guard, and the number that
    /// ran is compared with the number registered (the theory's ids, the table's rows): a row that is skipped, or a host that stopped half-way, cannot make the suite look complete. A
    /// SEHException in any row fails here with the recorded line. (An access violation ends the host, which fails the whole run instead; .NET cannot catch it.)
    /// </summary>
    [SkippableFact]
    public void EveryRegisteredNegativeRow_RunsToCompletion_ThroughTheBoundaryGuard_WithTheHostAlive()
    {
        SkipHelper.SkipIf(!_fixture.IsAvailable, _fixture.SkipReason);
        var rows = Cases();
        var registered = CaseIds().Count();
        Assert.Equal(registered, rows.Count);   // what the theory enumerates is what the table holds

        var executed = 0;
        foreach (var row in rows)
        {
            BoundaryGuard.Invoke(row.Id + " (negative)", row.Negative);
            executed++;
        }

        Assert.Equal(registered, executed);
    }

    /// <summary>
    /// The table is honest: every row cites a check that is in the native source today, and no check is cited twice (a path is counted once however many inputs reach it). Also pins the count
    /// (18) so adding or removing a check in the source without touching this table is visible. The 'at least 18' wording is deliberate: a new rejection path should get a new row.
    /// </summary>
    [Fact]
    public void EveryRow_CitesACheckThatExistsInTheNativeSource_AndNoCheckIsCountedTwice()
    {
        using var instance = new NegativeInputPathTests(_fixture);
        var rows = instance.Cases();
        Assert.Equal(Ids.Length, rows.Count);
        Assert.Equal(Ids.OrderBy(x => x), rows.Select(r => r.Id).OrderBy(x => x));
        Assert.True(rows.Count >= 18, $"expected the 18 distinct rejection paths of xpe_common, found {rows.Count} rows");

        var duplicates = rows.GroupBy(r => (r.File, Normalize(r.CheckText))).Where(g => g.Count() > 1).Select(g => string.Join("+", g.Select(r => r.Id))).ToList();
        Assert.True(duplicates.Count == 0, "the same check is cited by more than one row: " + string.Join("; ", duplicates));

        var sourceDir = FindNativeSourceDir();
        var missing = new List<string>();
        foreach (var row in rows)
        {
            var text = Normalize(File.ReadAllText(Path.Combine(sourceDir, row.File)));
            if (!text.Contains(Normalize(row.CheckText), StringComparison.Ordinal)) missing.Add($"{row.Id}: '{row.CheckText}' not in {row.File}");
        }

        Assert.True(missing.Count == 0, "cited checks not found in the native source:" + Environment.NewLine + string.Join(Environment.NewLine, missing));
    }

    /// <summary>The cited-source lookup must really read the source: a check text that is not there is reported (a search that cannot fail would pass an empty table).</summary>
    [Fact]
    public void Control_ACheckTextThatIsNotInTheSource_IsNotFound()
    {
        var text = Normalize(File.ReadAllText(Path.Combine(FindNativeSourceDir(), "xpe_common.cpp")));
        Assert.Contains(Normalize("if (!validBodyPart)"), text, StringComparison.Ordinal);
        Assert.DoesNotContain(Normalize("if (!thisCheckDoesNotExistAnywhere)"), text, StringComparison.Ordinal);
    }

    private static string Normalize(string s) => string.Join(' ', s.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries));

    private static string FindNativeSourceDir()
    {
        for (var dir = new DirectoryInfo(AppContext.BaseDirectory); dir is not null; dir = dir.Parent)
        {
            var candidate = Path.Combine(dir.FullName, "modules", "common", "src");
            if (File.Exists(Path.Combine(candidate, "xpe_common.cpp"))) return candidate;
        }

        Assert.Fail("modules/common/src/xpe_common.cpp was not found above " + AppContext.BaseDirectory);
        return string.Empty;
    }
}
