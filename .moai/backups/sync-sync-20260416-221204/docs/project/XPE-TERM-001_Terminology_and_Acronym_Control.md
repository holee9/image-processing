# XPE Terminology and Acronym Control

**Document ID**: XPE-TERM-001  
**Version**: 1.0.0  
**Date**: 2026-04-16  
**Status**: Controlled Draft  
**Scope**: Project-wide terminology, acronym ownership, and naming conflict prevention  
**Linked Issue**: GitHub #14

---

## 1. Purpose

This document prevents acronym collisions across XPE documents, APIs, GUI labels, and implementation notes.

The immediate control item is `AED`, which is a common X-ray detector term for automatic exposure detection and must not also be used for the common event/alert infrastructure.

---

## 2. Reserved Acronyms

| Acronym | Reserved meaning | Allowed scope | Forbidden alternate meanings |
|---|---|---|---|
| `AED` | Auto Exposure Detection / Automatic Exposure Detection | detector acquisition, exposure-event detection, wireless detector synchronization, `AED-0` algorithm references | Asynchronous Event Dispatcher, Abnormal Event Detection, generic alert/event queue |

Rules:

- `AED` shall only mean Auto Exposure Detection or Automatic Exposure Detection.
- When first used in a document, write `Auto Exposure Detection (AED)`.
- Do not use `AED` for the `xpe_common.dll` alert queue, event queue, event dispatcher, or notification system.
- Existing `xpe_aed_*` API names are treated as legacy names until the implementation is renamed or aliased. New documentation shall prefer `xpe_event_*` for common event infrastructure and `xpe_exposure_detect_*` for detector exposure detection if a new API is introduced.

---

## 3. Preferred Terms

| Concept | Preferred term | API prefix recommendation | GUI label |
|---|---|---|---|
| Common asynchronous event dispatch | XPE Event System | `xpe_event_*` | Events |
| User-visible warnings/errors/info | Alert Queue | `xpe_alert_*` | Alerts |
| Detector exposure auto-detection | Auto Exposure Detection (AED) | `xpe_exposure_detect_*` or legacy `xpe_aed_*` until migration | Exposure Detection |
| Detector fault/motion/saturation event producer | Detector Event Monitor | `xpe_detector_event_*` | Detector Events |

---

## 4. Migration Policy

### 4.1 Documentation

All active documentation shall use:

- `XPE Event System` for common event infrastructure;
- `Alert Queue` for user-visible alert buffering;
- `Auto Exposure Detection (AED)` for detector exposure detection only.

### 4.2 API documentation

If `xpe_aed_*` names remain in the C ABI temporarily, the API documentation shall label them as one of:

- `legacy Auto Exposure Detection API`, if the function is detector exposure related;
- `deprecated legacy event API`, if the function is common event/alert infrastructure and should be renamed to `xpe_event_*`.

### 4.3 Code migration

Future code migration should introduce non-conflicting aliases before removing old names:

```c
XpeErrorCode xpe_event_configure(const char* jsonConfigOrNull);
XpeErrorCode xpe_event_poll(int32_t* eventTypeOut, uint64_t* timestampOut, float* signalLevelOut);
XpeErrorCode xpe_event_get_status(int32_t* stateOut);
```

For detector exposure detection, prefer:

```c
XpeErrorCode xpe_exposure_detect_configure(const char* jsonConfigOrNull);
XpeErrorCode xpe_exposure_detect_poll(int32_t* eventTypeOut, uint64_t* timestampOut, float* signalLevelOut);
XpeErrorCode xpe_exposure_detect_get_status(int32_t* stateOut);
```

---

## 5. Review Checklist

Before approving a document or API change:

- [ ] `AED` is expanded as Auto Exposure Detection / Automatic Exposure Detection.
- [ ] `AED` is not used for common event/alert infrastructure.
- [ ] GUI labels avoid `AED` unless the feature is exposure detection.
- [ ] API names use `xpe_event_*`, `xpe_alert_*`, or `xpe_exposure_detect_*` according to responsibility.
- [ ] Legacy names are explicitly marked as legacy or deprecated.

---

*Document End - XPE-TERM-001 v1.0.0*
