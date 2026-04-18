# XPE Terminology and Acronym Control

**Document ID**: XPE-TERM-001  
**Version**: 2.0.0 — RETIRED  
**Date**: 2026-04-18  
**Status**: Controlled Draft (Archived)  
**Scope**: Project-wide terminology, acronym ownership, and naming conflict prevention  
**Linked Issue**: GitHub #14

---

## Reserved Acronyms (Active)

| Acronym | Reserved meaning | Allowed scope | Forbidden meanings |
|---|---|---|---|
| `EI` | Exposure Index | detector-domain image quality, rendering guidance | any other domain |
| `DI` | Deviation Index | detector-domain QA metric | any other domain |
| `ROI` | Region Of Interest | spatial crops, focused processing | any other meaning |
| `LUT` | Look-Up Table | color mappings, calibration tables | any other meaning |
| `QC` | Quality Control | detector validation, detector artifact detection | any other meaning |

---

## Preferred Terms (Active)

| Concept | Preferred term | API prefix | GUI label |
|---|---|---|---|
| Detector input validation | Readout artifact validation | `xpe_validate_readout_*` | Readout check |
| General event dispatch | XPE Event System | `xpe_event_*` | Events |
| User-visible alerts | Alert Queue | `xpe_alert_*` | Alerts |

---

## Review Checklist (Updated)

Before approving a document or API change:

- [ ] Detector functionality is described as "detector SDK" or "detector hardware", not XPE.
- [ ] API names use `xpe_event_*` or `xpe_alert_*` for common infrastructure.
- [ ] Exposure Index (EI) is clearly scoped to detector domain only.

---

*Retirement Effective: 2026-04-18*  
*Previous version (1.0.0) archived to docs/archive/superseded/ for reference only.*
