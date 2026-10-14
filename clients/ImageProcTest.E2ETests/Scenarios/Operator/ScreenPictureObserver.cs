// GUI-C-236c (Codex #191 finding 2): an observation of the SCREEN that does not depend on anything the app says about itself.
//
// The app records "open -> the second WPF composition tick after the processed picture was handed to the viewport". That is a proxy: CompositionTarget.Rendering is raised BEFORE the frame is rendered,
// so it is not proof that the picture was drawn. This file watches the pixels instead. From the moment the open is confirmed, a background thread copies the viewport's area off the screen again and
// again (a screen copy: it reads what the desktop shows; it sends no key, no mouse, no message to the app) and keeps a coarse grid of grey values with the time of each copy. Afterwards the captures are analysed:
// the LAST capture is the picture the screen settled on (F); the picture appeared when the final unbroken run of captures that equal F began. "The final run" and not "the first capture that equals F"
// because on the second and later opens of the same file the screen still shows the previous open's finished picture while the new file is being read (and a file dialog can cover part of it as it closes).
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using Xunit;

namespace ImageProcTest.E2ETests.Scenarios.Operator;

/// <summary>One screen copy: when it started and ended (Stopwatch ticks) and a grid of grey values (B+G+R)/3.</summary>
internal sealed record ScreenSample(long Before, long After, byte[] Gray);

/// <summary>
/// Copies the viewport's rectangle off the SCREEN in a loop until stopped (a screen copy: it reads what the desktop shows). PrintWindow was tried first and returns an all-white picture for this WPF window,
/// so the pixels are read from the desktop - which means nothing may stand in front of the viewport. On a shared desktop something often does, so the app window is first put above the others WITHOUT taking the
/// focus (SetWindowPos, HWND_TOPMOST, SWP_NOACTIVATE: window order only, no key, no mouse, no message to the app); and the analysis refuses a run whose screen never changed.
/// </summary>
internal sealed class ScreenPoller
{
    [DllImport("user32.dll")]
    private static extern bool SetWindowPos(IntPtr window, IntPtr insertAfter, int x, int y, int width, int height, uint flags);

    private static readonly IntPtr HwndTopmost = new(-1);
    private static readonly IntPtr HwndNoTopmost = new(-2);
    private const uint SwpNoSize = 0x0001;
    private const uint SwpNoMove = 0x0002;
    private const uint SwpNoActivate = 0x0010;

    public const int Columns = 48;
    public const int Rows = 48;

    private readonly IntPtr _window;
    private readonly Rectangle _area;
    private readonly List<ScreenSample> _samples = [];
    private readonly Thread _thread;
    private volatile bool _stop;
    private Bitmap? _lastShot;

    /// <summary>Stopwatch ticks at the moment the poller started (the caller starts it immediately before confirming the open).</summary>
    public long StartedAt { get; } = Stopwatch.GetTimestamp();

    private ScreenPoller(IntPtr window, Rectangle area)
    {
        _window = window;
        _area = area;
        _thread = new Thread(Loop) { IsBackground = true, Name = "screen-picture-poller" };
    }

    /// <summary>Puts the app window above the others without activating it. Call once, well before the observation starts.</summary>
    public static void RaiseWithoutFocus(IntPtr window) => SetWindowPos(window, HwndTopmost, 0, 0, 0, 0, SwpNoMove | SwpNoSize | SwpNoActivate);

    /// <param name="window">The app's main window handle.</param>
    /// <param name="area">The viewport's rectangle in SCREEN coordinates (UI Automation's bounds).</param>
    public static ScreenPoller Start(IntPtr window, Rectangle area)
    {
        Assert.True(area.Width > 100 && area.Height > 100, $"the viewport area {area} is too small to watch");
        var poller = new ScreenPoller(window, area);
        poller._thread.Start();
        return poller;
    }

    public IReadOnlyList<ScreenSample> Stop(out Bitmap? lastShot)
    {
        _stop = true;
        _thread.Join();
        SetWindowPos(_window, HwndNoTopmost, 0, 0, 0, 0, SwpNoMove | SwpNoSize | SwpNoActivate);
        lastShot = _lastShot;
        return _samples;
    }

    private void Loop()
    {
        using var bitmap = new Bitmap(_area.Width, _area.Height, PixelFormat.Format32bppArgb);
        using var graphics = Graphics.FromImage(bitmap);
        while (!_stop)
        {
            var before = Stopwatch.GetTimestamp();
            graphics.CopyFromScreen(_area.Location, Point.Empty, _area.Size);
            var after = Stopwatch.GetTimestamp();
            var data = bitmap.LockBits(new Rectangle(0, 0, bitmap.Width, bitmap.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            var gray = new byte[Columns * Rows];
            try
            {
                for (var row = 0; row < Rows; row++)
                {
                    var y = (int)((row + 0.5) * bitmap.Height / Rows);
                    for (var column = 0; column < Columns; column++)
                    {
                        var x = (int)((column + 0.5) * bitmap.Width / Columns);
                        var pixel = Marshal.ReadInt32(data.Scan0, (y * data.Stride) + (x * 4));
                        gray[(row * Columns) + column] = (byte)(((pixel & 0xFF) + ((pixel >> 8) & 0xFF) + ((pixel >> 16) & 0xFF)) / 3);
                    }
                }
            }
            finally
            {
                bitmap.UnlockBits(data);
            }

            _samples.Add(new ScreenSample(before, after, gray));
            Thread.Sleep(1);
        }

        _lastShot = (Bitmap)bitmap.Clone();
    }
}

/// <summary>What the captures of one open say: when the screen settled on its final picture.</summary>
internal sealed record ScreenSettleResult(bool Settled, string Why, double LatestMs, double EarliestMs, double LastBeforeMismatch, int Captures, int FinalRunLength, double FinalRunMs);

internal static class ScreenSettle
{
    /// <summary>Two grey values within this many levels are the same value (a screen copy of an unchanged picture is exact; this only absorbs a re-rendered edge).</summary>
    public const int Tolerance = 2;

    /// <summary>A capture equals the final picture when at least this share of the grid points does.</summary>
    public const double MatchShare = 0.995;

    public static double Match(byte[] a, byte[] b)
    {
        var same = 0;
        for (var i = 0; i < a.Length; i++)
        {
            if (Math.Abs(a[i] - b[i]) <= Tolerance)
            {
                same++;
            }
        }

        return same / (double)a.Length;
    }

    /// <summary>
    /// <paramref name="startedAt"/> is the Stopwatch time the open was confirmed. The picture is judged to have appeared in the interval ending at the FIRST capture of the final run (<c>LatestMs</c>: no later than)
    /// and starting at the beginning of the capture before it (<c>EarliestMs</c>: not yet there). Not settled when the screen never left the final picture (nothing was observed to appear), when it was not
    /// stable for <paramref name="minimumStableMs"/> at the end, or when there were too few captures to say.
    /// </summary>
    public static ScreenSettleResult Analyse(IReadOnlyList<ScreenSample> samples, long startedAt, double minimumStableMs)
    {
        static ScreenSettleResult No(string why, int captures) => new(false, why, double.NaN, double.NaN, double.NaN, captures, 0, 0);

        if (samples.Count < 4)
        {
            return No($"only {samples.Count} captures", samples.Count);
        }

        var final = samples[^1].Gray;
        var matches = samples.Select(s => Match(s.Gray, final) >= MatchShare).ToArray();
        var start = matches.Length - 1;
        while (start > 0 && matches[start - 1])
        {
            start--;
        }

        if (start == 0)
        {
            return No("every capture already equalled the final picture: nothing was seen to appear", samples.Count);
        }

        double Ms(long ticks) => (ticks - startedAt) * 1000.0 / Stopwatch.Frequency;
        var runMs = (samples[^1].After - samples[start].Before) * 1000.0 / Stopwatch.Frequency;
        if (runMs < minimumStableMs)
        {
            return No($"the final picture was on screen for only {runMs:0} ms at the end (needs {minimumStableMs:0})", samples.Count);
        }

        var lastBefore = 1.0 - Match(samples[start - 1].Gray, final);
        return new ScreenSettleResult(true, "ok", Ms(samples[start].After), Ms(samples[start - 1].Before), lastBefore, samples.Count, samples.Count - start, runMs);
    }
}

/// <summary>The analysis, on constructed captures: it runs on every runner (it needs no app and no screen).</summary>
public class ScreenSettleTests
{
    private static byte[] Picture(byte value) => Enumerable.Repeat(value, 64).ToArray();

    private static ScreenSample At(int ms, byte value)
    {
        var ticks = ms * Stopwatch.Frequency / 1000;
        return new ScreenSample(ticks, ticks + (2 * Stopwatch.Frequency / 1000), Picture(value));
    }

    [Fact]
    public void ThePictureThatReplacesAStaleOne_IsDatedByTheFinalRun_NotByTheFirstEqualCapture()
    {
        // 200 = the finished picture of the PREVIOUS open (still on screen while the file is read), 40 = the unprocessed picture, 200 = the new finished picture (equal to the stale one)
        var samples = new[] { At(0, 200), At(50, 200), At(100, 90), At(150, 200), At(200, 40), At(600, 40), At(900, 40), At(1000, 200), At(1050, 200), At(1100, 200), At(1150, 200), At(1700, 200) };
        var result = ScreenSettle.Analyse(samples, 0, 500);
        Assert.True(result.Settled, result.Why);
        Assert.InRange(result.LatestMs, 1000, 1005);     // the first capture of the final run, not the stale ones at 0/50/150 ms
        Assert.InRange(result.EarliestMs, 900, 905);     // the capture before it still showed the unprocessed picture
        Assert.InRange(result.LastBeforeMismatch, 0.99, 1.0);   // the picture before was entirely different
    }

    [Fact]
    public void AScreenThatNeverLeftTheFinalPicture_IsNotSettled()
    {
        var samples = Enumerable.Range(0, 20).Select(i => At(i * 100, 200)).ToArray();
        var result = ScreenSettle.Analyse(samples, 0, 500);
        Assert.False(result.Settled);
        Assert.Contains("nothing was seen to appear", result.Why, StringComparison.Ordinal);
    }

    [Fact]
    public void AFlickerAfterTheFirstAppearance_MovesTheSettledTimeToTheEnd()
    {
        var samples = new[] { At(0, 10), At(100, 200), At(200, 200), At(300, 90), At(400, 200), At(500, 200), At(1200, 200) };
        var result = ScreenSettle.Analyse(samples, 0, 500);
        Assert.True(result.Settled, result.Why);
        Assert.InRange(result.LatestMs, 400, 405);       // not 100: the screen left the picture at 300 ms
    }

    [Fact]
    public void ASettleTooShortAtTheEnd_IsNotTrusted()
    {
        var samples = new[] { At(0, 10), At(100, 10), At(200, 10), At(300, 200), At(400, 200) };
        var result = ScreenSettle.Analyse(samples, 0, 500);
        Assert.False(result.Settled);
        Assert.Contains("on screen for only", result.Why, StringComparison.Ordinal);
    }

    [Fact]
    public void AnEdgeThatMovesByTheToleranceIsTheSamePicture_AndOneBeyondIsNot()
    {
        var a = Picture(100);
        var within = Picture(100 + ScreenSettle.Tolerance);
        var beyond = Picture(100 + ScreenSettle.Tolerance + 1);
        Assert.Equal(1.0, ScreenSettle.Match(a, within));
        Assert.Equal(0.0, ScreenSettle.Match(a, beyond));
    }
}

/// <summary>
/// GUI-C-242: the approved U8 rule of milestone 3 (leader decision 2026-10-14, main 5537f4e5): the MEDIAN of the five proxy values (open -> second composition tick after the processed picture was handed to the
/// viewport) must be at most <see cref="ProxyLimitMs"/> - half of the 3000 ms PERF-P1-001 limit, because the end point is a proxy and not proof of presentation. Pure on its input so the boundary is tested.
/// </summary>
internal static class U8Verdict
{
    public const double ProxyLimitMs = 1500;

    public readonly record struct Result(double MedianMs, bool Passes);

    public static Result OfProxyMedian(IReadOnlyList<double> fiveValuesMs)
    {
        if (fiveValuesMs.Count != 5)
        {
            throw new ArgumentException($"the U8 rule takes the median of FIVE opens, got {fiveValuesMs.Count}", nameof(fiveValuesMs));
        }

        var median = fiveValuesMs.OrderBy(x => x).ElementAt(2);
        return new Result(median, median <= ProxyLimitMs);
    }
}

/// <summary>The verdict function on constructed values: it runs on every runner (no app, no data).</summary>
public class U8VerdictTests
{
    [Fact]
    public void TheMedianOfFive_At1500_Passes_And1501_Fails()
    {
        Assert.True(U8Verdict.OfProxyMedian([900, 1200, 1500, 1800, 2400]).Passes);
        Assert.False(U8Verdict.OfProxyMedian([900, 1200, 1501, 1800, 2400]).Passes);
        Assert.Equal(1500, U8Verdict.OfProxyMedian([1500, 1500, 1500, 1500, 1500]).MedianMs);
    }

    [Fact]
    public void TheMedian_NotTheMeanOrTheMax_DecidesIt()
    {
        // one slow first open (JIT) does not fail a run whose median is fast; a fast outlier does not rescue a slow run
        Assert.True(U8Verdict.OfProxyMedian([2900, 1200, 1100, 1300, 1250]).Passes);
        Assert.False(U8Verdict.OfProxyMedian([100, 200, 1600, 1700, 1800]).Passes);
    }

    [Fact]
    public void TheOrderOfTheOpens_DoesNotMatter()
    {
        Assert.Equal(U8Verdict.OfProxyMedian([1300, 900, 1600, 1100, 1400]), U8Verdict.OfProxyMedian([1600, 1400, 1300, 1100, 900]));
    }

    [Fact]
    public void AnythingButFiveValues_IsRefused()
    {
        Assert.Throws<ArgumentException>(() => U8Verdict.OfProxyMedian([1000, 1000, 1000, 1000]));
        Assert.Throws<ArgumentException>(() => U8Verdict.OfProxyMedian([1000, 1000, 1000, 1000, 1000, 1000]));
    }
}
