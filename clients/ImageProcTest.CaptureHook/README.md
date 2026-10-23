# GUI-C-255b WPF capture hook

Build this test-only library with `dotnet build clients/ImageProcTest.CaptureHook/ImageProcTest.CaptureHook.csproj -c RelWithDebInfo`.
The observation runner loads it only in its own child application using
`DOTNET_STARTUP_HOOKS` and `XPE_GUI_CAPTURE_DIR`. Normal product launches do not load it.

An atomic JSON request identifies the target WPF window HWND and a new output
PNG path within the evidence directory. On that application's dispatcher, the
hook paints the current window content using a VisualBrush and RenderTargetBitmap
at its actual WPF DPI, over an opaque white background. It writes an atomic
response containing dimensions, DPI, the nonwhite-pixel ratio, and visible
TextBlock text/font/bounds metadata. It does not invoke product commands or
change settings, layout, focus, foreground windows, or processing.

The runner overlays the client bitmap unscaled on the OS window frame after
checking WPF and Win32 client dimensions. It keeps the client and full-window
PNGs. Native file/folder dialogs are captured separately with PrintWindow 0x2.
The body of every final client/window/dialog image must contain more than 5%
nonwhite pixels; a white capture fails. This detects blank images and does not
establish layout quality, readable text, or product acceptance.

`RenderTargetBitmap` is an offscreen rendering of the current WPF visual tree.
It does not establish what the desktop compositor actually displayed. Native
dialogs/popups/child HWNDs are outside that visual tree; use the separate native
dialog evidence rather than claiming they appear in the WPF client bitmap.
