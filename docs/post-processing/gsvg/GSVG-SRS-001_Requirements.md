# GSVG-SRS-001: 소프트웨어 요구사항 명세

**문서 ID:** GSVG-SRS-001  
**버전:** 1.0 | **작성일:** 2026-04-03  
**IEC 62304 조항:** 5.2  
**안전 분류:** Class B

---

## 1. 범위

X-ray flat panel detector 시스템 영상에 대한 두 가지 독립 기능:

1. **Grid Suppression (GS):** 물리적 anti-scatter grid 사용 영상의 grid line artifact 제거
2. **Virtual Grid (VG):** Grid 미사용 영상의 scatter radiation 소프트웨어 보정

---

## 2. Grid Suppression 기능 요구사항

| ID | 요구사항 | 근거 | 검증 |
|----|-------------|-----------|--------------|
| GS-FR-001 | DICOM 헤더 및 grid specification으로부터 grid line frequency를 자동 계산해야 한다 | Grid frequency는 detector pixel pitch와 grid line density의 aliasing으로 결정됨 (Lin 2006) | Test |
| GS-FR-002 | 2D DWT (Discrete Wavelet Transform)를 사용하여 입력 영상을 multi-scale sub-band로 분해해야 한다 | DWT는 spatial-frequency 동시 분석이 가능하여 grid signal과 anatomy 분리에 최적 (Tang 2015) | Test |
| GS-FR-003 | 각 sub-band에서 gridline signal energy가 threshold를 초과하는지 자동 검출해야 한다 | Auto-stop condition으로 over-decomposition 방지 (Tang 2015) | Test |
| GS-FR-004 | 검출된 sub-band에 Gaussian band-stop filter를 적용하여 gridline signal을 제거해야 한다 | Gaussian shape은 notch filter 대비 ringing artifact 최소화 (Lin 2006) | Test |
| GS-FR-005 | 처리 후 영상의 gridline artifact가 시각적으로 인지 불가해야 한다 | 잔류 artifact는 진단 방해 가능 (HAZ-005) | Test + Review |
| GS-FR-006 | 처리 후 영상의 MTF 저하는 원본 대비 5% 이내여야 한다 | 과도한 filtering은 진단 해상도 저하 | Test |
| GS-FR-007 | 60~200 lines/inch 범위의 grid에 대해 동작해야 한다 | 시장 유통 grid의 실질적 범위 | Test |
| GS-FR-008 | Moiré pattern (aliasing artifact) 제거를 지원해야 한다 | Detector-grid frequency aliasing으로 발생하는 주요 artifact 유형 | Test |

---

## 3. Virtual Grid 기능 요구사항

| ID | 요구사항 | 근거 | 검증 |
|----|-------------|-----------|--------------|
| VG-FR-001 | 입력 영상과 촬영 조건(kVp, mAs, SID, field size)으로부터 body equivalent thickness를 추정해야 한다 | Thickness는 SPR의 주요 결정 요소 (Kyriakou 2007) | Test |
| VG-FR-002 | 추정된 thickness와 촬영 조건으로부터 Scatter-to-Primary Ratio (SPR)를 계산해야 한다 | SPR은 scatter correction 강도를 결정하는 핵심 파라미터 | Test |
| VG-FR-003 | 사전 계산된 scatter kernel LUT를 사용하여 scatter distribution을 추정해야 한다 | MC-based LUT는 real-time 가능하면서 물리적 정확도 확보 (Philips SkyFlow Plus 방식) | Test |
| VG-FR-004 | 추정된 scatter를 원본 영상에서 차감하여 primary-only 영상을 생성해야 한다 | `I_primary = I_total - I_scatter` | Test |
| VG-FR-005 | Laplacian Pyramid decomposition으로 multi-scale contrast enhancement를 수행해야 한다 | US8064676B2 특허 공개 알고리즘 | Test |
| VG-FR-006 | 고주파 band에 대해 de-noising을 수행해야 한다 | Scatter subtraction 후 noise 증폭 문제 (Lim 2023) | Test |
| VG-FR-007 | 출력 영상의 CNR은 동일 조건 6:1 physical grid 영상 대비 90% 이상이어야 한다 | 임상적으로 의미있는 최소 성능 기준 | Test |
| VG-FR-008 | 사용자가 가상 grid ratio (6:1, 8:1, 10:1, 12:1)를 선택할 수 있어야 한다 | 검사 부위/환자 체형에 따른 유연성 (Fujifilm Virtual Grid 제공 기능) | Test |
| VG-FR-009 | 10cm~30cm acrylic thickness 범위에서 동작해야 한다 | 소아~비만 환자 범위 커버 | Test |
| VG-FR-010 | 처리 후 인체 구조물에 인위적 artifact가 생성되지 않아야 한다 | Overcorrection artifact는 오진 유발 가능 (HAZ-003) | Test + Review |

---

## 4. 성능 요구사항

| ID | 요구사항 | 근거 | 검증 |
|----|-------------|-----------|--------------|
| PERF-001 | 3072×3072 16-bit 영상 처리 시간 ≤ 1.0초 (Intel i7 또는 동급) | 임상 워크플로우 지연 최소화 (HAZ-006) | Test |
| PERF-002 | Peak memory usage ≤ 512MB per frame | Console PC 메모리 제약 | Test |
| PERF-003 | Batch mode 연속 100 frame 처리 시 memory leak 없음 | 장시간 운영 안정성 | Test |
| PERF-004 | 출력 영상 bit depth는 입력과 동일 (16-bit) | DICOM conformance, downstream 호환성 | Test |

---

## 5. 인터페이스 요구사항

| ID | 요구사항 | 검증 |
|----|-------------|--------------|
| IF-001 | 입력: DICOM Part 10 파일 또는 Raw pixel array + metadata struct | Test |
| IF-002 | 출력: 동일 format의 처리된 영상 + processing log (JSON) | Test |
| IF-003 | 에러 발생 시 원본 영상을 unmodified로 pass-through (→ SAFE-003) | Test |
| IF-004 | Processing parameters를 JSON config 파일로 설정 가능 | Test |
| IF-005 | API는 C++ shared library (.so/.dll) + C-compatible header로 제공 | Test |

---

## 6. 안전 요구사항

| ID | 요구사항 | 위험 참조 | 근거 | 검증 |
|----|-------------|------------|-----------|--------------|
| SAFE-001 | 알고리즘 실패 시 원본 영상을 훼손하지 않아야 한다 | HAZ-001 | 원본 데이터 무결성 보호 | Test |
| SAFE-002 | 처리 영상에 "Processed" marking을 DICOM tag에 기록해야 한다 | HAZ-002 | 처리/미처리 영상 식별 보장 | Test |
| SAFE-003 | 처리 실패 시 에러 코드와 함께 원본 영상을 반환해야 한다 | HAZ-001 | Fail-safe: 최소한 원본은 확보 | Test |
| SAFE-004 | Scatter correction 강도가 물리적 최대값을 초과하지 않도록 clamping | HAZ-003 | SPR > 물리적 한계 시 overcorrection 방지 | Test |
| SAFE-005 | 출력 영상의 pixel value 범위는 유효 DICOM 범위 내 (0~65535) | HAZ-004 | Arithmetic overflow 방지 | Test |

---

## 개정 이력

| 버전 | 작성일 | 작성자 | 설명 |
|---------|------|--------|-------------|
| 1.0 | 2026-04-03 | — | 초기 배포 |
