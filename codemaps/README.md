# XPE Code Maps

Architecture documentation for the XPE (X-ray Image Processing Engine) post-processing lane.

## Overview

This directory contains comprehensive documentation of the XPE-Post codebase architecture, module organization, API boundaries, and build system.

## Documents

### [architecture.md](architecture.md)
System architecture overview including:
- Architecture layers (Foundation, Processing, Application)
- Module dependency graph
- Data flow diagrams
- C ABI contract specification
- IEC 62304 compliance notes

**Start here** for understanding the big picture.

### [module-catalog.md](module-catalog.md)
Complete module reference including:
- All 8 modules (Common, Preprocess, Enhance Basic/Advanced, Display, DICOM, AI, GSVG)
- Module responsibilities and APIs
- Source file organization
- Inter-module dependencies

**Reference** for detailed module information.

### [api-boundaries.md](api-boundaries.md)
API contract specification including:
- C ABI export macro and calling convention
- Error handling patterns
- Memory management rules
- Thread safety guarantees
- Version compatibility policy

**Essential** for integrating with XPE modules.

### [data-structures.md](data-structures.md)
Core data type reference including:
- `XpeImage` structure
- `XpeErrorCode` enumeration
- Configuration JSON schemas
- Calibration data format (XCAL)
- Display LUT structures
- DICOM tag definitions

**Reference** for working with XPE data types.

### [testing-strategy.md](testing-strategy.md)
Testing methodology including:
- Test organization and categories
- API header tests, lifecycle tests, pixel accuracy tests
- Test execution and coverage analysis
- CI/CD integration
- Coverage targets per module

**Guide** for understanding test requirements.

### [build-system.md](build-system.md)
Build and deployment guide including:
- Build environment setup
- CMake configuration options
- Dependency management (FetchContent, vcpkg)
- Compilation flags and SIMD configuration
- Installation and packaging

**Guide** for building XPE from source.

## Quick Reference

### Module Ownership (Lane-Based)

| Lane | Modules Owned | Branch |
|------|--------------|--------|
| Pre | preprocess, common | dev/preprocess |
| **Post** | **enhance_*, display, dicom, ai, gsvg** | **dev/postprocess** |
| GUI | clients/ImageProcTest | dev/gui |

### Build Commands

```powershell
# Configure and build (recommended)
pwsh -NoProfile -ExecutionPolicy Bypass -File tools\ci\Invoke-LocalVsCommonBuild.ps1

# Run tests
ctest --preset default -C Release
```

### API Call Pattern

```cpp
// Initialize
xpe_init(nullptr);

// Allocate image
XpeImage* img = nullptr;
xpe_alloc_image(2048, 2048, XPE_PIX_MONO16, &img);

// Process
xpe_enhance_basic_process(img, img, nullptr);

// Cleanup
xpe_free_image(img);
xpe_shutdown();
```

## Architecture Diagram

```
┌─────────────────────────────────────────────────┐
│ ImageProcTest GUI (C# WPF)                     │
└──────────────────┬──────────────────────────────┘
                   │ C ABI Import
┌──────────────────▼──────────────────────────────┐
│ Layer 1: Processing Modules (DLL)              │
│ ┌──────────┬──────────┬──────────┬──────────┐ │
│ │preprocess│enhance_  │enhance_  │display   │ │
│ │          │basic     │advanced  │          │ │
│ └──────────┴──────────┴──────────┴──────────┘ │
│ ┌──────────┬──────────┐                        │
│ │dicom     │ai        │                        │
│ └──────────┴──────────┘                        │
│ ┌──────────┐                                   │
│ │gsvg      │ (Independent)                    │
│ └──────────┘                                   │
└──────────────────┬──────────────────────────────┘
                   │
┌──────────────────▼──────────────────────────────┐
│ Layer 0: Common Foundation                      │
│ xpe_common.dll (Lifecycle, Config, Logging)    │
└─────────────────────────────────────────────────┘
```

## Compliance

**IEC 62304 Class B**:
- SRS: `docs/project/srs_*.md`
- SDD: `docs/project/sdd_*.md`
- VVP: `docs/project/vvp_*.md`
- RTM: `docs/project/rtm_*.md`

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 0.1.0 | 2026-04-19 | Initial codemaps for post-processing lane |

## Contributing

When adding new modules or modifying APIs:
1. Update [module-catalog.md](module-catalog.md)
2. Update [api-boundaries.md](api-boundaries.md) if API changes
3. Update [architecture.md](architecture.md) if dependencies change
4. Add tests per [testing-strategy.md](testing-strategy.md)

## Support

For questions about XPE architecture:
- Review documents in this directory
- Check `CLAUDE.md` for project-specific rules
- Consult `docs/project/` for IEC 62304 documentation

---

**Last Updated**: 2026-04-19
**Project**: XPE Post-Processing Lane (dev/postprocess)
**Specification Version**: 0.1.0
