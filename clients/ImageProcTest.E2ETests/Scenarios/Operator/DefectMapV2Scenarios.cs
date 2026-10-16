// GUI-C-249 (#273): the app loads a defect map of format version 2 (SRS-CALIB-FUNC-039, written by the module's generator) and says why when a defect file is damaged.
// UI Automation only: menus by pattern calls, the file dialog by the dialog's own edit and button (see GUI-C-230c). Nothing is sent to whichever window is in front.
using System.Security.Cryptography;
using System.Text;
using Xunit;
using Xunit.Abstractions;
using GuiApp = ImageProcTest.E2ETests.Scenarios.Operator.QuickStartWalkthroughScenarios.GuiApp;

namespace ImageProcTest.E2ETests.Scenarios.Operator;

/// <summary>
/// Four runs that each load a calibration folder whose defect.xcal was damaged in one way, and one control that was rebuilt the same way without the damage. The damaged files are made HERE, from the
/// version-2 set <c>XPE_C249_SET</c> (read only; offset.xcal and gain.xcal are copied, defect.xcal is rewritten) into <c>XPE_C249_WORK</c>, a folder of this session's own.
/// Needs <c>XPE_C231_NATIVE_DIR</c> (DLLs of a build that reads version 2), <c>XPE_C249_SET</c>, <c>XPE_C249_WORK</c> and <c>XPE_C247_RAW</c>. If one is unset the run is skipped with a reason; if they are set but a path is missing it fails.
/// The set lives outside the repository, so CI cannot run these (XPE-SKIP-ALLOWED:273).
/// </summary>
public sealed class DefectMapV2Scenarios(ITestOutputHelper output)
{
    private static string? Env(string name) => Environment.GetEnvironmentVariable(name) is { Length: > 0 } v ? v : null;

    private const int HeaderSize = 152, ConfigLenAt = 104, PayloadLenAt = 112, Sha256At = 120;
    private const int Side = 3072;

    public enum Damage { Truncated, UnsupportedVersion, LineOutOfRange, ControlLineInRange }

    [SkippableFact]
    public void D1_ATruncatedDefectFile_IsRefusedWithItsReason() => Run(Damage.Truncated, "XPE_ERR_XCAL_TRUNCATED");

    [SkippableFact]
    public void D2_AnUnsupportedVersionNumber_IsRefusedWithItsReason() => Run(Damage.UnsupportedVersion, "XPE_ERR_XCAL_VERSION");

    [SkippableFact]
    public void D3_ALineRecordOutsideTheFrame_IsRefusedWithItsReason() => Run(Damage.LineOutOfRange, "XPE_ERR_XCAL_EXTENSION", "outside the frame");

    [SkippableFact]
    public void D4_Control_TheSameLineRecordInsideTheFrame_IsAccepted() => Run(Damage.ControlLineInRange);

    /// <summary>The chain line says this when the module refused the calibration (the state the refusal tests wait for and assert).</summary>
    private const string RefusedState = "preprocess=RequestedNotApplied";

    private void Run(Damage damage, params string[] reasonTokens)
    {
        var native = Env("XPE_C231_NATIVE_DIR");
        var set = Env("XPE_C249_SET");
        var work = Env("XPE_C249_WORK");
        var raw = Env("XPE_C247_RAW");
        Skip.If(string.IsNullOrEmpty(native) || string.IsNullOrEmpty(set) || string.IsNullOrEmpty(work) || string.IsNullOrEmpty(raw),
            "XPE_C231_NATIVE_DIR/XPE_C249_SET/XPE_C249_WORK/XPE_C247_RAW not set - the version-2 set is not in the repository. XPE-SKIP-ALLOWED:273");
        Assert.True(Directory.Exists(set!), "the version-2 set does not exist: " + set);
        Assert.True(File.Exists(raw!), "the raw frame does not exist: " + raw);

        var folder = Path.Combine(work!, damage.ToString());
        if (Directory.Exists(folder)) Directory.Delete(folder, recursive: true);
        Directory.CreateDirectory(folder);
        foreach (var name in new[] { "offset.xcal", "gain.xcal" }) File.Copy(Path.Combine(set!, name), Path.Combine(folder, name));
        var original = File.ReadAllBytes(Path.Combine(set!, "defect.xcal"));
        var damaged = MakeDamaged(original, damage);
        File.WriteAllBytes(Path.Combine(folder, "defect.xcal"), damaged);
        output.WriteLine($"   damage: {damage}; defect.xcal {original.Length} -> {damaged.Length} bytes; folder {folder}");

        using var app = GuiApp.Launch(output, native!, extraArguments:
            ["--automation-backend", "Native", "--automation-calib", folder, "--automation-width", Side.ToString(), "--automation-height", Side.ToString()]);
        app.Menu("FileMenu");
        app.InvokeMenuItemThenDialog("OpenRawMenuItem", raw!, confirmId: "1");
        app.WaitFor(() => app.AnyTextContains($"RAW {Side}x{Side}"), "the image summary appears", 90);
        var accepted = reasonTokens.Length == 0;
        if (accepted)
        {
            app.WaitFor(() => app.Read("ChainStatusText").Contains("preprocess=Applied", StringComparison.Ordinal), "preprocess=Applied", 180);
            Thread.Sleep(3000);
        }
        else
        {
            // Wait for the refusal itself (a UIA read of the chain line, up to 60 s), not for a fixed time.
            app.WaitFor(() => app.Read("ChainStatusText").Contains(RefusedState, StringComparison.Ordinal), "the chain line says " + RefusedState, 60);
        }

        var chain = app.Read("ChainStatusText");
        var hud = app.Read("HudCalibStages");
        var status = app.Read("StatusBarText");
        app.Menu("ViewMenu");
        if (!app.IsToggled("ShowLogsPanelMenuItem")) app.InvokeMenuItem("ShowLogsPanelMenuItem");
        app.ClickButtonByName("Log");
        Thread.Sleep(1000);
        var log = app.ListItems("LogListBox");
        output.WriteLine($"   chain: {chain}");
        output.WriteLine($"   HUD: '{hud}'");
        output.WriteLine($"   status: {status}");
        foreach (var line in log.Take(40)) output.WriteLine("   LOG " + line);

        var everything = string.Join("\n", new[] { chain, hud, status }.Concat(log));
        if (accepted)
        {
            Assert.Contains("preprocess=Applied", chain, StringComparison.Ordinal);
            var serious = log.Where(l => l.Contains("ALERT ", StringComparison.Ordinal) && !l.Contains("ALERT INFO", StringComparison.Ordinal) && !l.Contains("Exposure deviation:", StringComparison.Ordinal)).ToList();
            Assert.True(serious.Count == 0, "the control raised alert(s): " + string.Join(" || ", serious));
            return;
        }
        Assert.Contains(RefusedState, chain, StringComparison.Ordinal);
        Assert.DoesNotContain("preprocess=Applied", chain, StringComparison.Ordinal);
        Assert.True(log.Any(l => l.Contains("ALERT ERROR", StringComparison.Ordinal)) || everything.Contains("fail", StringComparison.OrdinalIgnoreCase),
            "the refusal is not visible as an error (no ALERT ERROR line and no 'fail' on the screen)");
        foreach (var token in reasonTokens)
            Assert.True(everything.Contains(token, StringComparison.Ordinal), $"the reason '{token}' is not visible anywhere on the screen or in the log");
    }

    /// <summary>The damaged copy of a version-2 defect file. Layout: header 152 bytes, config JSON, payload; the payload is run-length coded ((value u8, count u32) runs) and holds the defect plane,
    /// the imaging-area plane and the extension block; the header's sha256 covers config + payload.</summary>
    internal static byte[] MakeDamaged(byte[] file, Damage damage)
    {
        switch (damage)
        {
            case Damage.Truncated:
                return file[..100_000];
            case Damage.UnsupportedVersion:
                var v = (byte[])file.Clone();
                Assert.Equal(2u, BitConverter.ToUInt32(v, 4));
                BitConverter.GetBytes(3u).CopyTo(v, 4);
                return v;
        }

        var configLen = checked((int)BitConverter.ToUInt64(file, ConfigLenAt));
        var payloadLen = checked((int)BitConverter.ToUInt64(file, PayloadLenAt));
        var config = Encoding.UTF8.GetString(file, HeaderSize, configLen);
        var plain = RleDecode(file.AsSpan(HeaderSize + configLen, payloadLen));
        var planes = 2 * Side * Side;
        var extLen = plain.Length - planes;
        Assert.Contains($"\"defect_ext_len\":{extLen}", config, StringComparison.Ordinal);
        Assert.Contains($"\"xcal_raw_payload_len\":{plain.Length}", config, StringComparison.Ordinal);
        var ext = plain.AsSpan(planes).ToArray();
        Assert.Equal("XDEX", Encoding.ASCII.GetString(ext, 0, 4));
        Assert.Equal(0u, BitConverter.ToUInt32(ext, 8));    // n_lines
        Assert.Equal(0u, BitConverter.ToUInt32(ext, 16));   // n_candidates
        Assert.Equal(0u, BitConverter.ToUInt32(ext, 20));   // n_segments
        var blobs = BitConverter.ToUInt32(ext, 12);
        Assert.Equal(32 + 20 * (int)blobs, ext.Length);

        // One confirmed row group of one line with one segment, on row 100 (inside) or row 5000 (the frame has 3072 rows).
        var row = damage == Damage.LineOutOfRange ? 5000u : 100u;
        var extHead = ext.AsSpan(0, 32).ToArray();
        BitConverter.GetBytes(1u).CopyTo(extHead, 8);    // n_lines
        BitConverter.GetBytes(1u).CopyTo(extHead, 20);   // n_segments
        var newExt = new List<byte>(ext.Length + 64);
        newExt.AddRange(extHead);
        foreach (var f in new uint[] { 0, row, 1, 0, 100, 1, 0, 0, 1 }) newExt.AddRange(BitConverter.GetBytes(f));   // orientation index width start end causes flags reason correction
        newExt.AddRange(BitConverter.GetBytes(1.0f));    // strength
        newExt.AddRange(BitConverter.GetBytes(0.5f));    // persistence
        newExt.AddRange(BitConverter.GetBytes(0u));      // segment_first
        newExt.AddRange(BitConverter.GetBytes(1u));      // segment_count
        newExt.AddRange(ext.AsSpan(32).ToArray());       // the blobs
        foreach (var f in new uint[] { row, 0, 100 }) newExt.AddRange(BitConverter.GetBytes(f));   // the segment: line start end

        var newPlain = new byte[planes + newExt.Count];
        Array.Copy(plain, newPlain, planes);
        newExt.CopyTo(newPlain, planes);
        var newConfig = config
            .Replace($"\"defect_ext_len\":{extLen}", $"\"defect_ext_len\":{newExt.Count}", StringComparison.Ordinal)
            .Replace($"\"xcal_raw_payload_len\":{plain.Length}", $"\"xcal_raw_payload_len\":{newPlain.Length}", StringComparison.Ordinal);
        var configBytes = Encoding.UTF8.GetBytes(newConfig);
        var payload = RleEncode(newPlain);

        var header = file.AsSpan(0, HeaderSize).ToArray();
        BitConverter.GetBytes((ulong)configBytes.Length).CopyTo(header, ConfigLenAt);
        BitConverter.GetBytes((ulong)payload.Length).CopyTo(header, PayloadLenAt);
        SHA256.HashData(configBytes.Concat(payload).ToArray()).CopyTo(header, Sha256At);
        return header.Concat(configBytes).Concat(payload).ToArray();
    }

    internal static byte[] RleDecode(ReadOnlySpan<byte> runs)
    {
        Assert.Equal(0, runs.Length % 5);
        long total = 0;
        for (var i = 0; i < runs.Length; i += 5) total += BitConverter.ToUInt32(runs.Slice(i + 1, 4));
        var output = new byte[total];
        long at = 0;
        for (var i = 0; i < runs.Length; i += 5)
        {
            var count = BitConverter.ToUInt32(runs.Slice(i + 1, 4));
            Array.Fill(output, runs[i], (int)at, (int)count);
            at += count;
        }
        return output;
    }

    internal static byte[] RleEncode(byte[] data)
    {
        var runs = new List<byte>();
        for (var i = 0; i < data.Length;)
        {
            var j = i + 1;
            while (j < data.Length && data[j] == data[i]) j++;
            runs.Add(data[i]);
            runs.AddRange(BitConverter.GetBytes((uint)(j - i)));
            i = j;
        }
        return runs.ToArray();
    }

    [Fact]
    public void D0_TheDamageBuilder_RoundTripsTheRunLengthCoding()
    {
        // A self-check that needs no app and no data: the coder the damaged files rely on decodes what it encoded.
        var data = new byte[] { 0, 0, 0, 7, 7, 1, 0, 0, 255 };
        Assert.Equal(data, RleDecode(RleEncode(data)));
    }
}
