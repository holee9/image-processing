using System.Windows.Media.Imaging;

namespace ImageProcTest
{
    internal sealed record RawPreviewResult(
        string FilePath,
        long FileSizeBytes,
        int Width,
        int Height,
        int PreviewWidth,
        int PreviewHeight,
        int SampleStride,
        ushort MinValue,
        ushort MaxValue,
        string Sha256,
        ushort[] SampledPixels,
        WriteableBitmap Bitmap);
}
