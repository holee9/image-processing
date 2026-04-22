# XPE DICOM Conformance Statement

**Document ID**: XPE-DICOM-CS-001  
**Version**: 0.1.0 (Draft)  
**Date**: 2026-04-22  
**SPEC Reference**: SPEC-XPE-IOP REQ-IOP-005  
**IEC 62304 Class**: B  
**Format**: Per DICOM PS 3.2

---

## 1. Conformance Statement Overview

This DICOM Conformance Statement describes the DICOM capabilities of the **XPE Image Processing Engine** (`xpe_dicom.dll` v1.0.x).

XPE is an X-ray image processing software library for flat-panel detector (FPD) systems.
It provides DICOM read/write capability for Digital Radiography (DR) images.

### 1.1 Implementation Model

| Component | Role | Version |
|-----------|------|---------|
| `xpe_dicom.dll` | DICOM Storage SCU/SCP + Query/Retrieve SCU | 1.0.x |
| DCMTK | Underlying DICOM toolkit | 3.6.8 |
| Network Transport | TCP/IP + TLS 1.2 | OS-provided |

### 1.2 AE Specifications Summary

| AE Title (default) | Role | Supported Services |
|--------------------|------|--------------------|
| `XPE_STORE` | Storage SCU | C-STORE (push to PACS) |
| `XPE_SCP` | Storage SCP | C-STORE (receive from modality) |
| `XPE_QR` | Query/Retrieve SCU | C-FIND, C-MOVE, C-GET |
| `XPE_ECHO` | Verification | C-ECHO SCU + SCP |

AE Titles are configurable via `xpe_config.json`.

---

## 2. Implementation Model

### 2.1 Application Data Flow

```
Modality → [C-STORE SCU] → XPE_SCP → xpe_dicom.dll → Internal Processing
Internal Processing → xpe_dicom.dll → [C-STORE SCU] → PACS / Archive
```

### 2.2 Sequencing Constraints

- C-ECHO Verification shall succeed before any storage operation.
- C-STORE SCU operations use the Transfer Syntax negotiated during Association.

---

## 3. AE Specifications

### 3.1 XPE_STORE — Storage SCU

**Purpose**: Push processed images to a PACS archive.

#### 3.1.1 SOP Classes (Storage SCU)

| SOP Class Name | SOP Class UID |
|----------------|---------------|
| Digital X-Ray Image Storage – For Presentation | 1.2.840.10008.5.1.4.1.1.1.1 |
| Digital X-Ray Image Storage – For Processing | 1.2.840.10008.5.1.4.1.1.1.1.1 |
| Grayscale Softcopy Presentation State Storage | 1.2.840.10008.5.1.4.1.1.11.1 |
| Secondary Capture Image Storage | 1.2.840.10008.5.1.4.1.1.7 |

#### 3.1.2 Transfer Syntaxes Proposed (Storage SCU)

| Transfer Syntax Name | UID | Role |
|----------------------|-----|------|
| Explicit VR Little Endian | 1.2.840.10008.1.2.1 | Default (proposed first) |
| JPEG Lossless, Non-Hierarchical, First-Order Prediction (14) | 1.2.840.10008.1.2.4.70 | Optional |
| JPEG 2000 Image Compression (Lossless Only) | 1.2.840.10008.1.2.4.90 | Optional |
| Implicit VR Little Endian | 1.2.840.10008.1.2 | Fallback only |

#### 3.1.3 Association Establishment

- Maximum PDU Size (SCU): 32,768 bytes
- Called AE Title: configurable
- Calling AE Title: `XPE_STORE` (default, configurable)
- Association Timeout: 30 seconds (configurable)
- Idle Association Release: after 60 seconds idle

---

### 3.2 XPE_SCP — Storage SCP

**Purpose**: Receive DICOM instances from upstream modalities.

#### 3.2.1 SOP Classes (Storage SCP)

| SOP Class Name | SOP Class UID |
|----------------|---------------|
| Digital X-Ray Image Storage – For Presentation | 1.2.840.10008.5.1.4.1.1.1.1 |
| Digital X-Ray Image Storage – For Processing | 1.2.840.10008.5.1.4.1.1.1.1.1 |

#### 3.2.2 Transfer Syntaxes Accepted (Storage SCP)

| Transfer Syntax Name | UID | Supported |
|----------------------|-----|-----------|
| Explicit VR Little Endian | 1.2.840.10008.1.2.1 | ✅ Required |
| Implicit VR Little Endian | 1.2.840.10008.1.2 | ✅ Required |
| JPEG 2000 Image Compression (Lossless Only) | 1.2.840.10008.1.2.4.90 | ✅ |

#### 3.2.3 Attribute Handling

| Attribute | Tag | Requirement |
|-----------|-----|-------------|
| Study Instance UID | (0020,000D) | Required |
| Series Instance UID | (0020,000E) | Required |
| SOP Instance UID | (0008,0018) | Required |
| Pixel Data | (7FE0,0010) | Required |
| Modality | (0008,0060) | Required |
| Patient ID | (0010,0020) | Required |
| Acquisition Date | (0008,0022) | Conditionally required |
| KVP | (0018,0060) | Optional |
| Exposure | (0018,1152) | Optional |

Unrecognized private attributes are preserved without modification.

---

### 3.3 XPE_QR — Query/Retrieve SCU

**Purpose**: Retrieve prior studies for comparison workflows.

#### 3.3.1 Service Classes

| Service | Role | SOP Class UID |
|---------|------|---------------|
| Study Root Query/Retrieve – FIND | SCU | 1.2.840.10008.5.1.4.1.2.2.1 |
| Study Root Query/Retrieve – MOVE | SCU | 1.2.840.10008.5.1.4.1.2.2.2 |

#### 3.3.2 Query Keys

| Level | Mandatory Keys | Optional Keys |
|-------|---------------|---------------|
| STUDY | StudyInstanceUID, PatientID, StudyDate | AccessionNumber, Modality |
| SERIES | SeriesInstanceUID, Modality | SeriesNumber, SeriesDate |
| IMAGE | SOPInstanceUID | InstanceNumber |

---

## 4. Communication Profiles

### 4.1 TCP/IP Stack

| Feature | Support |
|---------|---------|
| IPv4 | ✅ |
| IPv6 | ✅ |
| TLS 1.2 | ✅ |
| TLS 1.3 | ✅ (via DCMTK + OpenSSL) |
| Mutual TLS (client cert) | Optional |

Cipher suites follow NIST SP 800-52 Rev. 2 recommendations (AES-256-GCM, ECDHE key exchange).

### 4.2 Port Configuration

| Service | Default Port | Configurable |
|---------|-------------|--------------|
| DICOM (plain) | 104 | Yes |
| DICOM over TLS | 2762 | Yes |

---

## 5. Extended Negotiation

| Extension | Support |
|-----------|---------|
| Asynchronous Operations Window | SCU: 1 outstanding (synchronous) |
| SCP/SCU Role Selection | As specified per AE above |
| Extended Negotiation (Storage Commitment) | Not supported in v1.0 |

---

## 6. Configuration

XPE DICOM behavior is configured via `xpe_config.json` `dicom` section:

```json
{
  "dicom": {
    "ae_title": "XPE_STORE",
    "local_port": 104,
    "max_pdu_size": 32768,
    "tls_enabled": false,
    "tls_cert_file": "",
    "tls_key_file": "",
    "tls_ca_file": "",
    "association_timeout_sec": 30,
    "called_ae_title": "PACS_AE"
  }
}
```

---

## 7. Limitations and Conformance Exceptions

| Limitation | Detail |
|------------|--------|
| Media Storage | DICOMDIR creation: not supported in v1.0 (PDI profile: Phase 2) |
| WADO-RS / STOW-RS | Not supported in v1.0 (DICOMweb: Phase 2, SPEC-XPE-IOP §4.2) |
| FHIR ImagingStudy | Not supported in v1.0 (Phase 2-3) |
| Compressed pixel encoding | JPEG 2000 lossless only; lossy compression not supported |
| Structured Reporting (AI SR) | Not supported in v1.0 (Phase 3, SPEC-XPE-IOP §4.5) |

---

## 8. Change History

| Version | Date | Changes |
|---------|------|---------|
| 0.1.0 | 2026-04-22 | Initial draft per SPEC-XPE-IOP REQ-IOP-005. Covers DICOM 3.0 core (Must tier): SCU/SCP roles, SOP classes, Transfer Syntaxes, TLS. DICOMweb/FHIR/IHE deferred to later versions. |

---

*This document conforms to DICOM PS 3.2 (Conformance Statement format).*  
*Document owner: XPE development team. Review cycle: per major release.*
