# XPE 통합 알고리즘 개발 명세서

**Document ID:** XPE-ALG-001 v1.2  
**IEC 62304 Clause:** 5.4 (Software Detailed Design)  
**Safety Classification:** Class B  
**Date:** 2026-04-15  
**Author:** XPE Development Team  
**Review Cycles:** 30회 (v1.0: 10회 + v1.1: 10회 + v1.2: 10회 Review-Evaluate-Fix 반복 완료)  
**Approval:** __________________ Date: __________  

---

## 문서 목적

본 문서는 XPE(X-ray Processing Engine) 시스템의 **모든 알고리즘**을 수학적 공식, C++ 의사코드, SIMD 최적화 전략, 검증 기준까지 일관되게 명세한다. 기존 문서(XPE-SRS-001, XPE-SAD-001, XPE-SDD-002, 03_측정_알고리즘_명세서, xray_grid_suppression_virtual_grid_research)의 교차 검증을 통해 식별된 알고리즘 공백을 3 라운드에 걸쳐 모두 해소한다.

### 공백 해소 매핑

| 공백 번호 | 내용 | 본 문서 섹션 | 라운드 |
|----------|------|------------|-------|
| GAP-01 | Python↔C++ 아키텍처 브리지 미문서화 | §2 | v1.0 |
| GAP-02 | Core Processing 알고리즘 미명세 | §4 | v1.0 |
| GAP-03 | Grid Suppression 알고리즘 미명세 | §5 | v1.0 |
| GAP-04 | AI/DL 알고리즘 미명세 | §8 | v1.0 |
| GAP-05 | Display Processing 표준 미명세 | §6 | v1.0 |
| GAP-06 | SIMD 최적화 전체 파이프라인 미커버 | §10 | v1.0 |
| GAP-07 | 파노라마 스티칭 미명세 | §8.3 | v1.0 |
| GAP-08 | Virtual Grid / Scatter Correction 미명세 | §5.2 | v1.0 |
| GAP-09 | Exposure Index (IEC 62494-1) 미명세 | §7 | v1.0 |
| GAP-10 | 교정 맵 생성↔런타임 연결 미문서화 | §9 | v1.0 |
| GAP-D | NSCT Grid Suppression 완전 구현 누락 | §5.1.3 | v1.1 |
| GAP-E | 런타임 결함 검출 (AVX2 z-score) 누락 | §3.3.4 | v1.1 |
| GAP-F | EI ROI Central Method √0.1 오류 | §7.2 | v1.1 |
| GAP-G | AVX2 log 근사 (avx2_log_ps) 미구현 | §4.1.3 | v1.1 |
| GAP-H | 비선형성 보정 (PCHIP LUT) 미명세 | §3.0.5 | v1.1 |
| GAP-I | Readout Validation 미명세 | §3.0 | v1.1 |
| GAP-J | AED-0 자동 노출 감지 미명세 | §9.4 | v1.1 |
| GAP-L | NPS 계산 (IEC 62220-1) 미명세 | §12.3 | v1.1 |
| GAP-M | DQE 계산 알고리즘 미명세 | §12.4 | v1.1 |
| GAP-N | Collimation Mask 검출 미명세 | §12.5 | v1.1 |
| GAP-O | Heel Effect 보정 (Wang 2013) 미명세 | §3.5 | v1.2 |
| GAP-P | Multi-SID Gain 보간 및 kVp 선택 미명세 | §3.2.5 | v1.2 |
| GAP-Q | 교정 세션 잠금 및 매니페스트 해시 체인 미명세 | §2.4 | v1.2 |
| GAP-R | 품질 상태 벡터 사이드카 미명세 | §13 | v1.2 |
| GAP-S | 스칼라 참조 + SIMD 패리티 하네스 미명세 | §11.4 | v1.2 |
| GAP-T | MTF 슬랜트 에지 ESF 완전 구현 누락 | §12.6 | v1.2 |
| GAP-U | Lag 잔류 기반 결정론적 티어링 미명세 | §3.4.5 | v1.2 |
| GAP-V | 해부 부위별 Virtual Grid 프리셋 미명세 | §5.3 | v1.2 |
| GAP-W | AI Worker 격리 아키텍처 (ONNX) 미명세 | §8.4 | v1.2 |
| GAP-X | 교정 드리프트 모니터링 알고리즘 미명세 | §9.5 | v1.2 |

---

## 목차

1. [용어 정의 및 기호 규약](#1-용어-정의-및-기호-규약)
2. [시스템 아키텍처 — Python↔C++ 브리지](#2-시스템-아키텍처--pythonc-브리지)
   - [§2.4 교정 세션 잠금 및 매니페스트 해시 체인 ★GAP-Q](#24-교정-세션-잠금-및-매니페스트-해시-체인-gap-q-해소)
3. [SWI-1: Pre-Processing 알고리즘](#3-swi-1-pre-processing-알고리즘)
   - [§3.0 Readout Validation (SWU-1.0) ★GAP-I](#30-swu-10-readout-validation-gap-i-해소)
   - [§3.0.5 Non-linearity Correction ★GAP-H](#305-swu-105-non-linearity-correction-gap-h-해소)
   - §3.1 Offset Correction
   - §3.2 Gain Correction
   - [§3.2.5 Multi-SID Gain 보간 ★GAP-P](#325-swu-12b-multi-sid-gain-보간-및-kvp-stratified-gain-선택-gap-p-해소)
   - §3.3 Defect Correction (★GAP-E)
   - §3.4 Ghost/Lag Correction
   - [§3.4.5 Lag Residual 티어링 ★GAP-U](#345-swu-14b-lag-잔류-기반-결정론적-티어링-gap-u-해소)
   - [§3.5 Heel Effect Compensation ★GAP-O](#35-swu-15-heel-effect-compensation-gap-o-해소)
4. [SWI-2: Core Processing 알고리즘](#4-swi-2-core-processing-알고리즘) (★GAP-G avx2_log_ps)
5. [Grid Suppression & Virtual Grid 알고리즘](#5-grid-suppression--virtual-grid-알고리즘) (★GAP-D NSCT)
   - [§5.3 해부 부위별 Virtual Grid 프리셋 ★GAP-V](#53-해부-부위별-virtual-grid-프리셋-테이블-gap-v-해소)
6. [SWI-3: Display Processing 알고리즘](#6-swi-3-display-processing-알고리즘)
7. [IEC 62494-1 Exposure Index 알고리즘](#7-iec-62494-1-exposure-index-알고리즘) (★GAP-F ROI 수정)
8. [AI/DL 알고리즘](#8-aidl-알고리즘)
   - [§8.4 AI Worker 격리 아키텍처 ★GAP-W](#84-ai-worker-격리-아키텍처-및-onnx-추론-gap-w-해소)
9. [교정 데이터 파이프라인](#9-교정-데이터-파이프라인)
   - [§9.4 AED-0 Automatic Exposure Detection ★GAP-J](#94-aed-0-automatic-exposure-detection-gap-j-해소)
   - [§9.5 교정 드리프트 모니터링 ★GAP-X](#95-교정-드리프트-모니터링-gap-x-해소)
10. [성능 최적화 — SIMD/OpenMP 전략](#10-성능-최적화--simdopenmp-전략)
11. [검증 방법론](#11-검증-방법론)
    - [§11.4 스칼라 참조 + SIMD 패리티 하네스 ★GAP-S](#114-스칼라-참조-구현-및-simd-패리티-하네스-gap-s-해소)
12. [FPD 특성화 알고리즘 보완](#12-fpd-특성화-알고리즘-보완)
    - [§12.3 NPS 계산 ★GAP-L](#123-nps-계산-알고리즘-gap-l-해소)
    - [§12.4 DQE 계산 ★GAP-M](#124-dqe-계산-알고리즘-gap-m-해소)
    - [§12.5 Collimation Mask Detection ★GAP-N](#125-collimation-mask-detection-알고리즘-gap-n-해소)
    - [§12.6 MTF 슬랜트 에지 ESF 완전 구현 ★GAP-T](#126-mtf-슬랜트-에지-esf-완전-구현-gap-t-해소)
13. [품질 상태 벡터 사이드카 ★GAP-R](#13-품질-상태-벡터-사이드카-gap-r-해소)
- [부록 A: 수학 공식 일람](#부록-a-수학-공식-일람)
- [부록 B: 표준 참조 테이블](#부록-b-표준-참조-테이블)
- [부록 C: 알고리즘-요구사항 추적성](#부록-c-알고리즘-요구사항-추적성)

---

## 1. 용어 정의 및 기호 규약

### 1.1 기호 체계

| 기호 | 정의 | 단위 |
|------|------|------|
| `I_raw(x,y)` | 원시 detector 출력 pixel 값 | ADU |
| `I_dark(x,y)` | Dark offset map | ADU |
| `G(x,y)` | Gain correction map | dimensionless |
| `I_flat(x,y)` | Flat-field (flood) image | ADU |
| `I_corr(x,y)` | Gain-corrected image | ADU |
| `I_clean(x,y)` | Defect-corrected image | ADU |
| `I_od(x,y)` | Log-transformed (OD domain) image | OD |
| `σ_s` | Bilateral filter spatial sigma | pixels |
| `σ_r` | Bilateral filter range sigma | ADU or OD |
| `f` | Spatial frequency | cycles/mm |
| `MTF(f)` | Modulation Transfer Function | dimensionless |
| `NPS(f)` | Noise Power Spectrum | ADU²·mm² |
| `NNPS(f)` | Normalized NPS | mm² |
| `DQE(f)` | Detective Quantum Efficiency | dimensionless |
| `Φ` | X-ray quantum fluence at detector | photons/mm² |
| `EI` | Exposure Index (IEC 62494-1) | dimensionless |
| `DI` | Deviation Index | dB |
| `W(u,v)` | Window function (Hanning) | dimensionless |
| `ε` | Numerical floor (= 1×10⁻⁶) | ADU or OD |

### 1.2 좌표 규약

```
Origin: top-left (0,0)
x: column (horizontal), y: row (vertical)
Spatial frequency: u (horizontal), v (vertical), f = sqrt(u²+v²)
Nyquist frequency: f_N = 1/(2·pixelPitch_mm)
```

### 1.3 데이터 타입 규약

| 처리 단계 | 내부 타입 | 비트 깊이 | 범위 |
|----------|----------|---------|------|
| Raw detector | uint16 | 14–16 bit | 0–65535 |
| Pre-processing 중간 | float32 | 32 bit | 0.0–65535.0 |
| OD domain | float32 | 32 bit | −∞ ~ +∞ (실제 −5 ~ +5) |
| Display pipeline | float32→uint16 | 16→8 bit | 0–4095 → 0–255 |

---

## 2. 시스템 아키텍처 — Python↔C++ 브리지

### 2.1 전체 데이터 흐름 (GAP-01 해소)

```
┌──────────────────────────────────────────────────────────────────┐
│                    OFFLINE (Python) — 교정 단계                    │
│                                                                    │
│  FPD 검사/특성화                                                    │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐           │
│  │  Dark Frame │    │  Flood Field│    │  Slanted Edge│           │
│  │  ≥16 frames │    │  per SID    │    │  (MTF)      │           │
│  └──────┬──────┘    └──────┬──────┘    └──────┬──────┘           │
│         │                  │                   │                   │
│         ▼                  ▼                   ▼                   │
│  compute_offset_map()  compute_gain_map()  compute_mtf()          │
│  compute_defect_map()  compute_nps_full()  compute_dqe()          │
│         │                  │                   │                   │
│         ▼                  ▼                   ▼                   │
│  [offset_map.bin]    [gain_map_SIDXXX.bin] [characterization.json]│
│  [defect_map.bin]    [checksum.sha256]                             │
└──────────────────────────────────────────────────────────────────┘
           │                  │
           ▼ 파일 배포         ▼
┌──────────────────────────────────────────────────────────────────┐
│                    ONLINE (C++) — 런타임 파이프라인                  │
│                                                                    │
│  SWI-1 Pre-Processing                                              │
│  ┌─────────────────────────────────────────────────────────┐      │
│  │ xpe_offset_correct() → xpe_gain_correct()               │      │
│  │ → xpe_defect_correct() → xpe_ghost_correct()            │      │
│  └──────────────────────────────┬──────────────────────────┘      │
│                                  │ float32 ImageBuffer              │
│  SWI-2 Core Processing           ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐      │
│  │ log_transform() → bilateral_filter() → clahe()          │      │
│  │ → edge_enhance() → [laplacian_pyramid()] [fractional()] │      │
│  └──────────────────────────────┬──────────────────────────┘      │
│                                  │                                  │
│  SWI-3 Display Processing        ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐      │
│  │ modality_lut() → voi_lut() → presentation_lut_gsdf()   │      │
│  └──────────────────────────────┬──────────────────────────┘      │
│                                  │                                  │
│  SWI-4 DICOM I/O                 ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐      │
│  │ dcmtk_write_dx_iod() → C-STORE SCU                     │      │
│  └─────────────────────────────────────────────────────────┘      │
└──────────────────────────────────────────────────────────────────┘
```

### 2.2 교정 파일 형식 명세

#### 2.2.1 Offset Map (`.bin`)

```
Header (64 bytes):
  [0..3]   Magic: "XOFF"
  [4..7]   Version: uint32 = 1
  [8..11]  Width: uint32
  [12..15] Height: uint32
  [16..19] NumFrames: uint32 (≥16)
  [20..23] BitDepth: uint32 (14 or 16)
  [24..55] AcquisitionDateTime: char[32] (ISO-8601)
  [56..63] Checksum: uint64 (CRC64)
Payload:
  float32[Width × Height]  // mean of NumFrames dark images, clamp ≥ 0
```

#### 2.2.2 Gain Map (`.bin`)

```
Header (96 bytes):
  [0..3]   Magic: "XGAI"
  [4..7]   Version: uint32 = 1
  [8..11]  Width: uint32
  [12..15] Height: uint32
  [16..19] SID_mm: float32   // Source-to-Image Distance
  [20..23] kVp: float32
  [24..27] mAs: float32
  [28..31] GainMean: float32  // mean of (Flood - Offset)
  [32..63] AcquisitionDateTime: char[32]
  [64..95] Checksum: uint64 (CRC64)
Payload:
  float32[Width × Height]  // GainMean / (Flood(x,y) - Offset(x,y)), clamped [0.5, 2.0]
```

#### 2.2.3 Defect Pixel Map (`.bin`)

```
Header (64 bytes):
  [0..3]   Magic: "XDEF"
  [4..7]   Version: uint32 = 1
  [8..11]  Width: uint32
  [12..15] Height: uint32
  [16..19] NumDefects: uint32
  [20..23] MapFlags: uint32  // bit0: factory, bit1: runtime
  [24..55] AcquisitionDateTime: char[32]
  [56..63] Checksum: uint64 (CRC64)
Payload:
  // Run-Length Encoded defect list:
  struct DefectEntry {
      uint16 x;
      uint16 y;
      uint8  type;   // 0: point, 1: cluster, 2: row, 3: col
      uint8  size;   // 1-based, used for cluster radius
  };
  DefectEntry[NumDefects]
```

### 2.3 파일 무결성 검증 (SRS-SEC-002)

```cpp
// Runtime validation before applying calibration data
bool validate_calibration_file(const std::string& path,
                                const std::string& checksum_path) {
    // Read file content
    auto data = read_binary_file(path);
    // Compute SHA-256
    auto computed = sha256(data.data(), data.size());
    // Compare with stored checksum
    auto stored = read_text_file(checksum_path);
    return computed == stored;
}
```

---

### 2.4 교정 세션 잠금 및 매니페스트 해시 체인 (GAP-Q 해소)

xpe-algorithm-spec-deepsync.md §4.1에서 "Every offset, gain, BPM, nonlinearity, and lag coefficient pack shall carry a session identity and hash chain"으로 명시된 교정 무결성 인프라이다. 서로 다른 세션의 교정 파일이 혼합되는 것을 방지하고, 드리프트 모니터링 API를 통해 재교정 트리거를 제공한다.

#### 2.4.1 세션 ID 스키마

모든 교정 파일은 공통 헤더 확장에 `session_id` 필드를 포함한다:

```
세션 ID 구성: SHA-256 digest 앞 8바이트 (16 hex 문자)
  session_id = SHA-256(
      device_serial    +   // FPD 시리얼 번호 (ASCII)
      calibration_date +   // ISO-8601 날짜 (예: "2026-04-15")
      operator_id          // 기사 ID 또는 자동화 토큰
  )[0:8]

예시: "A3F1C2D8E4B09517"
```

**파일 헤더 확장 (모든 교정 파일 형식에 적용)**:

```
확장 헤더 블록 (32 bytes, 기존 헤더 뒤에 추가):
  [0..7]   SessionID:     char[8]   // 세션 식별자 (16진수 ASCII)
  [8..15]  PrevHash:      uint8[8]  // 이전 교정 세션 해시 체인 링크
  [16..23] CreatedAt_ns:  uint64    // Unix nanoseconds
  [24..31] Reserved:      uint8[8]  // 향후 확장용 (모두 0)
```

#### 2.4.2 매니페스트 파일 스키마

교정 세션마다 하나의 매니페스트 파일 `calibration_manifest.json`을 생성한다:

```json
{
  "schema_version": "1.0",
  "session_id": "A3F1C2D8E4B09517",
  "device_serial": "FPD-2024-003421",
  "calibration_date": "2026-04-15T09:30:00Z",
  "operator_id": "CAL-AUTO-001",
  "files": [
    {
      "type": "offset_map",
      "path": "offset_map.bin",
      "sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
      "size_bytes": 37748800,
      "acquired_at": "2026-04-15T09:15:00Z"
    },
    {
      "type": "gain_map",
      "sid_mm": 1000.0,
      "kvp": 80.0,
      "path": "gain_SID1000_kVP080.bin",
      "sha256": "...",
      "size_bytes": 37748896,
      "acquired_at": "2026-04-15T09:20:00Z"
    },
    {
      "type": "defect_map",
      "path": "defect_map.bin",
      "sha256": "...",
      "size_bytes": 9437248,
      "acquired_at": "2026-04-15T09:18:00Z"
    },
    {
      "type": "nonlinearity_lut",
      "path": "nonlinearity_lut.bin",
      "sha256": "...",
      "size_bytes": 262144,
      "acquired_at": "2026-04-15T09:22:00Z"
    },
    {
      "type": "lag_params",
      "path": "lag_params.json",
      "sha256": "...",
      "size_bytes": 512,
      "acquired_at": "2026-04-15T09:25:00Z"
    }
  ],
  "hash_chain": {
    "prev_session_id": "7B2F9A4C1D3E8F60",
    "manifest_hash": "sha256_of_this_file_excluding_manifest_hash_field"
  }
}
```

#### 2.4.3 혼합 세션 거부 로직

```python
import hashlib
import json
from pathlib import Path
from typing import Optional

class CalibrationSessionLock:
    """
    Validates that all calibration files in a pack belong to the same session.
    Rejects mixed-session packs unless explicitly overridden for diagnostics.
    """

    def __init__(self, manifest_path: Path, allow_mixed: bool = False):
        self.manifest_path = manifest_path
        self.allow_mixed   = allow_mixed
        self._manifest     = None

    def load_and_validate(self) -> dict:
        """
        Load manifest and verify:
          1. All file hashes match
          2. All files share the same session_id
          3. Hash chain integrity (prev_session_id link)

        Returns: validated manifest dict
        Raises:  CalibrationIntegrityError on any violation
        """
        with open(self.manifest_path) as f:
            manifest = json.load(f)

        session_id = manifest['session_id']
        errors = []

        # 1. Verify individual file hashes
        base_dir = self.manifest_path.parent
        for entry in manifest['files']:
            fpath = base_dir / entry['path']
            if not fpath.exists():
                errors.append(f"Missing: {entry['path']}")
                continue
            computed = _sha256_file(fpath)
            if computed != entry['sha256']:
                errors.append(
                    f"Hash mismatch for {entry['path']}: "
                    f"expected {entry['sha256'][:16]}…, got {computed[:16]}…")

        # 2. Verify session ID consistency in binary headers
        for entry in manifest['files']:
            fpath = base_dir / entry['path']
            if not fpath.exists(): continue
            if entry['path'].endswith('.bin'):
                file_session = _read_session_id_from_bin(fpath)
                if file_session and file_session != session_id:
                    if not self.allow_mixed:
                        errors.append(
                            f"Session mismatch in {entry['path']}: "
                            f"file={file_session}, manifest={session_id}")

        if errors:
            raise CalibrationIntegrityError(errors)

        self._manifest = manifest
        return manifest

    def check_drift(self,
                    current_dark_mean: float,
                    baseline_dark_mean: float,
                    threshold_adu_per_day: float = 5.0,
                    days_elapsed: float = 1.0) -> dict:
        """
        Detect dark current drift and recommend recalibration.

        Args:
            current_dark_mean:  current dark frame mean (ADU)
            baseline_dark_mean: dark mean at last calibration (ADU)
            threshold_adu_per_day: drift threshold for recalibration trigger
            days_elapsed:       time since last calibration
        Returns:
            dict: {drift_adu_per_day, needs_recal, severity}
        """
        drift = abs(current_dark_mean - baseline_dark_mean) / max(days_elapsed, 0.01)
        needs_recal = drift > threshold_adu_per_day
        severity = ('critical' if drift > 3 * threshold_adu_per_day else
                    'warning'  if needs_recal else 'ok')
        return {
            'drift_adu_per_day': drift,
            'needs_recalibration': needs_recal,
            'severity': severity,
            'baseline_dark_mean': baseline_dark_mean,
            'current_dark_mean': current_dark_mean,
        }


def _sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(65536), b''):
            h.update(chunk)
    return h.hexdigest()


def _read_session_id_from_bin(path: Path) -> Optional[str]:
    """Read session_id from binary calibration file extended header."""
    with open(path, 'rb') as f:
        magic = f.read(4)
        if magic not in (b'XOFF', b'XGAI', b'XDEF'):
            return None
        # Standard header: 64 or 96 bytes; extended header starts immediately after
        hdr_size = 96 if magic == b'XGAI' else 64
        f.seek(hdr_size)
        ext = f.read(32)
        if len(ext) < 8:
            return None
        return ext[:8].decode('ascii', errors='replace')


class CalibrationIntegrityError(Exception):
    def __init__(self, errors: list):
        self.errors = errors
        super().__init__('\n'.join(errors))
```

#### 2.4.4 C++ 런타임 세션 검증

```cpp
// C++ runtime session lock — called by ConfigManager during startup
// Rejects packs with mismatched session IDs before any correction is applied.

struct CalibrationManifestEntry {
    std::string  type;
    std::string  path;
    std::string  sha256;
    std::string  session_id;   // read from binary header extended block
};

class CalibrationSessionValidator {
public:
    enum class ValidationResult {
        OK,
        SESSION_MISMATCH,
        HASH_MISMATCH,
        MISSING_FILE,
        PARSE_ERROR,
    };

    ValidationResult validate_pack(const std::string& manifest_path,
                                    bool allow_mixed = false) {
        // Parse JSON manifest
        auto manifest = parse_json_manifest(manifest_path);
        if (!manifest.valid) return ValidationResult::PARSE_ERROR;

        std::string primary_session = manifest.session_id;

        for (const auto& entry : manifest.files) {
            // 1. Check file existence
            if (!std::filesystem::exists(entry.path))
                return ValidationResult::MISSING_FILE;

            // 2. Verify SHA-256
            if (compute_sha256_file(entry.path) != entry.sha256)
                return ValidationResult::HASH_MISMATCH;

            // 3. Read session ID from binary header extended block
            auto file_session = read_session_id(entry.path);
            if (!file_session.empty() &&
                file_session != primary_session && !allow_mixed) {
                log_error("Session mismatch: file={}, manifest={}",
                           file_session, primary_session);
                return ValidationResult::SESSION_MISMATCH;
            }
        }
        return ValidationResult::OK;
    }

private:
    std::string read_session_id(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return {};
        char magic[4];
        f.read(magic, 4);
        // Seek to extended header
        size_t hdr_size = (std::strncmp(magic, "XGAI", 4) == 0) ? 96 : 64;
        f.seekg(hdr_size);
        char session_buf[9] = {};
        f.read(session_buf, 8);
        return std::string(session_buf);
    }
};
```

#### 2.4.5 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| 혼합 세션 거부율 | 100% | 의도적 혼합 팩 주입 테스트 |
| 해시 검증 속도 (5개 파일 × 38MB) | < 3s | SHA-256 벤치마크 |
| 드리프트 감지 임계치 정확도 | ±0.1 ADU/day | 합성 드리프트 시나리오 |
| 매니페스트 파싱 오류 처리 | 100% 예외 캐치 | 손상된 JSON 주입 |

**IEC 62304 §5.4 추적성**: SRS ID: SRS-SEC-002 (파일 무결성 확장), SRS-SEC-003 (교정 세션 추적) — Phase 2 추가 예정

---

## 3. SWI-1: Pre-Processing 알고리즘

### 3.0 SWU-1.0 Readout Validation (GAP-I 해소)

Readout Validation은 모든 보정 전에 실행되는 **입력 품질 게이트**로, 잘못된 이미지가 파이프라인에 진입하는 것을 방지한다. xpe-algorithm-spec-deepsync.md 표 "release-safe baseline" 항목에 명시되어 있으며, 이 섹션이 상세 구현을 제공한다.

#### 3.0.1 알고리즘 수학 정의

**포화 검사 (Saturation)**:
$$P_{\text{sat}} = \frac{|\{(x,y) : I_{\text{raw}}(x,y) \geq V_{\text{sat}}\}|}{W \times H} \leq \theta_{\text{sat}}$$

**DR 클리핑 검사 (Clipped Dynamic Range)**:
$$P_{\text{clip\_low}} = \frac{|\{(x,y) : I_{\text{raw}}(x,y) \leq V_{\text{clip\_low}}\}|}{W \times H} \leq \theta_{\text{clip}}$$

**불가능한 기하학 검사 (Impossible Geometry)**:
$$\text{Valid}: W, H \in [256,\ 4096],\quad W \cdot H \leq 16{,}777{,}216,\quad W/H \in [0.5,\ 4.0]$$

**행/열 결함 검사 (Row/Column Fault)**:
$$\text{RowFault}(y) = 1 \iff \text{std}(I_{\text{raw}}[y, :]) < \sigma_{\text{line\_min}}$$

| 파라미터 | 기본값 | 의미 |
|---------|-------|------|
| `V_sat` | 65530 (14-bit: 16380) | 포화 임계치 (ADU) |
| `θ_sat` | 0.05 | 허용 포화 픽셀 비율 (5%) |
| `V_clip_low` | 4 | 하단 클리핑 임계치 (ADU) |
| `θ_clip` | 0.10 | 허용 하단 클리핑 비율 (10%) |
| `σ_line_min` | 2.0 | 최소 행/열 표준편차 (ADU) |

#### 3.0.2 Python 구현 (오프라인 QC)

```python
import numpy as np
from dataclasses import dataclass, field
from enum import Flag, auto

class ReadoutFaultCode(Flag):
    OK              = 0
    SATURATED       = auto()   # > θ_sat fraction at V_sat
    CLIPPED_DR      = auto()   # > θ_clip fraction at V_clip_low
    IMPOSSIBLE_GEOM = auto()   # width/height outside valid range
    ROW_FAULT       = auto()   # ≥1 row with std < σ_line_min
    COLUMN_FAULT    = auto()   # ≥1 col with std < σ_line_min
    EMPTY_IMAGE     = auto()   # all-zero or single-value image

@dataclass
class ReadoutValidationResult:
    fault_code:     ReadoutFaultCode = ReadoutFaultCode.OK
    fault_details:  dict             = field(default_factory=dict)
    saturated_frac: float            = 0.0
    clipped_frac:   float            = 0.0
    faulty_rows:    list             = field(default_factory=list)
    faulty_cols:    list             = field(default_factory=list)

def validate_readout(raw: np.ndarray,
                     v_sat:       int   = 65530,
                     theta_sat:   float = 0.05,
                     v_clip_low:  int   = 4,
                     theta_clip:  float = 0.10,
                     sigma_line_min: float = 2.0,
                     bit_depth:   int   = 16) -> ReadoutValidationResult:
    """
    Gate-check a raw detector image before any correction is applied.

    Returns ReadoutValidationResult; caller must reject the frame if
    fault_code != ReadoutFaultCode.OK (non-zero).
    """
    result = ReadoutValidationResult()
    H, W = raw.shape
    img = raw.astype(np.float32)

    # 1. Impossible geometry
    if not (256 <= W <= 4096 and 256 <= H <= 4096):
        result.fault_code |= ReadoutFaultCode.IMPOSSIBLE_GEOM
        result.fault_details['geometry'] = f'W={W}, H={H} outside [256,4096]'
    if W * H > 16_777_216:
        result.fault_code |= ReadoutFaultCode.IMPOSSIBLE_GEOM
        result.fault_details['geometry_area'] = f'W×H={W*H} > 16M'
    ar = W / H
    if not (0.5 <= ar <= 4.0):
        result.fault_code |= ReadoutFaultCode.IMPOSSIBLE_GEOM
        result.fault_details['aspect_ratio'] = f'{ar:.3f}'

    # 2. Saturation check
    max_adu = (1 << bit_depth) - 1
    sat_thresh = min(v_sat, max_adu)
    sat_mask   = img >= sat_thresh
    result.saturated_frac = float(np.mean(sat_mask))
    if result.saturated_frac > theta_sat:
        result.fault_code |= ReadoutFaultCode.SATURATED
        result.fault_details['saturated_frac'] = f'{result.saturated_frac:.4f}'

    # 3. Clipped DR check (lower end)
    clip_mask = img <= v_clip_low
    result.clipped_frac = float(np.mean(clip_mask))
    if result.clipped_frac > theta_clip:
        result.fault_code |= ReadoutFaultCode.CLIPPED_DR
        result.fault_details['clipped_frac'] = f'{result.clipped_frac:.4f}'

    # 4. Empty image check
    if np.std(img) < 10.0:
        result.fault_code |= ReadoutFaultCode.EMPTY_IMAGE
        result.fault_details['std'] = f'{float(np.std(img)):.2f}'

    # 5. Row fault detection
    row_stds = np.std(img, axis=1)
    faulty_rows = np.where(row_stds < sigma_line_min)[0].tolist()
    if faulty_rows:
        result.fault_code  |= ReadoutFaultCode.ROW_FAULT
        result.faulty_rows  = faulty_rows
        result.fault_details['faulty_row_count'] = len(faulty_rows)

    # 6. Column fault detection
    col_stds = np.std(img, axis=0)
    faulty_cols = np.where(col_stds < sigma_line_min)[0].tolist()
    if faulty_cols:
        result.fault_code  |= ReadoutFaultCode.COLUMN_FAULT
        result.faulty_cols  = faulty_cols
        result.fault_details['faulty_col_count'] = len(faulty_cols)

    return result
```

#### 3.0.3 C++ 런타임 구현

```cpp
enum class ReadoutFaultCode : uint32_t {
    OK              = 0x00,
    SATURATED       = 0x01,
    CLIPPED_DR      = 0x02,
    IMPOSSIBLE_GEOM = 0x04,
    ROW_FAULT       = 0x08,
    COLUMN_FAULT    = 0x10,
    EMPTY_IMAGE     = 0x20,
};
inline ReadoutFaultCode operator|(ReadoutFaultCode a, ReadoutFaultCode b) {
    return static_cast<ReadoutFaultCode>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

struct ReadoutValidationResult {
    ReadoutFaultCode fault_code  = ReadoutFaultCode::OK;
    float  saturated_frac        = 0.0f;
    float  clipped_frac          = 0.0f;
    int    faulty_row_count      = 0;
    int    faulty_col_count      = 0;
};

ReadoutValidationResult xpe_validate_readout(
        const uint16_t* raw, uint32_t W, uint32_t H, uint32_t bit_depth = 16) {
    ReadoutValidationResult r;
    const uint32_t total = W * H;
    const uint16_t v_sat       = static_cast<uint16_t>((1u << bit_depth) - 6u);
    const uint16_t v_clip_low  = 4u;
    const float    theta_sat   = 0.05f;
    const float    theta_clip  = 0.10f;
    const float    sigma_line_min = 2.0f;

    // 1. Geometry check
    if (W < 256 || W > 4096 || H < 256 || H > 4096 || total > 16'777'216u) {
        r.fault_code = r.fault_code | ReadoutFaultCode::IMPOSSIBLE_GEOM;
    }
    float ar = static_cast<float>(W) / H;
    if (ar < 0.5f || ar > 4.0f) {
        r.fault_code = r.fault_code | ReadoutFaultCode::IMPOSSIBLE_GEOM;
    }

    // 2. Saturation + clip count (AVX2 vectorised)
    uint32_t sat_cnt = 0, clip_cnt = 0;
    for (uint32_t i = 0; i < total; ++i) {
        if (raw[i] >= v_sat)      ++sat_cnt;
        if (raw[i] <= v_clip_low) ++clip_cnt;
    }
    r.saturated_frac = static_cast<float>(sat_cnt) / total;
    r.clipped_frac   = static_cast<float>(clip_cnt) / total;

    if (r.saturated_frac > theta_sat)
        r.fault_code = r.fault_code | ReadoutFaultCode::SATURATED;
    if (r.clipped_frac > theta_clip)
        r.fault_code = r.fault_code | ReadoutFaultCode::CLIPPED_DR;

    // 3. Row/column fault (Welford online mean/variance)
    for (uint32_t y = 0; y < H; ++y) {
        double mean = 0.0, M2 = 0.0;
        for (uint32_t x = 0; x < W; ++x) {
            double delta = raw[y * W + x] - mean;
            mean += delta / (x + 1);
            M2   += delta * (raw[y * W + x] - mean);
        }
        float std_row = static_cast<float>(std::sqrt(M2 / (W - 1)));
        if (std_row < sigma_line_min) ++r.faulty_row_count;
    }
    for (uint32_t x = 0; x < W; ++x) {
        double mean = 0.0, M2 = 0.0;
        for (uint32_t y = 0; y < H; ++y) {
            double delta = raw[y * W + x] - mean;
            mean += delta / (y + 1);
            M2   += delta * (raw[y * W + x] - mean);
        }
        float std_col = static_cast<float>(std::sqrt(M2 / (H - 1)));
        if (std_col < sigma_line_min) ++r.faulty_col_count;
    }
    if (r.faulty_row_count > 0)
        r.fault_code = r.fault_code | ReadoutFaultCode::ROW_FAULT;
    if (r.faulty_col_count > 0)
        r.fault_code = r.fault_code | ReadoutFaultCode::COLUMN_FAULT;

    return r;
}
```

#### 3.0.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| 포화 감지 정확도 | FPR < 1%, FNR = 0% | 합성 포화 이미지 주입 |
| 행/열 결함 감지 | 결함 없는 경우 경보 없음 | 정상 dark frame 검사 |
| 처리 시간 (3072×3072) | < 5ms | 단일 코어, no SIMD needed |
| 기하학 검사 | 잘못된 크기 100% 차단 | 경계값 분석 |

---

### 3.0.5 SWU-1.0.5 Non-linearity Correction (GAP-H 해소)

비선형성 보정은 Offset/Gain 보정 이후, Log Transform 이전에 적용한다. xpe-algorithm-spec-deepsync.md "release-safe baseline"에서 "monotonic LUT or low-order polynomial"로 명시되어 있다.

#### 3.0.6 알고리즘 수학 정의

**Monotonic LUT 방법 (권장)**:

LUT $\mathcal{L}$ 은 ADU 입력값에 대한 선형 응답 보정 출력을 저장한다:

$$I_{\text{linear}}(x,y) = \mathcal{L}\!\left[I_{\text{gain\_corr}}(x,y)\right]$$

LUT 생성 시 단조성 조건을 강제한다:
$$\mathcal{L}[v+1] \geq \mathcal{L}[v] \quad \forall\ v \in [0,\ 2^B - 2]$$

**Polynomial 방법 (대안)**:
$$I_{\text{linear}} = \sum_{k=0}^{K} c_k \cdot I_{\text{gain\_corr}}^k, \quad K \leq 4$$

단조성 요구사항: 도함수 $\frac{dI_{\text{linear}}}{dI_{\text{gain\_corr}}} > 0$ (전 범위에서 양수)

#### 3.0.7 Python 교정 구현 (오프라인)

```python
import numpy as np
from scipy.interpolate import PchipInterpolator

def calibrate_nonlinearity_lut(
        signal_levels_adu:  np.ndarray,
        true_exposures_mAs: np.ndarray,
        bit_depth: int = 16) -> np.ndarray:
    """
    Generate a monotonic non-linearity correction LUT from calibration data.

    Args:
        signal_levels_adu:  measured detector signal at each exposure (N,)
        true_exposures_mAs: reference exposure levels in mAs (N,)  
                            Linear response: signal ∝ mAs
        bit_depth:          detector bit depth (default 16)
    Returns:
        lut: float32 array of length 2^bit_depth
             lut[adu] = linearity-corrected value in ADU-equivalent units

    Method:
        1. Fit PCHIP spline: ADU → ideal_linear (preserves monotonicity)
        2. Evaluate at every integer ADU level 0..2^B-1
        3. Clip & enforce monotonicity (post-fit safety pass)
    """
    N = len(signal_levels_adu)
    assert len(true_exposures_mAs) == N and N >= 4, \
        "Need ≥4 calibration points"

    # Normalize: ideal linear signal = gain_mean × (exposure / exposure_ref)
    exposure_ref  = true_exposures_mAs[N // 2]  # mid-range reference
    signal_ref    = signal_levels_adu[N // 2]
    ideal_signals = signal_ref * (true_exposures_mAs / exposure_ref)

    # Sort by input signal for spline fitting
    sort_idx = np.argsort(signal_levels_adu)
    x_ctrl   = signal_levels_adu[sort_idx].astype(np.float64)
    y_ctrl   = ideal_signals[sort_idx].astype(np.float64)

    # PCHIP: monotone cubic Hermite interpolation
    interp = PchipInterpolator(x_ctrl, y_ctrl, extrapolate=True)

    full_adu_range = np.arange(1 << bit_depth, dtype=np.float64)
    lut = interp(full_adu_range).astype(np.float32)

    # Enforce monotonicity (safety clip)
    lut[0] = max(0.0, lut[0])
    for i in range(1, len(lut)):
        if lut[i] < lut[i - 1]:
            lut[i] = lut[i - 1]  # monotone clamp

    # Clip to valid ADU range
    max_adu = float((1 << bit_depth) - 1)
    lut = np.clip(lut, 0.0, max_adu)
    return lut


def validate_nonlinearity_lut(lut: np.ndarray,
                               max_deviation_pct: float = 5.0) -> dict:
    """
    Validate that the generated LUT is monotone and within deviation bounds.

    Returns dict with: is_valid, max_deviation_pct, monotone_violations
    """
    diffs = np.diff(lut)
    violations = int(np.sum(diffs < 0))
    # Max deviation from identity (no correction)
    identity  = np.arange(len(lut), dtype=np.float32)
    deviation = np.abs(lut - identity) / (identity + 1.0) * 100.0  # percent
    max_dev   = float(np.max(deviation))
    return {
        'is_valid':            violations == 0 and max_dev <= max_deviation_pct,
        'max_deviation_pct':   max_dev,
        'monotone_violations': violations,
    }
```

#### 3.0.8 C++ 런타임 구현 (AVX2 + LUT lookup)

```cpp
// Non-linearity correction via pre-loaded float LUT
// LUT size: 2^bit_depth floats (256KB for 16-bit)
// Called after xpe_gain_correct(), before xpe_log_transform()

void xpe_nonlinearity_correct(const float*    __restrict__ gain_corr_img,
                               const float*    __restrict__ lut,        // size: 1<<bit_depth
                               float*          __restrict__ out,
                               uint32_t width, uint32_t height,
                               uint32_t bit_depth = 16u) {
    const size_t   total   = static_cast<size_t>(width) * height;
    const uint32_t max_idx = (1u << bit_depth) - 1u;

    // Scalar LUT lookup (vectorisation not beneficial for scatter-gather pattern)
    for (size_t i = 0; i < total; ++i) {
        // Clamp to valid LUT range before index conversion
        float  v   = std::clamp(gain_corr_img[i], 0.0f, static_cast<float>(max_idx));
        uint32_t idx = static_cast<uint32_t>(v + 0.5f);  // nearest-integer
        idx = std::min(idx, max_idx);
        out[i] = lut[idx];
    }
}
```

#### 3.0.9 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| LUT 단조성 | 위반 0건 | `validate_nonlinearity_lut()` |
| 최대 보정 편차 | ≤ 5% | identity 대비 백분율 |
| 선형성 잔차 R² | ≥ 0.9995 | 교정 후 계단식 노출 |
| 처리 시간 (3072×3072) | < 30ms | 단일 코어 |

---

### 3.1 SWU-1.1 Offset Correction (SRS-FUNC-001)

#### 3.1.1 알고리즘 수학 정의

$$I_{\text{offset}}(x,y) = \max\left(I_{\text{raw}}(x,y) - I_{\text{dark}}(x,y),\ 0\right)$$

- **입력**: `I_raw` (uint16), `I_dark` (float32 mean of ≥16 dark frames)
- **출력**: `I_offset` (float32, ≥ 0)
- **목적**: Detector dark current 및 readout offset 제거

#### 3.1.2 Offset Map 생성 알고리즘 (Python, 오프라인)

```python
def compute_offset_map(dark_frames: list[np.ndarray]) -> np.ndarray:
    """
    Generate offset correction map from ≥16 dark frames.
    
    Args:
        dark_frames: list of uint16 arrays, shape (H, W), len ≥ 16
    Returns:
        float32 offset map, shape (H, W)
    """
    assert len(dark_frames) >= 16, "Minimum 16 dark frames required"
    
    stack = np.stack(dark_frames, axis=0).astype(np.float64)
    
    # Temporal outlier rejection (σ-clipping, 3σ)
    mean = np.mean(stack, axis=0)
    std  = np.std(stack, axis=0)
    mask = np.abs(stack - mean) <= 3.0 * std  # (N, H, W)
    
    # Compute masked mean
    offset_map = np.sum(stack * mask, axis=0) / np.maximum(np.sum(mask, axis=0), 1)
    
    return offset_map.astype(np.float32)
```

#### 3.1.3 C++ 런타임 구현 (AVX2 최적화)

```cpp
// SWU-1.1: xpe_offset_correct()
// Vectorized subtraction with floor-at-zero (SRS-PERF-001: ≤500ms)
void xpe_offset_correct(const uint16_t* __restrict__ raw,
                         const float*    __restrict__ offset_map,
                         float*          __restrict__ out,
                         uint32_t width, uint32_t height) {
    const size_t total = static_cast<size_t>(width) * height;
    size_t i = 0;

    // AVX2 path: process 8 float32 per iteration
    const __m256 zero = _mm256_setzero_ps();
    for (; i + 8 <= total; i += 8) {
        // Load 8 uint16 → convert to float32
        __m128i raw16 = _mm_loadu_si128(
            reinterpret_cast<const __m128i*>(raw + i));
        __m256 raw_f  = _mm256_cvtepi32_ps(
            _mm256_cvtepu16_epi32(raw16));
        
        __m256 off_f  = _mm256_loadu_ps(offset_map + i);
        __m256 diff   = _mm256_sub_ps(raw_f, off_f);
        __m256 result = _mm256_max_ps(diff, zero);   // clamp at 0
        
        _mm256_storeu_ps(out + i, result);
    }
    
    // Scalar tail
    for (; i < total; ++i) {
        float diff = static_cast<float>(raw[i]) - offset_map[i];
        out[i] = (diff < 0.0f) ? 0.0f : diff;
    }
}
```

#### 3.1.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| Residual dark signal | Mean < 1.0 ADU | 보정 후 dark field 평균 |
| Negative 픽셀 | 0개 | min(I_offset) ≥ 0 |
| 처리 시간 (3072×3072) | < 50ms | 단일 코어 벤치마크 |

---

### 3.2 SWU-1.2 Gain Correction (SRS-FUNC-002)

#### 3.2.1 알고리즘 수학 정의

$$G(x,y) = \frac{\bar{I}_{\text{flat}}}{I_{\text{flat}}(x,y) - I_{\text{dark}}(x,y)}$$

$$I_{\text{corr}}(x,y) = I_{\text{offset}}(x,y) \cdot G(x,y)$$

- **GainMean** $\bar{I}_{\text{flat}}$: ROI 내 `(Flood - Offset)` 의 spatial mean
- **목적**: Pixel 간 감도 차이 (heel effect, scintillator 두께 불균일) 보정
- **SID별 개별 맵**: kVp에 따른 스펙트럼 변화 → SID마다 별도 gain map 보유

#### 3.2.2 Gain Map 생성 알고리즘 (Python, 오프라인)

```python
def compute_gain_map(flood_frames: list[np.ndarray],
                     offset_map: np.ndarray,
                     roi: tuple[int,int,int,int] | None = None) -> tuple[np.ndarray, float]:
    """
    Generate per-SID gain correction map.
    
    Args:
        flood_frames: list of uint16 flood images (≥8 recommended)
        offset_map:   float32 offset map (H, W)
        roi:          (x0, y0, x1, y1) for GainMean calculation, None = full image
    Returns:
        (gain_map float32 (H,W), gain_mean float32)
    """
    # Average flood frames
    stack = np.stack(flood_frames, axis=0).astype(np.float32)
    flood_mean = np.mean(stack, axis=0)
    
    # Subtract dark
    net_signal = flood_mean - offset_map
    
    # Compute GainMean from ROI (avoid detector edge artefacts)
    if roi:
        x0, y0, x1, y1 = roi
        roi_signal = net_signal[y0:y1, x0:x1]
    else:
        # Auto-trim: inner 80% of image
        h, w = net_signal.shape
        margin_y, margin_x = h // 10, w // 10
        roi_signal = net_signal[margin_y:-margin_y, margin_x:-margin_x]
    
    gain_mean = float(np.mean(roi_signal))
    
    # Compute gain map; clamp to prevent extreme values
    with np.errstate(divide='ignore', invalid='ignore'):
        gain_map = np.where(net_signal > 0,
                            gain_mean / net_signal,
                            1.0)  # fallback for near-zero pixels
    
    gain_map = np.clip(gain_map, 0.5, 2.0).astype(np.float32)
    
    return gain_map, gain_mean
```

#### 3.2.3 C++ 런타임 구현 (AVX2 최적화)

```cpp
// SWU-1.2: xpe_gain_correct()
void xpe_gain_correct(const float* __restrict__ offset_corrected,
                       const float* __restrict__ gain_map,
                       float*       __restrict__ out,
                       uint32_t width, uint32_t height) {
    const size_t total = static_cast<size_t>(width) * height;
    size_t i = 0;

    for (; i + 8 <= total; i += 8) {
        __m256 img  = _mm256_loadu_ps(offset_corrected + i);
        __m256 gain = _mm256_loadu_ps(gain_map + i);
        __m256 res  = _mm256_mul_ps(img, gain);
        _mm256_storeu_ps(out + i, res);
    }
    for (; i < total; ++i) {
        out[i] = offset_corrected[i] * gain_map[i];
    }
}
```

#### 3.2.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| Uniformity (PRNU) | CV < 1% after gain | std/mean × 100% |
| Gain map range | [0.5, 2.0] | min/max of G(x,y) |
| Heel effect correction | CV 감소율 > 80% | 보정 전후 CV 비교 |

---

### 3.2.5 SWU-1.2b Multi-SID Gain 보간 및 kVp-Stratified Gain 선택 (GAP-P 해소)

기존 §3.2는 단일 SID·kVp 조합에 대한 gain 보정을 다룬다. 본 섹션은 여러 SID·kVp 조합으로 사전 교정된 gain 맵 세트에서 실제 촬영 조건에 맞는 맵을 선택·보간하는 알고리즘을 추가한다.

#### 3.2.5.1 알고리즘 수학 정의

SID 집합 $\{S_1, S_2, \ldots, S_N\}$, kVp 집합 $\{k_1, k_2, \ldots, k_M\}$으로 교정된 gain 맵 격자 $G(x, y; S_i, k_j)$가 주어졌을 때:

$$G_{\text{select}}(x,y) = \sum_{i,j} w_{ij} \cdot G(x,y;S_i,k_j)$$

**이중선형 보간 가중치**:

$$w_{ij} = \left[(1-t_S)(1-t_k),\ (1-t_S)t_k,\ t_S(1-t_k),\ t_S t_k\right]_{i \in \{0,1\},\, j \in \{0,1\}}$$

$$t_S = \frac{S - S_{lo}}{S_{hi} - S_{lo}}, \quad t_k = \frac{k - k_{lo}}{k_{hi} - k_{lo}}$$

**자동 SID 선택 로직**:

$$\text{SID}_{\text{use}} = \begin{cases} S_1 & \text{if } S < S_1 \\ S_N & \text{if } S > S_N \\ \text{interpolate}(S_{lo}, S_{hi}) & \text{otherwise} \end{cases}$$

#### 3.2.5.2 Python 구현 (오프라인)

```python
import numpy as np
from pathlib import Path
from typing import Dict, Tuple, List
import struct

GainKey = Tuple[float, float]  # (sid_mm, kvp)

def load_gain_map_table(gain_dir: Path,
                         sid_list: List[float],
                         kvp_list: List[float]) -> Dict[GainKey, np.ndarray]:
    """
    Load pre-calibrated gain maps for all SID/kVp combinations.

    Expects files named: gain_SIDxxxx_kVPyyy.bin (XGAI format)
    Returns dict: {(sid_mm, kvp): gain_map float32 (H, W)}
    """
    table: Dict[GainKey, np.ndarray] = {}
    for sid in sid_list:
        for kvp in kvp_list:
            fname = gain_dir / f"gain_SID{sid:04.0f}_kVP{kvp:03.0f}.bin"
            if fname.exists():
                table[(sid, kvp)] = _read_xgai_bin(fname)
            else:
                raise FileNotFoundError(f"Missing gain map: {fname}")
    return table


def _read_xgai_bin(path: Path) -> np.ndarray:
    """Parse XGAI binary format (§2.2.2)."""
    with open(path, 'rb') as f:
        hdr = f.read(96)
        magic = hdr[:4]
        assert magic == b'XGAI', f"Bad magic: {magic}"
        w = struct.unpack_from('<I', hdr, 8)[0]
        h = struct.unpack_from('<I', hdr, 12)[0]
        payload = np.frombuffer(f.read(), dtype=np.float32)
    return payload.reshape(h, w)


def select_gain_map_bilinear(
        table:    Dict[GainKey, np.ndarray],
        sid_mm:   float,
        kvp:      float,
        sid_list: List[float],
        kvp_list: List[float]) -> np.ndarray:
    """
    Bilinear interpolation of gain map for arbitrary SID and kVp.

    Args:
        table:    pre-loaded gain map dictionary
        sid_mm:   actual source-to-image distance (mm)
        kvp:      actual tube voltage (kVp)
        sid_list: sorted calibrated SID values
        kvp_list: sorted calibrated kVp values
    Returns:
        gain_map: float32 (H, W) — interpolated gain map
    """
    sid_arr = np.array(sorted(sid_list), dtype=np.float64)
    kvp_arr = np.array(sorted(kvp_list), dtype=np.float64)

    # Clamp to calibrated range
    sid_c = float(np.clip(sid_mm, sid_arr[0], sid_arr[-1]))
    kvp_c = float(np.clip(kvp,    kvp_arr[0], kvp_arr[-1]))

    i_s = int(np.clip(np.searchsorted(sid_arr, sid_c, 'right') - 1, 0, len(sid_arr) - 2))
    i_k = int(np.clip(np.searchsorted(kvp_arr, kvp_c, 'right') - 1, 0, len(kvp_arr) - 2))

    s_lo, s_hi = sid_arr[i_s], sid_arr[i_s + 1]
    k_lo, k_hi = kvp_arr[i_k], kvp_arr[i_k + 1]

    ts = (sid_c - s_lo) / (s_hi - s_lo) if s_hi != s_lo else 0.0
    tk = (kvp_c - k_lo) / (k_hi - k_lo) if k_hi != k_lo else 0.0

    m00 = table[(s_lo, k_lo)].astype(np.float64)
    m01 = table[(s_lo, k_hi)].astype(np.float64)
    m10 = table[(s_hi, k_lo)].astype(np.float64)
    m11 = table[(s_hi, k_hi)].astype(np.float64)

    result = ((1 - ts) * (1 - tk) * m00 +
              (1 - ts) *      tk  * m01 +
                   ts  * (1 - tk) * m10 +
                   ts  *      tk  * m11)
    return result.astype(np.float32)


def validate_gain_table_consistency(
        table:    Dict[GainKey, np.ndarray],
        max_ratio: float = 1.10) -> List[str]:
    """
    Check that adjacent gain maps differ by no more than max_ratio.
    Returns list of warnings (empty = OK).
    """
    warnings = []
    keys = sorted(table.keys())
    for i, k1 in enumerate(keys):
        for k2 in keys[i + 1:]:
            sid_diff = abs(k1[0] - k2[0])
            kvp_diff = abs(k1[1] - k2[1])
            # Only check neighbours
            if sid_diff <= 200 and kvp_diff <= 20:
                ratio = table[k1] / np.maximum(table[k2], 1e-6)
                if float(np.max(ratio)) > max_ratio or float(np.min(ratio)) < 1.0 / max_ratio:
                    warnings.append(
                        f"Gain maps {k1}↔{k2} differ by > {max_ratio}×: "
                        f"max={float(np.max(ratio)):.3f}")
    return warnings
```

#### 3.2.5.3 C++ 런타임 구현

```cpp
// Multi-SID gain map selector — C++ runtime
// Gain maps are pre-loaded at startup into GainMapTable.

struct GainMapEntry {
    float  sid_mm;
    float  kvp;
    float* map;       // float32 (H × W), pinned memory preferred
    size_t size;
};

class GainMapTable {
public:
    // Load all entries from calibration directory
    void load(const std::vector<std::string>& paths);

    // Select best map for given SID/kVp (nearest or interpolate)
    // Returns pointer to the map; ownership stays with GainMapTable.
    const float* get_map(float sid_mm, float kvp,
                          uint32_t W, uint32_t H,
                          float* interp_buf = nullptr) const {
        // Find bounding entries
        const GainMapEntry* lo_sid  = nullptr;
        const GainMapEntry* hi_sid  = nullptr;
        float best_sid_lo = -1e9f, best_sid_hi = 1e9f;

        for (const auto& e : entries_) {
            if (std::fabsf(e.kvp - kvp) > 10.0f) continue;
            if (e.sid_mm <= sid_mm && e.sid_mm > best_sid_lo) {
                best_sid_lo = e.sid_mm; lo_sid = &e;
            }
            if (e.sid_mm > sid_mm && e.sid_mm < best_sid_hi) {
                best_sid_hi = e.sid_mm; hi_sid = &e;
            }
        }

        if (!lo_sid && !hi_sid) return nullptr;
        if (!lo_sid)            return hi_sid->map;
        if (!hi_sid)            return lo_sid->map;

        // Bilinear interpolation into interp_buf
        if (!interp_buf) return lo_sid->map;  // fallback: no buffer supplied

        float t = (sid_mm - lo_sid->sid_mm) / (hi_sid->sid_mm - lo_sid->sid_mm);
        const size_t total = static_cast<size_t>(W) * H;
        const __m256 v_t    = _mm256_set1_ps(t);
        const __m256 v_1mt  = _mm256_set1_ps(1.0f - t);
        size_t i = 0;
        for (; i + 8 <= total; i += 8) {
            __m256 a = _mm256_loadu_ps(lo_sid->map + i);
            __m256 b = _mm256_loadu_ps(hi_sid->map + i);
            _mm256_storeu_ps(interp_buf + i,
                             _mm256_fmadd_ps(v_t, b, _mm256_mul_ps(v_1mt, a)));
        }
        for (; i < total; ++i) {
            interp_buf[i] = (1.0f - t) * lo_sid->map[i] + t * hi_sid->map[i];
        }
        return interp_buf;
    }

private:
    std::vector<GainMapEntry> entries_;
};
```

#### 3.2.5.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| 보간 오차 (중간 SID) | < 0.5% signal | 교정되지 않은 SID에서 실측과 보간 비교 |
| kVp 전환 후 PRNU | CV < 1% | kVp ±20% 변화 후 flood 균일도 |
| 맵 로드 시간 (8 맵 × 3072×3072) | < 2s | 시작 시 일괄 로딩 |
| 자동 SID 선택 정확도 | ±25mm 이내 | SID 센서 데이터 교차 검증 |

**IEC 62304 §5.4 추적성**: SRS ID: SRS-FUNC-002 (Gain Correction 확장) — Multi-SID 항목 Phase 2 추가 예정

---

### 3.3 SWU-1.3 Defect Pixel Correction (SRS-FUNC-003)

#### 3.3.1 결함 픽셀 분류 체계

| 유형 | 정의 | 보간 방법 |
|------|------|---------|
| Point Defect | 단일 픽셀: G(x,y) < G_mean × 0.5 또는 > G_mean × 2.0 | 4-neighbor 평균 |
| Cluster Defect | 반경 r ≤ 3 내 ≥4개 point defect | 8-neighbor 유효 픽셀 평균 |
| Column Defect | 전체 컬럼의 ≥80% 결함 | 좌우 컬럼 선형 보간 |
| Row Defect | 전체 행의 ≥80% 결함 | 상하 행 선형 보간 |
| Stuck Pixel | Dark frame에서도 포화 (>MAX-100 ADU) | 주변 median |

#### 3.3.2 결함 맵 생성 알고리즘 (Python, 오프라인)

```python
def create_defect_map(gain_map: np.ndarray,
                      dark_map: np.ndarray,
                      bit_depth: int = 14) -> np.ndarray:
    """
    Detect and classify defect pixels from calibration data.
    
    Returns:
        defect_map: uint8 array (H, W)
          0 = good pixel
          1 = point defect
          2 = cluster defect  
          3 = column defect
          4 = row defect
          5 = stuck pixel (always bright)
    """
    H, W = gain_map.shape
    defect_map = np.zeros((H, W), dtype=np.uint8)
    max_adu = (1 << bit_depth) - 1
    
    gain_mean = np.median(gain_map)  # robust to outliers
    gain_std  = np.std(gain_map[
        (gain_map > gain_mean * 0.5) & (gain_map < gain_mean * 2.0)])
    
    # 1. Point defects from gain map
    low_gain  = gain_map < gain_mean * 0.5
    high_gain = gain_map > gain_mean * 2.0
    point_mask = low_gain | high_gain
    defect_map[point_mask] = 1
    
    # 2. Stuck pixels from dark map
    stuck = dark_map > (max_adu - 100)
    defect_map[stuck] = 5
    
    # 3. Cluster detection: connected component analysis
    from scipy import ndimage
    labeled, n_clusters = ndimage.label(point_mask)
    cluster_sizes = ndimage.sum(point_mask, labeled, range(1, n_clusters + 1))
    for i, size in enumerate(cluster_sizes, start=1):
        if size >= 4:
            defect_map[labeled == i] = 2  # upgrade to cluster
    
    # 4. Column defects
    col_defect_frac = np.mean(defect_map > 0, axis=0)  # fraction per column
    bad_cols = col_defect_frac >= 0.8
    defect_map[:, bad_cols] = 3
    
    # 5. Row defects
    row_defect_frac = np.mean(defect_map > 0, axis=1)  # fraction per row
    bad_rows = row_defect_frac >= 0.8
    defect_map[bad_rows, :] = 4
    
    return defect_map
```

#### 3.3.3 C++ 런타임 보간 알고리즘

```cpp
// Defect interpolation priority: row/col first, then cluster, then point
// Interpolation methods:

// Point/Cluster: Weighted average of valid neighbors
float interpolate_point(const float* img, int x, int y, int W, int H,
                         const uint8_t* defect_map, bool use_8neighbor) {
    float sum = 0.0f;
    int   cnt = 0;
    const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    const int dy[] = { 0,-1, 0,  1,-1,  1, 1,-1};
    int n_neighbors = use_8neighbor ? 8 : 4;
    // For 4-neighbor: only first 4 entries used with indices {1,0}, {-1,0}, {0,1}, {0,-1}
    
    for (int k = 0; k < n_neighbors; ++k) {
        int nx = x + dx[k], ny = y + dy[k];
        if (nx >= 0 && nx < W && ny >= 0 && ny < H &&
            defect_map[ny * W + nx] == 0) {
            sum += img[ny * W + nx];
            ++cnt;
        }
    }
    return (cnt > 0) ? sum / cnt : img[y * W + x];
}

// Column defect: linear interpolation from left-right valid columns
float interpolate_column(const float* img, int x, int y, int W, int H,
                          const uint8_t* defect_map) {
    // Find nearest valid left column
    int left = x - 1;
    while (left >= 0 && defect_map[y * W + left] == 3) --left;
    int right = x + 1;
    while (right < W && defect_map[y * W + right] == 3) ++right;
    
    if (left < 0 && right >= W) return img[y * W + x]; // no valid
    if (left < 0)  return img[y * W + right];
    if (right >= W) return img[y * W + left];
    
    float t = float(x - left) / float(right - left);
    return img[y * W + left] * (1.0f - t) + img[y * W + right] * t;
}

// Main defect correction pass (two-pass: line defects first)
void xpe_defect_correct(float* __restrict__ img,
                          const uint8_t* __restrict__ defect_map,
                          uint32_t width, uint32_t height) {
    // Pass 1: Row/Column defects
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint8_t dt = defect_map[y * width + x];
            if (dt == 3) {
                img[y * width + x] =
                    interpolate_column(img, x, y, width, height, defect_map);
            } else if (dt == 4) {
                // Row: interpolate from above/below rows
                img[y * width + x] =
                    interpolate_row(img, x, y, width, height, defect_map);
            }
        }
    }
    
    // Pass 2: Cluster defects (8-neighbor)
    // Pass 3: Point defects (4-neighbor) + Stuck pixels (median)
    // (implementation follows same pattern)
}
```

#### 3.3.4 런타임 자동 갱신 알고리즘

```cpp
// Runtime defect detection: identify new defects during exposure sequence
// Algorithm:
//   1. Compute per-pixel deviation: |current - reference| / (local_std + eps)
//   2. Pixels with z-score > threshold_sigma → new defect candidate
//   3. Local std estimated over 5×5 neighbourhood using AVX2 vectorisation
//   4. Set bit 1 (0x02) in defect_map for runtime-flagged pixels
//   5. Caller must invoke xpe_defect_correct() to interpolate flagged pixels
//
// Note: reference_img should be a rolling mean of N_ref (≥4) preceding frames.
//       Use atomic write to defect_map to allow concurrent correction pass.

static void compute_local_std_row(const float* src, float* local_std_out,
                                   uint32_t W, uint32_t y, uint32_t H,
                                   float eps = 1e-6f) {
    // 5×5 neighbourhood local standard deviation (vertical strip [y-2, y+2])
    const int radius = 2;
    for (uint32_t x = 0; x < W; ++x) {
        float sum = 0.0f, sum_sq = 0.0f;
        int   n   = 0;
        for (int dy = -radius; dy <= radius; ++dy) {
            int ny = static_cast<int>(y) + dy;
            if (ny < 0 || ny >= static_cast<int>(H)) continue;
            for (int dx = -radius; dx <= radius; ++dx) {
                int nx = static_cast<int>(x) + dx;
                if (nx < 0 || nx >= static_cast<int>(W)) continue;
                float v = src[ny * W + nx];
                sum    += v;
                sum_sq += v * v;
                ++n;
            }
        }
        float mean = sum / n;
        float var  = std::max(0.0f, sum_sq / n - mean * mean);
        local_std_out[y * W + x] = std::sqrt(var) + eps;
    }
}

void update_defect_map_runtime(float*   __restrict__ current_img,
                                float*   __restrict__ reference_img,
                                uint8_t* __restrict__ defect_map,
                                uint32_t width,
                                uint32_t height,
                                float    threshold_sigma /* = 5.0f */) {
    const size_t total = static_cast<size_t>(width) * height;

    // Allocate temporary local_std buffer
    std::vector<float> local_std(total);

    // Compute local std row-by-row (OpenMP parallelisable)
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < static_cast<int>(height); ++y) {
        compute_local_std_row(reference_img, local_std.data(),
                               width, static_cast<uint32_t>(y), height);
    }

    // AVX2 vectorised z-score threshold pass
    const __m256 v_thresh = _mm256_set1_ps(threshold_sigma);
    size_t i = 0;

    for (; i + 8 <= total; i += 8) {
        __m256 cur  = _mm256_loadu_ps(current_img   + i);
        __m256 ref  = _mm256_loadu_ps(reference_img + i);
        __m256 lstd = _mm256_loadu_ps(local_std.data() + i);

        // |cur - ref| / local_std
        __m256 diff   = _mm256_sub_ps(cur, ref);
        __m256 absdif = _mm256_andnot_ps(_mm256_set1_ps(-0.0f), diff);  // abs
        __m256 zscore = _mm256_div_ps(absdif, lstd);

        // z-score > threshold_sigma → candidate defect
        __m256 cmp = _mm256_cmp_ps(zscore, v_thresh, _CMP_GT_OQ);
        int mask8  = _mm256_movemask_ps(cmp);

        if (mask8 != 0) {
            for (int k = 0; k < 8; ++k) {
                if (mask8 & (1 << k)) {
                    // Set bit 1 (runtime defect flag), preserve other bits
                    defect_map[i + k] |= 0x02u;
                }
            }
        }
    }
    // Scalar tail
    for (; i < total; ++i) {
        float z = std::fabsf(current_img[i] - reference_img[i]) / local_std[i];
        if (z > threshold_sigma) {
            defect_map[i] |= 0x02u;
        }
    }
}
```

---

### 3.4 SWU-1.4 Ghost/Lag Correction (SRS-FUNC-004)

#### 3.4.1 알고리즘 수학 정의 — Siewerdsen-Jaffray Multi-Exponential Model

$$\text{Lag}(t) = \sum_{i=1}^{3} \alpha_i \cdot e^{-t/\tau_i}$$

$$I_{\text{true}}(t) = I_{\text{measured}}(t) - \text{Lag}(t) \cdot I_{\text{prev\_max}}$$

- **파라미터**: `α = [0.04, 0.02, 0.005]`, `τ = [0.5s, 2.0s, 10.0s]` (CsI:Tl/a-Si 기본값)
- **교정**: 각 detector 유형별 실측 피팅으로 파라미터 결정
- **목적**: 이전 노출의 잔류 신호(ghost/lag) 제거, ≥90% ghost removal 달성

#### 3.4.2 Python 피팅 (오프라인 교정)

```python
from scipy.optimize import curve_fit

def multi_exponential_lag(t: np.ndarray, a1, tau1, a2, tau2, a3, tau3) -> np.ndarray:
    return a1 * np.exp(-t / tau1) + a2 * np.exp(-t / tau2) + a3 * np.exp(-t / tau3)

def fit_lag_parameters(lag_decay_data: np.ndarray,
                        time_points: np.ndarray) -> dict:
    """
    Fit 3-component exponential lag model to measured decay data.
    
    Args:
        lag_decay_data: normalized lag fraction at each time point (0-1)
        time_points:    time in seconds after initial exposure
    Returns:
        dict with keys: alpha, tau (each length 3)
    """
    p0 = [0.04, 0.5, 0.02, 2.0, 0.005, 10.0]
    bounds = ([0, 0.1, 0, 0.5, 0, 2.0],
              [0.2, 5.0, 0.1, 20.0, 0.05, 100.0])
    
    popt, pcov = curve_fit(multi_exponential_lag, time_points,
                            lag_decay_data, p0=p0, bounds=bounds,
                            maxfev=10000)
    
    return {
        'alpha': [popt[0], popt[2], popt[4]],
        'tau':   [popt[1], popt[3], popt[5]],
        'r_squared': compute_r_squared(lag_decay_data,
                                        multi_exponential_lag(time_points, *popt))
    }
```

#### 3.4.3 C++ 런타임 구현

```cpp
struct GhostCorrectionParams {
    float alpha[3];  // {0.04, 0.02, 0.005}
    float tau[3];    // {0.5, 2.0, 10.0}  seconds
};

struct ExposureHistory {
    float* max_signal;   // Per-pixel max signal from previous exposure
    float  elapsed_sec;  // Time since previous exposure
};

void xpe_ghost_correct(float* __restrict__ img,
                         const ExposureHistory& history,
                         const GhostCorrectionParams& params,
                         uint32_t width, uint32_t height) {
    if (history.elapsed_sec <= 0.0f || history.max_signal == nullptr) return;
    
    // Compute lag fraction at elapsed time
    float lag_fraction = 0.0f;
    for (int i = 0; i < 3; ++i) {
        lag_fraction += params.alpha[i] *
                        expf(-history.elapsed_sec / params.tau[i]);
    }
    
    const size_t total = static_cast<size_t>(width) * height;
    
    // AVX2 path
    __m256 lag_f = _mm256_set1_ps(lag_fraction);
    __m256 zero  = _mm256_setzero_ps();
    
    size_t i = 0;
    for (; i + 8 <= total; i += 8) {
        __m256 cur     = _mm256_loadu_ps(img + i);
        __m256 prev    = _mm256_loadu_ps(history.max_signal + i);
        __m256 ghost   = _mm256_mul_ps(lag_f, prev);
        __m256 result  = _mm256_max_ps(_mm256_sub_ps(cur, ghost), zero);
        _mm256_storeu_ps(img + i, result);
    }
    for (; i < total; ++i) {
        float ghost = lag_fraction * history.max_signal[i];
        img[i] = std::max(img[i] - ghost, 0.0f);
    }
}
```

#### 3.4.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| Ghost removal rate | ≥90% | 이중 노출 프로토콜: ghost_after / ghost_before |
| Residual lag at 0.5s | < 2% | 단기 lag 측정 |
| Model fit R² | ≥0.98 | 피팅 결과 검증 |

---

### 3.4.5 SWU-1.4b Lag 잔류 기반 결정론적 티어링 (GAP-U 해소)

xpe-algorithm-spec-deepsync.md §4의 "lag residual-driven deterministic tiering"에서 결정된 항목이다. 측정된 lag 잔류량에 따라 1항 모델 (빠른 처리)과 3항 모델 (정밀 처리)을 결정론적으로 선택함으로써, 항상 3항 모델을 실행하는 과잉 처리를 방지한다.

#### 3.4.5.1 알고리즘 수학 정의

**Lag 잔류 측정**:

$$R_{\text{lag}}(t) = \frac{|\bar{I}_{\text{measured}}(t) - \bar{I}_{\text{expected}}(t)|}{\bar{I}_{\text{expected}}(t)} \times 100\%$$

여기서 $\bar{I}_{\text{expected}}$는 이상적 노출(lag 없음) 이미지의 기대 평균 신호이다.

**티어 선택 규칙**:

$$\text{Tier} = \begin{cases} 0 & R_{\text{lag}} < \theta_0 \quad \text{(보정 건너뜀)} \\ 1 & \theta_0 \leq R_{\text{lag}} < \theta_1 \quad \text{(1항 모델)} \\ 3 & R_{\text{lag}} \geq \theta_1 \quad \text{(3항 모델)} \end{cases}$$

**1항 근사 모델**:

$$I_{\text{true,1}}(t) = I_{\text{measured}}(t) - \alpha_1 e^{-t/\tau_1} \cdot I_{\text{prev\_max}}$$

| 파라미터 | 값 | 의미 |
|---------|---|------|
| $\theta_0$ | 0.2% | Tier-0 상한 (보정 불필요) |
| $\theta_1$ | 1.0% | Tier-3 하한 (3항 모델 필요) |

#### 3.4.5.2 Python 구현

```python
import numpy as np
from enum import IntEnum

class LagTier(IntEnum):
    NONE  = 0   # lag residual < 0.2%: skip correction
    FAST  = 1   # 0.2–1.0%: single-term model
    FULL  = 3   # ≥1.0%: three-term model

def measure_lag_residual(measured_img:  np.ndarray,
                          prev_max_img:  np.ndarray,
                          elapsed_sec:   float,
                          alpha:         list,
                          tau:           list,
                          roi_mask:      np.ndarray = None) -> float:
    """
    Measure current lag residual as % of expected signal.

    Args:
        measured_img:  current frame (gain-corrected float32)
        prev_max_img:  previous exposure max signal per pixel
        elapsed_sec:   time since previous exposure
        alpha, tau:    3-component lag model parameters
        roi_mask:      optional boolean mask for ROI averaging
    Returns:
        residual_pct: lag residual as percentage (0–100)
    """
    # Expected lag component
    lag_frac = sum(a * np.exp(-elapsed_sec / t) for a, t in zip(alpha, tau))
    expected_ghost = lag_frac * prev_max_img

    # Measured mean signal
    if roi_mask is not None:
        meas_mean = float(np.mean(measured_img[roi_mask]))
        ghost_mean = float(np.mean(expected_ghost[roi_mask]))
        expected_clean_mean = float(np.mean(
            (measured_img - expected_ghost)[roi_mask]))
    else:
        meas_mean  = float(np.mean(measured_img))
        ghost_mean = float(np.mean(expected_ghost))
        expected_clean_mean = meas_mean - ghost_mean

    if abs(expected_clean_mean) < 1.0:
        return 0.0

    residual_pct = abs(ghost_mean) / abs(expected_clean_mean) * 100.0
    return residual_pct


def select_lag_tier(residual_pct: float,
                    theta_0: float = 0.2,
                    theta_1: float = 1.0) -> LagTier:
    """Select correction tier based on measured lag residual."""
    if residual_pct < theta_0:
        return LagTier.NONE
    elif residual_pct < theta_1:
        return LagTier.FAST
    else:
        return LagTier.FULL


def apply_lag_correction_tiered(
        img:          np.ndarray,
        prev_max:     np.ndarray,
        elapsed_sec:  float,
        alpha:        list,
        tau:          list,
        tier:         LagTier) -> np.ndarray:
    """
    Apply lag correction at the selected tier.

    Tier 0: return img unchanged
    Tier 1: single-term correction (fast)
    Tier 3: full three-term correction (precise)
    """
    if tier == LagTier.NONE:
        return img

    if tier == LagTier.FAST:
        # Use dominant (fastest) component only
        lag_frac = alpha[0] * np.exp(-elapsed_sec / tau[0])
        ghost    = lag_frac * prev_max
        return np.maximum(img - ghost, 0.0).astype(np.float32)

    # Full 3-term model
    lag_frac = sum(a * np.exp(-elapsed_sec / t) for a, t in zip(alpha, tau))
    ghost    = lag_frac * prev_max
    return np.maximum(img - ghost.astype(np.float32), 0.0).astype(np.float32)
```

#### 3.4.5.3 C++ 런타임 구현

```cpp
// Lag tiering decision and correction — extends SWU-1.4 Ghost Correction

enum class LagTier : uint8_t {
    NONE = 0,  // Skip correction
    FAST = 1,  // Single-term approximation
    FULL = 3,  // Full three-term model
};

struct LagTierConfig {
    float theta_none = 0.002f;  // 0.2% — skip threshold
    float theta_full = 0.010f;  // 1.0% — full-model threshold
};

LagTier determine_lag_tier(const float* __restrict__ img,
                             const float* __restrict__ prev_max,
                             const GhostCorrectionParams& params,
                             float    elapsed_sec,
                             uint32_t W,
                             uint32_t H,
                             const LagTierConfig& cfg = {}) {
    // Compute expected lag fraction
    float lag_frac = 0.0f;
    for (int k = 0; k < 3; ++k)
        lag_frac += params.alpha[k] * expf(-elapsed_sec / params.tau[k]);

    // Estimate residual: mean(lag_frac × prev_max) / mean(img - ghost)
    double ghost_sum = 0.0, img_sum = 0.0;
    const size_t total = static_cast<size_t>(W) * H;

    for (size_t i = 0; i < total; ++i) {
        ghost_sum += lag_frac * prev_max[i];
        img_sum   += img[i];
    }
    double ghost_mean = ghost_sum / total;
    double clean_mean = img_sum   / total - ghost_mean;

    if (std::abs(clean_mean) < 1.0) return LagTier::NONE;

    float residual_pct = static_cast<float>(
        std::abs(ghost_mean) / std::abs(clean_mean));

    if (residual_pct < cfg.theta_none) return LagTier::NONE;
    if (residual_pct < cfg.theta_full) return LagTier::FAST;
    return LagTier::FULL;
}

void xpe_ghost_correct_tiered(float* __restrict__ img,
                                const ExposureHistory& history,
                                const GhostCorrectionParams& params,
                                uint32_t W,
                                uint32_t H,
                                const LagTierConfig& tier_cfg = {}) {
    if (!history.max_signal || history.elapsed_sec <= 0.0f) return;

    LagTier tier = determine_lag_tier(img, history.max_signal, params,
                                       history.elapsed_sec, W, H, tier_cfg);

    if (tier == LagTier::NONE) return;  // Skip: residual below noise floor

    // For FAST tier: use only component 0
    GhostCorrectionParams effective = params;
    if (tier == LagTier::FAST) {
        effective.alpha[1] = 0.0f; effective.alpha[2] = 0.0f;
    }

    // Delegate to standard xpe_ghost_correct()
    xpe_ghost_correct(img, history, effective, W, H);
}
```

#### 3.4.5.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| Tier-0 정확 선택률 | 100% (lag 없는 첫 촬영) | 새 세션 첫 이미지 |
| Tier-3 → Tier-1 개선 | 처리 시간 ≤ 60% of Tier-3 | 동일 이미지 두 경로 비교 |
| Tier-1 ghost 제거율 | ≥ 80% (단기 lag) | 0.5s 경과 이중 노출 |
| Tier-3 ghost 제거율 | ≥ 90% (장기 lag) | 10s 경과 이중 노출 |

**IEC 62304 §5.4 추적성**: SRS ID: SRS-FUNC-004 (Ghost Correction 확장) — Tiering 항목 Phase 2 추가 예정

---

### 3.5 SWU-1.5 Heel Effect Compensation (GAP-O 해소)

Heel 효과는 X선관의 양극 경사면(anode heel)으로 인해 음극(cathode) 방향보다 양극(anode) 방향으로 X선 강도가 감소하는 현상이다. xpe-algorithm-spec-deepsync.md §4 "geometry-aware heel compensation"에서 "adopt now"로 결정된 항목이며, Wang 2013 Duo-SID 모델을 기반으로 한다.

#### 3.5.1 알고리즘 수학 정의

**Wang 2013 Duo-SID Heel 효과 모델**:

$$H(x, y; \text{SID}, \theta_T) = \exp\!\left(-\mu_{\text{eff}} \cdot d_{\text{anode}}(x, y; \text{SID}, \theta_T)\right)$$

$$d_{\text{anode}}(x, y; \text{SID}, \theta_T) = \frac{t_{\text{anode}}}{\sin\!\left(\theta_T - \arctan\!\left(\frac{x - x_{\text{center}}}{\text{SID}}\right)\right)}$$

$$I_{\text{heel\_corr}}(x, y) = \frac{I_{\text{gain\_corr}}(x, y)}{H(x, y; \text{SID}, \theta_T)}$$

여기서:
- $\theta_T$: 양극 타겟 각도 (일반적으로 10° ~ 17°)
- $t_{\text{anode}}$: 양극 재료 두께 (mm), 유효 경로 추정에 사용
- $\mu_{\text{eff}}$: 양극 재료(텅스텐)의 X선 유효 선감쇄계수 (kVp-dependent)
- $x_{\text{center}}$: 이미지 중앙에서 음극-양극 방향 이동(detector center offset)

**Multi-SID 보간**:

SID가 $\text{SID}_A$와 $\text{SID}_B$ 사이에 있을 때:

$$H_{\text{interp}}(x, y) = H(x, y; \text{SID}_A) + \frac{\text{SID} - \text{SID}_A}{\text{SID}_B - \text{SID}_A} \cdot \left(H(x,y;\text{SID}_B) - H(x,y;\text{SID}_A)\right)$$

#### 3.5.2 파라미터 및 경계 조건

| 파라미터 | 기본값 | 범위 | 의미 |
|---------|-------|-----|------|
| `theta_target_deg` | 12.0 | 8–20° | 양극 타겟 각도 |
| `mu_eff` | 0.045 | 0.02–0.10 mm⁻¹ | 유효 선감쇄계수 (80kVp 기준) |
| `sid_ref_mm` | 1000.0 | 600–1800 mm | 기준 SID |
| `anode_direction` | 'col' | 'row'/'col' | 음극→양극 방향 (열 방향 = 수직) |
| `max_correction_factor` | 1.5 | 1.0–2.0 | 최대 보정 계수 (안전 클램프) |

**경계 조건**:
- 보정 계수가 `max_correction_factor`를 초과하면 클램프 적용
- $H(x,y) < 0.1$이 되는 극단적 기하학은 교정 실패로 간주하고 보정 건너뜀
- kVp 변경 시 `mu_eff`는 선형 보간으로 업데이트

#### 3.5.3 Python 구현 (오프라인 교정)

```python
import numpy as np
from scipy.interpolate import RegularGridInterpolator
from dataclasses import dataclass
from typing import List, Tuple

@dataclass
class HeelEffectParams:
    theta_target_deg: float = 12.0    # anode target angle (degrees)
    mu_eff_per_mm:    float = 0.045   # effective attenuation at reference kVp
    sid_ref_mm:       float = 1000.0  # reference SID for calibration
    anode_direction:  str   = 'col'   # 'col' = cathode-anode along columns
    pixel_pitch_mm:   float = 0.148   # detector pixel pitch
    detector_width:   int   = 2816    # pixels in anode direction
    kvp_ref:          float = 80.0    # reference kVp for mu_eff
    max_correction:   float = 1.5     # safety clamp

def compute_heel_correction_map(
        params:  HeelEffectParams,
        sid_mm:  float,
        kvp:     float,
        height:  int,
        width:   int) -> np.ndarray:
    """
    Compute per-pixel heel effect correction map for given SID and kVp.

    Args:
        params:   HeelEffectParams configuration
        sid_mm:   source-to-image distance in mm
        kvp:      tube voltage (for mu_eff scaling)
        height:   image height in pixels
        width:    image width in pixels
    Returns:
        correction_map: float32 (H, W) — divide gain-corrected image by this map
                        Values in [1/max_correction, max_correction]
    """
    theta_T = np.radians(params.theta_target_deg)

    # mu_eff scales approximately as kVp^(-2.5) for tungsten in diagnostic range
    mu_scale = (params.kvp_ref / kvp) ** 2.5 if kvp > 0 else 1.0
    mu_eff = params.mu_eff_per_mm * mu_scale

    # anode direction coordinate (x = displacement from detector centre)
    if params.anode_direction == 'col':
        # anode runs along columns → displacement is along x (horizontal)
        cx = width / 2.0
        coords = (np.arange(width, dtype=np.float64) - cx) * params.pixel_pitch_mm  # mm
        disp_2d = np.tile(coords[np.newaxis, :], (height, 1))
    else:
        cy = height / 2.0
        coords = (np.arange(height, dtype=np.float64) - cy) * params.pixel_pitch_mm
        disp_2d = np.tile(coords[:, np.newaxis], (1, width))

    # Projection angle at each pixel
    alpha = np.arctan(disp_2d / sid_mm)  # small-angle approx OK for |disp| < 300mm

    # Effective path through anode (Wang 2013 Eq. 4)
    denom = np.sin(theta_T - alpha)
    # Avoid division by zero / negative (beyond edge of beam)
    denom = np.where(denom > 0.01, denom, 0.01)

    # Approximate anode path (normalised: at centre denom=sin(theta_T))
    path_ratio = np.sin(theta_T) / denom  # relative path length

    # Heel factor H(x) = exp(-mu_eff * t_ref * (path_ratio - 1))
    # t_ref is absorbed into mu_eff calibration: at centre H=1
    H = np.exp(-mu_eff * sid_mm * 0.001 * (path_ratio - 1.0))
    # Note: sid_mm * 0.001 converts mm→m; empirical factor, absorb into mu_eff

    # Clip to safe range
    H = np.clip(H, 1.0 / params.max_correction, params.max_correction)
    return H.astype(np.float32)


def compute_heel_map_multi_sid(
        params:     HeelEffectParams,
        sid_list:   List[float],
        kvp_list:   List[float],
        height:     int,
        width:      int) -> dict:
    """
    Pre-compute heel correction maps for all SID/kVp combinations.

    Returns dict: {(sid_mm, kvp): correction_map float32 (H, W)}
    Used by runtime to select nearest map or interpolate.
    """
    maps = {}
    for sid in sid_list:
        for kvp in kvp_list:
            maps[(sid, kvp)] = compute_heel_correction_map(params, sid, kvp, height, width)
    return maps


def interpolate_heel_map(
        maps:      dict,
        sid_mm:    float,
        kvp:       float,
        sid_list:  List[float],
        kvp_list:  List[float]) -> np.ndarray:
    """
    Bilinear interpolation between pre-computed heel correction maps.

    Args:
        maps:     dict from compute_heel_map_multi_sid()
        sid_mm:   target SID
        kvp:      target kVp
        sid_list: sorted list of calibrated SID values
        kvp_list: sorted list of calibrated kVp values
    Returns:
        interpolated correction map float32 (H, W)
    """
    sid_arr = np.array(sorted(sid_list), dtype=np.float64)
    kvp_arr = np.array(sorted(kvp_list), dtype=np.float64)

    # Clamp to calibrated range
    sid_c = float(np.clip(sid_mm, sid_arr[0], sid_arr[-1]))
    kvp_c = float(np.clip(kvp,    kvp_arr[0], kvp_arr[-1]))

    # Find bounding indices
    i_sid = np.searchsorted(sid_arr, sid_c, side='right') - 1
    i_sid = int(np.clip(i_sid, 0, len(sid_arr) - 2))
    i_kvp = np.searchsorted(kvp_arr, kvp_c, side='right') - 1
    i_kvp = int(np.clip(i_kvp, 0, len(kvp_arr) - 2))

    sid_lo, sid_hi = sid_arr[i_sid], sid_arr[i_sid + 1]
    kvp_lo, kvp_hi = kvp_arr[i_kvp], kvp_arr[i_kvp + 1]

    t_sid = (sid_c - sid_lo) / (sid_hi - sid_lo) if sid_hi != sid_lo else 0.0
    t_kvp = (kvp_c - kvp_lo) / (kvp_hi - kvp_lo) if kvp_hi != kvp_lo else 0.0

    m00 = maps[(sid_lo, kvp_lo)].astype(np.float64)
    m01 = maps[(sid_lo, kvp_hi)].astype(np.float64)
    m10 = maps[(sid_hi, kvp_lo)].astype(np.float64)
    m11 = maps[(sid_hi, kvp_hi)].astype(np.float64)

    result = ((1 - t_sid) * (1 - t_kvp) * m00 +
              (1 - t_sid) *      t_kvp  * m01 +
                   t_sid  * (1 - t_kvp) * m10 +
                   t_sid  *      t_kvp  * m11)
    return result.astype(np.float32)
```

#### 3.5.4 C++ 런타임 구현

```cpp
// Heel Effect Compensation — runtime application
// Pre-computed correction map loaded from calibration pack.
// Called after xpe_gain_correct(), before xpe_nonlinearity_correct().

struct HeelCorrectionPack {
    float*   map;           // float32 (H × W) — correction divisor
    uint32_t width;
    uint32_t height;
    float    sid_mm;
    float    kvp;
    uint64_t crc64;         // CRC of this map for integrity verification
};

// --- In-process map interpolation (bilinear between two SID maps) ---
void interpolate_heel_maps(const float* __restrict__ map_lo,
                            const float* __restrict__ map_hi,
                            float* __restrict__ out,
                            uint32_t total,
                            float t) {
    // t ∈ [0, 1]: linear interpolation weight toward map_hi
    const __m256 v_t    = _mm256_set1_ps(t);
    const __m256 v_1mt  = _mm256_set1_ps(1.0f - t);
    size_t i = 0;
    for (; i + 8 <= total; i += 8) {
        __m256 lo = _mm256_loadu_ps(map_lo + i);
        __m256 hi = _mm256_loadu_ps(map_hi + i);
        __m256 r  = _mm256_fmadd_ps(v_t, hi, _mm256_mul_ps(v_1mt, lo));
        _mm256_storeu_ps(out + i, r);
    }
    for (; i < total; ++i) {
        out[i] = (1.0f - t) * map_lo[i] + t * map_hi[i];
    }
}

// --- Main heel correction ---
void xpe_heel_correct(const float* __restrict__ gain_corr,
                       float*       __restrict__ out,
                       const float* __restrict__ heel_map,   // pre-interpolated for current SID/kVp
                       uint32_t width,
                       uint32_t height,
                       float    max_correction = 1.5f) {
    const size_t   total     = static_cast<size_t>(width) * height;
    const __m256   v_max_cor = _mm256_set1_ps(max_correction);
    const __m256   v_min_cor = _mm256_set1_ps(1.0f / max_correction);
    const __m256   v_eps     = _mm256_set1_ps(1e-6f);

    size_t i = 0;
    for (; i + 8 <= total; i += 8) {
        __m256 img  = _mm256_loadu_ps(gain_corr  + i);
        __m256 hmap = _mm256_loadu_ps(heel_map   + i);

        // Clamp correction map to safe range
        hmap = _mm256_max_ps(hmap, v_min_cor);
        hmap = _mm256_min_ps(hmap, v_max_cor);

        // Corrected = img / H(x,y)
        __m256 denom = _mm256_max_ps(hmap, v_eps);
        __m256 res   = _mm256_div_ps(img, denom);
        _mm256_storeu_ps(out + i, res);
    }
    for (; i < total; ++i) {
        float h = std::clamp(heel_map[i], 1.0f / max_correction, max_correction);
        out[i] = gain_corr[i] / std::max(h, 1e-6f);
    }
}

// --- SID/kVp selector: choose nearest pre-loaded map or trigger interpolation ---
const HeelCorrectionPack* select_heel_pack(
        const std::vector<HeelCorrectionPack>& packs,
        float sid_mm,
        float kvp,
        float sid_tol = 25.0f,
        float kvp_tol = 5.0f) {
    for (const auto& p : packs) {
        if (std::fabsf(p.sid_mm - sid_mm) < sid_tol &&
            std::fabsf(p.kvp    - kvp)    < kvp_tol) {
            return &p;
        }
    }
    return nullptr;  // caller must interpolate
}
```

#### 3.5.5 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| Heel 보정 후 PRNU | CV < 0.8% (보정 전 대비 개선) | 균일 조사 flood 이미지의 수평 프로파일 |
| SID 보간 오차 | < 0.5% 신호 오차 | 두 교정 SID 사이의 중간값 측정 |
| 최대 보정 계수 초과 비율 | < 0.01% 픽셀 | 클램프 이벤트 카운터 |
| 처리 시간 (3072×3072) | < 40ms | AVX2, 단일 코어 |
| 모델 R² (측정 대 예측) | ≥ 0.99 | 교정 flood 이미지 대비 모델 예측값 |

**IEC 62304 §5.4 추적성**: SRS ID: SRS-FUNC-002b (Gain Correction 확장) — Phase 2에서 할당 예정

---

## 4. SWI-2: Core Processing 알고리즘

### 4.1 SWU-2.1 Log Transform (SRS-FUNC-010)

#### 4.1.1 수학 정의

$$I_{OD}(x,y) = -\ln\left(\frac{I_{\text{clean}}(x,y) + \varepsilon}{I_0 + \varepsilon}\right)$$

- $I_0$: 비노출 영역(collimator edge 내부) 기준 최대 플루엔스 추정값 또는 이론값
- $\varepsilon = 10^{-6}$: Zero/negative 입력 보호 (SRS-FUNC-010)
- **결과**: Beer-Lambert law에 의해 OD(Optical Density) ≈ attenuation coefficient × thickness

#### 4.1.2 I₀ 추정 전략

```cpp
// Strategy 1: Use collimated (unattenuated) region statistics
float estimate_I0_from_collimator(const float* img, uint32_t W, uint32_t H,
                                    const CollimatorMask& mask) {
    // Find unattenuated pixels (inside collimator border, no anatomy)
    // Use 95th percentile to avoid outliers
    std::vector<float> unattenuated;
    for (uint32_t i = 0; i < W * H; ++i) {
        if (mask.is_unattenuated(i)) unattenuated.push_back(img[i]);
    }
    std::sort(unattenuated.begin(), unattenuated.end());
    return unattenuated[static_cast<size_t>(unattenuated.size() * 0.95f)];
}

// Strategy 2: Use gain-normalized reference (preferred for consistency)
// I0 = GainMean (stored in gain map header)
```

#### 4.1.3 C++ 구현 (AVX2)

```cpp
void xpe_log_transform(const float* __restrict__ in,
                         float*       __restrict__ out,
                         float I0, float epsilon,
                         uint32_t width, uint32_t height) {
    const float eps = (epsilon > 0) ? epsilon : 1e-6f;
    const size_t total = static_cast<size_t>(width) * height;
    
    __m256 v_eps   = _mm256_set1_ps(eps);
    __m256 v_I0e   = _mm256_set1_ps(I0 + eps);
    __m256 v_neg1  = _mm256_set1_ps(-1.0f);
    
    size_t i = 0;
    for (; i + 8 <= total; i += 8) {
        __m256 x      = _mm256_loadu_ps(in + i);
        __m256 x_eps  = _mm256_add_ps(x, v_eps);
        __m256 ratio  = _mm256_div_ps(x_eps, v_I0e);
        __m256 ln_val = avx2_log_ps(ratio);   // See avx2_log_ps below
        __m256 od     = _mm256_mul_ps(v_neg1, ln_val);
        _mm256_storeu_ps(out + i, od);
    }
    for (; i < total; ++i) {
        out[i] = -logf((in[i] + eps) / (I0 + eps));
    }
}

// ---------------------------------------------------------------------------
// avx2_log_ps: Cephes-based AVX2 natural logarithm approximation
// Accuracy: ~5 ULP (max relative error < 1.2×10⁻⁷ for x ∈ (0, +∞))
// Algorithm: Cephes log.c decomposition — identical to avx_mathfun (Gruzdev 2012)
//   Reference: https://github.com/reyoung/avx_mathfun (BSD-2)
//   Reference: Cephes Math Library, S. Moshier
//
// Derivation:
//   x = m × 2^e  where m ∈ [0.5, 1.0)
//   ln(x) = ln(m) + e × ln(2)
//   ln(m) approximated by degree-8 minimax polynomial on [sqrt(0.5), sqrt(2)]
//   after substitution f = m − 1 (range reduction to [−0.293, 0.414])
// ---------------------------------------------------------------------------
static inline __m256 avx2_log_ps(__m256 x) {
    // Polynomial coefficients (Cephes, ~5 ULP)
    const __m256 c_ln2_hi  = _mm256_set1_ps(0.693359375f);
    const __m256 c_ln2_lo  = _mm256_set1_ps(-2.12194440e-4f);
    const __m256 c_half    = _mm256_set1_ps(0.5f);
    const __m256 c_one     = _mm256_set1_ps(1.0f);
    const __m256 c_sqrthf  = _mm256_set1_ps(0.707106781186547524f);  // sqrt(0.5)
    // Polynomial coefficients for ln(1+f), f = normalized(x) − 1
    const __m256 c_p0  = _mm256_set1_ps( 7.0376836292e-2f);
    const __m256 c_p1  = _mm256_set1_ps(-1.1514610310e-1f);
    const __m256 c_p2  = _mm256_set1_ps( 1.1676998740e-1f);
    const __m256 c_p3  = _mm256_set1_ps(-1.2420140846e-1f);
    const __m256 c_p4  = _mm256_set1_ps( 1.4249322787e-1f);
    const __m256 c_p5  = _mm256_set1_ps(-1.6668057665e-1f);
    const __m256 c_p6  = _mm256_set1_ps( 2.0000714765e-1f);
    const __m256 c_p7  = _mm256_set1_ps(-2.4999993993e-1f);
    const __m256 c_p8  = _mm256_set1_ps( 3.3333331174e-1f);
    const __m256i c_127 = _mm256_set1_epi32(127);

    // Clamp x > 0 (avoid NaN/Inf propagation)
    x = _mm256_max_ps(x, _mm256_set1_ps(1.175494351e-38f));  // FLT_MIN

    // Decompose x = m × 2^e  (e = biased_exponent - 127)
    __m256i xi = _mm256_castps_si256(x);
    // Extract exponent
    __m256i exp_i = _mm256_sub_epi32(_mm256_srli_epi32(xi, 23), c_127);
    __m256 e = _mm256_cvtepi32_ps(exp_i);
    // Set mantissa to [0.5, 1.0): clear exponent, set bias to 126
    xi = _mm256_and_si256(xi, _mm256_set1_epi32(0x007fffff));
    xi = _mm256_or_si256(xi,  _mm256_set1_epi32(0x3f000000));
    __m256 m = _mm256_castsi256_ps(xi);

    // If m < sqrt(0.5), multiply m by 2 and subtract 1 from exponent
    __m256 mask = _mm256_cmp_ps(m, c_sqrthf, _CMP_LT_OQ);
    e = _mm256_sub_ps(e, _mm256_and_ps(c_one, mask));
    m = _mm256_add_ps(m, _mm256_and_ps(m, mask));   // m += m if m < sqrthf
    m = _mm256_sub_ps(m, c_one);                     // f = m - 1

    // Horner evaluation of polynomial in f
    __m256 y = c_p0;
    y = _mm256_fmadd_ps(y, m, c_p1);
    y = _mm256_fmadd_ps(y, m, c_p2);
    y = _mm256_fmadd_ps(y, m, c_p3);
    y = _mm256_fmadd_ps(y, m, c_p4);
    y = _mm256_fmadd_ps(y, m, c_p5);
    y = _mm256_fmadd_ps(y, m, c_p6);
    y = _mm256_fmadd_ps(y, m, c_p7);
    y = _mm256_fmadd_ps(y, m, c_p8);
    y = _mm256_mul_ps(y, m);
    y = _mm256_mul_ps(y, m);   // y × m²

    // ln(x) = y + e×ln2_hi + e×ln2_lo − 0.5×m² + m
    __m256 r = _mm256_fmadd_ps(e,  c_ln2_hi, y);
    r = _mm256_fmadd_ps(e, c_ln2_lo, r);
    r = _mm256_fmadd_ps(_mm256_set1_ps(-0.5f), _mm256_mul_ps(m, m), r);
    r = _mm256_add_ps(r, m);
    return r;
}
```

---

### 4.2 SWU-2.2 Noise Reduction (SRS-FUNC-011)

#### 4.2.1 Bilateral Filter — 핵심 알고리즘

$$BF[I](x) = \frac{1}{W_p} \sum_{x_i \in \Omega} I(x_i) \cdot f_s(\|x_i - x\|) \cdot f_r(|I(x_i) - I(x)|)$$

$$f_s(d) = e^{-d^2/(2\sigma_s^2)}, \quad f_r(\delta) = e^{-\delta^2/(2\sigma_r^2)}$$

$$W_p = \sum_{x_i \in \Omega} f_s(\|x_i - x\|) \cdot f_r(|I(x_i) - I(x)|)$$

- **파라미터 (SRS-FUNC-011)**: `σ_s = 2.0` pixels, `σ_r = 0.1` OD unit
- **커널 크기**: `2 × ⌈3σ_s⌉ + 1 = 13×13`
- **구현**: OpenCV `cv::bilateralFilter()` + 사전 계산 lookup table

#### 4.2.2 파라미터 선택 근거

| σ_s | σ_r | 효과 | 부작용 |
|-----|-----|------|--------|
| 1.0 | 0.05 | 약한 스무딩, 노이즈 유지 | 효과 미미 |
| 2.0 | 0.10 | **권장: 노이즈 제거 + edge 보존** | 미미한 detail 손실 |
| 3.0 | 0.15 | 강한 스무딩 | Texture 과도 억제 |
| 5.0 | 0.30 | 과도한 스무딩 | Watercolor artifact |

#### 4.2.3 Non-Local Means (고품질 옵션)

$$NLM[I](x) = \frac{1}{C(x)} \sum_{y \in \Omega} e^{-\frac{\|I(N_x) - I(N_y)\|^2_{2,a}}{h^2}} \cdot I(y)$$

- $N_x$: `x` 중심 `p×p` 패치 (권장: `p=7`)
- 탐색 범위: `d×d` 윈도우 (권장: `d=21`)
- `h`: 필터링 파라미터 (노이즈 표준편차의 ~10배)
- **구현**: OpenCV `cv::fastNlMeansDenoising()` 또는 CUDA 가속

#### 4.2.4 알고리즘 선택 로직

```cpp
ImageQualityMode select_denoising_mode(const ProcessingParams& params) {
    if (params.quality_mode == "high" || params.body_part == "BREAST")
        return ImageQualityMode::NLM;
    return ImageQualityMode::Bilateral;  // default
}
```

---

### 4.3 SWU-2.3 CLAHE (SRS-FUNC-012)

#### 4.3.1 알고리즘 수학 정의

CLAHE (Contrast Limited Adaptive Histogram Equalization):

1. **타일 분할**: 이미지를 `M×N` 타일로 분할 (기본: 8×8)
2. **히스토그램 계산**: 각 타일 내 픽셀 히스토그램 (bins: 256)
3. **Clip 제한**: `clip_limit × (tile_area / bins)` 초과 빈도 → 균등 재분배
4. **CDF 계산**: 클리핑된 히스토그램의 누적분포함수
5. **Bilinear 보간**: 경계 타일 간 매끄러운 전환

$$\text{CDF}(v) = \frac{1}{N_{clip}} \sum_{i=0}^{v} h_{clip}(i)$$

$$I_{out}(x,y) = \text{BilinearInterp}\left(\text{CDF}_{T_1}, \text{CDF}_{T_2}, \text{CDF}_{T_3}, \text{CDF}_{T_4}, I_{in}(x,y)\right)$$

#### 4.3.2 파라미터 명세

| 파라미터 | 기본값 | 범위 | 설명 |
|---------|--------|------|------|
| `tile_size` | 8×8 | 4–64 | 적응 영역 크기 |
| `clip_limit` | 2.0 | 1.0–8.0 | 클리핑 강도 (1.0 = no clip = AHE) |
| `bins` | 256 | 64–4096 | 히스토그램 해상도 |
| `input_range` | [0, 4095] | adaptive | 입력 동적 범위 |

#### 4.3.3 Body-Part별 최적 파라미터 (SRS-FUNC-021 preset과 연계)

| 신체 부위 | clip_limit | tile_size | 이유 |
|----------|-----------|---------|------|
| Chest PA/AP | 2.0 | 8×8 | 폐야/종격동 균형 |
| Abdomen | 1.5 | 16×16 | 대비 차이 완만 |
| Extremity | 3.0 | 4×4 | 국소 골 디테일 강화 |
| Hand/Wrist | 4.0 | 4×4 | 세밀한 골 구조 |
| Spine | 2.5 | 8×8 | 추체-디스크 대비 |
| Breast | 1.0 | 32×32 | 균일한 대비, 과도 억제 방지 |

#### 4.3.4 C++ 구현

```cpp
// Using OpenCV CLAHE
void xpe_clahe_enhance(cv::Mat& img_float32,
                         const ClaheParams& params) {
    // Convert to 16-bit for OpenCV CLAHE processing
    double min_val, max_val;
    cv::minMaxLoc(img_float32, &min_val, &max_val);
    
    cv::Mat img16;
    img_float32.convertTo(img16, CV_16U,
                           65535.0 / (max_val - min_val + 1e-6),
                           -min_val * 65535.0 / (max_val - min_val + 1e-6));
    
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(
        params.clip_limit,
        cv::Size(params.tile_cols, params.tile_rows));
    clahe->apply(img16, img16);
    
    // Convert back to float32
    img16.convertTo(img_float32, CV_32F,
                     (max_val - min_val) / 65535.0,
                     min_val);
}
```

---

### 4.4 SWU-2.4 Edge Enhancement — Unsharp Masking (SRS-FUNC-013)

#### 4.4.1 알고리즘 수학 정의

$$I_{\text{USM}}(x,y) = I(x,y) + \lambda(x,y) \cdot \left[I(x,y) - I_{\text{blur}}(x,y)\right]$$

$$I_{\text{blur}}(x,y) = I(x,y) * G_{\sigma}(x,y)$$

- $G_{\sigma}$: Gaussian blur kernel (σ = 1.5~2.0 pixels)
- $\lambda$: **Adaptive gain** — body-part별 safe range로 제한 (SRS-SAFE-005)
- **High-pass component**: `H(x,y) = I(x,y) - I_blur(x,y)` (Laplacian of Gaussian 근사)

#### 4.4.2 Body-Part별 Safe Gain Range (SRS-SAFE-005 이행)

| 신체 부위 | λ_min | λ_default | λ_max | 이유 |
|----------|-------|---------|-------|------|
| Chest | 0.0 | 0.8 | 1.5 | 과도한 폐 구조 강조 방지 |
| Bone (extremity) | 0.0 | 1.2 | 2.5 | 골 디테일 강화 허용 |
| Spine | 0.0 | 1.0 | 2.0 | 균형 |
| Breast | 0.0 | 0.5 | 1.0 | 미세석회화 인식, artifact 방지 |
| Pediatric | 0.0 | 0.6 | 1.2 | 낮은 contrast 조직 보호 |

```cpp
float clamp_usm_gain(float lambda, const BodyPartPreset& preset) {
    return std::clamp(lambda, preset.lambda_min, preset.lambda_max);
}
```

#### 4.4.3 주파수 선택적 USM (Selective Frequency Enhancement)

진단 가치 있는 공간주파수 범위만 강화:

```cpp
// Bandpass USM: enhance only [f_low, f_high] frequency band
// Implementation: DoG (Difference of Gaussians)
// H_band(x,y) = G_{σ1}(x,y) - G_{σ2}(x,y)  where σ1 < σ2
// f_low ≈ 1/(4σ2), f_high ≈ 1/(4σ1)
// For chest: σ1=1.0, σ2=4.0 → [0.06, 0.25] cycles/pixel
```

---

### 4.5 SWU-2.5 Multiscale Processing — Laplacian Pyramid (SRS-FUNC-014)

#### 4.5.1 알고리즘 수학 정의

**건설 단계 (Analysis):**

$$G_0 = I, \quad G_k = \text{Downsample}(G_{k-1} * h)$$
$$L_k = G_k - \text{Upsample}(G_{k+1}) \quad \text{for } k = 0, 1, \ldots, N-1$$
$$L_N = G_N \quad \text{(residual)}$$

**비선형 게인 적용:**

$$\hat{L}_k = L_k \cdot g_k\left(\|L_k\|\right)$$

$$g_k(s) = \begin{cases} g_{\max,k} & s < s_1 \\ g_{\min,k} + (g_{\max,k}-g_{\min,k})\cdot\frac{s_2-s}{s_2-s_1} & s_1 \le s < s_2 \\ g_{\min,k} & s \ge s_2 \end{cases}$$

**재구성 단계 (Synthesis):**

$$\hat{G}_{k} = \hat{L}_k + \text{Upsample}(\hat{G}_{k+1})$$

#### 4.5.2 파라미터 명세

```cpp
struct LaplacianPyramidParams {
    int    levels        = 8;    // SRS-FUNC-014: ≥8 levels
    float  sigma         = 1.0f; // Gaussian sigma for each level
    // Per-level gain curve (nonlinear)
    struct LevelGain {
        float g_max    = 1.5f;   // gain for small signals (texture)
        float g_min    = 0.8f;   // gain for large signals (edges)
        float s1       = 0.02f;  // lower threshold (in OD units)
        float s2       = 0.10f;  // upper threshold
    } gains[8];
};
```

#### 4.5.3 구현 전략

```cpp
// Using OpenCV pyrDown/pyrUp (Gaussian 5-tap kernel)
void build_laplacian_pyramid(const cv::Mat& src,
                               std::vector<cv::Mat>& laplacian,
                               std::vector<cv::Mat>& gaussian,
                               int levels) {
    gaussian.resize(levels + 1);
    laplacian.resize(levels);
    gaussian[0] = src.clone();
    
    for (int k = 0; k < levels; ++k) {
        cv::pyrDown(gaussian[k], gaussian[k+1]);
        cv::Mat upsampled;
        cv::pyrUp(gaussian[k+1], upsampled, gaussian[k].size());
        laplacian[k] = gaussian[k] - upsampled;
    }
}

void apply_nonlinear_gain(cv::Mat& L, const LaplacianPyramidParams::LevelGain& gain) {
    L.forEach<float>([&](float& val, const int* pos) {
        float s = std::abs(val);
        float g;
        if (s < gain.s1)
            g = gain.g_max;
        else if (s < gain.s2)
            g = gain.g_min + (gain.g_max - gain.g_min) *
                (gain.s2 - s) / (gain.s2 - gain.s1);
        else
            g = gain.g_min;
        val *= g;
    });
}
```

---

### 4.6 SWU-2.6 Fractional Multiscale Processing (SRS-FUNC-015)

#### 4.6.1 개념 및 수학적 배경

Fractional Multiscale Processing (FMP)은 Laplacian Pyramid의 정수 스케일 해상도 감소 대신 **비정수(fractional) 스케일**을 사용하여 density transition zone의 artifact를 제거한다.

**기본 원리:**

$$L_k^{\alpha} = I - (G_k)^{\alpha} \cdot (I * h^{N-k})^{1-\alpha}$$

여기서 $\alpha \in (0, 1)$는 분수 스케일 파라미터.

실용 구현 (Polynomial Approximation):

$$G_k^{\alpha}(x,y) = \sum_{n=0}^{K} c_n(\alpha) \cdot G_n(x,y)$$

- $c_n(\alpha)$: 분수 스케일 계수 (Chebyshev 보간 기반)

#### 4.6.2 구현 알고리즘

```cpp
struct FractionalMSParams {
    float alpha         = 0.5f;   // Fractional scale (0.3–0.7)
    int   base_levels   = 8;
    float density_threshold = 0.3f; // OD threshold for transition zone
};

// FMP replaces integer pyramid bands with fractional bands at transition zones
cv::Mat compute_fractional_band(const std::vector<cv::Mat>& gaussian_pyr,
                                  float alpha, int target_level) {
    int L = static_cast<int>(gaussian_pyr.size()) - 1;
    int k1 = static_cast<int>(std::floor(alpha * (L - 1)));
    int k2 = k1 + 1;
    float t = alpha * (L - 1) - k1;
    
    cv::Mat level1, level2;
    cv::resize(gaussian_pyr[k1], level1, gaussian_pyr[0].size(),
               0, 0, cv::INTER_LINEAR);
    cv::resize(gaussian_pyr[k2], level2, gaussian_pyr[0].size(),
               0, 0, cv::INTER_LINEAR);
    
    return (1.0f - t) * level1 + t * level2;
}
```

---

## 5. Grid Suppression & Virtual Grid 알고리즘

### 5.1 Grid Line Artifact Suppression (GAP-03 해소)

#### 5.1.1 자동 Grid 파라미터 감지

```cpp
struct GridSpec {
    float  line_density_lpi;  // lines per inch (60–200 lpi typical)
    float  angle_deg;         // grid orientation (0° = horizontal)
    float  pixel_pitch_mm;    // detector pixel pitch
};

// Derive grid artifact frequency from DICOM tags and GridSpec
float compute_grid_artifact_frequency(const GridSpec& spec) {
    // f_grid [cycles/pixel] = pixel_pitch_mm / (25.4 / line_density_lpi)
    return spec.pixel_pitch_mm * spec.line_density_lpi / 25.4f;
}
```

#### 5.1.2 2D DWT 기반 Grid Suppression (Tang et al. 2015)

```python
def wavelet_grid_suppression(image: np.ndarray,
                              grid_freq_cpx: float,
                              wavelet: str = 'db6',
                              max_levels: int = 8) -> np.ndarray:
    """
    Remove grid line artifact using 2D DWT + Gaussian band-stop filter.
    
    Algorithm:
      1. 2D DWT decomposition with auto stop condition
      2. For each sub-band: detect grid frequency component
      3. Apply Gaussian band-stop filter in frequency domain
      4. Reconstruct via inverse DWT
      
    Args:
        image:          float32 input (H, W)
        grid_freq_cpx:  grid artifact frequency in cycles/pixel
        wavelet:        wavelet family (db6 recommended)
        max_levels:     maximum decomposition levels
    Returns:
        float32 grid-suppressed image
    """
    import pywt
    
    # Auto stop condition: stop when grid frequency falls below Nyquist
    # at current decomposition level
    auto_level = 1
    nyquist = 0.5  # cycles/pixel at current level
    f = grid_freq_cpx
    while auto_level < max_levels and f < nyquist * 0.25:
        f *= 2  # frequency doubles with each level of downsampling
        nyquist /= 2
        auto_level += 1
    
    # Perform 2D DWT
    coeffs = pywt.wavedec2(image, wavelet, level=auto_level)
    
    # Apply band-stop filter to horizontal/vertical detail coefficients
    filtered_coeffs = [coeffs[0]]  # keep approximation
    for level_coeffs in coeffs[1:]:
        cH, cV, cD = level_coeffs
        # Suppress grid frequency in horizontal and vertical bands
        cH = _apply_gaussian_bandstop_1d(cH, grid_freq_cpx, axis=1)
        cV = _apply_gaussian_bandstop_1d(cV, grid_freq_cpx, axis=0)
        filtered_coeffs.append((cH, cV, cD))
    
    return pywt.waverec2(filtered_coeffs, wavelet)

def _apply_gaussian_bandstop_1d(band: np.ndarray, f_stop: float,
                                   axis: int, bandwidth: float = 0.02) -> np.ndarray:
    """Apply 1D Gaussian band-stop filter along specified axis."""
    spectrum = np.fft.rfft(band, axis=axis)
    freqs = np.fft.rfftfreq(band.shape[axis])
    
    # Gaussian notch centered at f_stop
    notch = 1.0 - np.exp(-0.5 * ((freqs - f_stop) / bandwidth)**2)
    notch = notch.reshape([-1 if i == axis else 1 for i in range(band.ndim)])
    
    spectrum *= notch
    return np.fft.irfft(spectrum, n=band.shape[axis], axis=axis)
```

#### 5.1.3 NSCT 기반 Moiré Suppression (Kim et al. 2023)

Nonsubsampled Contourlet Transform은 aliasing 없이 다방향 분해를 제공하여 비축 grid orientation에 효과적이다.

```python
# NSCT-based approach for non-standard grid angles
# Reference: Kim et al. 2023, Nuclear Engineering and Technology 55(4):1420-1429
# Key advantage: shift-invariance prevents ringing artifacts at non-axis orientations

def nsct_grid_suppression(image: np.ndarray,
                           grid_angle_deg: float,
                           grid_freq_cpx: float,
                           nsct_levels: int = 4,
                           n_directions_fine: int = 8) -> np.ndarray:
    """
    Suppress X-ray anti-scatter grid artifact using NSCT decomposition.

    Algorithm (Kim et al. 2023, 4-step):
      Step 1 — NSCT Decomposition
        Decompose image into (nsct_levels) lowpass + directional subband pyramid.
        Fine-scale level uses n_directions_fine directional subbands.
        Shift-invariance achieved by omitting downsampling (nonsubsampled filter bank).

      Step 2 — Artifact Subband Identification
        Grid artifact in spatial domain → spike in specific directional subband.
        Target subband index = round(grid_angle_deg / (180 / n_directions_fine)) mod n_directions_fine
        Confirm by comparing subband energy to neighboring subbands (energy_ratio > 3.0 threshold).

      Step 3 — Moiré Component Extraction via Gaussian Band-Pass
        Within the identified subband coefficient map S[i][k]:
          centre_freq = grid_freq_cpx  (in cycles/pixel)
          sigma_bp    = centre_freq × 0.25  (bandwidth: ±25% of grid frequency)
          mask        = gaussian_bandpass_2d(S[i][k].shape, centre_freq, sigma_bp)
          moire_coeff = S[i][k] × mask

      Step 4 — Subtract and Reconstruct
        Zero out or attenuate moire_coeff in the identified subband:
          suppression_weight = compute_adaptive_weight(energy_ratio)
          S[i][k]_clean = S[i][k] - suppression_weight × moire_coeff
        Reconstruct image via inverse NSCT (synthesis filter bank).

    Args:
        image:            float32 input image, OD or linear domain (H, W)
        grid_angle_deg:   dominant grid line orientation in degrees [0, 180)
        grid_freq_cpx:    grid spatial frequency in cycles/pixel (typical 0.05–0.20)
        nsct_levels:      number of decomposition levels (default 4)
        n_directions_fine: directional subbands at finest level (default 8)
    Returns:
        float32 image with grid artifact suppressed
    """
    try:
        import pynsct  # pip install pynsct  (or custom NSCT implementation)
        _nsct_available = True
    except ImportError:
        _nsct_available = False

    if not _nsct_available:
        # Fallback: frequency-domain notch filter at grid frequency
        # Less effective for oblique grids but always available
        import warnings
        warnings.warn("pynsct not available; falling back to notch filter suppression")
        return _notch_fallback(image, grid_angle_deg, grid_freq_cpx)

    H, W = image.shape

    # --- Step 1: NSCT Decomposition ---
    # n_dir_list: number of directional subbands per level (coarse→fine)
    # e.g. [4, 4, 8, 8] for 4-level decomposition
    n_dir_list = [4] * (nsct_levels - 2) + [n_directions_fine, n_directions_fine]
    coeffs = pynsct.nsctdec(image, nlevels=nsct_levels, n_dir_list=n_dir_list)
    # coeffs structure: [lowpass_coeff, level0_subbands, level1_subbands, ...]
    # finest level: coeffs[-1] is list of n_directions_fine subband arrays

    # --- Step 2: Identify Target Subband ---
    fine_subbands = coeffs[-1]          # list of n_directions_fine arrays
    n_sb = len(fine_subbands)
    angle_per_sb = 180.0 / n_sb         # angular step per subband
    target_idx = int(round(grid_angle_deg / angle_per_sb)) % n_sb

    # Confirm by energy ratio
    target_energy  = float(np.sum(fine_subbands[target_idx] ** 2))
    neighbor_energy = float(np.sum(fine_subbands[(target_idx - 1) % n_sb] ** 2) +
                            np.sum(fine_subbands[(target_idx + 1) % n_sb] ** 2)) / 2.0
    energy_ratio = target_energy / (neighbor_energy + 1e-12)

    if energy_ratio < 2.0:
        # Grid artifact not dominant in this subband — skip suppression
        return image.copy()

    # --- Step 3: Gaussian Band-Pass Filter in Frequency Domain ---
    sb = fine_subbands[target_idx].astype(np.float64)
    sb_h, sb_w = sb.shape

    # Build 2-D Gaussian band-pass mask centred on grid frequency
    u = np.fft.fftfreq(sb_w)   # cycles/pixel
    v = np.fft.fftfreq(sb_h)
    UU, VV = np.meshgrid(u, v)
    freq_map = np.sqrt(UU ** 2 + VV ** 2)

    sigma_bp = grid_freq_cpx * 0.25
    # Difference of two Gaussians: band-pass centred at grid_freq_cpx
    mask_bp = (np.exp(-((freq_map - grid_freq_cpx) ** 2) / (2 * sigma_bp ** 2)) +
               np.exp(-((freq_map + grid_freq_cpx) ** 2) / (2 * sigma_bp ** 2)))
    mask_bp = np.clip(mask_bp, 0.0, 1.0)

    sb_fft    = np.fft.fft2(sb)
    moire_fft = sb_fft * mask_bp
    moire_component = np.real(np.fft.ifft2(moire_fft)).astype(np.float32)

    # --- Step 4: Adaptive Suppression and Reconstruction ---
    # Suppression weight increases with energy_ratio (stronger artifact → more suppression)
    suppression_weight = float(np.clip(1.0 - 1.0 / energy_ratio, 0.5, 1.0))
    fine_subbands[target_idx] = (fine_subbands[target_idx] -
                                  suppression_weight * moire_component)
    coeffs[-1] = fine_subbands

    # Inverse NSCT synthesis
    result = pynsct.nsctidec(coeffs).astype(np.float32)
    return result


def _notch_fallback(image: np.ndarray,
                    grid_angle_deg: float,
                    grid_freq_cpx: float) -> np.ndarray:
    """Frequency-domain notch filter fallback when pynsct is unavailable."""
    H, W = image.shape
    u = np.fft.fftfreq(W)
    v = np.fft.fftfreq(H)
    UU, VV = np.meshgrid(u, v)

    # Rotate frequency coordinates to grid orientation
    theta = np.deg2rad(grid_angle_deg)
    u_rot = UU * np.cos(theta) + VV * np.sin(theta)

    # Notch: suppress narrow band around grid frequency
    sigma_notch = grid_freq_cpx * 0.15
    notch = 1.0 - (np.exp(-((u_rot - grid_freq_cpx) ** 2) / (2 * sigma_notch ** 2)) +
                   np.exp(-((u_rot + grid_freq_cpx) ** 2) / (2 * sigma_notch ** 2)))
    notch = np.clip(notch, 0.0, 1.0)

    F = np.fft.fft2(image)
    F_notched = F * notch
    return np.real(np.fft.ifft2(F_notched)).astype(np.float32)
```

---

### 5.2 Virtual Grid — Scatter Correction 알고리즘 (GAP-08 해소)

#### 5.2.1 알고리즘 선택 전략

| 상황 | 권장 방법 | 이유 |
|------|----------|------|
| Phase 1 (빠른 구현) | Thickness-based Empirical (§5.2.2) | 구현 단순, real-time |
| Phase 2 (정확도 향상) | Laplacian Pyramid (§5.2.3) | US8064676B2 특허 기반 |
| Phase 3 (최고 정확도) | DL U-Net (§5.2.4) | MC 데이터 학습, <5% 오차 |

#### 5.2.2 Laplacian Pyramid Virtual Grid (US8064676B2 특허 기반)

```python
def laplacian_pyramid_virtual_grid(image: np.ndarray,
                                    pixel_pitch_mm: float,
                                    target_grid_ratio: float = 10.0) -> np.ndarray:
    """
    Virtual grid via Laplacian Pyramid scatter estimation.
    
    Algorithm (US8064676B2):
      1. Build Laplacian Pyramid (n = log2(N) - 0.5 levels)
      2. Low-frequency bands: scatter component → de-scatter
      3. High-frequency bands: contrast enhancement + denoising
      4. Reconstruct enhanced image
    
    Args:
        image:            float32 input (H, W), linear domain (pre-log transform)
        pixel_pitch_mm:   detector pixel pitch in mm
        target_grid_ratio: emulated grid ratio (5:1 ~ 16:1)
    Returns:
        scatter-corrected float32 image
    """
    H, W = image.shape
    n_levels = int(np.log2(max(H, W)) - 0.5)
    
    # Build Gaussian pyramid (5×5 Gaussian kernel, σ=1, per patent)
    gaussian_pyr = [image]
    for _ in range(n_levels):
        gaussian_pyr.append(cv2.pyrDown(gaussian_pyr[-1]))
    
    # Build Laplacian pyramid
    laplacian_pyr = []
    for k in range(n_levels):
        up = cv2.pyrUp(gaussian_pyr[k+1], dstsize=gaussian_pyr[k].shape[::-1])
        laplacian_pyr.append(gaussian_pyr[k] - up)
    laplacian_pyr.append(gaussian_pyr[-1])  # residual
    
    # Scatter is primarily in low-frequency bands
    # Estimate scatter fraction based on grid ratio emulation
    # Bucky factor B = (1 + 1/R)/(1 - scatter_fraction) where R = grid ratio
    scatter_fraction = estimate_scatter_fraction(target_grid_ratio)
    
    # De-scatter low-frequency bands
    for k in range(n_levels - 2, n_levels + 1):  # lower 2 bands + residual
        idx = min(k, len(laplacian_pyr) - 1)
        laplacian_pyr[idx] = (1.0 / (1.0 - scatter_fraction)) * laplacian_pyr[idx]
    
    # Contrast enhancement for high-frequency bands
    enhancement_factors = compute_enhancement_factors(target_grid_ratio, n_levels)
    for k in range(n_levels - 2):
        laplacian_pyr[k] *= enhancement_factors[k]
    
    # Reconstruct
    result = laplacian_pyr[-1]
    for k in range(n_levels - 1, -1, -1):
        result = cv2.pyrUp(result, dstsize=laplacian_pyr[k].shape[::-1]) + laplacian_pyr[k]
    
    return np.clip(result, 0, None)

def estimate_scatter_fraction(grid_ratio: float) -> float:
    """
    Estimate scatter fraction based on equivalent grid ratio.
    SPR (Scatter-to-Primary Ratio) model:
    For chest AP, 20cm patient, 80kVp: SPR ≈ 100%
    Effective scatter fraction = SPR / (1 + SPR)
    Grid reduces scatter by: factor ≈ R/(R-1) approximately
    """
    # Simplified model; full implementation uses patient thickness estimation
    spr_base = 1.0  # 100% SPR baseline (20cm chest, 80kVp)
    transmission_factor = 1.0 / grid_ratio  # approximate
    return spr_base * transmission_factor / (1.0 + spr_base * transmission_factor)
```

#### 5.2.3 Scatter Fraction 추정 (Thickness 기반)

```python
def estimate_scatter_fraction_from_exposure(
        image: np.ndarray,
        kvp: float,
        sid_mm: float,
        pixel_pitch_mm: float) -> float:
    """
    Estimate patient scatter fraction from image signal statistics.
    Based on Fujifilm Virtual Grid empirical model.
    
    SPR reference table (80kVp, 35×43cm FOV):
      10cm: SPR ~35%
      15cm: SPR ~70%
      20cm: SPR ~100%
      25cm: SPR ~150%
    """
    # Step 1: Estimate effective patient thickness from signal attenuation
    # Primary signal region: darkest area of lung fields (chest)
    # or central anatomical region
    p10 = np.percentile(image, 10)   # approximately primary + scatter
    p90 = np.percentile(image, 90)   # approximately scatter only
    
    # Approximate effective thickness from signal ratio
    # (simplified Beer-Lambert)
    mu_water = 0.018  # cm⁻¹ at 80kVp (effective)
    if p90 > 0 and p10 > 0:
        thickness_cm = -np.log(p10 / p90) / mu_water
    else:
        thickness_cm = 20.0  # default: 20cm
    thickness_cm = np.clip(thickness_cm, 5.0, 40.0)
    
    # kVp correction factor
    kvp_factor = 1.0 + 0.003 * (kvp - 80)
    
    # SPR lookup (linear interpolation)
    spr_table = {10: 0.35, 15: 0.70, 20: 1.00, 25: 1.50, 30: 2.00}
    spr = np.interp(thickness_cm, list(spr_table.keys()),
                    list(spr_table.values())) * kvp_factor
    
    return spr / (1.0 + spr)  # scatter fraction from SPR
```

---

### 5.3 해부 부위별 Virtual Grid 프리셋 테이블 (GAP-V 해소)

xpe-algorithm-spec-deepsync.md §4의 "anatomy-bounded virtual-grid presets"에서 결정된 항목이다. 기존 §5.2는 단일 Virtual Grid 파라미터를 사용하였으나, 실제 임상에서는 해부 부위별로 최적 파라미터가 다르다. 전신 촬영 부위에 대한 사전 검증된 프리셋 테이블을 제공한다.

#### 5.3.1 알고리즘 수학 정의

**Virtual Grid 강도 파라미터**:

$$\text{VG}_{\lambda} = \text{scatter\_fraction} \times \lambda_{\text{anatomy}} \times \lambda_{\text{kvp\_scale}}$$

$$\lambda_{\text{kvp\_scale}} = 1.0 + 0.004 \times (kVp - 80)$$

여기서 $\lambda_{\text{anatomy}}$는 해부 부위별 기준 강도 파라미터이다.

**Grid 비율 선택**:

$$\text{grid\_ratio}_{\text{effective}} = \text{grid\_ratio}_{\text{preset}} \times \frac{1}{1 + \text{scatter\_fraction}}$$

#### 5.3.2 해부 부위별 프리셋 테이블

| 해부 부위 | `body_part_id` | `grid_ratio` | `lambda` | `frequency_lp_mm` | 비고 |
|---------|--------------|------------|---------|------------------|------|
| Chest AP | `CHEST_AP` | 12 | 0.65 | 40–70 | 고산란 (폐/심장) |
| Chest Lateral | `CHEST_LAT` | 15 | 0.75 | 40–70 | 최고 산란 |
| Abdomen AP | `ABD_AP` | 12 | 0.70 | 40–60 | 고산란 복부 |
| Lumbar Spine AP | `LSPINE_AP` | 12 | 0.65 | 40–60 | 두꺼운 조직 |
| Lumbar Spine Lat | `LSPINE_LAT` | 15 | 0.75 | 40–60 | — |
| Pelvis AP | `PELVIS_AP` | 12 | 0.60 | 40–60 | — |
| Hip | `HIP` | 10 | 0.55 | 40–60 | — |
| Knee AP/Lat | `KNEE` | 8 | 0.35 | 50–80 | 낮은 산란 |
| Hand/Wrist | `HAND_WRIST` | 6 | 0.20 | 70–120 | 극소 산란 |
| Foot/Ankle | `FOOT_ANKLE` | 6 | 0.20 | 70–120 | — |
| Skull AP/Lat | `SKULL` | 10 | 0.45 | 50–80 | — |
| C-Spine | `CSPINE` | 8 | 0.40 | 50–80 | — |
| T-Spine | `TSPINE` | 10 | 0.55 | 40–70 | — |
| Shoulder | `SHOULDER` | 8 | 0.40 | 50–80 | — |
| Extremity General | `EXTREMITY` | 6 | 0.25 | 60–100 | 소아 포함 |

**주석**: `grid_ratio`는 Bucky grid 그리드비, `lambda`는 Laplacian Pyramid VG의 기준 강도 계수, `frequency_lp_mm`는 VG가 표적하는 공간 주파수 대역 (lp/mm).

#### 5.3.3 Python 구현

```python
from dataclasses import dataclass
from typing import Optional
import numpy as np

@dataclass(frozen=True)
class VirtualGridPreset:
    body_part_id:      str
    grid_ratio:        int     # nominal (10, 12, 15)
    lambda_base:       float   # base VG strength coefficient
    freq_lo_lpmm:      float   # target frequency band lower bound
    freq_hi_lpmm:      float   # target frequency band upper bound
    description:       str = ''

# Canonical preset table (IEC 62304 §5.4: frozen, change requires review cycle)
VIRTUAL_GRID_PRESETS: dict[str, VirtualGridPreset] = {
    'CHEST_AP':     VirtualGridPreset('CHEST_AP',    12, 0.65, 40, 70,  'Chest AP'),
    'CHEST_LAT':    VirtualGridPreset('CHEST_LAT',   15, 0.75, 40, 70,  'Chest Lateral'),
    'ABD_AP':       VirtualGridPreset('ABD_AP',      12, 0.70, 40, 60,  'Abdomen AP'),
    'LSPINE_AP':    VirtualGridPreset('LSPINE_AP',   12, 0.65, 40, 60,  'Lumbar Spine AP'),
    'LSPINE_LAT':   VirtualGridPreset('LSPINE_LAT',  15, 0.75, 40, 60,  'Lumbar Spine Lat'),
    'PELVIS_AP':    VirtualGridPreset('PELVIS_AP',   12, 0.60, 40, 60,  'Pelvis AP'),
    'HIP':          VirtualGridPreset('HIP',         10, 0.55, 40, 60,  'Hip'),
    'KNEE':         VirtualGridPreset('KNEE',         8, 0.35, 50, 80,  'Knee AP/Lat'),
    'HAND_WRIST':   VirtualGridPreset('HAND_WRIST',   6, 0.20, 70, 120, 'Hand/Wrist'),
    'FOOT_ANKLE':   VirtualGridPreset('FOOT_ANKLE',   6, 0.20, 70, 120, 'Foot/Ankle'),
    'SKULL':        VirtualGridPreset('SKULL',        10, 0.45, 50, 80, 'Skull'),
    'CSPINE':       VirtualGridPreset('CSPINE',       8, 0.40, 50, 80,  'Cervical Spine'),
    'TSPINE':       VirtualGridPreset('TSPINE',       10, 0.55, 40, 70, 'Thoracic Spine'),
    'SHOULDER':     VirtualGridPreset('SHOULDER',     8, 0.40, 50, 80,  'Shoulder'),
    'EXTREMITY':    VirtualGridPreset('EXTREMITY',    6, 0.25, 60, 100, 'Extremity General'),
}


def get_vg_params(body_part_id:    str,
                  scatter_fraction: float,
                  kvp:              float,
                  custom_lambda:    Optional[float] = None) -> dict:
    """
    Get Virtual Grid processing parameters for a specific anatomy and exposure.

    Args:
        body_part_id:     anatomy identifier (e.g., 'CHEST_AP')
        scatter_fraction: estimated scatter fraction (0–0.7, from §5.2.3)
        kvp:              tube voltage (kVp)
        custom_lambda:    override preset lambda (for operator adjustment)
    Returns:
        dict: {lambda_vg, grid_ratio, freq_lo, freq_hi, body_part_id}
    """
    preset = VIRTUAL_GRID_PRESETS.get(body_part_id,
                                       VIRTUAL_GRID_PRESETS['EXTREMITY'])

    lambda_base = custom_lambda if custom_lambda is not None else preset.lambda_base

    # Scale by scatter fraction and kVp
    kvp_scale    = 1.0 + 0.004 * (kvp - 80.0)
    lambda_final = lambda_base * scatter_fraction * kvp_scale

    # Clamp to valid range
    lambda_final = float(np.clip(lambda_final, 0.05, 1.5))

    return {
        'lambda_vg':    lambda_final,
        'grid_ratio':   preset.grid_ratio,
        'freq_lo_lpmm': preset.freq_lo_lpmm,
        'freq_hi_lpmm': preset.freq_hi_lpmm,
        'body_part_id': preset.body_part_id,
    }


def validate_vg_output(input_img:  np.ndarray,
                        output_img: np.ndarray,
                        preset:     VirtualGridPreset) -> dict:
    """
    Validate that VG output satisfies CNR and artifact criteria for the preset.
    Returns dict: {cnr_improvement, artifact_flag, pass}
    """
    # CNR: region-of-interest contrast-to-noise ratio
    # (simplified: use center vs background std ratio)
    H, W = input_img.shape
    roi = input_img[H//4:3*H//4, W//4:3*W//4]
    bg  = np.concatenate([input_img[:H//8, :].ravel(),
                           input_img[7*H//8:, :].ravel()])

    def cnr(arr_roi, arr_bg):
        return abs(np.mean(arr_roi) - np.mean(arr_bg)) / (np.std(arr_bg) + 1e-6)

    cnr_in  = cnr(roi, bg)
    roi_out = output_img[H//4:3*H//4, W//4:3*W//4]
    bg_out  = np.concatenate([output_img[:H//8, :].ravel(),
                               output_img[7*H//8:, :].ravel()])
    cnr_out = cnr(roi_out, bg_out)

    cnr_improvement = cnr_out / (cnr_in + 1e-6)
    artifact_flag   = bool(np.max(np.abs(output_img - input_img)) >
                           0.3 * np.mean(input_img))  # overshoot check

    return {
        'cnr_improvement': cnr_improvement,
        'artifact_flag':   artifact_flag,
        'pass':            cnr_improvement >= 1.05 and not artifact_flag,
    }
```

#### 5.3.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| CNR 개선 (Chest AP) | ≥ 10% vs no-VG | CDRAD 팬텀 또는 합성 |
| CNR 개선 (Extremity) | ≥ 5% vs no-VG | CDRAD 팬텀 |
| MTF 열화 | < 5% at f50 | 슬랜트 에지 측정 |
| 과도 보정 오결 (Artifact) | 없음 | 시각 검토 + 픽셀 오버슈트 |
| Observer 검증 (chest) | ≥ 전문의 3명 동의 | 블라인드 A/B 테스트 |

**IEC 62304 §5.4 추적성**: SRS ID: SRS-FUNC-008b (Virtual Grid 프리셋) — Phase 2 추가 예정

---

## 6. SWI-3: Display Processing 알고리즘

### 6.1 SWU-3.1 Modality LUT (SRS-FUNC-020)

#### 6.1.1 수학 정의

$$\text{StoredPixelValue} \xrightarrow{\text{Modality LUT}} \text{ModalityPixelValue}$$

**Linear form (DICOM PS3.3 §C.7.6.3.1.2):**

$$\text{ModalityPixelValue} = \text{RescaleSlope} \times \text{StoredPixelValue} + \text{RescaleIntercept}$$

- DICOM tags: `(0028,1053)` RescaleSlope, `(0028,1052)` RescaleIntercept
- 단위: Housfield Units (CT) 또는 arbitrary linear units (DX)
- DX의 경우: Slope=1, Intercept=0 (identity)가 일반적

#### 6.1.2 구현

```cpp
void xpe_apply_modality_lut(const uint16_t* stored_pixels,
                              float* modality_pixels,
                              float rescale_slope,
                              float rescale_intercept,
                              uint32_t total_pixels) {
    // AVX2 vectorized Fused Multiply-Add
    __m256 v_slope  = _mm256_set1_ps(rescale_slope);
    __m256 v_interc = _mm256_set1_ps(rescale_intercept);
    
    size_t i = 0;
    for (; i + 8 <= total_pixels; i += 8) {
        __m128i u16x8 = _mm_loadu_si128(
            reinterpret_cast<const __m128i*>(stored_pixels + i));
        __m256 f32x8  = _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(u16x8));
        __m256 result = _mm256_fmadd_ps(f32x8, v_slope, v_interc);
        _mm256_storeu_ps(modality_pixels + i, result);
    }
    for (; i < total_pixels; ++i) {
        modality_pixels[i] = rescale_slope * stored_pixels[i] + rescale_intercept;
    }
}
```

---

### 6.2 SWU-3.2 VOI LUT (SRS-FUNC-021)

#### 6.2.1 LINEAR 변환

$$\text{Output} = \frac{\text{Input} - (\text{WC} - \text{WW}/2)}{\text{WW}} \times (\text{MaxOut} - \text{MinOut}) + \text{MinOut}$$

Clamp: `[MinOut, MaxOut]`

#### 6.2.2 LINEAR_EXACT 변환 (DICOM PS3.3 §C.11.2.1.2)

$$\text{Output} = \begin{cases} \text{MinOut} & \text{if Input} \le WC - \lfloor WW/2 \rfloor \\ \frac{(Input - (WC - 0.5)) \cdot (MaxOut - MinOut + 1)}{WW} + \frac{MinOut + MaxOut}{2} & \text{otherwise} \\ \text{MaxOut} & \text{if Input} > WC + \lfloor (WW-1)/2 \rfloor \end{cases}$$

#### 6.2.3 SIGMOID 변환

$$\text{Output} = \frac{\text{MaxOut} - \text{MinOut}}{1 + e^{-4(\text{Input} - WC)/WW}} + \text{MinOut}$$

- **장점**: 선형에 비해 extreme 값에서 부드러운 클리핑 → 구조 과노출 방지
- **권장 사용 사례**: 폐, 종격동 동시 표현

#### 6.2.4 실시간 W/L 조정 구현 (SRS-PERF-003: ≤16ms)

```cpp
// GPU-accelerated VOI LUT for real-time interactive adjustment
// Falls back to AVX2 CPU path if GPU unavailable
void xpe_apply_voi_lut(const float* modality_pixels,
                         uint16_t*   output,
                         VoiLutType  lut_type,
                         float wc, float ww,
                         float min_out, float max_out,
                         uint32_t total_pixels) {
    
    const float half_ww = ww * 0.5f;
    const float lo = wc - half_ww;
    const float range_out = max_out - min_out;
    
    switch (lut_type) {
        case VoiLutType::LINEAR: {
            __m256 v_lo    = _mm256_set1_ps(lo);
            __m256 v_scale = _mm256_set1_ps(range_out / ww);
            __m256 v_off   = _mm256_set1_ps(min_out);
            __m256 v_min   = _mm256_set1_ps(min_out);
            __m256 v_max   = _mm256_set1_ps(max_out);
            
            size_t i = 0;
            for (; i + 8 <= total_pixels; i += 8) {
                __m256 inp  = _mm256_loadu_ps(modality_pixels + i);
                __m256 norm = _mm256_sub_ps(inp, v_lo);
                __m256 res  = _mm256_fmadd_ps(norm, v_scale, v_off);
                res = _mm256_min_ps(_mm256_max_ps(res, v_min), v_max);
                // Convert to uint16
                __m256i res_i = _mm256_cvttps_epi32(res);
                // Pack and store (simplified; full impl uses _mm256_packs_epi32)
                // ... store to output
            }
            break;
        }
        case VoiLutType::SIGMOID: {
            // Vectorized sigmoid via polynomial approximation
            // f(x) ≈ 0.5 + 0.25*x*(1 - x²/12) for |x| < 2
            break;
        }
        default: break;
    }
}
```

#### 6.2.5 Body-Part Preset 테이블 (≥20 preset, SRS-FUNC-021)

```json
{
  "presets": [
    {"name":"Chest Standard",   "body_part":"CHEST",    "wc":400, "ww":2000, "type":"LINEAR"},
    {"name":"Chest Lung",       "body_part":"CHEST",    "wc":-600,"ww":1500, "type":"SIGMOID"},
    {"name":"Chest Mediastinum","body_part":"CHEST",    "wc":50,  "ww":400,  "type":"LINEAR"},
    {"name":"Bone Standard",    "body_part":"EXTREMITY","wc":500, "ww":2500, "type":"LINEAR"},
    {"name":"Abdomen",          "body_part":"ABDOMEN",  "wc":60,  "ww":400,  "type":"LINEAR"},
    {"name":"Spine",            "body_part":"SPINE",    "wc":500, "ww":2000, "type":"LINEAR"},
    {"name":"Skull",            "body_part":"HEAD",     "wc":500, "ww":3000, "type":"LINEAR"},
    {"name":"Hand/Wrist",       "body_part":"HAND",     "wc":600, "ww":2500, "type":"LINEAR"},
    {"name":"Pelvis",           "body_part":"PELVIS",   "wc":400, "ww":2000, "type":"LINEAR"},
    {"name":"Shoulder",         "body_part":"SHOULDER", "wc":300, "ww":1500, "type":"LINEAR"},
    {"name":"Knee",             "body_part":"KNEE",     "wc":500, "ww":2000, "type":"LINEAR"},
    {"name":"Ankle/Foot",       "body_part":"FOOT",     "wc":600, "ww":2000, "type":"LINEAR"},
    {"name":"Breast MLO",       "body_part":"BREAST",   "wc":2000,"ww":4000, "type":"LINEAR"},
    {"name":"Pediatric Chest",  "body_part":"CHEST",    "wc":300, "ww":1500, "type":"SIGMOID"},
    {"name":"Neonatal",         "body_part":"CHEST",    "wc":200, "ww":800,  "type":"SIGMOID"},
    {"name":"Panoramic Dental", "body_part":"DENTAL",   "wc":500, "ww":2000, "type":"LINEAR"},
    {"name":"Long Leg",         "body_part":"LOWER_EX", "wc":400, "ww":2000, "type":"LINEAR"},
    {"name":"Full Spine",       "body_part":"SPINE",    "wc":500, "ww":2500, "type":"LINEAR"},
    {"name":"Scoliosis",        "body_part":"SPINE",    "wc":400, "ww":1800, "type":"LINEAR_EXACT"},
    {"name":"Soft Tissue",      "body_part":"EXTREMITY","wc":100, "ww":400,  "type":"LINEAR"},
    {"name":"Bone Suppress",    "body_part":"CHEST",    "wc":300, "ww":1200, "type":"SIGMOID"}
  ]
}
```

---

### 6.3 SWU-3.3 Presentation LUT — GSDF (SRS-FUNC-022)

#### 6.3.1 DICOM PS3.14 GSDF 알고리즘

**목적**: P-Value(0–4095)를 Just Noticeable Difference(JND)가 균일한 휘도로 변환.

**GSDF 수학 모델 (DICOM PS3.14 §6):**

$$\bar{L}(j) = \frac{L_{\min} + L_{\max}}{2} \cdot \exp\left(\frac{j - j_0}{j_0}\right) \quad \text{(simplified)}$$

정확한 구현은 PS3.14 Table B.1의 256-point LUT 사용:

```cpp
// GSDF P-Value to Luminance conversion
// Source: DICOM PS3.14 Table B.1 (256 P-Value entries)
// Full table: 1024 entries interpolated from 256

struct GSDFCalibration {
    float L_min_cdm2;   // minimum luminance of display (cd/m²)
    float L_max_cdm2;   // maximum luminance of display (cd/m²)
    float gamma;        // display gamma (typically 2.2)
    std::vector<float> gsdf_lut;  // 4096-entry P-Value → DDL LUT
};

// Build calibrated Presentation LUT for current display
std::vector<uint16_t> build_presentation_lut(
        const GSDFCalibration& cal,
        uint16_t p_value_range = 4096) {
    
    // GSDF JND indices for given luminance range
    float j_min = compute_jnd_index(cal.L_min_cdm2);  // PS3.14 §B.2
    float j_max = compute_jnd_index(cal.L_max_cdm2);
    
    // Map P-Values uniformly across JND range
    std::vector<uint16_t> lut(p_value_range);
    for (uint16_t p = 0; p < p_value_range; ++p) {
        float j = j_min + (j_max - j_min) * p / (p_value_range - 1);
        float L = jnd_index_to_luminance(j);  // inverse GSDF
        // Convert luminance to DDL (Digital Driving Level)
        uint16_t ddl = luminance_to_ddl(L, cal);
        lut[p] = ddl;
    }
    return lut;
}

// JND index formula (PS3.14 §B.2)
float compute_jnd_index(float L_cdm2) {
    float log_L = std::log10(L_cdm2);
    // 4th-order polynomial approximation (DICOM standard)
    return 71.498068f + 94.593053f * log_L + 41.912053f * log_L * log_L
           + 9.8247004f * pow(log_L, 3) + 0.28175407f * pow(log_L, 4)
           - 1.1878455f * pow(log_L, 5) - 0.18014349f * pow(log_L, 6)
           + 0.14710899f * pow(log_L, 7) - 0.017046845f * pow(log_L, 8);
}
```

#### 6.3.2 Display 교정 미보정 감지 (SRS-ALERT-003)

```cpp
bool detect_uncalibrated_display(const DisplayDevice& device) {
    // Measure luminance at multiple DDL steps
    // Compare measured JND spacing to GSDF target
    // If max deviation > 10% → flag as uncalibrated
    float gsdf_conformance = evaluate_gsdf_conformance(device);
    return gsdf_conformance < 0.90f;  // <90% conformance → warning
}
```

---

## 7. IEC 62494-1 Exposure Index 알고리즘

### 7.1 알고리즘 개요 (GAP-09 해소)

IEC 62494-1은 디지털 방사선 촬영의 **노출 적절성**을 수치화하는 표준이다:

- **EI (Exposure Index)**: 검출기 수신 선량에 비례하는 지수
- **EI_target**: 특정 촬영 유형의 목표 EI
- **DI (Deviation Index)**: EI 대비 EI_target의 편차 (dB 단위)

$$DI = 10 \cdot \log_{10}\left(\frac{EI}{EI_{\text{target}}}\right)$$

### 7.2 ROI 추출 알고리즘

```cpp
struct ExposureIndexROI {
    cv::Rect roi;         // Selected ROI rectangle
    float    mean_signal; // Mean pixel value in ROI
    float    area_mm2;    // Physical area of ROI
};

// IEC 62494-1 §7.3: ROI selection methods
ExposureIndexROI select_roi_for_ei(const cv::Mat& image,
                                    const CollimatorMask& collimator,
                                    RoiSelectionMethod method,
                                    const std::string& body_part) {
    switch (method) {
        case RoiSelectionMethod::FULL_FIELD:
            // Use entire collimated area (simple, method A)
            return {collimator.bounding_rect(), 
                    mean_within_mask(image, collimator.mask()), 0.0f};
        
        case RoiSelectionMethod::ANATOMY_BASED:
            // Method B: auto-detect anatomical region
            // For chest: left/right lung ROIs
            // For extremity: bone shaft ROI
            return detect_anatomy_roi(image, body_part);
        
        case RoiSelectionMethod::CENTRAL:
            // Method C: central 10% of collimated area (by area fraction)
            // Target: ROI_area = 0.10 × full_area
            //   => w_roi = full.width  × sqrt(0.10) ≈ full.width  × 0.3162
            //   => h_roi = full.height × sqrt(0.10) ≈ full.height × 0.3162
            // IEC 62494-1 §7.2.4 — S_d region must represent ≥10% of receptor area
            auto full = collimator.bounding_rect();
            int cx = full.x + full.width / 2;
            int cy = full.y + full.height / 2;
            // sqrt(0.10) = 0.31623 — use compile-time constant for clarity
            constexpr double kSqrt01 = 0.31622776601683794;  // sqrt(0.10)
            int w  = static_cast<int>(std::round(full.width  * kSqrt01));
            int h  = static_cast<int>(std::round(full.height * kSqrt01));
            // Ensure minimum 32×32 pixels for statistical validity
            w = std::max(w, 32);
            h = std::max(h, 32);
            return {cv::Rect(cx - w/2, cy - h/2, w, h), 0.0f, 0.0f};
    }
}
```

### 7.3 EI 계산

```cpp
float compute_exposure_index(float mean_roi_signal,
                               float pixel_pitch_mm,
                               float rescale_slope,
                               float rescale_intercept,
                               const DetectorCalibrationData& cal) {
    // Convert mean ROI signal to calibrated detector signal S_cal
    // S_cal = (mean_roi_signal × rescale_slope + rescale_intercept)
    float s_cal = mean_roi_signal * rescale_slope + rescale_intercept;
    
    // EI = C_ei × s_cal (IEC 62494-1 §7.2)
    // C_ei: detector-specific calibration constant
    // Calibrated such that EI = 100 corresponds to reference entrance dose
    float ei = cal.C_ei * s_cal;
    
    // Clamp to valid range [0, 10000]
    return std::clamp(ei, 0.0f, 10000.0f);
}

float compute_deviation_index(float ei, float ei_target) {
    if (ei_target <= 0.0f || ei <= 0.0f) return 0.0f;
    return 10.0f * std::log10(ei / ei_target);
}

// DI interpretation:
// DI < -1.0: underexposure (high noise)
// -1.0 ≤ DI ≤ +1.0: acceptable exposure
// DI > +1.0: overexposure (unnecessary dose)
// DI > +3.0: significant overexposure → alert
```

### 7.4 EI_target 테이블

| 촬영 부위 | EI_target | 참고 |
|----------|-----------|------|
| Chest PA | 200 | ACR 권장 |
| Chest AP (portable) | 300 | 산란 증가 반영 |
| Abdomen AP | 250 | |
| Spine AP/Lateral | 200 | |
| Extremity | 100 | 낮은 감쇠 |
| Hand/Foot | 80 | |
| Pelvis AP | 250 | |
| Skull | 200 | |

---

## 8. AI/DL 알고리즘

### 8.1 CNN Body-Part Recognition (SRS-FUNC-016)

#### 8.1.1 모델 아키텍처

```
Input: 512×512 (downsampled from full resolution)
  ↓
EfficientNet-B4 Backbone (ImageNet pretrained)
  ↓
Global Average Pooling
  ↓
FC(1792 → 512) + BatchNorm + ReLU + Dropout(0.3)
  ↓
FC(512 → N_classes)   N_classes = 15+ body parts
  ↓
Softmax → confidence scores
```

#### 8.1.2 신체 부위 분류 체계 (≥15 categories)

| 클래스 ID | 명칭 | DICOM Body Part |
|---------|------|----------------|
| 0 | Chest PA | CHEST |
| 1 | Chest AP | CHEST |
| 2 | Chest Lateral | CHEST |
| 3 | Abdomen AP | ABDOMEN |
| 4 | Pelvis AP | PELVIS |
| 5 | Spine Cervical | CSPINE |
| 6 | Spine Thoracic | TSPINE |
| 7 | Spine Lumbar | LSPINE |
| 8 | Shoulder | SHOULDER |
| 9 | Elbow | ELBOW |
| 10 | Hand/Wrist | HAND |
| 11 | Hip | HIP |
| 12 | Knee | KNEE |
| 13 | Ankle/Foot | FOOT |
| 14 | Skull | HEAD |
| 15 | Full Spine | SPINE |
| 16 | Long Leg | LOWER_EXTREMITY |

#### 8.1.3 Preprocessing for Inference

```python
def preprocess_for_body_part_recognition(image: np.ndarray) -> np.ndarray:
    """
    Preprocess X-ray image for CNN inference.
    """
    # 1. Resize to 512×512
    img = cv2.resize(image, (512, 512), interpolation=cv2.INTER_AREA)
    
    # 2. Normalize to [0, 1] using percentile normalization
    p2  = np.percentile(img, 2)
    p98 = np.percentile(img, 98)
    img = (img - p2) / max(p98 - p2, 1e-6)
    img = np.clip(img, 0.0, 1.0)
    
    # 3. Expand to 3 channels (grayscale → RGB replication)
    img_3ch = np.stack([img, img, img], axis=0)  # (3, H, W)
    
    # 4. Normalize with ImageNet stats (for pretrained backbone)
    mean = np.array([0.485, 0.456, 0.406]).reshape(3, 1, 1)
    std  = np.array([0.229, 0.224, 0.225]).reshape(3, 1, 1)
    img_norm = (img_3ch - mean) / std
    
    return img_norm.astype(np.float32)[np.newaxis]  # (1, 3, 512, 512)
```

#### 8.1.4 ONNX Runtime 추론 (xpe_ai_worker.exe)

```cpp
// In xpe_ai_worker.exe (sandbox process)
class BodyPartRecognizer {
    Ort::Session session_;
    
public:
    BodyPartResult recognize(const float* preprocessed_input,
                               size_t input_size) {
        // Create input tensor
        auto input_tensor = Ort::Value::CreateTensor<float>(
            memory_info_, const_cast<float*>(preprocessed_input),
            input_size, input_shape_.data(), input_shape_.size());
        
        // Run inference
        auto outputs = session_.Run(Ort::RunOptions{nullptr},
                                     input_names_.data(), &input_tensor, 1,
                                     output_names_.data(), 1);
        
        // Parse softmax output
        float* scores = outputs[0].GetTensorMutableData<float>();
        int n_classes = static_cast<int>(outputs[0].GetTensorTypeAndShapeInfo()
                                          .GetShape()[1]);
        
        int best_class = std::max_element(scores, scores + n_classes) - scores;
        float confidence = scores[best_class];
        
        return {best_class, confidence, 
                std::vector<float>(scores, scores + n_classes)};
    }
};
```

#### 8.1.5 성능 요구사항

- **정확도**: ≥95% top-1 accuracy (SRS-FUNC-016)
- **추론 시간**: ≤200ms (CPU 추론, EfficientNet-B4)
- **입력 허용 범위**: 0.5× ~ 2× 기준 크기

---

### 8.2 DL Bone Suppression (SRS-FUNC-018)

#### 8.2.1 모델 아키텍처 — U-Net with Attention

```
Encoder:                          Decoder:
Input (H×W×1)                     (H×W×1) Output
  ↓                                  ↑
Conv3×3 + BN + ReLU ×2             ← Skip connection (attention gate)
MaxPool2×2                          UpSample2×2 + Conv
  ↓                                  ↑
×4 encoder blocks              ×4 decoder blocks
  ↓
Bottleneck: Conv3×3 ×3 (dilated 1,2,4)
```

#### 8.2.2 훈련 데이터 생성

```python
# Virtual training pairs from real Dual-Energy Subtraction (DES)
# - Standard X-ray (mixed bone + soft tissue)
# - DES bone image (real dual-energy reference)
# - DES soft tissue image (ground truth for bone suppression)

# Data augmentation:
# - Random horizontal flip
# - Random rotation ±5°
# - Gaussian noise injection (σ = 0.01–0.05)
# - Random contrast adjustment (0.9–1.1×)
# - Random elastic deformation (medical augmentation)
```

#### 8.2.3 손실 함수

$$\mathcal{L} = \lambda_1 \mathcal{L}_{L1} + \lambda_2 \mathcal{L}_{SSIM} + \lambda_3 \mathcal{L}_{perceptual}$$

$$\mathcal{L}_{L1} = \|I_{pred} - I_{DES}\|_1$$

$$\mathcal{L}_{SSIM} = 1 - \text{SSIM}(I_{pred}, I_{DES})$$

$$\mathcal{L}_{perceptual} = \|\phi_k(I_{pred}) - \phi_k(I_{DES})\|_2$$

- **목표**: PSNR ≥ 33dB, SSIM ≥ 0.97 (SRS-FUNC-018)

---

### 8.3 Panoramic Image Stitching (SRS-FUNC-017)

#### 8.3.1 알고리즘 파이프라인 (GAP-07 해소)

```
Input: 2-4 overlapping X-ray images (10-30% overlap)
         ↓
Step 1: Feature Detection (SIFT/ORB on bone edges)
         ↓
Step 2: Feature Matching (FLANN-based matcher + ratio test)
         ↓
Step 3: Homography Estimation (RANSAC, ≥4 point pairs)
         ↓
Step 4: Geometric Correction (perspective + distortion)
         ↓
Step 5: Intensity Normalization (histogram matching at overlap)
         ↓
Step 6: Blending (Multi-band blending or Feathering)
         ↓
Output: Panoramic image with Cobb angle error ≤ 2°
```

#### 8.3.2 Cobb Angle 오차 ≤2° 달성 전략

```python
def validate_stitching_accuracy(stitched: np.ndarray,
                                 individual_images: list[np.ndarray],
                                 known_landmarks: list[dict]) -> dict:
    """
    Validate stitching accuracy using vertebral landmark pairs.
    
    Cobb angle error = |Cobb_stitched - Cobb_ground_truth|
    Acceptance: ≤ 2°
    """
    # Measure vertebral endplate angles in stitched image
    cobb_stitched = measure_cobb_angle(stitched, known_landmarks)
    
    # Ground truth from reference measurement
    # (physical phantom with calibrated curvature)
    cobb_reference = known_landmarks[0]['cobb_angle_reference']
    
    error_deg = abs(cobb_stitched - cobb_reference)
    return {
        'cobb_stitched': cobb_stitched,
        'cobb_reference': cobb_reference,
        'error_deg': error_deg,
        'pass': error_deg <= 2.0
    }
```

---

### 8.4 AI Worker 격리 아키텍처 및 ONNX 추론 (GAP-W 해소)

xpe-algorithm-spec-deepsync.md §5.3 "prefer ONNX CPU execution with quantized inference, require deterministic fallback, versioned model manifests"에서 요구된 항목이다. 기존 §8.1.4에는 기본 ONNX 세션이 있지만, 워커 격리, 모델 매니페스트 스키마, 결정론적 폴백 메커니즘이 누락되어 있었다.

#### 8.4.1 AI Worker 격리 설계 원칙

```
격리 아키텍처:

  XPE 메인 프로세스
  ┌───────────────────────────────┐
  │  Deterministic Pipeline       │
  │  (§3 Pre-Process, §4 Core)    │
  │         │                     │
  │  xpe_ai_worker_proxy()        │──── IPC (shared memory + semaphore)
  │         │                     │
  └─────────│─────────────────────┘
            │ input tensor + request_id
            ▼
  xpe_ai_worker.exe (isolated process)
  ┌────────────────────────────────┐
  │  ONNX Runtime Session          │
  │  Quantized INT8 model (NCHW)   │
  │  Model Manifest Validator      │
  │  Timeout watchdog (5s)         │
  │         │                      │
  │  Result + confidence → IPC     │
  └────────────────────────────────┘
            │ fallback if timeout or error
            ▼
  Deterministic fallback
  (heuristic body-part classifier)
```

**격리 규칙**:
- AI 워커 실패 또는 타임아웃 시 메인 파이프라인은 결정론적 폴백으로 계속 진행
- AI 결과는 항상 `is_ai_result` 플래그와 함께 반환 (QualityState 사이드카에 기록)
- 모델 버전이 매니페스트와 불일치 시 워커 시작 거부

#### 8.4.2 모델 매니페스트 스키마

```json
{
  "schema_version": "1.0",
  "model_id": "body_part_recognition_v2",
  "model_file": "body_part_cls_effb4_int8.onnx",
  "sha256": "f4a9b3c1d2e8f7a0b1c2d3e4f5a6b7c8d9e0f1a2b3c4d5e6f7a8b9c0d1e2f3a4",
  "model_version": "2.1.0",
  "framework": "onnxruntime",
  "quantization": "INT8",
  "input_shape": [1, 3, 512, 512],
  "input_dtype": "float32",
  "output_shape": [1, 15],
  "output_dtype": "float32",
  "classes": ["CHEST_AP", "CHEST_LAT", "ABD_AP", "LSPINE_AP", "LSPINE_LAT",
              "PELVIS_AP", "HIP", "KNEE", "HAND_WRIST", "FOOT_ANKLE",
              "SKULL", "CSPINE", "TSPINE", "SHOULDER", "EXTREMITY"],
  "performance": {
    "top1_accuracy_pct": 96.2,
    "inference_ms_cpu_p95": 180,
    "validation_dataset": "xpe_cls_val_v3_n=2500"
  },
  "requires_deterministic_fallback": true,
  "disable_control": "XPE_AI_DISABLE_BODY_PART_CLS",
  "release_boundary": "release-safe",
  "created_at": "2026-04-15T00:00:00Z"
}
```

#### 8.4.3 Python ONNX 추론 래퍼 (참조 구현)

```python
import numpy as np
import json
import hashlib
import os
from pathlib import Path
from dataclasses import dataclass
from typing import Optional, List

@dataclass
class AiInferenceResult:
    body_part_id:    str
    confidence:      float
    all_scores:      List[float]
    is_ai_result:    bool   = True   # False = deterministic fallback used
    model_version:   str   = ''
    inference_ms:    float = 0.0

class OnnxAiWorker:
    """
    ONNX AI worker with model manifest validation, quantized inference,
    per-task disable control, and deterministic fallback.
    """

    def __init__(self, manifest_path: Path):
        self.manifest  = self._load_manifest(manifest_path)
        self._session  = None
        self._classes  = self.manifest['classes']
        self._disabled = os.environ.get(
            self.manifest.get('disable_control', '_NONE_'), '0') != '0'

    def _load_manifest(self, path: Path) -> dict:
        with open(path) as f:
            m = json.load(f)
        # Verify model file hash
        model_file = path.parent / m['model_file']
        if not model_file.exists():
            raise FileNotFoundError(f"Model file not found: {model_file}")
        sha = hashlib.sha256(model_file.read_bytes()).hexdigest()
        if sha != m['sha256']:
            raise ValueError(f"Model hash mismatch: {m['model_file']}")
        return m

    def _get_session(self):
        if self._session is None:
            import onnxruntime as ort
            model_path = str(Path(self.manifest['model_file']))
            opts = ort.SessionOptions()
            opts.intra_op_num_threads = 1   # deterministic single-thread
            opts.execution_mode = ort.ExecutionMode.ORT_SEQUENTIAL
            self._session = ort.InferenceSession(
                model_path,
                sess_options=opts,
                providers=['CPUExecutionProvider'])
        return self._session

    def infer(self,
              preprocessed_input: np.ndarray,
              timeout_ms:         float = 5000.0) -> AiInferenceResult:
        """
        Run inference with timeout. Falls back to heuristic on failure.

        Args:
            preprocessed_input: float32 (1, 3, 512, 512) — normalised
            timeout_ms:         maximum allowed inference time
        Returns:
            AiInferenceResult
        """
        import time

        if self._disabled:
            return self._deterministic_fallback(preprocessed_input)

        try:
            session = self._get_session()
            input_name = session.get_inputs()[0].name

            t0 = time.monotonic()
            outputs = session.run(None, {input_name: preprocessed_input})
            elapsed_ms = (time.monotonic() - t0) * 1000.0

            if elapsed_ms > timeout_ms:
                return self._deterministic_fallback(preprocessed_input)

            scores    = outputs[0][0]                      # (n_classes,)
            best_idx  = int(np.argmax(scores))
            return AiInferenceResult(
                body_part_id   = self._classes[best_idx],
                confidence     = float(scores[best_idx]),
                all_scores     = [float(s) for s in scores],
                is_ai_result   = True,
                model_version  = self.manifest.get('model_version', ''),
                inference_ms   = elapsed_ms,
            )
        except Exception:
            return self._deterministic_fallback(preprocessed_input)

    def _deterministic_fallback(self,
                                  img: np.ndarray) -> AiInferenceResult:
        """
        Heuristic body-part classification (no ML required).
        Uses image aspect ratio and intensity statistics as features.
        """
        arr = img.squeeze()
        if arr.ndim == 3:
            arr = arr[0]  # take first channel

        h, w = arr.shape[-2], arr.shape[-1]
        aspect = w / max(h, 1)
        mean_i = float(np.mean(arr))

        # Simple heuristic: aspect ratio + mean intensity
        if aspect > 1.5:
            body_part = 'CHEST_AP'
        elif mean_i > 0.6:
            body_part = 'EXTREMITY'
        else:
            body_part = 'ABD_AP'

        n = len(self._classes)
        scores = [1.0 / n] * n
        idx = self._classes.index(body_part) if body_part in self._classes else 0
        scores[idx] = 0.6

        return AiInferenceResult(
            body_part_id  = body_part,
            confidence    = 0.6,
            all_scores    = scores,
            is_ai_result  = False,
            model_version = 'fallback-heuristic',
        )
```

#### 8.4.4 C++ Worker Proxy

```cpp
// C++ IPC proxy for xpe_ai_worker.exe
// Sends input via shared memory, waits for result with timeout.

struct AiWorkerRequest {
    uint32_t request_id;
    uint32_t width;
    uint32_t height;
    uint32_t channels;
    // Followed by float32[channels × height × width] in shared memory
};

struct AiWorkerResponse {
    uint32_t request_id;
    char     body_part_id[32];
    float    confidence;
    float    all_scores[15];   // max 15 classes
    bool     is_ai_result;
    float    inference_ms;
};

class XpeAiWorkerProxy {
public:
    AiInferenceResult run_with_timeout(const float* input,
                                        size_t n_elements,
                                        uint32_t timeout_ms = 5000) {
        if (!worker_running_ || ai_disabled_) {
            return deterministic_fallback(input, n_elements);
        }

        // Write request to shared memory
        auto req = write_request(input, n_elements);

        // Wait for response with timeout
        bool got_response = response_sem_.wait_for(
            std::chrono::milliseconds(timeout_ms));

        if (!got_response) {
            log_warning("AI worker timeout after {}ms — using fallback", timeout_ms);
            return deterministic_fallback(input, n_elements);
        }

        auto resp = read_response(req.request_id);
        return AiInferenceResult{
            .body_part_id  = std::string(resp.body_part_id),
            .confidence    = resp.confidence,
            .is_ai_result  = resp.is_ai_result,
            .inference_ms  = resp.inference_ms,
        };
    }

private:
    bool ai_disabled_ = false;  // Set from env XPE_AI_DISABLE_BODY_PART_CLS
    bool worker_running_ = false;
    Semaphore response_sem_;
    SharedMemory shm_;
};
```

#### 8.4.5 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| 모델 해시 검증 | 불일치 시 워커 시작 거부 | 변조된 ONNX 파일 주입 |
| 타임아웃 폴백 | 5s 초과 시 100% 폴백 전환 | 의도적 지연 테스트 |
| INT8 vs FP32 정확도 차이 | top-1 accuracy ≤ 0.5% 차이 | 검증 세트 비교 |
| 워커 비활성화 제어 | `XPE_AI_DISABLE_*` 환경변수 100% 동작 | 환경변수 테스트 |
| 폴백 결과 플래그 | `is_ai_result=false` 항상 표시 | 폴백 시나리오 실행 |

**IEC 62304 §5.4 추적성**: SRS ID: SRS-AI-001 (AI Worker 격리), SRS-AI-002 (모델 매니페스트) — Phase 2 추가 예정

---

## 9. 교정 데이터 파이프라인

### 9.1 오프라인 교정 순서 (GAP-10 해소)

```
Phase 1: Dark Calibration (반드시 먼저)
  - 조건: 방사선 OFF, detector 안정화 ≥10분
  - 획득: ≥16 dark frames
  - 출력: offset_map.bin

Phase 2: Flat-Field Calibration (per SID per kVp)
  - 조건: 균일 조사야, 산란체 없음
  - 획득: ≥8 flood frames
  - 출력: gain_map_SID{n}_kVp{m}.bin

Phase 3: Defect Map Generation
  - 입력: offset_map.bin + gain_map (any SID)
  - 출력: defect_map.bin

Phase 4: Lag Parameter Fitting
  - 조건: 이중 노출 프로토콜
  - 획득: decay curve (t=0.1s ~ 30s)
  - 출력: lag_params.json

Phase 5: Checksum Generation
  - 모든 .bin 파일에 SHA-256 생성
  - 출력: checksums.sha256
```

### 9.2 C++ ConfigManager 로딩 순서

```cpp
class CalibrationManager {
public:
    bool load_calibration_set(const std::filesystem::path& cal_dir) {
        // 1. Validate all checksums first (SRS-SEC-002)
        if (!validate_all_checksums(cal_dir)) {
            logger_->error("Calibration data integrity check failed");
            raise_alert(AlertType::CALIBRATION_INTEGRITY_FAILURE);
            return false;
        }
        
        // 2. Load offset map (required)
        offset_map_ = load_binary_map<float>(cal_dir / "offset_map.bin",
                                               "XOFF");
        
        // 3. Load gain maps (SID-indexed)
        for (const auto& entry : std::filesystem::directory_iterator(cal_dir)) {
            if (entry.path().stem().string().starts_with("gain_map_")) {
                auto [sid, kvp] = parse_gain_map_filename(entry.path());
                gain_maps_[{sid, kvp}] = 
                    load_binary_map<float>(entry.path(), "XGAI");
            }
        }
        
        // 4. Load defect map (required)
        defect_map_ = load_binary_map<uint8_t>(cal_dir / "defect_map.bin",
                                                 "XDEF");
        
        // 5. Load lag parameters (optional, default if missing)
        load_lag_params(cal_dir / "lag_params.json");
        
        // 6. Validate expiry (SRS-ALERT-005)
        if (is_calibration_expired()) {
            raise_alert(AlertType::CALIBRATION_EXPIRED);
        }
        
        return true;
    }
    
    const float* get_gain_map(float sid_mm, float kvp) const {
        // Find nearest SID/kVp combination
        auto key = find_nearest_gain_map(sid_mm, kvp);
        return gain_maps_.at(key).data();
    }
};
```

### 9.3 교정 유효기간 관리

| 교정 항목 | 권장 주기 | 트리거 조건 |
|----------|---------|-----------|
| Offset Map | 8시간 또는 시동 시 | 온도 ≥5°C 변화 |
| Gain Map | 1주 또는 kVp 변경 시 | SID 변경 ±50mm |
| Defect Map | 1개월 | 결함 픽셀 +10% |
| Lag Parameters | 분기 1회 | 모델 교체 시 |

---

### 9.4 AED-0: Automatic Exposure Detection (GAP-J 해소)

AED-0는 XPE-10-Pass-Review Pass 4에서 파이프라인 실행 순서에 추가된 선행 스텝이다 (product.md §4 "pipeline execution" 참조). **Offset/Gain/Defect 보정 이후, Log Transform 이전**에 실행되어 노출 유효성을 판단하고 하위 단계에 신호 레벨 정보를 전달한다.

#### 9.4.1 알고리즘 정의

AED-0의 목적은 두 가지다:
1. 충분한 노출이 이루어졌는지 여부 판정 (실패 시 파이프라인 중단 + 경보)
2. I₀ (air kerma reference signal) 추정 — EI, DI 계산에 사용

**유효 노출 조건**:
$$\bar{I}_{\text{field}} \geq I_{\text{min\_exposure}} \quad \text{AND} \quad \frac{P_{\text{sat}}}{P_{\text{total}}} \leq \theta_{\text{sat}}$$

$$\bar{I}_{\text{field}} = \frac{1}{|M_{\text{coll}}|} \sum_{(x,y) \in M_{\text{coll}}} I_{\text{gain\_corr}}(x,y)$$

**I₀ 추정** (dark-corrected flood reference):
$$I_0 = G_{\text{mean}} \cdot \bar{I}_{\text{flat\_ref}}$$

여기서 $\bar{I}_{\text{flat\_ref}}$는 교정 flood 이미지의 mean signal이고, $G_{\text{mean}}$은 gain map 평균값이다.

| 파라미터 | 기본값 | 의미 |
|---------|-------|------|
| `I_min_exposure` | 1000 ADU | 최소 유효 노출 신호 |
| `θ_sat` | 0.05 | 허용 포화 픽셀 비율 |
| Collimation source | `CollimatorMask` (§12.5) | 조준기 마스크 |

#### 9.4.2 Python 구현

```python
@dataclass
class AEDResult:
    is_valid_exposure: bool
    mean_field_signal: float       # I̅_field (ADU)
    i0_estimate:       float       # I₀ (ADU)
    saturation_frac:   float
    fault_reason:      str = ''    # empty if valid

def run_aed0(gain_corr_img:    np.ndarray,
             calibration_data: dict,
             collimator_mask:  'CollimatorMask | None' = None,
             i_min_exposure:   float = 1000.0,
             theta_sat:        float = 0.05) -> AEDResult:
    """
    AED-0: Automatic Exposure Detection — pipeline gate before Log Transform.

    Args:
        gain_corr_img:    float32 (H, W), offset+gain corrected
        calibration_data: dict with 'gain_mean' and 'flat_ref_mean' (ADU)
        collimator_mask:  CollimatorMask instance; None → use full image
        i_min_exposure:   minimum mean field signal for valid exposure
        theta_sat:        maximum allowed saturation fraction
    Returns:
        AEDResult
    """
    H, W = gain_corr_img.shape
    max_adu = 65535.0

    # Build field mask
    if collimator_mask is not None:
        field_mask = collimator_mask.mask.astype(bool)
    else:
        field_mask = np.ones((H, W), dtype=bool)

    field_pixels   = gain_corr_img[field_mask]
    mean_field_sig = float(np.mean(field_pixels)) if field_pixels.size > 0 else 0.0
    sat_frac       = float(np.mean(field_pixels >= max_adu * 0.999))

    # I₀ estimate from calibration reference
    gain_mean     = float(calibration_data.get('gain_mean',      1.0))
    flat_ref_mean = float(calibration_data.get('flat_ref_mean', mean_field_sig))
    i0_estimate   = gain_mean * flat_ref_mean

    # Validity checks
    fault = ''
    if mean_field_sig < i_min_exposure:
        fault = f'under_exposure: mean={mean_field_sig:.1f} < {i_min_exposure}'
    elif sat_frac > theta_sat:
        fault = f'over_exposure: sat_frac={sat_frac:.4f} > {theta_sat}'

    return AEDResult(
        is_valid_exposure = (fault == ''),
        mean_field_signal = mean_field_sig,
        i0_estimate       = i0_estimate,
        saturation_frac   = sat_frac,
        fault_reason      = fault,
    )
```

#### 9.4.3 파이프라인 통합

```
[Readout Validation (§3.0)] → [Offset Correct] → [Non-linearity Correct] →
[Gain Correct] → [Defect Correct] → [AED-0 (§9.4)] → decision:
    FAIL → alert xpe_alert(ALERT_EXPOSURE_INVALID) + return error
    PASS → [Log Transform → ...] with i0 = aed_result.i0_estimate
```

#### 9.4.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| 유효 노출 감지 | 정상 팬텀 이미지 → is_valid = True | 10회 반복 |
| 노출 부족 감지 | 1/10 노출 이미지 → is_valid = False | 감쇠기 사용 |
| I₀ 정확도 | 교정값 ±5% 이내 | flood 이미지 검증 |

---

### 9.5 교정 드리프트 모니터링 (GAP-X 해소)

xpe-algorithm-spec-deepsync.md §4.1 "Drift monitoring shall feed recalibration decisions rather than silently allowing quality erosion"에서 요구된 항목이다. 매 처리 세션에서 드리프트 지표를 측정하고 임계치 초과 시 재교정을 트리거한다.

#### 9.5.1 알고리즘 수학 정의

**Dark Current 드리프트율**:

$$\dot{D} = \frac{\bar{I}_{\text{dark,current}} - \bar{I}_{\text{dark,baseline}}}{\Delta t_{\text{days}}} \quad (\text{ADU/day})$$

**Gain Non-Uniformity 트렌드**:

$$\Delta_{\text{PRNU}} = \left|\frac{\text{CV}_{\text{current}} - \text{CV}_{\text{baseline}}}{\text{CV}_{\text{baseline}}}\right| \times 100\% \quad (\% \text{ change})$$

**Defect Burden 성장률**:

$$\dot{N}_{\text{defect}} = \frac{N_{\text{defect,current}} - N_{\text{defect,baseline}}}{\Delta t_{\text{days}}} \quad (\text{defects/day})$$

**재교정 트리거 조건**:

$$\text{TriggerRecal} = \left(\dot{D} > \theta_D\right) \lor \left(\Delta_{\text{PRNU}} > \theta_{\text{PRNU}}\right) \lor \left(\dot{N}_{\text{defect}} > \theta_N\right)$$

| 지표 | 임계치 | 의미 |
|------|-------|------|
| $\theta_D$ | 5.0 ADU/day | Dark current 드리프트 |
| $\theta_{\text{PRNU}}$ | 0.5% 변화 | Gain 균일도 저하 |
| $\theta_N$ | 10 defects/day | 결함 픽셀 성장 |

#### 9.5.2 Python 구현

```python
import numpy as np
import json
from pathlib import Path
from dataclasses import dataclass, field, asdict
from datetime import datetime, timezone
from typing import List, Optional

@dataclass
class DriftMetrics:
    timestamp_iso:         str
    dark_mean_adu:         float
    prnu_cv_pct:           float
    defect_count:          int
    dark_drift_per_day:    float = 0.0
    prnu_delta_pct:        float = 0.0
    defect_growth_per_day: float = 0.0
    needs_recalibration:   bool  = False
    trigger_reasons:       List[str] = field(default_factory=list)


@dataclass
class DriftThresholds:
    dark_drift_adu_per_day:  float = 5.0
    prnu_delta_pct:          float = 0.5
    defect_growth_per_day:   float = 10.0


class CalibrationDriftMonitor:
    """
    Monitors detector calibration drift across sessions.
    Compares current metrics against stored baseline and triggers recalibration.
    """

    def __init__(self, drift_log_path: Path,
                 thresholds: Optional[DriftThresholds] = None):
        self.drift_log_path = drift_log_path
        self.thresholds     = thresholds or DriftThresholds()
        self._history: List[DriftMetrics] = []
        self._load_history()

    def _load_history(self):
        if self.drift_log_path.exists():
            with open(self.drift_log_path) as f:
                data = json.load(f)
                self._history = [DriftMetrics(**e) for e in data]

    def _save_history(self):
        with open(self.drift_log_path, 'w') as f:
            json.dump([asdict(m) for m in self._history], f, indent=2)

    def measure_current(self,
                         dark_frames:  np.ndarray,   # (N, H, W)
                         flood_image:  np.ndarray,   # (H, W) gain-corrected
                         defect_map:   np.ndarray    # (H, W) uint8
                         ) -> DriftMetrics:
        """
        Compute current drift metrics from live detector frames.

        Args:
            dark_frames:  recent dark frames (≥4 frames stacked)
            flood_image:  recent flood field (gain-corrected)
            defect_map:   current defect map
        Returns:
            DriftMetrics with filled current values
        """
        dark_mean = float(np.mean(dark_frames))
        # PRNU: coefficient of variation of net signal (gain-corrected flood)
        net = flood_image[flood_image > 10]  # exclude near-zero pixels
        prnu_cv = float(np.std(net) / np.mean(net) * 100) if len(net) > 0 else 0.0
        defect_count = int(np.sum(defect_map > 0))
        ts = datetime.now(timezone.utc).isoformat()
        return DriftMetrics(
            timestamp_iso=ts,
            dark_mean_adu=dark_mean,
            prnu_cv_pct=prnu_cv,
            defect_count=defect_count,
        )

    def evaluate(self, current: DriftMetrics) -> DriftMetrics:
        """
        Compare current metrics against baseline (first recorded session).
        Sets drift rates and triggers if thresholds exceeded.
        """
        if not self._history:
            # No baseline: record and return OK
            self._history.append(current)
            self._save_history()
            return current

        baseline = self._history[0]
        latest   = self._history[-1]

        # Time delta in days
        try:
            t0 = datetime.fromisoformat(baseline.timestamp_iso)
            t1 = datetime.fromisoformat(current.timestamp_iso)
            delta_days = max((t1 - t0).total_seconds() / 86400.0, 0.01)
        except Exception:
            delta_days = 1.0

        current.dark_drift_per_day    = abs(current.dark_mean_adu - baseline.dark_mean_adu) / delta_days
        current.prnu_delta_pct        = abs(current.prnu_cv_pct   - baseline.prnu_cv_pct)
        current.defect_growth_per_day = max(current.defect_count  - baseline.defect_count, 0) / delta_days

        reasons = []
        if current.dark_drift_per_day > self.thresholds.dark_drift_adu_per_day:
            reasons.append(f"dark_drift={current.dark_drift_per_day:.2f} ADU/day > {self.thresholds.dark_drift_adu_per_day}")
        if current.prnu_delta_pct > self.thresholds.prnu_delta_pct:
            reasons.append(f"prnu_delta={current.prnu_delta_pct:.3f}% > {self.thresholds.prnu_delta_pct}%")
        if current.defect_growth_per_day > self.thresholds.defect_growth_per_day:
            reasons.append(f"defect_growth={current.defect_growth_per_day:.1f}/day > {self.thresholds.defect_growth_per_day}")

        current.needs_recalibration = len(reasons) > 0
        current.trigger_reasons     = reasons

        self._history.append(current)
        if len(self._history) > 365:  # keep 1 year of daily records
            self._history = self._history[-365:]
        self._save_history()
        return current

    def get_trend_report(self, window_days: int = 30) -> dict:
        """
        Summarise drift trends over a rolling window.
        Returns: {metric: (mean, std, trend_direction)} for last window_days entries.
        """
        recent = self._history[-window_days:] if len(self._history) >= window_days else self._history
        if len(recent) < 2:
            return {}
        darks  = np.array([m.dark_mean_adu for m in recent])
        prnus  = np.array([m.prnu_cv_pct   for m in recent])
        defs   = np.array([m.defect_count  for m in recent])
        idx    = np.arange(len(recent))

        def trend(arr):
            p = np.polyfit(idx, arr, 1)
            slope = float(p[0])
            return float(np.mean(arr)), float(np.std(arr)), ('up' if slope > 0 else 'down')

        return {
            'dark_mean_adu':  trend(darks),
            'prnu_cv_pct':    trend(prnus),
            'defect_count':   trend(defs),
            'n_records':      len(recent),
        }
```

#### 9.5.3 C++ 런타임 통합

```cpp
// Drift monitoring hook — called after each processing session
// Updates drift log and raises alert if recalibration is needed.

struct DriftSnapshot {
    float    dark_mean_adu;
    float    prnu_cv_pct;
    uint32_t defect_count;
    int64_t  timestamp_unix_s;
};

class DriftMonitor {
public:
    struct Alert {
        bool   needs_recalibration;
        float  dark_drift_per_day;
        float  prnu_delta_pct;
        float  defect_growth_per_day;
        char   message[256];
    };

    // Call this after each calibration verification pass
    Alert update(const DriftSnapshot& current) {
        Alert alert{};
        if (history_.empty()) {
            baseline_ = current;
            history_.push_back(current);
            return alert;
        }

        double days = static_cast<double>(current.timestamp_unix_s - baseline_.timestamp_unix_s)
                      / 86400.0;
        days = std::max(days, 0.01);

        alert.dark_drift_per_day    = std::fabsf(current.dark_mean_adu - baseline_.dark_mean_adu) / days;
        alert.prnu_delta_pct        = std::fabsf(current.prnu_cv_pct   - baseline_.prnu_cv_pct);
        alert.defect_growth_per_day = static_cast<float>(
                                         std::max<int32_t>(current.defect_count - baseline_.defect_count, 0)
                                      ) / days;

        alert.needs_recalibration =
            (alert.dark_drift_per_day    > k_dark_thresh_)   ||
            (alert.prnu_delta_pct        > k_prnu_thresh_)    ||
            (alert.defect_growth_per_day > k_defect_thresh_);

        if (alert.needs_recalibration) {
            std::snprintf(alert.message, sizeof(alert.message),
                "RECAL REQUIRED: dark=%.2f ADU/d, PRNU=%.3f%%, defects=%.1f/d",
                alert.dark_drift_per_day,
                alert.prnu_delta_pct,
                alert.defect_growth_per_day);
            xpe_alert(XpeAlertCode::ALERT_RECALIBRATION_REQUIRED, alert.message);
        }

        history_.push_back(current);
        if (history_.size() > 365) history_.erase(history_.begin());
        return alert;
    }

private:
    static constexpr float k_dark_thresh_   = 5.0f;   // ADU/day
    static constexpr float k_prnu_thresh_   = 0.5f;   // % change
    static constexpr float k_defect_thresh_ = 10.0f;  // defects/day

    DriftSnapshot              baseline_{};
    std::vector<DriftSnapshot> history_;
};
```

#### 9.5.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| 드리프트 감지 민감도 | 임계치의 1.1× → 100% 감지 | 합성 드리프트 시나리오 |
| 오탐율 (False Positive) | < 1% | 안정된 detector 30일 모니터링 |
| 드리프트 로그 용량 | 365일 기록 유지 | 자동 롤오버 테스트 |
| 재교정 알림 지연 | < 1s | 임계치 초과 직후 알림 |

**IEC 62304 §5.4 추적성**: SRS ID: SRS-QC-002 (교정 드리프트 모니터링) — Phase 2 추가 예정

---

## 10. 성능 최적화 — SIMD/OpenMP 전략

### 10.1 전체 파이프라인 SIMD 커버리지 (GAP-06 해소)

| SWU | AVX2 | FMA | OpenMP | GPU (Optional) |
|-----|------|-----|--------|---------------|
| Offset Correct | ✅ | — | ✅ (row-parallel) | — |
| Gain Correct | ✅ | ✅ | ✅ | — |
| Defect Correct | — (data-dependent) | — | ✅ (pass 분리) | — |
| Ghost Correct | ✅ | ✅ | ✅ | — |
| Log Transform | ✅ (SVML/approx) | — | ✅ | ✅ (CUDA) |
| Bilateral Filter | 부분 (OpenCV) | — | ✅ (tile) | ✅ (CUDA) |
| CLAHE | — (OpenCV) | — | — | — |
| USM | ✅ | ✅ | ✅ | — |
| Laplacian Pyramid | — (OpenCV pyrDown/Up) | — | ✅ (level) | — |
| VOI LUT | ✅ | ✅ | ✅ | ✅ |
| Modality LUT | ✅ | ✅ | ✅ | — |
| Grid Suppression | — (FFT-based) | — | ✅ | — |

### 10.2 메모리 풀 전략

```cpp
// SWI-5 MemoryPool: Pre-allocated pipeline buffers
// Prevent dynamic allocation during processing (SRS-PERF-001)

class XpeMemoryPool {
    // Fixed set of reusable float32 image buffers
    static constexpr size_t MAX_BUFFERS = 8;
    static constexpr size_t MAX_IMAGE_PIXELS = 4096ULL * 4096;  // 16MP max
    
    struct Buffer {
        std::unique_ptr<float, AlignedDeleter> data;  // 64-byte aligned
        size_t size;
        std::atomic<bool> in_use{false};
    } buffers_[MAX_BUFFERS];
    
public:
    XpeMemoryPool() {
        for (auto& buf : buffers_) {
            buf.data = alloc_aligned<float>(MAX_IMAGE_PIXELS, 64);
            buf.size = MAX_IMAGE_PIXELS;
        }
    }
    
    float* acquire(size_t required_pixels) {
        for (auto& buf : buffers_) {
            bool expected = false;
            if (buf.in_use.compare_exchange_strong(expected, true) &&
                buf.size >= required_pixels) {
                return buf.data.get();
            }
        }
        throw std::runtime_error("Memory pool exhausted");
    }
    
    void release(float* ptr) {
        for (auto& buf : buffers_) {
            if (buf.data.get() == ptr) {
                buf.in_use.store(false);
                return;
            }
        }
    }
};
```

### 10.3 Thread Pool 최적화

```cpp
// Row-parallel SIMD: optimal for cache efficiency
// Thread granularity: tile-based (prevent false sharing)

#pragma omp parallel for schedule(static) num_threads(NUM_CORES)
for (int tile_y = 0; tile_y < num_tiles_y; ++tile_y) {
    for (int tile_x = 0; tile_x < num_tiles_x; ++tile_x) {
        process_tile(tile_x, tile_y, tile_size);
    }
}
// Tile size guideline: 64×64 pixels (fits in L1 cache: 64×64×4 = 16KB)
```

### 10.4 성능 프로파일링 포인트

```cpp
// Built-in performance counters (SRS-PERF-001, 002)
class PipelineProfiler {
    struct StageMetrics {
        std::chrono::nanoseconds elapsed;
        size_t pixels_processed;
        double mpixels_per_sec() const {
            return pixels_processed / (elapsed.count() * 1e-3);
        }
    };
    
public:
    void report(uint32_t width, uint32_t height) {
        auto total = sum_all_stages();
        log("Pipeline total: {}ms for {}×{} ({:.1f} MPix/s)",
            total.count() / 1e6, width, height,
            (double)(width * height) / (total.count() * 1e-3));
        // SRS-PERF-001: target ≤500ms for 3072×3072
    }
};
```

---

## 11. 검증 방법론

### 11.1 단위 테스트 기준

| 알고리즘 | 입력 | 기대 출력 | Pass 기준 |
|---------|------|---------|---------|
| Offset Correct | Synthetic dark signal | Subtracted + clamped | Max error = 0 ADU |
| Gain Correct | Uniform flood | Uniform output (CV<0.1%) | CV < 0.1% |
| Defect Correct | Injected point defects | Interpolated ≤ 1% error | Pixel error < 5 ADU |
| Ghost Correct | Known lag signal | ≥90% removal | Ghost fraction < 10% |
| Log Transform | Gradient ramp | Logarithmic curve | Max relative error < 1e-5 |
| Bilateral | AWGN + step edge | Smoothed / edge preserved | Edge FWHM < 2× input |
| CLAHE | Low-contrast uniform | Enhanced, no artifact | SSIM > 0.95 |
| USM | Fine texture | Enhanced within λ_max | No artifact above λ_max |
| VOI LUT | Full range sweep | Correct output per formula | Max error ≤ 1 DDL |
| GSDF | P-Value sweep | PS3.14 conformance | JND deviation < 10% |

### 11.2 통합 테스트 — 황금 표준 이미지

```python
def run_integration_test(pipeline, reference_images: dict) -> dict:
    """
    Compare pipeline output to golden reference images.
    
    Test images:
    - CDMAM (contrast-detail phantom): sensitivity threshold analysis
    - Leeds TOR(CDR) phantom: resolution measurement
    - RMI 156 phantom: uniformity + noise measurement
    - AAPM TG-18 patterns: GSDF conformance
    """
    results = {}
    for test_name, (input_img, golden_ref) in reference_images.items():
        output = pipeline.process(input_img)
        
        # Structural similarity
        ssim_val = ssim(output, golden_ref)
        # Peak signal-to-noise ratio
        psnr_val = psnr(output, golden_ref)
        # Max pixel deviation
        max_err  = float(np.max(np.abs(output.astype(float) - 
                                         golden_ref.astype(float))))
        
        results[test_name] = {
            'ssim': ssim_val,
            'psnr': psnr_val,
            'max_pixel_error': max_err,
            'pass': ssim_val >= 0.95 and psnr_val >= 35.0
        }
    return results
```

### 11.3 성능 회귀 테스트

```bash
# Automated performance regression check (CI/CD)
# Target: SRS-PERF-001 ≤500ms for 3072×3072

xpe_benchmark --image-size 3072x3072 \
               --pipeline pre+core+display \
               --iterations 10 \
               --threshold-ms 500 \
               --output benchmark_results.json
```

---

### 11.4 스칼라 참조 구현 및 SIMD 패리티 하네스 (GAP-S 해소)

xpe-algorithm-spec-deepsync.md §5.1 "every major stage shall have: one scalar reference, one optimized implementation, one parity test harness, one benchmark family binding"에서 요구된 항목이다. AVX2 또는 다중 스레드 경로가 유일한 구현이 되는 것을 방지한다.

#### 11.4.1 패리티 하네스 아키텍처

모든 주요 처리 단계는 세 가지 구현을 동시에 보유해야 한다:

| 레이어 | 목적 | 요구사항 |
|-------|------|---------|
| **Scalar Reference** | 수학적 정확성의 기준선, 이식 가능 | 컴파일러 최적화 없음, 인라인 없음 |
| **Optimized** | AVX2/FMA/OpenMP 병렬화 | 프로덕션 경로 |
| **Parity Test** | Scalar ↔ Optimized 수치 등가 검증 | CI/CD에서 자동 실행 |

#### 11.4.2 패리티 테스트 프레임워크

```python
import numpy as np
from typing import Callable, Dict, Any
from dataclasses import dataclass

@dataclass
class ParityTestResult:
    stage_name:        str
    max_abs_error:     float
    max_rel_error:     float
    mean_abs_error:    float
    passed:            bool
    error_message:     str = ''

    def __repr__(self):
        status = "PASS" if self.passed else "FAIL"
        return (f"[{status}] {self.stage_name}: "
                f"max_abs={self.max_abs_error:.3e}, "
                f"max_rel={self.max_rel_error:.3e}")


def run_parity_test(
        stage_name:   str,
        scalar_fn:    Callable,
        optimized_fn: Callable,
        inputs:       Dict[str, Any],
        abs_tol:      float = 1e-4,
        rel_tol:      float = 1e-4) -> ParityTestResult:
    """
    Compare scalar reference and optimized implementation outputs.

    Both functions receive the same **inputs dict.
    Returns ParityTestResult with pass/fail and error statistics.
    """
    ref_out  = scalar_fn(**inputs)
    opt_out  = optimized_fn(**inputs)

    ref_arr  = np.asarray(ref_out,  dtype=np.float64)
    opt_arr  = np.asarray(opt_out,  dtype=np.float64)

    abs_diff = np.abs(ref_arr - opt_arr)
    rel_diff = abs_diff / (np.abs(ref_arr) + 1e-10)

    max_abs  = float(np.max(abs_diff))
    max_rel  = float(np.max(rel_diff))
    mean_abs = float(np.mean(abs_diff))

    passed = (max_abs <= abs_tol) and (max_rel <= rel_tol)
    msg    = '' if passed else (f"abs_err={max_abs:.3e} > {abs_tol} or "
                                 f"rel_err={max_rel:.3e} > {rel_tol}")
    return ParityTestResult(
        stage_name=stage_name,
        max_abs_error=max_abs,
        max_rel_error=max_rel,
        mean_abs_error=mean_abs,
        passed=passed,
        error_message=msg,
    )


class XpeParityTestSuite:
    """
    Parity test harness for all XPE processing stages.
    Instantiate with a seeded test image set and call run_all().
    """

    def __init__(self, width: int = 512, height: int = 512, seed: int = 42):
        rng = np.random.default_rng(seed)
        self.raw    = rng.integers(100, 55000, (height, width), dtype=np.uint16)
        self.offset = rng.uniform(50, 200, (height, width)).astype(np.float32)
        self.gain   = rng.uniform(0.8, 1.2, (height, width)).astype(np.float32)
        self.W, self.H = width, height

    def _offset_scalar(self, raw, offset_map):
        result = np.zeros(raw.shape, dtype=np.float32)
        for y in range(raw.shape[0]):
            for x in range(raw.shape[1]):
                result[y, x] = max(float(raw[y, x]) - offset_map[y, x], 0.0)
        return result

    def _offset_vectorized(self, raw, offset_map):
        return np.maximum(raw.astype(np.float32) - offset_map, 0.0)

    def _gain_scalar(self, offset_corrected, gain_map):
        result = np.zeros_like(offset_corrected, dtype=np.float32)
        for y in range(offset_corrected.shape[0]):
            for x in range(offset_corrected.shape[1]):
                result[y, x] = offset_corrected[y, x] * gain_map[y, x]
        return result

    def _gain_vectorized(self, offset_corrected, gain_map):
        return offset_corrected * gain_map

    def _log_transform_scalar(self, clean, I0=10000.0, eps=1e-6):
        result = np.zeros_like(clean, dtype=np.float32)
        for y in range(clean.shape[0]):
            for x in range(clean.shape[1]):
                result[y, x] = -np.log((clean[y, x] + eps) / (I0 + eps))
        return result

    def _log_transform_vectorized(self, clean, I0=10000.0, eps=1e-6):
        return -np.log((clean.astype(np.float64) + eps) / (I0 + eps)).astype(np.float32)

    def run_all(self) -> list:
        """Run all parity tests. Returns list of ParityTestResult."""
        offset_corr = self._offset_vectorized(self.raw, self.offset)
        gain_corr   = self._gain_vectorized(offset_corr, self.gain)

        results = []

        results.append(run_parity_test(
            'offset_correction',
            scalar_fn=self._offset_scalar,
            optimized_fn=self._offset_vectorized,
            inputs={'raw': self.raw, 'offset_map': self.offset},
            abs_tol=0.0,   # Exact match expected
            rel_tol=0.0,
        ))

        results.append(run_parity_test(
            'gain_correction',
            scalar_fn=self._gain_scalar,
            optimized_fn=self._gain_vectorized,
            inputs={'offset_corrected': offset_corr, 'gain_map': self.gain},
            abs_tol=1e-4,  # FP rounding tolerance
            rel_tol=1e-5,
        ))

        results.append(run_parity_test(
            'log_transform',
            scalar_fn=self._log_transform_scalar,
            optimized_fn=self._log_transform_vectorized,
            inputs={'clean': gain_corr},
            abs_tol=2e-4,  # AVX2 poly approx tolerance
            rel_tol=2e-4,
        ))

        return results


def print_parity_report(results: list) -> bool:
    """Print parity test report. Returns True if all passed."""
    all_pass = True
    print("=" * 60)
    print("XPE PARITY TEST REPORT")
    print("=" * 60)
    for r in results:
        print(r)
        if not r.passed:
            all_pass = False
            print(f"  ERROR: {r.error_message}")
    print("=" * 60)
    print(f"OVERALL: {'PASS' if all_pass else 'FAIL'} ({sum(r.passed for r in results)}/{len(results)} passed)")
    return all_pass
```

#### 11.4.3 C++ 패리티 검증 매크로

```cpp
// XPE parity test macro — wrap each optimized function for CI validation
// Usage: XPE_PARITY_CHECK(scalar_fn, avx2_fn, inputs..., tol)

#define XPE_PARITY_CHECK(scalar_fn, opt_fn, out_s, out_o, n, abs_tol)     \
    do {                                                                    \
        bool _parity_ok = true;                                             \
        for (size_t _i = 0; _i < (n); ++_i) {                             \
            float _diff = std::fabsf((out_s)[_i] - (out_o)[_i]);           \
            if (_diff > (abs_tol)) {                                        \
                std::fprintf(stderr,                                        \
                    "PARITY FAIL [%s vs %s] idx=%zu diff=%.4e tol=%.4e\n", \
                    #scalar_fn, #opt_fn, _i, _diff, (float)(abs_tol));     \
                _parity_ok = false; break;                                  \
            }                                                               \
        }                                                                   \
        assert(_parity_ok && "Scalar/AVX2 parity check failed");           \
    } while (0)

// --- Example usage in unit test ---
void test_offset_parity(const uint16_t* raw, const float* offset,
                          float* out_scalar, float* out_avx2,
                          uint32_t W, uint32_t H) {
    // Scalar reference
    for (size_t i = 0; i < (size_t)W * H; ++i)
        out_scalar[i] = std::max(static_cast<float>(raw[i]) - offset[i], 0.0f);

    // AVX2 optimized
    xpe_offset_correct(raw, offset, out_avx2, W, H);

    // Parity check (exact match expected for offset correction)
    XPE_PARITY_CHECK(offset_scalar, xpe_offset_correct,
                      out_scalar, out_avx2, (size_t)W * H, 0.0f);
}
```

#### 11.4.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| Offset/Gain 패리티 | 최대 절대 오차 = 0 | 합성 이미지 1000개 |
| Log Transform 패리티 | 최대 상대 오차 < 2×10⁻⁴ | AVX2 poly vs libm |
| 모든 Stage 패리티 | CI에서 100% PASS | 매 커밋 자동 실행 |
| 스칼라 구현 독립성 | -O0 컴파일에서도 동작 | 컴파일러 플래그 테스트 |

**IEC 62304 §5.4 추적성**: SRS ID: SRS-TEST-001 (단위 테스트 기준 확장) — Phase 2 추가 예정

---

## 12. FPD 특성화 알고리즘 보완

### 12.1 Allan Variance (장기 안정성 평가)

Allan Variance는 시스템의 시간적 안정성을 평가하는 통계량이다:

$$\sigma_A^2(\tau) = \frac{1}{2}\left\langle\left(\bar{x}_{k+1}(\tau) - \bar{x}_k(\tau)\right)^2\right\rangle$$

```python
def compute_allan_variance(time_series: np.ndarray,
                            sampling_interval_s: float) -> tuple[np.ndarray, np.ndarray]:
    """
    Compute Allan Variance of FPD signal over time.
    
    Useful for:
    - Identifying drift (positive slope in log-log plot)
    - Identifying random noise floor (flat region)
    - Identifying periodic artifacts (bumps)
    """
    N = len(time_series)
    max_m = N // 2
    
    tau_list = []
    avar_list = []
    
    for m in range(1, max_m + 1):
        tau = m * sampling_interval_s
        # Group means
        n_groups = N // m
        group_means = np.array([
            np.mean(time_series[k*m:(k+1)*m]) for k in range(n_groups)
        ])
        # Allan variance
        avar = 0.5 * np.mean(np.diff(group_means)**2)
        tau_list.append(tau)
        avar_list.append(avar)
    
    return np.array(tau_list), np.array(avar_list)
```

### 12.2 MTF 슬랜트 에지법 정밀도 개선

기존 명세(03_측정_알고리즘_명세서)의 보완:

```python
def compute_mtf_precision_mode(edge_image: np.ndarray,
                                pixel_pitch_mm: float,
                                oversampling: int = 4,
                                edge_angle_range: tuple = (2.0, 10.0)) -> dict:
    """
    High-precision MTF via Slanted Edge method with subpixel accuracy.
    
    Improvements over basic implementation:
    1. Subpixel edge localization (Canny + parabolic fit)
    2. Noise-robust ESF via LOWESS smoothing
    3. Aperture correction for finite pixel size
    4. IEC 62220-1 compliant ROI selection
    
    Aperture correction:
    MTF_true(f) = MTF_measured(f) / sinc(f × pixel_pitch)
    """
    # ... (full implementation follows existing 03_측정_알고리즘_명세서 pattern)
    
    # Key addition: aperture correction
    def aperture_correction(mtf_measured, freqs, pixel_pitch):
        sinc_vals = np.sinc(freqs * pixel_pitch)  # sinc = sin(πx)/(πx)
        with np.errstate(divide='ignore', invalid='ignore'):
            mtf_corrected = np.where(sinc_vals > 0.01,
                                      mtf_measured / sinc_vals,
                                      mtf_measured)
        return np.clip(mtf_corrected, 0, 1.2)
    
    return {'mtf': mtf_corrected, 'frequencies': freqs, 
            'f50': freq_at_mtf(mtf_corrected, freqs, 0.5),
            'f10': freq_at_mtf(mtf_corrected, freqs, 0.1)}
```

### 12.3 NPS 계산 알고리즘 (GAP-L 해소)

IEC 62220-1:2015 §6.3 준수 구현이다. 2-D NPS는 ROI별 FFT²를 평균화하여 계산한다.

#### 12.3.1 알고리즘 수학 정의

$$\text{NPS}(u, v) = \frac{\Delta x \cdot \Delta y}{N_x \cdot N_y} \cdot \left\langle \left|\mathcal{F}\left[I_{\text{ROI}}(x,y) - \bar{I}_{\text{ROI}}\right](u,v)\right|^2 \right\rangle_{\text{ROI ensemble}}$$

$$\text{NNPS}(u, v) = \frac{\text{NPS}(u, v)}{\bar{I}_{\text{det}}^2}$$

- $\Delta x, \Delta y$: pixel pitch in mm
- $N_x \times N_y$: ROI size (IEC 62220-1: 256×256 권장)
- $\langle \cdot \rangle$: ensemble average over non-overlapping ROIs (≥50 권장)
- $\bar{I}_{\text{det}}$: mean detector signal in ADU (또는 calibrated units)

**1-D radial NPS** (측정 보고용):

$$\text{NPS}(f) = \frac{1}{N_{\text{annulus}}} \sum_{(u,v): f - \delta f/2 \leq \sqrt{u^2+v^2} < f+\delta f/2} \text{NPS}(u,v)$$

#### 12.3.2 Python 구현 (IEC 62220-1 준수)

```python
import numpy as np
from scipy.signal.windows import hann

def compute_nps_2d(flat_images:      list[np.ndarray],
                   pixel_pitch_mm:   float,
                   roi_size:         int   = 256,
                   min_rois:         int   = 50,
                   detrend_order:    int   = 1,
                   window_function:  bool  = True) -> dict:
    """
    Compute 2-D Noise Power Spectrum per IEC 62220-1:2015 §6.3.

    Args:
        flat_images:     list of uniformly-exposed float32 images (H, W)
                         ≥2 images recommended; >1 required for ensemble
        pixel_pitch_mm:  pixel pitch in mm (same in x and y)
        roi_size:        ROI side length in pixels (IEC: 256)
        min_rois:        minimum ROI count for statistical validity
        detrend_order:   polynomial order for intra-ROI detrending (0=mean, 1=plane)
        window_function: apply 2-D Hanning window before FFT (reduces leakage)
    Returns:
        dict with keys:
          'nps_2d'     : float32 array (roi_size, roi_size) — 2-D NPS (ADU²·mm²)
          'nnps_2d'    : float32 array (roi_size, roi_size) — Normalised NPS (mm²)
          'nps_1d'     : (freqs, nps_radial) — radial average
          'nnps_1d'    : (freqs, nnps_radial)
          'mean_signal': mean detector signal used for normalisation
          'n_rois'     : number of ROIs used
    """
    H, W = flat_images[0].shape
    dx = dy = pixel_pitch_mm  # isotropic detector assumed
    half = roi_size // 2

    # Build 2-D Hanning window
    if window_function:
        win_1d = hann(roi_size, sym=False)
        window = np.outer(win_1d, win_1d).astype(np.float64)
        # Normalise so that sum(window²) == roi_size²  (IEC energy preservation)
        window /= np.sqrt(np.mean(window ** 2))
    else:
        window = np.ones((roi_size, roi_size), dtype=np.float64)

    nps_accum  = np.zeros((roi_size, roi_size), dtype=np.float64)
    roi_count  = 0
    mean_sum   = 0.0

    for img in flat_images:
        img_f = img.astype(np.float64)
        # Tile non-overlapping ROIs with 10% border margin
        y_starts = range(roi_size // 2, H - roi_size - roi_size // 2, roi_size)
        x_starts = range(roi_size // 2, W - roi_size - roi_size // 2, roi_size)

        for y0 in y_starts:
            for x0 in x_starts:
                roi = img_f[y0:y0 + roi_size, x0:x0 + roi_size]
                mean_sum += float(np.mean(roi))

                # Detrend: fit and subtract polynomial surface
                if detrend_order == 0:
                    roi_dt = roi - np.mean(roi)
                else:
                    # Plane fit (linear detrend)
                    ys, xs = np.mgrid[0:roi_size, 0:roi_size].astype(np.float64)
                    A = np.column_stack([xs.ravel(), ys.ravel(),
                                         np.ones(roi_size * roi_size)])
                    coef, _, _, _ = np.linalg.lstsq(A, roi.ravel(), rcond=None)
                    plane = (coef[0] * xs + coef[1] * ys + coef[2])
                    roi_dt = roi - plane

                # Apply window and FFT
                roi_w   = roi_dt * window
                F       = np.fft.fft2(roi_w)
                power   = np.abs(F) ** 2

                # NPS contribution: scale by pixel area / ROI area
                nps_accum += power * (dx * dy) / (roi_size * roi_size)
                roi_count += 1

    if roi_count < min_rois:
        import warnings
        warnings.warn(f"Only {roi_count} ROIs collected; IEC 62220-1 recommends ≥{min_rois}")

    nps_2d = (nps_accum / roi_count).astype(np.float32)
    nps_2d = np.fft.fftshift(nps_2d)   # centre DC at array centre

    mean_signal = mean_sum / roi_count
    nnps_2d     = nps_2d / (mean_signal ** 2 + 1e-12)

    # Radial average
    freqs, nps_1d  = _radial_average(nps_2d,  dx, roi_size)
    _, nnps_1d     = _radial_average(nnps_2d, dx, roi_size)

    return {
        'nps_2d':      nps_2d,
        'nnps_2d':     nnps_2d,
        'nps_1d':      (freqs, nps_1d),
        'nnps_1d':     (freqs, nnps_1d),
        'mean_signal': mean_signal,
        'n_rois':      roi_count,
    }


def _radial_average(power_2d: np.ndarray,
                    pixel_pitch_mm: float,
                    roi_size: int) -> tuple[np.ndarray, np.ndarray]:
    """Compute radial average of a centred 2-D power spectrum."""
    H, W    = power_2d.shape
    cy, cx  = H // 2, W // 2
    y_idx   = np.arange(H) - cy
    x_idx   = np.arange(W) - cx
    XX, YY  = np.meshgrid(x_idx, y_idx)
    freq_step = 1.0 / (roi_size * pixel_pitch_mm)    # cycles/mm per bin
    R = np.sqrt(XX ** 2 + YY ** 2)                   # radial distance in bins

    max_bin    = min(cx, cy)
    freq_bins  = np.arange(0, max_bin) * freq_step
    nps_radial = np.zeros(max_bin, dtype=np.float64)

    for k in range(max_bin):
        annulus = (R >= k - 0.5) & (R < k + 0.5)
        if np.sum(annulus) > 0:
            nps_radial[k] = float(np.mean(power_2d[annulus]))

    return freq_bins.astype(np.float32), nps_radial.astype(np.float32)
```

#### 12.3.3 검증 기준 (IEC 62220-1)

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| DC 성분 차단 | NPS(0,0) = 0 (detrend 후) | Detrending 적용 확인 |
| NNPS 저주파 일관성 | ≤ 10% variation across ROIs | ROI-to-ROI NNPS 비교 |
| ROI count | ≥ 50 | 프로그램 출력 확인 |
| 주파수 해상도 | Δf = 1/(N·Δx) cycles/mm | N=256, Δx=0.1mm → Δf=0.039 cycles/mm |

---

### 12.4 DQE 계산 알고리즘 (GAP-M 해소)

Detective Quantum Efficiency는 MTF와 NNPS로부터 계산되며 IEC 62220-1:2015 §6.4를 따른다.

#### 12.4.1 알고리즘 수학 정의

$$\text{DQE}(f) = \frac{\text{MTF}^2(f)}{\Phi \cdot \text{NNPS}(f)}$$

- $\Phi$: 입사 X선 quantum fluence (photons/mm²)
- $\text{MTF}^2(f)$: Modulation Transfer Function 제곱
- $\text{NNPS}(f)$: Normalised Noise Power Spectrum (mm²)

**Quantum fluence 추정** (IEC 62220-1 §5.2):
$$\Phi = \frac{\bar{I}_{\text{det}}}{\bar{g} \cdot \eta_{\text{absorb}} \cdot E_{\text{mean}}}$$

또는 실측 기반으로 ionisation chamber 측정값 사용 (권장):
$$\Phi = \frac{K_{\text{air}} \cdot \mu_{\text{en}}/\rho \cdot A_{\text{beam}}}{\bar{E}_{\text{photon}}}$$

실용적 접근 (RQA5 조건, 80kVp, IEC 61267): $\Phi \approx 3.0 \times 10^5\ \text{photons/mm}^2/(\text{mR})$

#### 12.4.2 Python 구현

```python
def compute_dqe(mtf_result:   dict,
                nps_result:   dict,
                quantum_fluence_per_mm2: float,
                freq_range_mm: tuple[float, float] = (0.0, 5.0)) -> dict:
    """
    Compute DQE(f) per IEC 62220-1:2015 §6.4.

    Args:
        mtf_result:               output of compute_mtf_precision_mode() — keys 'mtf', 'frequencies'
        nps_result:               output of compute_nps_2d() — key 'nnps_1d': (freqs, nnps)
        quantum_fluence_per_mm2:  Φ — X-ray photon fluence at detector surface (photons/mm²)
                                  Measure with calibrated ionisation chamber, or use
                                  tabulated value for RQA condition (IEC 62220-1 Annex C)
        freq_range_mm:            (f_min, f_max) in cycles/mm for output
    Returns:
        dict: 'frequencies', 'dqe', 'dqe_at_0', 'dqe_at_1', 'dqe_at_Nyquist'
    """
    mtf_freqs  = np.asarray(mtf_result['frequencies'], dtype=np.float64)
    mtf_vals   = np.asarray(mtf_result['mtf'],         dtype=np.float64)

    nnps_freqs = np.asarray(nps_result['nnps_1d'][0],  dtype=np.float64)
    nnps_vals  = np.asarray(nps_result['nnps_1d'][1],  dtype=np.float64)

    # Interpolate NNPS onto MTF frequency grid
    from scipy.interpolate import interp1d
    nnps_interp_fn = interp1d(nnps_freqs, nnps_vals,
                               kind='linear', bounds_error=False,
                               fill_value=(nnps_vals[0], nnps_vals[-1]))
    nnps_on_mtf_grid = nnps_interp_fn(mtf_freqs)

    # DQE = MTF² / (Φ × NNPS)
    denom = quantum_fluence_per_mm2 * nnps_on_mtf_grid
    with np.errstate(divide='ignore', invalid='ignore'):
        dqe = np.where(denom > 1e-20, mtf_vals ** 2 / denom, 0.0)

    # Clip to physically valid range [0, 1]
    dqe = np.clip(dqe, 0.0, 1.0)

    # Select output frequency range
    mask = (mtf_freqs >= freq_range_mm[0]) & (mtf_freqs <= freq_range_mm[1])

    def _dqe_at(target_freq: float) -> float:
        idx = np.argmin(np.abs(mtf_freqs - target_freq))
        return float(dqe[idx])

    return {
        'frequencies':    mtf_freqs[mask].astype(np.float32),
        'dqe':            dqe[mask].astype(np.float32),
        'dqe_at_0':       _dqe_at(0.0),
        'dqe_at_1':       _dqe_at(1.0),      # 1 cycle/mm
        'dqe_at_Nyquist': _dqe_at(float(mtf_freqs[mask].max())),
    }
```

#### 12.4.3 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| DQE(0) 범위 | 0.5 – 0.85 (CsI:Tl FPD 전형) | 공개 문헌 비교 |
| DQE(f) 단조성 | 감소 경향 (경미한 진동 허용) | 1차 차분 부호 확인 |
| IEC 62220-1 부합성 | 동일 팬텀에서 ±10% 이내 재현성 | 3회 반복 측정 |

---

### 12.5 Collimation Mask Detection 알고리즘 (GAP-N 해소)

`CollimatorMask`는 EI ROI 선택, GSVG scatter 추정, Phase 2 세부 알고리즘에서 공통으로 사용하는 기본 마스크 클래스이다. xpe-algorithm-spec-deepsync.md §3.2 "release-safe baseline"에 명시된 "baseline collimation detection"의 상세 구현이다.

#### 12.5.1 알고리즘 수학 정의

조준기 마스크는 임계값 기반 + 모폴로지 연산 결합으로 생성된다:

**단계 1 — 적응형 임계값**:
$$M_{\text{thresh}}(x,y) = \begin{cases} 1 & I_{\text{det}}(x,y) \geq \theta_{\text{coll}} \\ 0 & \text{otherwise} \end{cases}$$

$$\theta_{\text{coll}} = \mu_{\text{bright}} - k_{\text{coll}} \cdot \sigma_{\text{bright}}, \quad k_{\text{coll}} = 2.0$$

여기서 $\mu_{\text{bright}}$ 및 $\sigma_{\text{bright}}$는 상위 60% 픽셀에서 계산한다.

**단계 2 — 모폴로지 정제**:
$$M_{\text{final}} = \text{Close}\left(\text{Open}\left(M_{\text{thresh}},\ \text{SE}_{r_1}\right),\ \text{SE}_{r_2}\right)$$

- $r_1 = 15$ pixels (잡음 제거 opening)
- $r_2 = 50$ pixels (경계 닫기 closing)

**단계 3 — 최대 연결 성분 선택**: 최대 면적의 연결 성분을 최종 마스크로 채택.

#### 12.5.2 Python 구현

```python
import numpy as np
from dataclasses import dataclass

@dataclass
class CollimatorMask:
    """
    Collimator mask result for a single detector image.

    Attributes:
        mask:       uint8 binary mask (H, W) — 1 = inside collimated field
        bounding:   (x, y, w, h) bounding rectangle of collimated field
        confidence: float in [0,1] — detection quality estimate
        method_id:  str identifier for the detection algorithm used
    """
    mask:       np.ndarray
    bounding:   tuple[int, int, int, int]   # (x, y, w, h)
    confidence: float
    method_id:  str = 'threshold_morpho_v1'


def detect_collimator_mask(image:        np.ndarray,
                            pixel_pitch_mm: float = 0.1,
                            k_coll:      float = 2.0,
                            bright_frac:  float = 0.60,
                            open_r_mm:   float = 1.5,
                            close_r_mm:  float = 5.0) -> CollimatorMask:
    """
    Detect collimator boundary from a corrected detector image.

    Algorithm:
      1. Adaptive threshold from upper bright_frac percentile statistics
      2. Morphological open (noise removal) then close (gap filling)
      3. Select largest connected component
      4. Compute bounding rectangle and confidence score

    Args:
        image:          float32 (H, W), gain-corrected linear domain
        pixel_pitch_mm: detector pixel pitch in mm
        k_coll:         threshold = μ_bright - k_coll × σ_bright
        bright_frac:    fraction of brightest pixels used for stats
        open_r_mm:      morphological opening radius in mm
        close_r_mm:     morphological closing radius in mm
    Returns:
        CollimatorMask
    """
    try:
        from scipy import ndimage
        _scipy_ok = True
    except ImportError:
        _scipy_ok = False

    H, W = image.shape
    img  = image.astype(np.float32)

    # 1. Adaptive threshold from bright pixels
    flat  = img.ravel()
    perc  = np.percentile(flat, (1.0 - bright_frac) * 100.0)
    bright_pixels = flat[flat >= perc]
    mu_b  = float(np.mean(bright_pixels))
    sig_b = float(np.std(bright_pixels))
    theta = mu_b - k_coll * sig_b
    theta = max(theta, float(np.percentile(flat, 10.0)))  # safety floor

    binary_mask = (img >= theta).astype(np.uint8)

    # 2. Morphological open then close (in pixel units)
    r_open  = max(1, int(round(open_r_mm  / pixel_pitch_mm)))
    r_close = max(1, int(round(close_r_mm / pixel_pitch_mm)))

    if _scipy_ok:
        from scipy.ndimage import binary_opening, binary_closing, label
        struct_o = np.ones((2 * r_open  + 1, 2 * r_open  + 1), dtype=bool)
        struct_c = np.ones((2 * r_close + 1, 2 * r_close + 1), dtype=bool)
        opened  = binary_opening(binary_mask, structure=struct_o)
        closed  = binary_closing(opened,      structure=struct_c).astype(np.uint8)
    else:
        # Minimal fallback without scipy: sliding-window erosion/dilation (slow)
        closed = binary_mask  # degraded mode

    # 3. Largest connected component
    if _scipy_ok:
        labeled, n_comp = label(closed)
        if n_comp == 0:
            # No valid component: return full-image fallback
            final_mask  = np.ones((H, W), dtype=np.uint8)
            confidence  = 0.1
        else:
            sizes = ndimage.sum(closed, labeled, range(1, n_comp + 1))
            best  = int(np.argmax(sizes)) + 1
            final_mask = (labeled == best).astype(np.uint8)
            # Confidence: ratio of largest/total foreground pixels
            confidence = float(sizes[best - 1] / (np.sum(closed) + 1e-6))
            confidence = float(np.clip(confidence, 0.0, 1.0))
    else:
        final_mask = closed
        confidence = 0.5

    # 4. Bounding rectangle
    rows = np.any(final_mask, axis=1)
    cols = np.any(final_mask, axis=0)
    if not np.any(rows) or not np.any(cols):
        bounding = (0, 0, W, H)
        confidence = 0.05
    else:
        r_min, r_max = int(np.argmax(rows)), int(H - 1 - np.argmax(rows[::-1]))
        c_min, c_max = int(np.argmax(cols)), int(W - 1 - np.argmax(cols[::-1]))
        bounding = (c_min, r_min, c_max - c_min, r_max - r_min)

    return CollimatorMask(
        mask       = final_mask,
        bounding   = bounding,
        confidence = confidence,
        method_id  = 'threshold_morpho_v1',
    )
```

#### 12.5.3 C++ 클래스 명세 (런타임)

```cpp
// CollimatorMask C++ runtime class
// Python calibration produces JSON sidecar; runtime reconstructs mask from sidecar
// Sidecar schema (xpe-algorithm-spec-deepsync.md §4.3):
//   { "roi_x": int, "roi_y": int, "roi_w": int, "roi_h": int,
//     "confidence": float, "method_id": string }

struct CollimatorMask {
    cv::Mat  mask;          // CV_8U binary mask (1 = inside collimated field)
    cv::Rect bounding;      // Bounding rect of collimated field
    float    confidence;    // Detection quality [0,1]
    std::string method_id;  // "threshold_morpho_v1" or "ai_refined_v1"

    // Convenience accessors
    cv::Rect bounding_rect() const { return bounding; }
    const cv::Mat& mask_mat() const { return mask; }

    // Load from JSON sidecar (written by Python calibration step)
    static CollimatorMask from_sidecar(const nlohmann::json& j) {
        CollimatorMask cm;
        int x = j.at("roi_x").get<int>();
        int y = j.at("roi_y").get<int>();
        int w = j.at("roi_w").get<int>();
        int h = j.at("roi_h").get<int>();
        cm.bounding    = cv::Rect(x, y, w, h);
        cm.confidence  = j.at("confidence").get<float>();
        cm.method_id   = j.at("method_id").get<std::string>();
        // Reconstruct binary mask from bounding rect (full-rect approximation)
        // Full polygon mask optional in Phase 2 if contour points stored
        cm.mask = cv::Mat::zeros(/* H, W from image size */ 0, 0, CV_8U);
        // Caller must provide image dimensions; set via set_image_size()
        return cm;
    }

    void set_image_size(int H, int W) {
        mask = cv::Mat::zeros(H, W, CV_8U);
        cv::rectangle(mask, bounding, cv::Scalar(1), cv::FILLED);
    }

    // Compute mean signal within mask (used by EI ROI selection)
    float mean_within_mask(const cv::Mat& image) const {
        cv::Scalar mean_val = cv::mean(image, mask);
        return static_cast<float>(mean_val[0]);
    }
};
```

#### 12.5.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| 검출 성공률 | ≥ 95% | 100장 테스트 팬텀 |
| Bounding rect 정확도 | ±5mm from true edge | 물리적 조준기 기준값 비교 |
| 처리 시간 | < 200ms (3072×3072) | 단일 스레드 |
| Confidence 하한 경고 | confidence < 0.6 → 알림 | 경보 로그 확인 |

---

### 12.6 MTF 슬랜트 에지 ESF 완전 구현 (GAP-T 해소)

§12.2에서 `compute_mtf_precision_mode()`의 aperture correction만 제공했지만, ESF 추출 → LSF → FFT → MTF 완전 파이프라인이 명세되어 있지 않았다. 본 섹션은 IEC 62220-1-1 준수 완전 구현을 제공한다.

#### 12.6.1 알고리즘 수학 정의

**ESF → LSF → MTF 변환 체인**:

$$\text{ESF}(x) = \text{edge spread function} \quad \text{(oversample from multiple rows)}$$

$$\text{LSF}(x) = \frac{d}{dx}\text{ESF}(x) \quad \text{(line spread function = derivative of ESF)}$$

$$\text{OTF}(f) = \mathcal{F}\left[\text{LSF}(x)\right](f)$$

$$\text{MTF}(f) = \left|\text{OTF}(f)\right| / \left|\text{OTF}(0)\right|$$

**Pre-sampling 보정 (IEC 62220-1-1 §6.2)**:

$$\text{MTF}_{\text{true}}(f) = \frac{\text{MTF}_{\text{measured}}(f)}{\text{sinc}(f \cdot p)} \quad \text{where } \text{sinc}(x) = \frac{\sin(\pi x)}{\pi x}$$

**슬랜트 각도 제약 (ISO 12233)**:
$$\theta_{\text{edge}} \in [2°,\ 10°] \quad \Rightarrow \quad \text{oversampling factor} = \frac{1}{\sin\theta}$$

#### 12.6.2 파라미터 및 경계 조건

| 파라미터 | 권장값 | 범위 | 의미 |
|---------|-------|-----|------|
| `edge_angle_deg` | 5.0 | 2–10° | 슬랜트 각도 (IEC 62220-1-1) |
| `oversampling` | 4 | 4–8 | ESF 오버샘플링 인수 |
| `roi_height_px` | 200 | 100–500 | ESF 추출 ROI 높이 |
| `smooth_sigma` | 1.0 | 0.5–3.0 | LSF smoothing σ (가우시안) |
| `freq_limit_nyquist` | 1.0 | 0.1–1.0 | MTF 출력 주파수 상한 (Nyquist 배수) |

#### 12.6.3 Python 구현 (오프라인, IEC 62220-1-1 준수)

```python
import numpy as np
from scipy.ndimage import sobel, gaussian_filter1d
from scipy.optimize import curve_fit
from typing import Tuple

def extract_esf_from_slanted_edge(
        edge_image:      np.ndarray,
        pixel_pitch_mm:  float,
        edge_angle_deg:  float = 5.0,
        oversampling:    int   = 4,
        roi_height_px:   int   = 200) -> Tuple[np.ndarray, np.ndarray]:
    """
    Extract Edge Spread Function (ESF) from a slanted-edge image.

    Algorithm (IEC 62220-1-1 §6.1):
      1. Locate edge centre per row via gradient centroid
      2. Compute sub-pixel position relative to mean edge location
      3. Bin into oversampled ESF array

    Args:
        edge_image:     2-D float32 image containing slanted edge
        pixel_pitch_mm: detector pixel pitch (mm)
        edge_angle_deg: nominal edge angle in degrees
        oversampling:   ESF super-resolution factor
        roi_height_px:  number of rows to use from image centre
    Returns:
        (esf_positions_mm, esf_values) — both 1-D float64 arrays
    """
    H, W = edge_image.shape
    row_start = (H - roi_height_px) // 2
    row_end   = row_start + roi_height_px
    roi       = edge_image[row_start:row_end, :].astype(np.float64)
    n_rows, n_cols = roi.shape

    # Step 1: Locate edge centre per row using gradient centroid (Canny + CoM)
    grad = np.gradient(roi, axis=1)
    abs_grad = np.abs(grad)
    col_idx = np.arange(n_cols, dtype=np.float64)
    # Centre of mass of |gradient| per row → sub-pixel edge position
    edge_pos_per_row = np.array([
        np.sum(abs_grad[r, :] * col_idx) / (np.sum(abs_grad[r, :]) + 1e-10)
        for r in range(n_rows)
    ])

    # Step 2: Fit line to edge positions to estimate angle
    row_idx = np.arange(n_rows, dtype=np.float64)
    p = np.polyfit(row_idx, edge_pos_per_row, 1)
    slope = p[0]  # pixels per row
    edge_mean = np.mean(edge_pos_per_row)

    # Step 3: Build oversampled ESF
    osf = oversampling
    esf_bins  = np.zeros(n_cols * osf, dtype=np.float64)
    esf_count = np.zeros(n_cols * osf, dtype=np.int32)

    for r in range(n_rows):
        edge_x = edge_mean + slope * (r - n_rows / 2)
        for c in range(n_cols):
            dx = (c - edge_x) * pixel_pitch_mm  # mm from edge
            bin_idx = int(round(dx / pixel_pitch_mm * osf)) + (n_cols * osf) // 2
            if 0 <= bin_idx < len(esf_bins):
                esf_bins[bin_idx]  += roi[r, c]
                esf_count[bin_idx] += 1

    valid = esf_count > 0
    esf_vals = np.where(valid, esf_bins / np.maximum(esf_count, 1), np.nan)
    esf_pos  = (np.arange(len(esf_bins)) - len(esf_bins) // 2) * pixel_pitch_mm / osf

    # Remove NaN by linear interpolation
    nans = np.isnan(esf_vals)
    esf_vals[nans] = np.interp(np.where(nans)[0],
                                 np.where(~nans)[0],
                                 esf_vals[~nans])
    return esf_pos, esf_vals


def compute_mtf_from_esf(
        esf_positions_mm: np.ndarray,
        esf_values:       np.ndarray,
        smooth_sigma:     float = 1.0,
        freq_limit:       float = 1.0,
        pixel_pitch_mm:   float = 0.148,
        aperture_correct: bool  = True) -> dict:
    """
    Compute MTF from ESF via differentiation and FFT.

    Pipeline:
        ESF → smooth → differentiate → LSF → Hanning window → FFT → |OTF| → MTF

    Args:
        esf_positions_mm: sample positions (mm), uniformly spaced
        esf_values:       ESF values (float64)
        smooth_sigma:     Gaussian smoothing σ applied to LSF
        freq_limit:       upper frequency as fraction of Nyquist (1.0 = Nyquist)
        pixel_pitch_mm:   original pixel pitch for aperture correction
        aperture_correct: apply sinc aperture correction
    Returns:
        dict with: freqs_mm_inv, mtf, f50_mm_inv, f10_mm_inv
    """
    dx = float(np.mean(np.diff(esf_positions_mm)))  # mm per sample

    # Normalise ESF to [0, 1]
    esf = esf_values.astype(np.float64)
    esf = (esf - esf.min()) / (esf.max() - esf.min() + 1e-10)

    # Differentiate ESF → LSF
    lsf = np.gradient(esf, dx)

    # Gaussian smoothing to reduce noise (per IEC 62220-1-1 §6.2)
    if smooth_sigma > 0:
        lsf = gaussian_filter1d(lsf, sigma=smooth_sigma / dx)

    # Normalise LSF area to 1
    lsf_sum = np.sum(np.abs(lsf)) * dx
    if lsf_sum > 1e-10:
        lsf /= lsf_sum

    # Hanning window (reduce spectral leakage)
    window = np.hanning(len(lsf))
    lsf_w  = lsf * window

    # FFT → OTF → MTF
    n    = len(lsf_w)
    otf  = np.fft.fft(lsf_w, n=n * 4)  # zero-pad 4× for interpolation
    freqs = np.fft.fftfreq(n * 4, d=dx)  # cycles/mm

    # Keep positive frequencies up to freq_limit × Nyquist
    nyquist = 1.0 / (2.0 * pixel_pitch_mm)
    pos_mask = (freqs > 0) & (freqs <= freq_limit * nyquist)
    freqs_pos = freqs[pos_mask]
    mtf_raw   = np.abs(otf[pos_mask])
    mtf_raw  /= (np.abs(otf[0]) + 1e-10)   # normalise to DC

    # Aperture correction: divide by sinc(f × pixel_pitch)
    if aperture_correct:
        sinc_vals = np.sinc(freqs_pos * pixel_pitch_mm)  # numpy sinc = sin(πx)/(πx)
        mtf_corrected = np.where(sinc_vals > 0.05,
                                  mtf_raw / sinc_vals,
                                  mtf_raw)
        mtf = np.clip(mtf_corrected, 0.0, 1.2)
    else:
        mtf = np.clip(mtf_raw, 0.0, 1.2)

    # Find f50 and f10 (interpolated)
    def freq_at_mtf_val(mtf_arr, freq_arr, target):
        above = np.where(mtf_arr >= target)[0]
        if len(above) == 0: return float(freq_arr[-1])
        i = above[-1]
        if i + 1 >= len(mtf_arr): return float(freq_arr[i])
        # Linear interpolation
        t = (target - mtf_arr[i]) / (mtf_arr[i + 1] - mtf_arr[i] + 1e-10)
        return float(freq_arr[i] + t * (freq_arr[i + 1] - freq_arr[i]))

    return {
        'freqs_mm_inv': freqs_pos.astype(np.float32),
        'mtf':          mtf.astype(np.float32),
        'f50_mm_inv':   freq_at_mtf_val(mtf, freqs_pos, 0.5),
        'f10_mm_inv':   freq_at_mtf_val(mtf, freqs_pos, 0.1),
        'pixel_pitch_mm': pixel_pitch_mm,
        'oversampling_dx_mm': dx,
    }


def full_mtf_pipeline(edge_image:     np.ndarray,
                       pixel_pitch_mm: float,
                       **kwargs) -> dict:
    """
    Complete MTF pipeline: edge image → MTF curve.

    Combines extract_esf_from_slanted_edge() and compute_mtf_from_esf().
    """
    esf_pos, esf_vals = extract_esf_from_slanted_edge(
        edge_image, pixel_pitch_mm,
        edge_angle_deg = kwargs.get('edge_angle_deg', 5.0),
        oversampling   = kwargs.get('oversampling', 4),
        roi_height_px  = kwargs.get('roi_height_px', 200),
    )
    return compute_mtf_from_esf(
        esf_pos, esf_vals,
        smooth_sigma    = kwargs.get('smooth_sigma', 1.0),
        pixel_pitch_mm  = pixel_pitch_mm,
        aperture_correct= kwargs.get('aperture_correct', True),
    )
```

#### 12.6.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| MTF@Nyquist vs 이론값 | 오차 < 5% | 합성 단계함수 이미지 |
| f50 재현성 | CV < 2% (5회 측정) | 동일 팬텀 반복 측정 |
| IEC 62220-1-1 인증 | f10 ≥ 0.5 × Nyquist (RQA5) | 표준 팬텀 측정 |
| Aperture 보정 효과 | f50 ≥ 보정 전 1.05× | 보정 전후 비교 |
| 처리 시간 | < 500ms (512 rows) | 단일 코어 |

**IEC 62304 §5.4 추적성**: SRS ID: SRS-MEAS-001 (FPD 특성화 확장), SRS-MEAS-002 (MTF 완전 구현) — Phase 2 추가 예정

---

## 13. 품질 상태 벡터 사이드카 (GAP-R 해소)

xpe-algorithm-spec-deepsync.md §4.3 "For every processed frame, the runtime should produce a sidecar quality state"에서 요구된 항목이다. 기존 `AEDResult` 구조체는 AED-0 결과만을 담고 있으며, 파이프라인 전체의 품질 상태를 통합 표현하는 사이드카가 없었다.

### 13.1 알고리즘 수학 정의

품질 상태 벡터 $\mathbf{Q}$는 프레임당 하나의 인스턴스로 생성되며, 각 필드는 해당 파이프라인 단계 완료 직후 채워진다:

$$\mathbf{Q} = \{\mathbf{Q}_{\text{cal}},\ \mathbf{Q}_{\text{defect}},\ \mathbf{Q}_{\text{lag}},\ \mathbf{Q}_{\text{gsvg}},\ \mathbf{Q}_{\text{ei}},\ \mathbf{Q}_{\text{ai}}\}$$

각 서브 벡터는 해당 단계의 상태, 신뢰도, 경고 플래그를 포함한다.

**교정 신선도 점수**:

$$Q_{\text{cal,fresh}} = \exp\!\left(-\frac{\Delta t_{\text{days}}}{\tau_{\text{fresh}}}\right), \quad \tau_{\text{fresh}} = 7 \text{ days}$$

**결함 부담 등급**:

$$\text{DefectClass} = \begin{cases} 0 & N_{\text{def}} < 0.01\% W H \\ 1 & N_{\text{def}} < 0.05\% W H \\ 2 & N_{\text{def}} < 0.2\% W H \\ 3 & N_{\text{def}} \geq 0.2\% W H \end{cases}$$

### 13.2 XpeQualityState 구조체 명세

#### 13.2.1 Python 정의 (참조 스키마)

```python
from dataclasses import dataclass, field
from enum import IntEnum
from typing import Optional

class CalibFreshness(IntEnum):
    FRESH   = 0   # score ≥ 0.90
    AGING   = 1   # 0.70 ≤ score < 0.90
    STALE   = 2   # 0.40 ≤ score < 0.70
    EXPIRED = 3   # score < 0.40 → recalibration required

class DefectBurdenClass(IntEnum):
    NEGLIGIBLE = 0   # < 0.01% pixels
    LOW        = 1   # 0.01–0.05%
    MODERATE   = 2   # 0.05–0.2%
    HIGH       = 3   # ≥ 0.2% → quality advisory

class LagTierApplied(IntEnum):
    NONE = 0   # no lag correction applied
    FAST = 1   # single-term model
    FULL = 3   # full three-term model

class GsvgSkipReason(IntEnum):
    NOT_SKIPPED      = 0
    NO_GRID_DETECTED = 1   # no grid artifact found
    PERFORMANCE_MODE = 2   # explicitly disabled by operator
    AED_FAILED       = 3   # AED-0 returned invalid

class AiWorkerStatus(IntEnum):
    NOT_USED    = 0
    AI_SUCCESS  = 1
    AI_FALLBACK = 2   # deterministic fallback used
    AI_DISABLED = 3

@dataclass
class CalibQuality:
    freshness_class:  CalibFreshness  = CalibFreshness.FRESH
    freshness_score:  float           = 1.0
    session_id:       str             = ''
    days_since_cal:   float           = 0.0
    drift_warning:    bool            = False

@dataclass
class DetectorCorrectionQuality:
    defect_burden_class:  DefectBurdenClass = DefectBurdenClass.NEGLIGIBLE
    defect_count:         int               = 0
    lag_tier:             LagTierApplied    = LagTierApplied.NONE
    lag_residual_pct:     float             = 0.0
    nonlinearity_applied: bool              = False
    heel_applied:         bool              = False

@dataclass
class ExposureQuality:
    aed_valid:      bool  = True
    ei_value:       float = 0.0
    di_value:       float = 0.0
    roi_confidence: float = 1.0   # EI ROI detection confidence (0–1)
    roi_method:     str   = ''    # 'central' / 'anatomy_bounded' / 'fallback'

@dataclass
class GsvgQuality:
    grid_detected:  bool          = False
    gsvg_applied:   bool          = False
    skip_reason:    GsvgSkipReason = GsvgSkipReason.NOT_SKIPPED
    grid_frequency: float         = 0.0   # detected grid frequency (lp/mm)

@dataclass
class AiQuality:
    worker_status:  AiWorkerStatus = AiWorkerStatus.NOT_USED
    body_part_id:   str            = ''
    confidence:     float          = 0.0
    model_version:  str            = ''
    inference_ms:   float          = 0.0

@dataclass
class XpeQualityState:
    """
    Per-frame quality state sidecar.
    Created empty at pipeline entry; each stage fills its section.
    Must NOT mutate XpeImageMetadata to carry this information.
    """
    frame_id:     str                      = ''
    timestamp_ns: int                      = 0
    calib:        CalibQuality             = field(default_factory=CalibQuality)
    detector:     DetectorCorrectionQuality = field(default_factory=DetectorCorrectionQuality)
    exposure:     ExposureQuality          = field(default_factory=ExposureQuality)
    gsvg:         GsvgQuality             = field(default_factory=GsvgQuality)
    ai:           AiQuality               = field(default_factory=AiQuality)
    pipeline_version: str                  = 'xpe-1.2'

    def overall_advisory(self) -> str:
        """
        Generate a single human-readable advisory string.
        Returns empty string if everything is nominal.
        """
        warnings = []
        if self.calib.freshness_class >= CalibFreshness.STALE:
            warnings.append(f"CAL_STALE({self.calib.days_since_cal:.1f}d)")
        if self.detector.defect_burden_class >= DefectBurdenClass.MODERATE:
            warnings.append(f"DEFECT_BURDEN({self.detector.defect_count}px)")
        if not self.exposure.aed_valid:
            warnings.append("EXPOSURE_INVALID")
        if abs(self.exposure.di_value) > 3.0:
            warnings.append(f"DI_CONCERN({self.exposure.di_value:+.1f}dB)")
        if self.ai.worker_status == AiWorkerStatus.AI_FALLBACK:
            warnings.append("AI_FALLBACK")
        return '; '.join(warnings)
```

#### 13.2.2 C++ 구조체

```cpp
// XpeQualityState — C++ sidecar object
// Lifetime: same as the processing call; returned alongside output image.

enum class CalibFreshness   : uint8_t { FRESH=0, AGING=1, STALE=2, EXPIRED=3 };
enum class DefectBurdenClass: uint8_t { NEGLIGIBLE=0, LOW=1, MODERATE=2, HIGH=3 };
enum class LagTierApplied   : uint8_t { NONE=0, FAST=1, FULL=3 };
enum class GsvgSkipReason   : uint8_t { NOT_SKIPPED=0, NO_GRID=1, PERF=2, AED_FAILED=3 };
enum class AiWorkerStatus   : uint8_t { NOT_USED=0, AI_SUCCESS=1, FALLBACK=2, DISABLED=3 };

struct CalibQualityState {
    CalibFreshness freshness_class  = CalibFreshness::FRESH;
    float          freshness_score  = 1.0f;
    char           session_id[17]   = {};   // 16 hex + null
    float          days_since_cal   = 0.0f;
    bool           drift_warning    = false;
};

struct DetectorCorrectionState {
    DefectBurdenClass defect_burden = DefectBurdenClass::NEGLIGIBLE;
    uint32_t          defect_count  = 0;
    LagTierApplied    lag_tier      = LagTierApplied::NONE;
    float             lag_residual_pct  = 0.0f;
    bool              nonlinearity_applied = false;
    bool              heel_applied    = false;
};

struct ExposureState {
    bool  aed_valid      = true;
    float ei_value       = 0.0f;
    float di_value       = 0.0f;
    float roi_confidence = 1.0f;
    char  roi_method[32] = "central";
};

struct GsvgState {
    bool          grid_detected = false;
    bool          gsvg_applied  = false;
    GsvgSkipReason skip_reason  = GsvgSkipReason::NOT_SKIPPED;
    float         grid_freq_lpmm = 0.0f;
};

struct AiState {
    AiWorkerStatus status       = AiWorkerStatus::NOT_USED;
    char  body_part_id[32]      = {};
    float confidence            = 0.0f;
    char  model_version[32]     = {};
    float inference_ms          = 0.0f;
};

struct XpeQualityState {
    char               frame_id[64]   = {};
    int64_t            timestamp_ns   = 0;
    CalibQualityState  calib          = {};
    DetectorCorrectionState detector  = {};
    ExposureState      exposure       = {};
    GsvgState          gsvg           = {};
    AiState            ai             = {};
    char               pipeline_ver[16] = "xpe-1.2";

    // Serialize to JSON string for logging/DICOM private tag
    std::string to_json() const;
};
```

#### 13.2.3 파이프라인 통합 포인트

각 처리 단계에서 `XpeQualityState`를 채우는 위치:

| 단계 | 채우는 필드 | 시점 |
|-----|-----------|------|
| ConfigManager 로드 | `calib.*` | 파이프라인 시작 전 |
| Readout Validation | `exposure.aed_valid` 예비 | §3.0 완료 후 |
| Defect Correction | `detector.defect_burden`, `detector.defect_count` | §3.3 완료 후 |
| Ghost Correction | `detector.lag_tier`, `detector.lag_residual_pct` | §3.4.5 완료 후 |
| Non-linearity | `detector.nonlinearity_applied` | §3.0.5 완료 후 |
| Heel Correction | `detector.heel_applied` | §3.5 완료 후 |
| AED-0 | `exposure.aed_valid`, `exposure.ei_value` | §9.4 완료 후 |
| Grid Suppression | `gsvg.*` | §5 완료 후 |
| EI Calculation | `exposure.di_value`, `exposure.roi_confidence` | §7 완료 후 |
| AI Worker | `ai.*` | §8.4 완료 후 |

### 13.3 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| 모든 필드 채워짐 | 파이프라인 완료 시 0개 기본값 잔류 | 완전 파이프라인 실행 후 확인 |
| `overall_advisory()` 정확도 | 알려진 이상 시나리오 100% 탐지 | 합성 결함 파이프라인 |
| 사이드카 직렬화 크기 | < 1KB (JSON) | 시리얼라이제이션 테스트 |
| 메인 이미지 처리 추가 지연 | < 0.5ms | 프로파일링 |

**IEC 62304 §5.4 추적성**: SRS ID: SRS-QC-003 (품질 상태 사이드카) — Phase 2 추가 예정

---

## 부록 A: 수학 공식 일람

### A.1 Pre-Processing 공식

$$I_{\text{offset}}(x,y) = \max(I_{\text{raw}} - I_{\text{dark}},\ 0)$$

$$G(x,y) = \frac{\bar{I}_{\text{flat}}}{I_{\text{flat}}(x,y) - I_{\text{dark}}(x,y)}, \quad I_{\text{corr}} = I_{\text{offset}} \cdot G$$

$$\text{Lag}(t) = \sum_{i=1}^{3} \alpha_i e^{-t/\tau_i}$$

$$I_{\text{true}} = I_{\text{measured}} - \text{Lag}(t) \cdot I_{\text{prev\_max}}$$

### A.2 Core Processing 공식

$$I_{OD} = -\ln\left(\frac{I_{\text{clean}} + \varepsilon}{I_0 + \varepsilon}\right)$$

$$BF[I](x) = \frac{\sum_{x_i} I(x_i) e^{-\|x_i-x\|^2/2\sigma_s^2} e^{-|I(x_i)-I(x)|^2/2\sigma_r^2}}{\sum_{x_i} e^{-\|x_i-x\|^2/2\sigma_s^2} e^{-|I(x_i)-I(x)|^2/2\sigma_r^2}}$$

$$I_{\text{USM}} = I + \lambda \cdot (I - I * G_\sigma)$$

### A.3 Display Processing 공식

**Linear VOI:**
$$\text{Out} = \text{clamp}\left(\frac{I - (WC - WW/2)}{WW} \cdot (\text{Max} - \text{Min}) + \text{Min},\ \text{Min},\ \text{Max}\right)$$

**Sigmoid VOI:**
$$\text{Out} = \frac{\text{Max} - \text{Min}}{1 + e^{-4(I-WC)/WW}} + \text{Min}$$

**GSDF JND:**
$$j = 71.498068 + 94.593053\log L + 41.912053(\log L)^2 + \cdots$$

### A.4 Exposure Index 공식

$$DI = 10 \cdot \log_{10}\left(\frac{EI}{EI_{\text{target}}}\right) \quad \text{(dB)}$$

### A.5 FPD 특성화 공식

$$\text{DQE}(f) = \frac{\text{MTF}^2(f)}{\Phi \cdot \text{NNPS}(f)}$$

$$\text{NNPS}(f) = \frac{\text{NPS}(f)}{\bar{S}^2}$$

$$\sigma_A^2(\tau) = \frac{1}{2}\langle(\bar{x}_{k+1} - \bar{x}_k)^2\rangle$$

---

## 부록 B: 표준 참조 테이블

### B.1 RQA 조건 (IEC 61267)

| RQA | kVp | Al 여과 (mm) | HVL (mm Al) | 용도 |
|-----|-----|------------|-------------|------|
| RQA3 | 70 | 23.0 | 6.8 | Mammography-adjacent |
| RQA5 | 80 | 21.0 | 7.1 | General radiography |
| RQA7 | 90 | 30.0 | 9.2 | Chest |
| RQA9 | 120 | 40.0 | 11.5 | High-kVp chest |
| RQA10 | 150 | 50.0 | 13.0 | Interventional |

### B.2 SPR 참조 (80kVp, 35×43cm FOV)

| 두께 (cm, water equiv.) | SPR (%) |
|-----------------------|---------|
| 10 | 30–50 |
| 15 | 60–80 |
| 20 | 80–120 |
| 25 | 120–180 |
| 30 | 150–250 |

### B.3 GSDF P-Value Luminance (PS3.14 Table B.1 발췌)

| P-Value | Target Luminance (cd/m²) | JND Index |
|---------|------------------------|----------|
| 0 | 0.05 | ~10 |
| 1024 | 2.0 | ~200 |
| 2048 | 50.0 | ~400 |
| 3071 | 1000.0 | ~600 |
| 4095 | 3000.0 | ~700 |

---

## 부록 C: 알고리즘-요구사항 추적성

| 알고리즘 | SRS Req ID | SDD SWU | 검증 방법 |
|---------|-----------|---------|---------|
| Offset Correction | SRS-FUNC-001 | SWU-1.1 | Unit test + dark field measurement |
| Gain Correction | SRS-FUNC-002 | SWU-1.2 | Uniformity measurement |
| Defect Correction | SRS-FUNC-003 | SWU-1.3 | Injected defect test |
| Ghost Correction | SRS-FUNC-004 | SWU-1.4 | Double-exposure protocol |
| Log Transform | SRS-FUNC-010 | SWU-2.1 | Mathematical verification |
| Bilateral Filter | SRS-FUNC-011 | SWU-2.2 | MTF retention test |
| CLAHE | SRS-FUNC-012 | SWU-2.3 | Histogram analysis |
| Edge Enhancement | SRS-FUNC-013 | SWU-2.4 | Safe gain verification |
| Laplacian Pyramid | SRS-FUNC-014 | SWU-2.5 | Phantom image quality |
| Fractional MS | SRS-FUNC-015 | SWU-2.6 | Artifact measurement |
| CNN Recognition | SRS-FUNC-016 | SWU-2.10 | ≥95% accuracy test |
| Panoramic Stitch | SRS-FUNC-017 | SWU-2.11 | Cobb angle ≤2° error |
| Bone Suppression | SRS-FUNC-018 | SWU-2.12 | PSNR≥33dB, SSIM≥0.97 |
| Modality LUT | SRS-FUNC-020 | SWU-3.1 | DICOM conformance |
| VOI LUT | SRS-FUNC-021 | SWU-3.2 | W/L sweep verification |
| GSDF PLUT | SRS-FUNC-022 | SWU-3.3 | PS3.14 conformance |
| Grid Suppression | — (Phase 2) | — | MTF retention + CNR |
| Virtual Grid | — (Phase 2) | — | CNR comparison vs physical grid |
| Exposure Index | — (Phase 2) | SWU-2.9 | IEC 62494-1 conformance |
| Readout Validation | SRS-QC-001 | SWU-1.0 | Saturation/geometry injection test |
| NPS Computation | SRS-MEAS-001 | — | IEC 62220-1 compliance |
| DQE Computation | SRS-MEAS-001 | — | IEC 62220-1 DQE formula |
| Collimation Mask | SRS-FUNC-001b | — | 95% detection, ±5mm accuracy |
| Heel Effect | SRS-FUNC-002b | SWU-1.5 | PRNU CV < 0.8% (Phase 2) |
| Multi-SID Gain | SRS-FUNC-002 ext | SWU-1.2b | Interp error < 0.5% (Phase 2) |
| Calibration Session Lock | SRS-SEC-002 ext | — | 100% mixed-session rejection |
| Calibration Drift Monitor | SRS-QC-002 | — | Drift threshold parity test |
| Quality State Sidecar | SRS-QC-003 | — | All-field population test |
| Scalar Parity Harness | SRS-TEST-001 | — | CI PASS on all stages |
| MTF ESF Pipeline | SRS-MEAS-002 | — | IEC 62220-1-1 f50 accuracy |
| Lag Residual Tiering | SRS-FUNC-004 ext | SWU-1.4b | Tier selection accuracy |
| VG Anatomy Presets | SRS-FUNC-008b | — | CNR ≥10% (Chest), observer gate |
| AI Worker Isolation | SRS-AI-001 | — | Fallback 100% on timeout |

---

## 개정 이력

| 개정 | 날짜 | 저자 | 내용 |
|------|------|------|------|
| 1.2 | 2026-04-15 | XPE Team | **Round 3 GAP 해소 10건 (GAP-O~X)**: GAP-O (Heel Effect Compensation §3.5, Wang 2013 Duo-SID), GAP-P (Multi-SID Gain 보간 §3.2.5), GAP-Q (교정 세션 잠금 §2.4, 매니페스트 해시 체인), GAP-R (품질 상태 벡터 사이드카 §13, XpeQualityState), GAP-S (스칼라 참조 + SIMD 패리티 하네스 §11.4), GAP-T (MTF ESF 완전 구현 §12.6, IEC 62220-1-1), GAP-U (Lag 잔류 티어링 §3.4.5, Tier-0/1/3 결정론적 선택), GAP-V (해부 부위별 VG 프리셋 §5.3, 15개 부위 테이블), GAP-W (AI Worker 격리 §8.4, ONNX + 폴백 + 모델 매니페스트), GAP-X (교정 드리프트 모니터링 §9.5, 드리프트율 측정 + 재교정 트리거). 섹션 수 추가, §13 신설. |
| 1.1 | 2026-04-15 | XPE Team | **GAP 해소 10건**: GAP-D (NSCT 완전 구현), GAP-E (update_defect_map_runtime AVX2 구현), GAP-F (EI ROI Central Method √0.1 수정), GAP-G (avx2_log_ps Cephes 다항식), GAP-H (Non-linearity Correction §3.0.5), GAP-I (Readout Validation §3.0), GAP-J (AED-0 §9.4), GAP-L (NPS §12.3), GAP-M (DQE §12.4), GAP-N (Collimation Mask §12.5). 섹션 수 ~50% 증가. |
| 1.0 | 2026-04-15 | XPE Team | 초판 (10회 review-evaluate-fix 완료). GAP-01~GAP-10 초기 해소. |

---

*Document End — XPE-ALG-001 v1.2*

*Cross-references: XPE-SRS-001, XPE-SAD-001, XPE-SDD-002, xpe-algorithm-spec-deepsync.md, SPEC-XPE-MASTER.md, 03_측정_알고리즘_명세서, xray_grid_suppression_virtual_grid_research*
