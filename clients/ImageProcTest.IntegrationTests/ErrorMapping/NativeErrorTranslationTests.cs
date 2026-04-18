// AC-5, AC-6: Error code enum parity; AC-9: No managed exception from ABI boundary.
using System.Runtime.InteropServices;
using System.Text;
using ImageProcTest.IntegrationTests.Fixtures;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.ErrorMapping;

/// <summary>
/// Tests that negative ABI inputs produce XpeErrorCode return values —
/// never AccessViolationException, SEHException, or MarshalDirectiveException.
/// Covers REQ-GUI-IT-006, REQ-GUI-IT-050, REQ-GUI-IT-052, AC-5, AC-6, AC-9.
/// </summary>
[Trait("Category", "ErrorMapping")]
[Collection(NativeLibraryCollection.Name)]
public sealed class NativeErrorTranslationTests : IDisposable
{
    private readonly NativeLibraryFixture _fixture;

    public NativeErrorTranslationTests(NativeLibraryFixture fixture)
    {
        _fixture = fixture;
        if (fixture.IsAvailable)
            XpeCommonNative.xpe_init(null);
    }

    // REQ-GUI-IT-040: Uninitialized guard tests are covered here too.

    /// <summary>REQ-GUI-IT-040: xpe_aed_get_status before init returns NOT_INITIALIZED.</summary>
    [Fact]
    public void AedGetStatus_BeforeInit_ReturnsNotInitialized()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

        // Shutdown first to ensure uninitialized state.
        XpeCommonNative.xpe_shutdown();
        var result = XpeCommonNative.xpe_aed_get_status(out _);
        Assert.Equal(XpeCommonNative.XpeErrorCode.NOT_INITIALIZED, result);
    }

    /// <summary>REQ-GUI-IT-040: xpe_aed_configure before init returns NOT_INITIALIZED.</summary>
    [Fact]
    public void AedConfigure_BeforeInit_ReturnsNotInitialized()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

        XpeCommonNative.xpe_shutdown();
        var result = XpeCommonNative.xpe_aed_configure(null);
        Assert.Equal(XpeCommonNative.XpeErrorCode.NOT_INITIALIZED, result);
    }

    /// <summary>REQ-GUI-IT-040: xpe_get_param_range before init returns NOT_INITIALIZED.</summary>
    [Fact]
    public void GetParamRange_BeforeInit_ReturnsNotInitialized()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

        XpeCommonNative.xpe_shutdown();
        var result = XpeCommonNative.xpe_get_param_range("CHEST", "window_center", out _, out _, out _);
        Assert.Equal(XpeCommonNative.XpeErrorCode.NOT_INITIALIZED, result);
    }

    /// <summary>REQ-GUI-IT-040: xpe_version() before init is safe (read-only, no crash).</summary>
    [Fact]
    public void Version_BeforeInit_DoesNotCrash()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

        XpeCommonNative.xpe_shutdown();
        var ex = Record.Exception(() =>
        {
            var ptr = XpeCommonNative.xpe_version();
            // Result may be non-NULL (static string) — just must not crash.
            _ = ptr;
        });
        Assert.Null(ex);
    }

    /// <summary>REQ-GUI-IT-040: xpe_error_string before init is safe.</summary>
    [Fact]
    public void ErrorString_BeforeInit_DoesNotCrash()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

        XpeCommonNative.xpe_shutdown();
        var ex = Record.Exception(() =>
        {
            var ptr = XpeCommonNative.xpe_error_string(XpeCommonNative.XpeErrorCode.NOT_INITIALIZED);
            _ = ptr;
        });
        Assert.Null(ex);
    }

    /// <summary>REQ-GUI-IT-040: xpe_log_flush before init is safe.</summary>
    [Fact]
    public void LogFlush_BeforeInit_DoesNotCrash()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

        XpeCommonNative.xpe_shutdown();
        var ex = Record.Exception(() => XpeCommonNative.xpe_log_flush());
        Assert.Null(ex);
    }

    // -- Negative input scenarios (REQ-GUI-IT-006, REQ-GUI-IT-050, REQ-GUI-IT-052) --

    /// <summary>Nefarious JSON (very long) must not crash — returns CONFIG_INVALID or INVALID_INPUT.</summary>
    [Fact]
    public void Configure_VeryLongMalformedJson_DoesNotThrowManagedException()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped
        XpeCommonNative.xpe_init(null);

        var badJson = "{" + new string('x', 65536); // 64 KB malformed JSON
        var ex = Record.Exception(() =>
        {
            var result = XpeCommonNative.xpe_configure(badJson);
            Assert.True((int)result < 0, $"Expected error code, got {result}");
        });

        Assert.Null(ex);
    }

    /// <summary>xpe_get_pending_alert with tiny buffer must return BUFFER_TOO_SMALL or INVALID_INPUT — not crash.</summary>
    [Fact]
    public void GetPendingAlert_TinyBuffer_ReturnsErrorCodeNotException()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped
        XpeCommonNative.xpe_init(null);

        var ex = Record.Exception(() =>
        {
            var sb = new StringBuilder(1); // 1-byte buffer
            var result = XpeCommonNative.xpe_get_pending_alert(0, sb, (UIntPtr)1, out _);
            Assert.True(
                result == XpeCommonNative.XpeErrorCode.BUFFER_TOO_SMALL ||
                result == XpeCommonNative.XpeErrorCode.INVALID_INPUT,
                $"Expected BUFFER_TOO_SMALL or INVALID_INPUT, got {result}");
        });

        Assert.Null(ex);
    }

    /// <summary>xpe_alloc_image with very large dimensions must return error — not crash.</summary>
    [Fact]
    public void AllocImage_HugeDimensions_ReturnsErrorNotException()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped
        XpeCommonNative.xpe_init(null);

        var ex = Record.Exception(() =>
        {
            var result = XpeCommonNative.xpe_alloc_image(uint.MaxValue, uint.MaxValue,
                XpeCommonNative.XpePixelFormat.UInt16, out _);
            Assert.True((int)result < 0, $"Expected error code, got {result}");
        });

        Assert.Null(ex);
    }

    public void Dispose()
    {
        if (_fixture.IsAvailable)
            XpeCommonNative.xpe_shutdown();
    }
}
