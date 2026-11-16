using System;
using System.IO;
using System.Runtime.InteropServices;

namespace ImageProcTest
{
    /// <summary>
    /// GUI-C-253 (#273): calls <c>xpe_verify_offset_signed</c> and <c>xpe_verify_offset_signed_file</c> of xpe_preprocess.dll (preprocess_api.h, dev/preprocess 026ea384). Both are stateless: the module reads no loaded
    /// calibration set and needs no init. The raw frame and the map buffer are pinned for the whole call and freed in a finally; the path of the file variant is marshalled as UTF-8.
    /// A module that does not export the function (an older DLL) is a "not measured" outcome with that reason, not an exception.
    /// </summary>
    internal static class OffsetQualityNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode VerifyOffsetSignedDelegate(
            ref XpeCommonApi.XpeImageBuffer rawImage,
            ref XpeCommonApi.XpeImageBuffer offsetMap,
            ref XpeOffsetQuality quality);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate XpeCommonApi.XpeErrorCode VerifyOffsetSignedFileDelegate(
            ref XpeCommonApi.XpeImageBuffer rawImage,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string offsetXcalPath,
            ref XpeOffsetQuality quality);

        /// <summary>The raw frame (UINT16, width x height, little endian, no header) of a file, or null with the reason when the file is not that size.</summary>
        internal static ushort[]? ReadRaw(string path, int width, int height, out string? problem)
        {
            problem = null;
            var length = new FileInfo(path).Length;
            var expected = (long)width * height * sizeof(ushort);
            if (length != expected)
            {
                problem = $"원시 파일이 {length} 바이트라 {width}x{height} UINT16({expected} 바이트)이 아님";
                return null;
            }

            var bytes = File.ReadAllBytes(path);
            var pixels = new ushort[width * height];
            Buffer.BlockCopy(bytes, 0, pixels, 0, bytes.Length);
            return pixels;
        }

        /// <summary>Measures a raw frame against the offset map in an XCal OFFSET file (the app's path: the maps it writes are files).</summary>
        internal static OffsetQualityOutcome MeasureWithFile(IntPtr module, ushort[] raw, int width, int height, string offsetXcalPath)
        {
            if (!NativeLibrary.TryGetExport(module, "xpe_verify_offset_signed_file", out var symbol))
            {
                return OffsetQualityOutcome.NotMeasured("이 xpe_preprocess.dll 에 xpe_verify_offset_signed_file 이 없음(낡은 DLL)");
            }

            var call = Marshal.GetDelegateForFunctionPointer<VerifyOffsetSignedFileDelegate>(symbol);
            var rawHandle = GCHandle.Alloc(raw, GCHandleType.Pinned);
            try
            {
                var rawBuffer = ImageBuffer(width, height, XpeCommonApi.XpePixelFormat.UInt16, raw.Length * sizeof(ushort), rawHandle.AddrOfPinnedObject());
                var quality = NewQuality();
                var code = call(ref rawBuffer, offsetXcalPath, ref quality);
                return OffsetQualityOutcome.FromModule((int)code, quality);
            }
            finally
            {
                rawHandle.Free();
            }
        }

        /// <summary>Measures a raw frame against an offset map held in memory (FLOAT32, the same size).</summary>
        internal static OffsetQualityOutcome MeasureWithMap(IntPtr module, ushort[] raw, float[] map, int width, int height)
        {
            if (!NativeLibrary.TryGetExport(module, "xpe_verify_offset_signed", out var symbol))
            {
                return OffsetQualityOutcome.NotMeasured("이 xpe_preprocess.dll 에 xpe_verify_offset_signed 가 없음(낡은 DLL)");
            }

            var call = Marshal.GetDelegateForFunctionPointer<VerifyOffsetSignedDelegate>(symbol);
            var rawHandle = GCHandle.Alloc(raw, GCHandleType.Pinned);
            var mapHandle = GCHandle.Alloc(map, GCHandleType.Pinned);
            try
            {
                var rawBuffer = ImageBuffer(width, height, XpeCommonApi.XpePixelFormat.UInt16, raw.Length * sizeof(ushort), rawHandle.AddrOfPinnedObject());
                var mapBuffer = ImageBuffer(width, height, XpeCommonApi.XpePixelFormat.Float32, map.Length * sizeof(float), mapHandle.AddrOfPinnedObject());
                var quality = NewQuality();
                var code = call(ref rawBuffer, ref mapBuffer, ref quality);
                return OffsetQualityOutcome.FromModule((int)code, quality);
            }
            finally
            {
                mapHandle.Free();
                rawHandle.Free();
            }
        }

        internal static XpeOffsetQuality NewQuality() => new() { StructSize = OffsetQualityBits.StructBytes, MapSha256 = new byte[32] };

        private static XpeCommonApi.XpeImageBuffer ImageBuffer(int width, int height, XpeCommonApi.XpePixelFormat format, int dataSize, IntPtr data)
        {
            var bits = format == XpeCommonApi.XpePixelFormat.UInt16 ? 16u : 32u;
            return new XpeCommonApi.XpeImageBuffer
            {
                Width = checked((uint)width),
                Height = checked((uint)height),
                BitsAllocated = bits,
                BitsStored = bits,
                Format = format,
                DataSize = (nuint)dataSize,
                Data = data,
            };
        }
    }
}
