using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using ImageProcTest.Services.Native;

internal static class PreCli
{
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern bool SetDllDirectory(string path);
    static void Ok(int code, string operation) { if (code != 0) throw new InvalidOperationException($"{operation} exit={code}"); }
    public static void Compare(string root, string cal, string raw, string gui, string evidence)
    {
        Directory.CreateDirectory(evidence);
        string native = Path.Combine(root, "build/gui-c255-native/bin");
        if (!SetDllDirectory(native)) throw new InvalidOperationException("SetDllDirectory failed");
        NativeLibrary.SetDllImportResolver(typeof(PreCli).Assembly, (name, _, _) =>
            name.StartsWith("xpe_", StringComparison.Ordinal) ? NativeLibrary.Load(Path.Combine(native, name)) : IntPtr.Zero);
        byte[] bytes = File.ReadAllBytes(raw);
        var input = default(XpeImageBufferNative); var output = default(XpeImageBufferNative);
        Ok(XpePreprocessNative.xpe_preprocess_init(null), "init");
        try
        {
            Ok(XpePreprocessNative.xpe_calib_load_offset(Path.Combine(cal, "offset.xcal")), "load offset");
            Ok(XpePreprocessNative.xpe_calib_load_gain(Path.Combine(cal, "gain.xcal")), "load gain");
            Ok(XpePreprocessNative.xpe_calib_load_defect_map(Path.Combine(cal, "defect.xcal")), "load defect");
            Ok(XpeCommonNative.xpe_alloc_image(3072, 2560, XpePixelFormatNative.UInt16, out input), "alloc input");
            Ok(XpeCommonNative.xpe_alloc_image(3072, 2560, XpePixelFormatNative.Float32, out output), "alloc output");
            if ((ulong)bytes.Length != input.DataSize.ToUInt64()) throw new InvalidOperationException("RAW length differs from allocated image");
            Marshal.Copy(bytes, 0, input.Data, bytes.Length);
            var metadata = XpeImageMetadataNative.Create("Abdomen", 70, 2, 1000, 0.14f);
            const string config = "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true}";
            Ok(XpePreprocessNative.xpe_preprocess_pipeline_out(ref input, ref output, ref metadata, IntPtr.Zero, IntPtr.Zero, config), "pipeline_out");
            byte[] result = new byte[checked((int)output.DataSize.ToUInt64())]; Marshal.Copy(output.Data, result, 0, result.Length);
            string cli = Path.Combine(evidence, "cli-corrected.f32.raw");
            using (var stream = new FileStream(cli, FileMode.CreateNew)) stream.Write(result);
            var guiBytes = File.ReadAllBytes(gui);
            File.WriteAllText(Path.Combine(evidence, "bit-comparison.json"), JsonSerializer.Serialize(new {
                raw, cal, native, config, width = 3072, height = 2560, gui, cli,
                guiBytes = guiBytes.Length, cliBytes = result.Length,
                equal = guiBytes.AsSpan().SequenceEqual(result),
                guiSha256 = Convert.ToHexString(SHA256.HashData(guiBytes)), cliSha256 = Convert.ToHexString(SHA256.HashData(result)),
                rawSha256 = Convert.ToHexString(SHA256.HashData(bytes)), flags = metadata.Flags
            }, new JsonSerializerOptions { WriteIndented = true }));
            if (!guiBytes.AsSpan().SequenceEqual(result)) throw new InvalidOperationException("GUI/CLI bytes differ; see bit-comparison.json");
        }
        finally
        {
            if (input.Data != IntPtr.Zero) XpeCommonNative.xpe_free_image(ref input);
            if (output.Data != IntPtr.Zero) XpeCommonNative.xpe_free_image(ref output);
            XpePreprocessNative.xpe_preprocess_shutdown();
        }
    }
}
