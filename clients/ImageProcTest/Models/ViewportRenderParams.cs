namespace ImageProcTest
{
    internal enum ViewportLutType
    {
        Linear,
        Sigmoid
    }

    internal sealed record ViewportRenderParams(
        float WindowCenter,
        float WindowWidth,
        bool Invert = false,
        ViewportLutType Lut = ViewportLutType.Linear)
    {
        public static ViewportRenderParams Default { get; } = new(2048f, 4096f);

        public ViewportRenderParams WithSafeWidth() =>
            this with { WindowWidth = WindowWidth < 1f ? 1f : WindowWidth };
    }
}
