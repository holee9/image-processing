// #110 / SRS-ALERT-007: the gui drain turns a native alert queue into displayable entries.
using ImageProcTest.Models;
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Regression for <see cref="NativeAlertDrain"/> — the decision logic RealXpeBackend runs after a
/// processing call. The queue is faked through the reader delegate, so these cases run without
/// xpe_common.dll and without QA-A-28 being finished.
///
/// What is NOT covered here: that the native layer actually emits these strings. That joint is
/// A-28's, and it needs one case fed by a real queue once both sides are merged (GUI-C-19 §5).
/// </summary>
[Trait("Category", "Functional")]
public sealed class NativeAlertDrainTests
{
    private static readonly DateTimeOffset Now = new(2026, 9, 10, 12, 0, 0, TimeSpan.Zero);

    /// <summary>An empty queue drains to nothing — no placeholder entry, no throw.</summary>
    [Fact]
    public void EmptyQueue_ProducesNoEntries()
    {
        var drained = NativeAlertDrain.Drain(0, FailIfCalled, Now);

        Assert.Empty(drained);
    }

    /// <summary>Two queued alerts arrive in queue order, with their severities mapped, not flattened.</summary>
    [Fact]
    public void TwoAlerts_PreserveQueueOrderAndSeverity()
    {
        var queue = new (string Message, int Severity)[]
        {
            ("first alert", 0),
            ("second alert", 1),
        };

        var drained = NativeAlertDrain.Drain(queue.Length, Reader(queue), Now);

        Assert.Equal(["first alert", "second alert"], drained.Select(a => a.Message));
        Assert.Equal(["INFO", "WARN"], drained.Select(a => a.Severity));
    }

    /// <summary>
    /// §5.17: the loss notice renders at Error and carries its own code, even when the queue
    /// reported it as Info. Severity is never lowered; the count becomes a sentence.
    /// </summary>
    [Fact]
    public void OverflowAlert_RendersAsError_WithFormattedMessage()
    {
        var queue = new (string Message, int Severity)[]
        {
            ("alert queue overflow: 7 alert(s) dropped", 0),
        };

        var entry = Assert.Single(NativeAlertDrain.Drain(1, Reader(queue), Now));

        Assert.Equal("ERROR", entry.Severity);
        Assert.Equal(NativeAlertDrain.OverflowCode, entry.Code);
        Assert.Contains("7 alerts were dropped", entry.Message);
    }

    /// <summary>
    /// BUFFER_TOO_SMALL grows the buffer and retries once; the retry's text is what gets displayed.
    /// </summary>
    [Fact]
    public void BufferTooSmall_RetriesOnceWithALargerBuffer()
    {
        var attempts = new List<int>();
        int Read(int index, int bufferLength, out string? message, out int severity)
        {
            attempts.Add(bufferLength);
            severity = 2;
            if (bufferLength < NativeAlertDrain.RetryBufferLength)
            {
                message = null;
                return NativeAlertDrain.BufferTooSmall;
            }

            message = "a very long alert";
            return NativeAlertDrain.Ok;
        }

        var entry = Assert.Single(NativeAlertDrain.Drain(1, Read, Now));

        Assert.Equal([NativeAlertDrain.InitialBufferLength, NativeAlertDrain.RetryBufferLength], attempts);
        Assert.Equal("a very long alert", entry.Message);
    }

    /// <summary>
    /// A read that fails on both attempts still yields one entry. Dropping it would erase the fact
    /// that an alert existed — the failure this whole card exists to prevent.
    /// </summary>
    [Fact]
    public void UnreadableAlert_IsKeptAsAPlaceholder_NotDropped()
    {
        static int AlwaysTooSmall(int index, int bufferLength, out string? message, out int severity)
        {
            message = null;
            severity = 0;
            return NativeAlertDrain.BufferTooSmall;
        }

        var entry = Assert.Single(NativeAlertDrain.Drain(1, AlwaysTooSmall, Now));

        Assert.Equal(NativeAlertDrain.UnreadableMessage, entry.Message);
        Assert.Equal("ERROR", entry.Severity);
    }

    /// <summary>An unknown native severity value is not quietly downgraded to Info.</summary>
    [Fact]
    public void UnknownSeverity_IsNotLowered()
    {
        Assert.Equal("ERROR", NativeAlertDrain.MapSeverity(99));
        Assert.Equal("ERROR", NativeAlertDrain.MapSeverity(-1));
    }

    private static NativeAlertDrain.ReadAlert Reader((string Message, int Severity)[] queue) =>
        (int index, int bufferLength, out string? message, out int severity) =>
        {
            message = queue[index].Message;
            severity = queue[index].Severity;
            return NativeAlertDrain.Ok;
        };

    private static int FailIfCalled(int index, int bufferLength, out string? message, out int severity)
    {
        Assert.Fail("The reader must not be called when the queue reports no pending alerts.");
        message = null;
        severity = 0;
        return NativeAlertDrain.Ok;
    }
}
