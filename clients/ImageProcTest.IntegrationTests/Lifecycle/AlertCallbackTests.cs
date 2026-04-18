// AC-10: AED state machine cycle. AC-11: Alert queue never crashes on empty.
using ImageProcTest.IntegrationTests.Fixtures;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.Lifecycle;

/// <summary>
/// Tests for xpe_aed_configure, xpe_aed_poll_event, xpe_aed_get_status,
/// and the alert queue (get_pending_alert_count / get_pending_alert / clear_alerts).
/// Covers REQ-GUI-IT-027, REQ-GUI-IT-028, REQ-GUI-IT-032, REQ-GUI-IT-033,
/// REQ-GUI-IT-034, AC-10, AC-11.
/// </summary>
[Trait("Category", "Lifecycle")]
[Collection(NativeLibraryCollection.Name)]
public sealed class AlertCallbackTests : IDisposable
{
    private readonly NativeLibraryFixture _fixture;

    public AlertCallbackTests(NativeLibraryFixture fixture)
    {
        _fixture = fixture;
        if (fixture.IsAvailable)
            XpeCommonNative.xpe_init(null);
    }

    // -- AED state machine --

    /// <summary>
    /// REQ-GUI-IT-032: init → aed_configure(null) → aed_get_status returns ARMED (=1).
    /// </summary>
    [Fact]
    public void AedConfigure_WithNullConfig_StatusBecomesArmed()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

        var configResult = XpeCommonNative.xpe_aed_configure(null);
        Assert.Equal(XpeCommonNative.XpeErrorCode.OK, configResult);

        var statusResult = XpeCommonNative.xpe_aed_get_status(out var state);
        Assert.Equal(XpeCommonNative.XpeErrorCode.OK, statusResult);
        Assert.Equal(1, state); // ARMED
    }

    /// <summary>
    /// REQ-GUI-IT-033: aed_configure with bad JSON returns CONFIG_INVALID without mutating AED state.
    /// </summary>
    [Fact]
    public void AedConfigure_MalformedJson_ReturnsConfigInvalidAndStateUnchanged()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

        // Arm first
        XpeCommonNative.xpe_aed_configure(null);
        XpeCommonNative.xpe_aed_get_status(out var stateBefore);

        var result = XpeCommonNative.xpe_aed_configure(TestDataLoader.AedMalformedJson);
        Assert.Equal(XpeCommonNative.XpeErrorCode.CONFIG_INVALID, result);

        XpeCommonNative.xpe_aed_get_status(out var stateAfter);
        Assert.Equal(stateBefore, stateAfter);
    }

    /// <summary>
    /// REQ-GUI-IT-034: aed_poll_event on empty queue returns non-negative status without exception.
    /// </summary>
    [Fact]
    public void AedPollEvent_EmptyQueue_ReturnsNonNegativeAndNoException()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

        var ex = Record.Exception(() =>
        {
            var result = XpeCommonNative.xpe_aed_poll_event(out _, out _, out _);
            // OK (0) or XPE_STATUS_NO_EVENT (1) per REQ-P0-028a — both non-negative.
            Assert.True((int)result >= 0, $"Expected non-negative, got {result}");
        });

        Assert.Null(ex);
    }

    // -- Alert queue --

    /// <summary>
    /// REQ-GUI-IT-027: empty queue → count == 0; get_pending_alert(0) → INVALID_INPUT.
    /// </summary>
    [Fact]
    public void AlertQueue_Empty_CountIsZeroAndFetchReturnsInvalidInput()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

        XpeCommonNative.xpe_clear_alerts();
        var count = XpeCommonNative.xpe_get_pending_alert_count();
        Assert.Equal(0, count);

        var sb = new System.Text.StringBuilder(256);
        var result = XpeCommonNative.xpe_get_pending_alert(0, sb, (UIntPtr)256, out _);
        Assert.Equal(XpeCommonNative.XpeErrorCode.INVALID_INPUT, result);
    }

    /// <summary>
    /// REQ-GUI-IT-028: clear_alerts called 3+ times on empty queue must not throw.
    /// </summary>
    [Fact]
    public void ClearAlerts_CalledRepeatedly_DoesNotThrow()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

        var ex = Record.Exception(() =>
        {
            for (var i = 0; i < 3; i++)
                XpeCommonNative.xpe_clear_alerts();
        });

        Assert.Null(ex);
        Assert.Equal(0, XpeCommonNative.xpe_get_pending_alert_count());
    }

    public void Dispose()
    {
        if (_fixture.IsAvailable)
            XpeCommonNative.xpe_shutdown();
    }
}
