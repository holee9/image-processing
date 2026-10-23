# GUI-C-255 observation runner

This Windows-only console runner observes the shipped WPF application through
FlaUI UIA3 patterns. It records facts for separate QA and reviewer assessment;
an exit code of zero is not a visual acceptance decision.

Build the GUI with `dotnet build gui/ImageProcTest/ImageProcTest.csproj -c RelWithDebInfo -p:Platform=x64`
and build the PRE and Display DLLs in `build/gui-c255-native` after initializing
MSVC with the repository build helpers. Build this runner with
`dotnet build clients/ImageProcTest.GuiObservation/ImageProcTest.GuiObservation.csproj -c RelWithDebInfo`.

Run its DLL with three positional arguments: repository root, a new evidence run
directory, and `s0_gf` or `s1_um300`. Each invocation creates a new set directory.
The runner copies the selected batch RAW into that directory, browses the
matching existing XCal directories, opens the RAW copy, invokes preprocessing,
reads the Metrics view, saves using the application's suggested filename, and
exits the application. A direct PRE DLL call then writes a separate CLI result
and records an exact byte comparison, lengths, configuration, and SHA-256 hashes.

The observation inputs are the existing `xpe-2g-offline/batch-20261022-1230` RAW
and `pre-w3072-20261022-1240` XCal files. No vendor processing library is used.
Original inputs are not written. Outputs use a new directory, and overwrite
confirmation is never accepted. Only the application process started by this
invocation is terminated during cleanup.

Each stage produces a UIA JSON dump and a PrintWindow PNG. Screen PNGs are
additionally captured only when the foreground window belongs to the application
under observation. A successful PrintWindow call can still produce a blank
client area; inspect the images before drawing a visual conclusion. Unsupported
UIA font attributes are recorded as unsupported, not as passing font sizes.

`--display-settings <dump.json> [100|150]` opens Windows Display settings, optionally
selects its scale through UIA, and records the controls. This changes desktop
scaling; the caller must restore the original scale in a finally block. The card's
`dpi150.ps1` wrapper restores the measured initial 100% setting. Actual application
DPI is recorded with GetDpiForWindow and must be checked against the requested scale.

`--compare <root> <xcal-directory> <raw> <gui-float32> <output-directory>` performs
the direct DLL comparison for an already completed GUI run. The CLI output path
uses CreateNew so an existing comparison cannot be overwritten.

This probe deliberately does not change application behavior, fix display issues,
claim image quality, or replace independent QA. Inspect `flow.log` and the UIA
status strings as well as the process exit code: optional chain stages may report
missing DLLs, and Metrics may still contain a placeholder.
