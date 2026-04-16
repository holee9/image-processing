# Doxygen Native API Reference

Generated from public headers in `modules/*/include/`.

Covers all exported C ABI functions, types, enums, and defines for:
- `xpe_common` — shared types and error codes
- `xpe_preprocess` — detector correction (ghost, gain, defect, readout)
- `xpe_enhance_basic` — log transform, noise reduction, CLAHE, USM, EI/DI
- `xpe_display` — windowing, LUT, VOI
- `xpe_dicom` — DICOM file I/O
- `xpe_enhance_advanced` — premium enhancement stages
- `xpe_gsvg` — scatter and vignette correction

## Prerequisites

- Doxygen 1.12 or later
- doxygen-awesome-css (place in `doxygen-awesome/` subdirectory):

```bash
git clone https://github.com/jothepro/doxygen-awesome-css doxygen-awesome
```

## Generate

```bash
cd docs/help/doxygen
git clone https://github.com/jothepro/doxygen-awesome-css doxygen-awesome
doxygen Doxyfile
# Output: docs/help/generated/doxygen/html/index.html
# XML for DocFX cross-reference: docs/help/generated/doxygen/xml/
```

## CI

See `.github/workflows/docs-generate.yml`.

The CI workflow runs on every push to `main` or `dev/integration` that touches
a public header or doc file. Artifacts are uploaded for 30 days.

## Notes

- `WARN_AS_ERROR = FAIL_ON_WARNINGS` is enabled. All public API symbols must
  have Doxygen comments or the build fails.
- `EXTRACT_ALL = NO` — only documented symbols appear in the output.
  Undocumented internal helpers are intentionally excluded.
- `doxygen-awesome/` is `.gitignore`'d; CI clones it fresh each run.
