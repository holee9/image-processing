// #225 row 9 (GUI-C-196 M3): the real xpe_dicom.dll behind IDicomSession. The verdict logic is in BaselineDicomExport; this only makes the calls.
using System.Runtime.InteropServices;
using System.Text;

namespace ImageProcTest.Services.Native;

internal sealed class NativeDicomSession : IDicomSession
{
    private const int FirstReportBytes = 8192;

    private readonly Action<string> _auditLoadedModules;
    private readonly Action<IntPtr> _closeHandle;

    public NativeDicomSession()
        : this(NativeModuleAudit.ThrowIfOutsidePinnedFolder, XpeDicomNative.xpe_dicom_close)
    {
    }

    /// <summary>The two seams a test replaces: the check of the loaded modules (after a call) and the close of the reader's handle.</summary>
    internal NativeDicomSession(Action<string> auditLoadedModules, Action<IntPtr> closeHandle)
    {
        _auditLoadedModules = auditLoadedModules;
        _closeHandle = closeHandle;
    }

    /// <summary>GUI-C-245 (Codex #196): before ANY call into xpe_dicom.dll - the module and what it imports are loaded from the pinned folder and nothing else is loaded, or the call is not made.</summary>
    private static void Preflight(string feature) => NativeModuleAudit.PreflightModule("xpe_dicom.dll", feature);

    public int Write(string path, ushort[] pixels, int width, int height, BaselineDicomMetadata metadata)
    {
        Preflight("xpe_dicom_write");   // before the call that creates a file: a stray module means no file
        var handle = GCHandle.Alloc(pixels, GCHandleType.Pinned);
        try
        {
            var image = new XpeImageBufferNative
            {
                Width = (uint)width,
                Height = (uint)height,
                BitsAllocated = 16,
                BitsStored = 16,
                Format = (int)XpePixelFormatNative.UInt16,
                Data = handle.AddrOfPinnedObject(),
                DataSize = (UIntPtr)(ulong)((long)pixels.Length * sizeof(ushort)),
            };

            // mAs, SID and the acquisition time are unknown to the gui and are written as 0 (design D6).
            var meta = XpeImageMetadataNative.Create(metadata.BodyPart, metadata.KVp, 0f, 0f, metadata.PixelPitchMm);
            var code = XpeDicomNative.xpe_dicom_write(path, ref image, ref meta);
            _auditLoadedModules("xpe_dicom_write");   // GUI-C-243: after the call too (a lazily loaded import shows here); the check that matters is the one in front of the call
            return code;
        }
        finally
        {
            handle.Free();
        }
    }

    public (int Code, string Json) Validate(string path)
    {
        Preflight("xpe_dicom_validate");
        var buffer = new byte[FirstReportBytes];
        var code = XpeDicomNative.xpe_dicom_validate(path, buffer, (uint)buffer.Length);
        if (code != 0 && BitConverter.ToUInt32(buffer, 0) is var needed && needed > buffer.Length && needed <= 1_048_576)
        {
            // BUFFER_TOO_SMALL writes the required size into the first four bytes: ask again with that much, once.
            buffer = new byte[needed];
            code = XpeDicomNative.xpe_dicom_validate(path, buffer, (uint)buffer.Length);
        }

        var length = Array.IndexOf(buffer, (byte)0);
        return (code, Encoding.UTF8.GetString(buffer, 0, length < 0 ? buffer.Length : length));
    }

    public DicomReadBack ReadBack(string path)
    {
        Preflight("xpe_dicom_open");
        var openCode = XpeDicomNative.xpe_dicom_open(path, out var handle);
        if (openCode != 0 || handle == IntPtr.Zero)
        {
            _auditLoadedModules("xpe_dicom_open");
            return new DicomReadBack(openCode, -1, -1, 0, 0, null, null, null);
        }

        XpeImageBufferNative image = default;
        var imageAllocated = false;
        try
        {
            // GUI-C-245: an open that succeeded has a handle to give back, whatever this check decides (it is inside the try, so the finally below closes it)
            _auditLoadedModules("xpe_dicom_open");
            var readCode = XpeDicomNative.xpe_dicom_read_image(handle, out image);
            imageAllocated = readCode == 0;
            if (readCode != 0)
            {
                return new DicomReadBack(0, readCode, -1, 0, 0, null, null, null);
            }

            if (image.Format != (int)XpePixelFormatNative.UInt16)
            {
                return new DicomReadBack(0, 0, -1, (int)image.Width, (int)image.Height, null, null, $"the module returned pixel format {image.Format}, not 16-bit");
            }

            var count = checked((int)(image.Width * image.Height));
            var shorts = new short[count];
            Marshal.Copy(image.Data, shorts, 0, count);
            var pixels = new ushort[count];
            Buffer.BlockCopy(shorts, 0, pixels, 0, count * sizeof(ushort));

            var meta = new XpeImageMetadataNative { BodyPart = new byte[64] };
            var metaCode = XpeDicomNative.xpe_dicom_get_metadata(handle, ref meta);
            if (metaCode != 0)
            {
                return new DicomReadBack(0, 0, metaCode, (int)image.Width, (int)image.Height, pixels, null, null);
            }

            var end = Array.IndexOf(meta.BodyPart, (byte)0);
            var bodyPart = Encoding.ASCII.GetString(meta.BodyPart, 0, end < 0 ? meta.BodyPart.Length : end);
            return new DicomReadBack(0, 0, 0, (int)image.Width, (int)image.Height, pixels, new BaselineDicomMetadata(bodyPart, meta.KVp, meta.PixelPitchMm), null);
        }
        finally
        {
            if (imageAllocated)
            {
                XpeCommonNative.xpe_free_image(ref image);
            }

            _closeHandle(handle);
        }
    }
}
