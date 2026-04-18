// AC-11: Alert queue never crashes on empty.
using ImageProcTest.IntegrationTests.Fixtures;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.Lifecycle;

/// <summary>
/// Tests for the alert queue (get_pending_alert_count / get_pending_alert / clear_alerts).
/// Covers REQ-GUI-IT-027, REQ-GUI-IT-028, AC-11.
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
