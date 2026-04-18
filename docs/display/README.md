# X-ray FPD 디스플레이 처리 모듈 (xpe_display.dll)

**모듈**: `xpe_display.dll` (Layer 1, Phase 1b)
**안전 등급**: IEC 62304 Class B
**버전**: 1.1
**날짜**: 2026-04-16
**규범 사양**: [SPEC-XPE-MASTER v2.5.0](../../.moai/specs/SPEC-XPE-MASTER.md)

---

## 개요

`xpe_display.dll`은 X-ray Flat Panel Detector (FPD) 이미지 처리 엔진의 **표시 처리 모듈**입니다. 강화 도메인 float32 이미지를 입력받아 **DICOM Grayscale Standard Display Function (GSDF)** 파이프라인을 통해 표시 준비 완료된 uint16 출력으로 변환합니다.

### 핵심 특징 (Phase 1b 구현 범위)

> ⚠️ **구현 상태 공지 (2026-04-14)**
> Phase 1b (`SPEC-XPE-P1B-DISP`)에서 **SWU-3.1 ~ SWU-3.3이 구현 완료**되었습니다.
> **SWU-3.4 LUTManager는 Phase 1b 범위 외(OUT OF SCOPE)**이며 후속 릴리스에서 구현 예정입니다.
> GUI 통합 참조: [`docs/project/XPE-GUI-DISP-INT-001_Display_Integration_Guide.md`](../project/XPE-GUI-DISP-INT-001_Display_Integration_Guide.md)
> 대용량 비교 뷰어 참조: [`docs/project/XPE-GUI-COMPARE-001_Large_Image_Comparison_Viewer_Spec.md`](../project/XPE-GUI-COMPARE-001_Large_Image_Comparison_Viewer_Spec.md)

✓ **3개 소프트웨어 단위 구현됨** (SWU-3.1 ~ SWU-3.3)
⏸ **SWU-3.4 LUTManager** — 연기됨 (deferred), 후속 릴리스 예정
✓ **3개 의무 처리 단계**: Modality LUT → VOI LUT → Presentation LUT  
✓ **1개 핵심 형식 경계**: float32 → uint16 (Presentation LUT)  
✓ **4개 신체 부위 프리셋** (BONE/LUNG/ABDOMEN/HEAD) — `xpe_voi_preset_create()` 제공
✓ **DICOM PS3.14 GSDF 준수**: JND 기반 광도 매핑  
✓ **의료 display 호환**: 0.05 ~ 4000 cd/m² 범위  

---

## 문서 패키지

이 디렉토리는 IEC 62304 Class B 문서 패키지입니다. 역할별로 필요한 문서를 선택하세요:

| 역할 | 읽어야 할 문서 | 목적 |
|------|--------------|------|
| **소프트웨어 개발자** | xpe-display-prd.md → SRS → SAD | 파이프라인 구조, API, 알고리즘 이해 |
| **임상의 / QA 엔지니어** | README.md (이 파일) → SRS | 기능, 윈도우 프리셋, 성능 |
| **테스트 엔지니어** | TDS-DISPLAY-001 → RTM | 테스트 케이스, 합격 기준 |
| **안전/위험 담당자** | SHA-DISPLAY-001 → RTM | 위험 식별, 통제 방법 |
| **규제 담당자** | SRS → SAD → RTM → SHA | IEC 62304 추적성 패키지 |

### 문서 체계

```
┌──────────────────────────────────────────────────────────┐
│          디스플레이 모듈 문서 패키지 (v1.0)              │
│                                                          │
│  ┌─────────────────────────────────────┐                │
│  │   xpe-display-prd.md (PRD)          │                │
│  │   · 알고리즘 요구사항 원본           │                │
│  │   · 4개 SWU 상세 정의               │                │
│  │   · DICOM PS3.14 GSDF 수식          │                │
│  └──────────────┬──────────────────────┘                │
│                 │                                        │
│    ┌────────────┼─────────────┐                         │
│    v            v             v                         │
│  ┌──────────┐ ┌──────────┐ ┌──────────────┐            │
│  │   SRS    │ │   SAD    │ │    SHA       │            │
│  │  요건    │ │ 아키텍   │ │   위험분석   │            │
│  │명세서    │ │처 설계   │ │ (7개 위험)   │            │
│  └────┬─────┘ └────┬─────┘ └──────┬───────┘            │
│       │            │              │                     │
│       └────────────┼──────────────┘                     │
│                    │                                    │
│                    v                                    │
│             ┌──────────────┐                            │
│             │  RTM-001     │                            │
│             │ 요건 추적    │                            │
│             │ 행렬         │                            │
│             │(SRS↔SAD↔Test)│                            │
│             └──────────────┘                            │
│                    │                                    │
│                    v                                    │
│          TDS-DISPLAY-001                               │
│          (테스트 데이터명세)                             │
│                                                          │
│  ▶ 이 파일 (README.md) = 기술 개요 & 가이드             │
└──────────────────────────────────────────────────────────┘
```

---

## 데이터 흐름 다이어그램

### 처리 파이프라인

```
┌─────────────────────────────────────────────────────────────┐
│                    표시 처리 파이프라인                      │
│                                                             │
│  입력: float32 강화 도메인                                 │
│  범위: [0.0, 4095.0]                                      │
│  크기: 3072 × 3072 픽셀                                    │
│  출처: xpe_enhance_advanced.dll                           │
│                                                             │
│    ▼                                                       │
│  ┌──────────────────────────────────────┐                │
│  │  SWU-3.1 ModalityLUT                 │                │
│  │  · Slope/Intercept 또는 LUT 테이블   │                │
│  │  · 입력: float32, 출력: float32      │                │
│  │  · 시간: ~5ms                        │                │
│  │  · 인터페이스: xpe_modality_lut_*()  │                │
│  └──────────────────────────────────────┘                │
│    ▼                                                       │
│  ┌──────────────────────────────────────┐                │
│  │  SWU-3.2 VoiLUT (Window/Level)       │                │
│  │  · 선형 모드 (Linear)                │                │
│  │  · 시그모이드 모드 (Sigmoid)         │                │
│  │  · LUT 시퀀스 모드                   │                │
│  │  · 입력: float32, 출력: float32      │                │
│  │  · 시간: ~10-30ms                    │                │
│  │  · 인터페이스: xpe_voi_lut_*()       │                │
│  │  · 프리셋: 7가지 신체 부위            │                │
│  └──────────────────────────────────────┘                │
│    ▼                                                       │
│  ┌──────────────────────────────────────┐                │
│  │  SWU-3.3 PresentationLUT / GSDF      │                │
│  │  · DICOM PS3.14 Grayscale Standard   │                │
│  │  · luminance 매핑 (JND → cd/m²)     │                │
│  │  · 입력: float32, 출력: uint16       │                │
│  │  · 시간: ~5ms                        │                │
│  │  │                                    │                │
│  │  │  ⚠️  FORMAT BOUNDARY               │                │
│  │  │  float32 → uint16 변환             │                │
│  │  │                                    │                │
│  │  · 인터페이스: xpe_presentation_lut_*()
│  │  · GSDF LUT: 1024 JND 레벨           │                │
│  └──────────────────────────────────────┘                │
│    ▼                                                       │
│  ┌──────────────────────────────────────┐                │
│  │  SWU-3.4 LUTManager [⏸ DEFERRED]       │                │
│  │  · 프리셋 CRUD 관리                  │                │
│  │  · 자동 선택 (body_part 기반)        │                │
│  │  · 보간 (Cubic spline)                │                │
│  │  · 저장: JSON (~/.xpe/luts/)         │                │
│  │  · Phase 1b 범위 외, 후속 릴리스     │                │
│  └──────────────────────────────────────┘                │
│    ▼                                                       │
│  출력: uint16 표시 도메인                                │
│  범위: [0, 65535]                                        │
│  크기: 3072 × 3072 픽셀                                  │
│  다음: DICOM 인코딩, 디스플레이 렌더링                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## LUT 프리셋 라이브러리

### Factory Presets (기본 제공)

```
~/.xpe/luts/factory/
├── chest_pa.json          ← 흉부 정면
├── chest_lateral.json     ← 흉부 측면
├── extremity.json         ← 사지 (팔, 다리)
├── spine.json             ← 척추
├── abdomen.json           ← 복부 (간, 신장)
├── pediatric.json         ← 소아 (저선량)
└── fluoroscopy.json       ← 투시 (형광)
```

### 프리셋 자동 선택

```cpp
// 신체 부위 → 최적 프리셋 매핑
xpe_lut_auto_select("chest") → "chest_pa"
xpe_lut_auto_select("extremity") → "extremity_bone"
xpe_lut_auto_select("spine") → "spine_vertebral"
xpe_lut_auto_select("abdomen") → "abdomen_liver"
xpe_lut_auto_select("pediatric") → "pediatric_low_dose"
xpe_lut_auto_select("fluoroscopy") → "fluoroscopy_realtime"
```

### 사용자 정의 프리셋

```
~/.xpe/luts/user/
└── my_custom_chest.json  ← 임상의가 저장한 프리셋
```

---

## DICOM PS3.14 GSDF 공식

### 역함수 (p-value → luminance)

**GSDF는 의료용 display의 광도 특성을 정의합니다:**

```
log10(L) = a + c*ln(j) + e*(ln(j))² + g*(ln(j))³ + m*(ln(j))⁴

여기서:
  L = 광도 (cd/m²)
  j = JND (Just Noticeable Difference) index
  
  계수 (DICOM PS3.14에서):
    a = -2.0
    c = 2.4
    e = -0.0525
    g = -0.0205
    m = 0.0099
```

### 광도 범위

| 조건 | 광도 | 사용 예 |
|------|------|--------|
| 최소 (어둔 방) | 0.05 cd/m² | 야간 진료실 |
| 표준 (밝은 방) | 100 ~ 500 cd/m² | 일반 진료실 |
| 최대 (매우 밝음) | 4000 cd/m² | 밝은 수술실 |

### 정확도

- **JND 정확도**: ±10% 이내 (의료 display 표준)
- **Gamma Fallback**: 의료 display 없을 때 γ=2.2 사용 (일반 모니터 호환성)

---

## Window/Level 프리셋 상세

### 흉부 (Chest)

```
chest_pa (정면)
  · 폐: WC=-400, WW=1500 (미세 결절 감지)
  · 종격동: WC=40, WW=400 (심장 경계, 종격동 질환)
  · 뼈: WC=500, WW=2000 (늑골 골절, 척추)

chest_lateral (측면)
  · 심장: WC=0, WW=400 (심실 크기, 심낭)
  · 폐: WC=-400, WW=1500
```

### 골격 (Extremity)

```
extremity_bone
  · 뼈: WC=300, WW=1500 (골다공증, 미세균열)
  · 부드러운 조직: WC=50, WW=400 (종양, 염증)
```

### 복부 (Abdomen)

```
abdomen_liver
  · 간: WC=60, WW=150 (간경변, 종양)
  · 신장: WC=40, WW=350 (신장 결석, 종양)
  · 뼈: WC=400, WW=2000
```

### 소아 (Pediatric)

```
pediatric_low_dose
  · 낮은 선량에 최적화
  · 시그모이드 모드 (부드러운 전환)
  · WC=100, WW=500
```

### 투시 (Fluoroscopy)

```
fluoroscopy_realtime
  · 실시간 보기 최적화
  · 시그모이드 모드
  · WC=100, WW=500
```

---

## API 레퍼런스

> 아래는 **Phase 1b에 실제 구현된** `display_api.h` 기준 시그니처입니다.
> PRD/SAD의 시그니처와 일부 다를 수 있음에 유의하세요.

### Phase 1b 구현 완료 함수 (SWU-3.1 ~ SWU-3.3)

```c
// 버전 조회
const char* xpe_display_version(void);

// SWU-3.1 ModalityLUT: float32 → float32 (in-place)
XpeErrorCode xpe_apply_modality_lut(
    XpeImageBuffer* img,              // 입출력 버퍼 (in-place)
    const XpeModalityLutParams* params // slope/intercept 또는 LUT 테이블
);

// SWU-3.2 VoiLUT: float32 → float32 (in-place)
// mode: XPE_VOI_LINEAR / XPE_VOI_LINEAR_EXACT / XPE_VOI_SIGMOID
XpeErrorCode xpe_apply_voi_lut(
    XpeImageBuffer* img,
    const XpeVoiLutParams* params      // center, width, minOut, maxOut
);

// SWU-3.2 VoiLUT 프리셋 생성 (신체 부위 기반)
// bodyPart: XPE_BODY_BONE(0) / XPE_BODY_LUNG(1) / XPE_BODY_ABDOMEN(2) / XPE_BODY_HEAD(3)
XpeErrorCode xpe_voi_preset_create(
    XpeVoiLutParams* params,           // 출력: 채워진 프리셋
    XpeBodyPart bodyPart
);

// SWU-3.3 PresentationLUT: float32 → uint16 (in-place)
XpeErrorCode xpe_apply_presentation_lut(
    XpeImageBuffer* img,
    const XpePresentationLutParams* params // lutData[1024] + gsdfEnabled
);

// SWU-3.3 GSDF 보정: luminance 측정값으로 GSDF LUT 생성
XpeErrorCode xpe_gsdf_calibrate(
    const float* luminanceValues,      // 측정 광도 배열
    uint32_t count,                    // 배열 크기
    XpePresentationLutParams* outParams // 출력: GSDF LUT
);
```

### SWU-3.4 LUTManager (⏸ Phase 1b 범위 외 — 연기됨)

LUT Manager 기능(프리셋 CRUD, 자동 선택, 보간 등)은 Phase 1b에 구현되지 않았습니다.
현재 Phase 1b에서 프리셋은 `xpe_voi_preset_create()` 단일 함수로 제공됩니다.

---

## 성능 사양

### 처리 시간 (3072 × 3072 이미지)

| 단계 | 모드 | SPEC 예산 | 비고 |
|------|------|----------|------|
| ModalityLUT | 기본 | ≤ 20ms | SWU-3.1 구현됨 |
| VoiLUT | 선형/시그모이드 | ≤ 16ms | SWU-3.2 구현됨 |
| PresentationLUT | GSDF | ≤ 25ms | SWU-3.3 구현됨 |
| LUTManager | 선택 | — | ⏸ 연기됨 |
| **전체 파이프라인** | | **≤ 65ms** | SWU-3.1+3.2+3.3 합계 |

### 메모리 사용

| 항목 | 크기 | 참고 |
|------|------|------|
| 입력 버퍼 | 37.7 MB | float32, 3072×3072 |
| 출력 버퍼 | 18.9 MB | uint16, 3072×3072 |
| GSDF LUT | 4 KB | 1024 항목, 정적 |
| 워킹 메모리 | < 10 MB | 임시 버퍼 |
| **합계** | **< 60 MB** | — |

---

## 안전 기능

### 입력 검증

```
✓ Null 포인터 확인
✓ 버퍼 크기 검증 (≤ 4096 × 4096)
✓ Format 검사 (float32, uint16)
✓ NaN/Inf 감지 (부동소수점 이상값)
```

### 데이터 무결성

```
✓ 원본 데이터 보존 (read-only 접근)
✓ 메타데이터 추적 (XPE_FLAG_* 플래그)
✓ Clipping 감지 (XPE_FLAG_CLIPPED)
✓ 적용 프리셋 기록 (metadata.applied_lut_id)
```

### DICOM 준수

```
✓ DICOM (0028,1053)/(0028,1052) Modality LUT
✓ DICOM (0028,1050)/(0028,1051) VOI LUT
✓ DICOM PS3.14 Presentation LUT (GSDF)
✓ 출력: DX IOD 또는 XC IOD 호환
```

### 임상 안전성

```
✓ 기본 Window 프리셋 제공 (7가지)
✓ 자동 선택으로 사용자 실수 방지
✓ WW 범위 검증 (WW > 0)
✓ GSDF 편차 > 10% 경고
```

---

## 문제 해결 가이드

### Window가 어둡거나 밝음

**원인**: 부적절한 Window Center/Width  
**해결**: 
1. 신체 부위별 기본 프리셋 사용 (자동 선택)
2. 임상의가 WC/WW 수동 조정
3. 자동 window 기능 활성화 (히스토그램 기반)

### 일부 픽셀이 손상됨 (Clipping)

**원인**: Window/Level이 너무 좁아 정보 손실  
**해결**:
1. 경고 확인: "이미지가 부분 클리핑됨"
2. Window Width 증가
3. 진단 신뢰도 검토

### GSDF 광도가 표준과 다름

**원인**: 의료 display 보정 부족  
**해결**:
1. Display calibration 수행
2. Peak luminance 측정 (전문 계측기)
3. `xpe_gsdf_set_display_params()` 호출로 파라미터 설정
4. `xpe_gsdf_check_display_capability()`로 검증

### 프리셋 저장/로드 실패

**원인**: 파일 시스템 권한 문제  
**해결**:
1. `~/.xpe/luts/user/` 디렉토리 권한 확인 (write 필요)
2. 디스크 용량 확인
3. 시스템 로그 확인 (I/O 오류)

---

## 참고문헌

### DICOM 표준

- **DICOM PS3.3**: Information Object Definitions
  - (0028,1053) Rescale Slope
  - (0028,1052) Rescale Intercept
  - (0028,1050)/(0028,1051) Window Center/Width
  - (0028,3000) Modality LUT Sequence
  - (0028,3010) VOI LUT Sequence

- **DICOM PS3.14**: Grayscale Rendering
  - §3.1 GSDF (Grayscale Standard Display Function)

### 논문

- **Barten, P. G. (1999)** — GSDF 원본 논문 (DICOM PS3.14 기반)
- **AAPM TG-30** — Medical Display 보정 기준
- **AAPM TG-232** — Exposure Index (EI) 임상 응용

### 프로젝트 문서

| 문서 | 경로 | 설명 |
|------|------|------|
| PRD | xpe-display-prd.md | 알고리즘 요구사항 (SWU-3.4 포함 전체 설계) |
| SRS | SRS-DISPLAY-001.md | 소프트웨어 요건 |
| SAD | SAD-DISPLAY-001.md | 아키텍처 설계 |
| SHA | SHA-DISPLAY-001.md | 위험 분석 (7개 위험) |
| RTM | RTM-DISPLAY-001.md | 추적성 (100% 커버) |
| **GUI 통합 가이드** | **../project/XPE-GUI-DISP-INT-001_Display_Integration_Guide.md** | **Phase 1b → GUI 연동 교차검증 및 구현 명세** |
| **GUI 비교 뷰어 사양** | **../project/XPE-GUI-COMPARE-001_Large_Image_Comparison_Viewer_Spec.md** | **4096x4096 16-bit 원본/처리 비교, swipe/zoom/pan 설계** |

---

## 연락처

- **개발**: XPE 디스플레이팀
- **QA**: XPE QA팀
- **안전**: XPE 안전팀

---

## 버전 이력

| 버전 | 날짜 | 저자 | 설명 |
|------|------|------|------|
| 1.0 | 2026-04-14 | XPE 디스플레이팀 | 초기 배포 |
| 1.1 | 2026-04-16 | MoAI | GUI 통합 완료 반영. XPE-GUI-COMPARE-001 v0.2.0 구현 완료. SPEC-XPE-MASTER v2.5.0 동기화. |

---

**문서 끝**  
*xpe_display.dll README v1.0*
