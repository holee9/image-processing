using System.Windows.Media.Imaging;

namespace ImageProcTest.Models;

public sealed class LoadedImageFrame
{
    public required BitmapSource Preview { get; init; }

    public required string Summary { get; init; }

    public required string MetadataText { get; init; }
}
