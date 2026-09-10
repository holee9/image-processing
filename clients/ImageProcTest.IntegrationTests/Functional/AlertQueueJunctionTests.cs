// #110 / SRS-ALERT-007: the real native queue, drained by the real gui drain logic.
using System.Text;
using ImageProcTest.IntegrationTests.Fixtures;
using ImageProcTest.IntegrationTests.PInvoke;
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Joint between QA-A-28 (the native overflow policy) and GUI-C-21 (the gui drain).
///
/// GUI-C-19 and GUI-C-21 both verified their halves against strings and a fake reader, and both
/// reported the same gap: nobody had run the real queue through the real drain. This class closes
/// that — every case here calls xpe_common.dll and feeds NativeAlertDrain the actual reader
/// RealXpeBackend uses.
///
/// It also pins the return contract of xpe_get_pending_alert, which GUI-C-21 could only assume.
/// </summary>
[Trait("Category", "Functional")]
[Collection(NativeLibraryCollection.Name)]
public sealed class AlertQueueJunctionTests : IDisposable
{
    private readonly NativeLibraryFixture _fixture;
    private static readonly DateTimeOffset Now = new(2026, 9, 10, 12, 0, 0, TimeSpan.Zero);

    public AlertQueueJunctionTests(NativeLibraryFixture fixture)
    {
        _fixture = fixture;
        if (fixture.IsAvailable)
        {
            XpeCommonNative.xpe_init(null);
            XpeCommonNative.xpe_clear_alerts();
        }
    }

    /// <summary>
    /// api-spec 5.17: 64 Info alerts fill the queue; the 65th costs two evictions — one to make
    /// room for it, one to make room for the loss alert itself — so the notice reads N=2 and the
    /// effective payload capacity is 63.
    ///
    /// Drained through NativeAlertDrain, that must arrive as 63 Info entries in push order plus one
    /// Error entry carrying the formatted overflow sentence.
    /// </summary>
    [SkippableFact]
    public void RealOverflow_DrainsAs63InfoInOrder_PlusOneErrorLossNotice()
    {
        SkipHelper.SkipIf(!_fixture.IsAvailable, _fixture.SkipReason);

        XpeCommonNative.xpe_clear_alerts();
        for (var i = 1; i <= 65; i++)
        {
            XpeCommonNative.xpe_alert_push($"payload alert {i}", 0);
        }

        var pending = XpeCommonNative.xpe_get_pending_alert_count();
        Assert.Equal(64, pending);

        var drained = NativeAlertDrain.Drain(pending, ReadNativeAlert, Now);

        var payload = drained.Where(a => a.Code == NativeAlertDrain.NativeAlertCode).ToList();
        var loss = Assert.Single(drained, a => a.Code == NativeAlertDrain.OverflowCode);

        Assert.Equal(63, payload.Count);
        Assert.All(payload, a => Assert.Equal("INFO", a.Severity));

        // Two evictions took the two oldest: 1 and 2. What survives is 3..65, in push order.
        Assert.Equal(
            Enumerable.Range(3, 63).Select(i => $"payload alert {i}"),
            payload.Select(a => a.Message));

        Assert.Equal("ERROR", loss.Severity);
        Assert.Equal("2 alerts were dropped because the alert queue overflowed.", loss.Message);

        XpeCommonNative.xpe_clear_alerts();
        Assert.Equal(0, XpeCommonNative.xpe_get_pending_alert_count());
    }

    /// <summary>
    /// Contract observation 1: an empty queue rejects index 0 with INVALID_INPUT — it does not
    /// return OK with an empty string. The drain never asks, because it reads the count first.
    /// </summary>
    [SkippableFact]
    public void EmptyQueue_IndexZero_ReturnsInvalidInput()
    {
        SkipHelper.SkipIf(!_fixture.IsAvailable, _fixture.SkipReason);

        XpeCommonNative.xpe_clear_alerts();

        var buffer = new StringBuilder(256);
        var code = XpeCommonNative.xpe_get_pending_alert(0, buffer, (UIntPtr)256, out _);

        Assert.Equal(XpeCommonNative.XpeErrorCode.INVALID_INPUT, code);
    }

    /// <summary>
    /// Contract observation 2: an index past the end is INVALID_INPUT, and a negative index too.
    /// </summary>
    [SkippableFact]
    public void IndexOutOfRange_ReturnsInvalidInput()
    {
        SkipHelper.SkipIf(!_fixture.IsAvailable, _fixture.SkipReason);

        XpeCommonNative.xpe_clear_alerts();
        XpeCommonNative.xpe_alert_push("only entry", 1);

        var buffer = new StringBuilder(256);
        Assert.Equal(
            XpeCommonNative.XpeErrorCode.INVALID_INPUT,
            XpeCommonNative.xpe_get_pending_alert(5, buffer, (UIntPtr)256, out _));
        Assert.Equal(
            XpeCommonNative.XpeErrorCode.INVALID_INPUT,
            XpeCommonNative.xpe_get_pending_alert(-1, buffer, (UIntPtr)256, out _));

        XpeCommonNative.xpe_clear_alerts();
    }

    /// <summary>
    /// Contract observation 3: a buffer shorter than the message returns BUFFER_TOO_SMALL rather
    /// than truncating, so the drain's grow-and-retry actually fires against the real DLL and
    /// recovers the full text.
    ///
    /// This is the assumption GUI-C-21 could not test. It also contradicts the xpe_error.h prose,
    /// which says the message "is truncated and null-terminated" — reported, not changed
    /// (modules/ is not this lane's to edit).
    /// </summary>
    [SkippableFact]
    public void ShortBuffer_ReturnsBufferTooSmall_AndTheDrainRecoversByRetrying()
    {
        SkipHelper.SkipIf(!_fixture.IsAvailable, _fixture.SkipReason);

        var message = new string('x', 300);   // longer than the drain's first buffer (256)
        XpeCommonNative.xpe_clear_alerts();
        XpeCommonNative.xpe_alert_push(message, 2);

        var tiny = new StringBuilder(8);
        Assert.Equal(
            XpeCommonNative.XpeErrorCode.BUFFER_TOO_SMALL,
            XpeCommonNative.xpe_get_pending_alert(0, tiny, (UIntPtr)8, out _));

        var entry = Assert.Single(NativeAlertDrain.Drain(1, ReadNativeAlert, Now));
        Assert.Equal(message, entry.Message);
        Assert.Equal("ERROR", entry.Severity);

        XpeCommonNative.xpe_clear_alerts();
    }

    /// <summary>The reader RealXpeBackend supplies, against the real DLL.</summary>
    private static int ReadNativeAlert(int index, int bufferLength, out string? message, out int severity)
    {
        var buffer = new StringBuilder(bufferLength);
        var code = XpeCommonNative.xpe_get_pending_alert(index, buffer, (UIntPtr)bufferLength, out severity);
        message = code == XpeCommonNative.XpeErrorCode.OK ? buffer.ToString() : null;
        return (int)code;
    }

    public void Dispose()
    {
        if (_fixture.IsAvailable)
        {
            XpeCommonNative.xpe_clear_alerts();
        }
    }
}
