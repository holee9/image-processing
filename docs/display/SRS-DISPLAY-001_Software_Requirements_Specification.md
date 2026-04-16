# SRS-DISPLAY-001: 소프트웨어 요건 명세서

**문서 ID**: SRS-DISPLAY-001  
**IEC 62304 절**: 5.2 소프트웨어 요구사항  
**안전 분류**: Class B  
**모듈**: `xpe_display.dll`  
**버전**: 1.1  
**날짜**: 2026-04-16  
**작성자**: XPE 디스플레이팀  
**승인**: __________________ 날짜: __________

---

## 1. 목적

XPE Display Processing Module의 소프트웨어 요구사항을 정의합니다. 이 문서는 IEC 62304 §5.2 소프트웨어 요구사항에 해당하며, 제품 요구사항서(PRD)의 기술적 요구사항을 상세화합니다.

---

## 2. 요구사항 구조

4개 카테고리로 분류:
- **기능 요구사항** (FR): MUST, MAY 기능 정의
- **성능 요구사항** (PERF): 타이밍, 처리량, 메모리
- **안전 요구사항** (SAFE): 위험 통제, 데이터 무결성
- **인터페이스 요구사항** (IF): DICOM 표준, API 계약

---

## 3. 기능 요구사항 (FUNC)

### 3.1 ModalityLUT 요구사항 (FR-MODAL-xxx)

| ID | 요구사항 | 상세 | 시험 | 우선도 |
|----|---------|------|------|--------|
| FR-MODAL-101 | Slope/Intercept 지원 | `output = slope × input + intercept` 공식 정확 구현 | 회귀 테스트: Golden Reference 비교 (±1 LSB) | **MUST** |
| FR-MODAL-102 | LUT 테이블 지원 | 최대 65536 항목 LUT 읽기 및 적용 | 합성 LUT (선형, 비선형) 테스트 | **MUST** |
| FR-MODAL-103 | 선형 보간 | LUT 항목 간 선형 보간 지원 | 부분 인덱스 입력 (예: index 512.5) | MAY |
| FR-MODAL-104 | Negative 값 처리 | CT HU < 0 (물, 지방) 지원 | HU=-1000 (공기), 0 (물), 50 (지방) 입력 | **MUST** |
| FR-MODAL-105 | DICOM 태그 읽기 | (0028,1053) Rescale Slope, (0028,1052) Intercept 파싱 | DICOM 파일에서 태그 추출 | **MUST** |
| FR-MODAL-106 | (0028,3000) 지원 | Modality LUT Sequence 읽기 (LUT 항목 시퀀스) | DICOM 파일 수동 검증 | MAY |
| FR-MODAL-107 | 범위 검증 | slope > 0, |intercept| ≤ 1000, 모든 LUT 항목 유효 범위 검증 | 경계값 입력 (0, ±1000, ±10000) | **MUST** |
| FR-MODAL-108 | 에러 처리 | 잘못된 slope/intercept → `XPE_ERR_INVALID_PARAM` 반환 | null 포인터, 범위 초과 값 입력 | **MUST** |

### 3.2 VOI LUT 요구사항 (FR-VOI-xxx)

| ID | 요구사항 | 상세 | 시험 | 우선도 |
|----|---------|------|------|--------|
| FR-VOI-201 | 선형 모드 구현 | 공식: `output = clip((input - (wc - ww/2)) / ww * MAX, 0, MAX)` | 기울기, 절편, 클리핑 테스트 | **MUST** |
| FR-VOI-202 | 시그모이드 모드 구현 | 공식: `output = MAX / (1 + exp(-4 * (input - wc) / ww))` | exp 정확도 ≤ 0.1% | **MUST** |
| FR-VOI-203 | LUT 시퀀스 모드 | (0028,3010) VOI LUT Sequence 적용 | DICOM 파일 LUT 시퀀스 검증 | MAY |
| FR-VOI-204 | Preset 라이브러리 | 최소 6가지 신체 부위 프리셋: chest_pa, chest_lateral, extremity, spine, abdomen, pediatric | 각 프리셋 임상 검증 | **MUST** |
| FR-VOI-205 | 자동 Window 계산 | EI-derived 통계에서 WC/WW 계산 (선택) | 히스토그램 기반 자동 window | MAY |
| FR-VOI-206 | 동적 Windowing | 임상의가 마우스 드래그로 WC/WW 실시간 변경 | GUI 상호작용 시뮬레이션 | MAY |
| FR-VOI-207 | WW 검증 | WW > 0, WC ≤ 4096 검증 | WW=0 (거부), WW<0 (거부), WC=5000 (거부) | **MUST** |
| FR-VOI-208 | (0028,1050)/(0028,1051) | DICOM Window Center/Width 태그 읽기 | DICOM 파일 WC/WW 추출 | **MUST** |
| FR-VOI-209 | 빠른 경로 | 사전 계산된 4096-entry LUT로 고속 적용 | 성능: ≤ 10ms (3072×3072) | MAY |

### 3.3 Presentation LUT / GSDF 요구사항 (FR-GSDF-xxx)

| ID | 요구사항 | 상세 | 시험 | 우선도 |
|----|---------|------|------|--------|
| FR-GSDF-301 | GSDF 역함수 구현 | `log10(L) = a + c*ln(j) + e*(ln(j))^2 + ...` (DICOM PS3.14) | Barten 1999 참고, ±1% 정확도 | **MUST** |
| FR-GSDF-302 | 순함수 구현 | luminance → p-value 역계산 (Newton-Raphson) | 수치 수렴 ≤ 1e-6 | **MUST** |
| FR-GSDF-303 | 광도 범위 | [0.05, 4000] cd/m² 지원 | 경계값 및 일반 의료 display (100~500 cd/m²) | **MUST** |
| FR-GSDF-304 | JND 정확도 | 최대 광도 편차 ≤ 10% | Golden Reference display와 비교 | **MUST** |
| FR-GSDF-305 | Display 보정 | Peak luminance, ambient illumination, gamma 파라미터 지원 | Calibration 데이터 입력 | MAY |
| FR-GSDF-306 | Gamma Fallback | 의료 display 없을 때 γ=2.2 사용 | 일반 모니터 호환성 테스트 | **MUST** |
| FR-GSDF-307 | 메모리 효율 | 1024-entry LUT (4KB) 이하 | 정적 메모리 할당 | **MUST** |
| FR-GSDF-308 | Format 변환 | float32 (0~4095) → uint16 (0~65535) | 손실 ≤ 1 LSB (1/65535) | **MUST** |

### 3.4 LUT Manager 요구사항 (FR-LUT-xxx) — ⏸ Phase 1b 범위 외

> **[DEFERRED]** 이 섹션의 요구사항(`FR-LUT-401 ~ FR-LUT-409`)은 `SPEC-XPE-P1B-DISP`에서 **OUT OF SCOPE**로 명시되었습니다.
> 후속 릴리스(Phase 1c 또는 Phase 2)에서 구현 예정입니다.
> Phase 1b에서는 `xpe_voi_preset_create(params, bodyPart)` 함수가 4개 신체 부위(BONE/LUNG/ABDOMEN/HEAD)의 기본 프리셋을 제공합니다.

| ID | 요구사항 | 상세 | 시험 | 우선도 | 상태 |
|----|---------|------|------|--------|------|
| FR-LUT-401 | Preset 저장 | `xpe_lut_add_preset(preset, lut_id)` 구현 | 유효한 프리셋, 중복 ID 거부 | **MUST** | ⏸ 연기됨 |
| FR-LUT-402 | Preset 조회 | `xpe_lut_get_preset(lut_id, output)` 구현 | 존재하는 ID 조회 성공, 비존재 ID 거부 | **MUST** | ⏸ 연기됨 |
| FR-LUT-403 | Preset 삭제 | `xpe_lut_remove_preset(lut_id)` 구현 | 사용자 프리셋 삭제, Factory 거부 | **MUST** | ⏸ 연기됨 |
| FR-LUT-404 | 자동 선택 | `xpe_lut_auto_select(body_part)` → 최적 LUT ID | Chest → chest_pa, Extremity → extremity | **MUST** | ⏸ 연기됨 |
| FR-LUT-405 | 보간 | 두 프리셋 간 cubic spline 보간 | 보간 계수 t ∈ [0, 1] 테스트 | MAY | ⏸ 연기됨 |
| FR-LUT-406 | JSON 지속성 | `~/.xpe/luts/factory/`, `~/.xpe/luts/user/` 저장 | 파일 쓰기/읽기 검증 | **MUST** | ⏸ 연기됨 |
| FR-LUT-407 | Factory Presets | 7가지 기본 프리셋 읽기 전용 | chest_pa, chest_lateral, extremity, spine, abdomen, pediatric, fluoroscopy | **MUST** | ⏸ 연기됨 |
| FR-LUT-408 | Preset 리스트 | `xpe_lut_list_presets(list, count)` 구현 | 모든 활성 프리셋 열거 | **MUST** | ⏸ 연기됨 |
| FR-LUT-409 | 버전 관리 | 각 프리셋에 버전 정보 포함 | 호환성 검증 | MAY | ⏸ 연기됨 |

---

## 4. 성능 요구사항 (PERF)

### 4.1 처리 시간 (PERF-TIME-xxx)

| ID | 요구사항 | 상세 | 합격 기준 | 측정 환경 |
|----|---------|------|---------|---------|
| PERF-TIME-101 | ModalityLUT 시간 | Slope/Intercept 적용 | ≤ 5ms (3072×3072 float32) | Intel i7-11700, 8GB RAM |
| PERF-TIME-102 | VoiLUT 선형 시간 | Window/Level 선형 모드 | ≤ 10ms (3072×3072) | 동일 환경 |
| PERF-TIME-103 | VoiLUT 시그모이드 시간 | Window/Level 시그모이드 모드 | ≤ 30ms (3072×3072) | 동일 환경 |
| PERF-TIME-104 | PresentationLUT 시간 | GSDF 적용 | ≤ 5ms (3072×3072) | 동일 환경 |
| PERF-TIME-105 | LUTManager 선택 시간 | 프리셋 선택 | ≤ 1ms | 동일 환경 |
| PERF-TIME-106 | LUTManager 보간 시간 | Cubic spline 보간 | ≤ 5ms | 동일 환경 |
| PERF-TIME-107 | 전체 파이프라인 | 모든 단계 순차 (4 SWU) | ≤ 40ms (Phase 1b 예산 내) | 동일 환경 |
| PERF-TIME-108 | GSDF LUT 생성 (일회) | 시작 시에만 | ≤ 500ms | 동일 환경 |

### 4.2 메모리 (PERF-MEM-xxx)

| ID | 요구사항 | 상세 | 합격 기준 | 참고 |
|----|---------|------|---------|-------|
| PERF-MEM-101 | 입력 버퍼 | float32 이미지 | 37.7 MB (3072×3072) | 공유 포인터 |
| PERF-MEM-102 | 출력 버퍼 | uint16 이미지 | 18.9 MB (3072×3072) | 공유 포인터 |
| PERF-MEM-103 | GSDF LUT | 1024-entry 테이블 | ≤ 4 KB | 정적, 일회 |
| PERF-MEM-104 | Modality LUT | 최대 65536 항목 | ≤ 256 KB | 선택 |
| PERF-MEM-105 | VOI LUT | 최대 65536 항목 | ≤ 256 KB | 선택 |
| PERF-MEM-106 | Preset 캐시 | JSON 역직렬화 메모리 | < 1 MB | 동적 할당 |
| PERF-MEM-107 | 최고 메모리 | 모든 버퍼 + 캐시 | < 60 MB | 표시 처리만 |

### 4.3 처리량 (PERF-THRU-xxx)

| ID | 요구사항 | 상세 | 합격 기준 | 참고 |
|----|---------|------|---------|-------|
| PERF-THRU-101 | 이미지율 | 1280×960 @ 30 fps | 33ms/frame 내 처리 | Fluoroscopy |
| PERF-THRU-102 | 이미지율 | 3072×3072 @ 2-5 fps | 200-500ms/frame 내 처리 | 일반 X선 |

---

## 5. 안전 요구사항 (SAFE)

### 5.1 데이터 무결성 (SAFE-DATA-xxx)

| ID | 요구사항 | 상세 | 통제 | 영향 |
|----|---------|------|------|------|
| SAFE-DATA-101 | 입력 보존 | 입력 버퍼를 read-only로 접근, 별도 출력 버퍼 사용 | 포인터 const 검증 | 원본 데이터 손상 방지 |
| SAFE-DATA-102 | Null 포인터 검사 | 모든 함수 진입점에서 null 검사 | 매개변수 유효성 검증 | Crash 방지 |
| SAFE-DATA-103 | 버퍼 크기 검증 | width, height ≤ 4096 검증 | 범위 검사 | 메모리 초과 할당 방지 |
| SAFE-DATA-104 | Format 경계 검증 | float32 → uint16 conversion 전 범위 검사 ([0, 4095]) | NaN/Inf 거부, clamp 적용 | Format 손상 방지 |
| SAFE-DATA-105 | 메타데이터 추적 | XPE_FLAG_PRESENTATION_APPLIED 설정 | 플래그 비트 관리 | 적용된 단계 추적 |

### 5.2 DICOM 준수 (SAFE-DICOM-xxx)

| ID | 요구사항 | 상세 | 통제 | 영향 |
|----|---------|------|------|------|
| SAFE-DICOM-101 | 태그 매핑 | (0028,1053), (0028,1052), (0028,1050), (0028,1051) 정확 파싱 | DICOM 파서 검증 | 태그 혼동 방지 |
| SAFE-DICOM-102 | PS3.14 준수 | GSDF 수식 정확 | Barten 1999 참고 논문과 비교 | 의료 display 호환성 |
| SAFE-DICOM-103 | 출력 IOD 호환성 | 생성된 uint16 이미지가 DX/XC IOD 호환 | IOD validation tool 사용 | PACS 전송 호환성 |

### 5.3 임상 안전성 (SAFE-CLIN-xxx)

| ID | 요구사항 | 상세 | 통제 | 영향 |
|----|---------|------|------|------|
| SAFE-CLIN-101 | Window 기본값 | 기본값 부재 → 에러 반환, 기본값 제공 | Preset 자동 선택 | 잘못된 Window 초기화 방지 |
| SAFE-CLIN-102 | Clipping 알림 | 클리핑 발생 시 메타데이터 플래그 설정 | 플래그: XPE_FLAG_CLIPPED | 진단 손실 추적 |
| SAFE-CLIN-103 | GSDF 편차 경고 | 광도 편차 > 10% 경고 알림 | 임계값 검사 | 보정되지 않은 display 감지 |
| SAFE-CLIN-104 | Preset 추적성 | 적용된 LUT ID 메타데이터 저장 | metadata.applied_lut_id 필드 | 감사 추적 |

---

## 6. 인터페이스 요구사항 (IF)

### 6.1 내부 인터페이스 (IF-INT-xxx)

| ID | 요구사항 | 상세 | 호출 규약 |
|----|---------|------|---------|
| IF-INT-301 | xpe_modality_lut_apply | Slope/Intercept 적용 | `XpeErrorCode (*)(XpeImageBuffer* in, float slope, float intercept, XpeImageBuffer* out)` |
| IF-INT-302 | xpe_modality_lut_apply_table | LUT 테이블 적용 | `XpeErrorCode (*)(XpeImageBuffer* in, float* lut, uint32_t size, XpeImageBuffer* out)` |
| IF-INT-303 | xpe_voi_lut_apply_linear | Window/Level 선형 | `XpeErrorCode (*)(XpeImageBuffer* in, float wc, float ww, XpeImageBuffer* out)` |
| IF-INT-304 | xpe_voi_lut_apply_sigmoid | Window/Level 시그모이드 | `XpeErrorCode (*)(XpeImageBuffer* in, float wc, float ww, XpeImageBuffer* out)` |
| IF-INT-305 | xpe_presentation_lut_apply | GSDF 적용 | `XpeErrorCode (*)(XpeImageBuffer* in, float* gsdf_lut, XpeImageBuffer* out)` |
| IF-INT-306 | xpe_lut_auto_select | 자동 선택 | `XpeErrorCode (*)(const char* body_part, char* out_lut_id)` |
| IF-INT-307 | xpe_lut_get_preset | Preset 조회 | `XpeErrorCode (*)(const char* lut_id, XpeLutPreset* out)` |
| IF-INT-308 | xpe_lut_add_preset | Preset 저장 | `XpeErrorCode (*)(const XpeLutPreset* preset, const char* lut_id)` |

### 6.2 외부 인터페이스 (IF-EXT-xxx)

| ID | 요구사항 | 상세 | 프로토콜 |
|----|---------|------|---------|
| IF-EXT-301 | DICOM 읽기 | (0028,1053), (0028,1052) 등 태그 읽기 | DICOM PS3.3 |
| IF-EXT-302 | 메타데이터 전달 | ImageMetadata 구조체로 body_part, EI 통과 | 공유 메모리 포인터 |
| IF-EXT-303 | 에러 반환 | XPE_OK, XPE_ERR_INVALID_PARAM 등 | C enum |

### 6.3 Host GUI Comparison Interface (IF-GUI-xxx)

| ID | 요구사항 | 상세 | 프로토콜 |
|----|---------|------|---------|
| IF-GUI-301 | 원본/처리 레이어 분리 | `xpe_display.dll` 출력은 GUI의 processed layer로 전달되어야 하며 source layer를 덮어쓰지 않아야 한다. | C# P/Invoke / `LoadedImageFrame` |
| IF-GUI-302 | 동기화 viewport | GUI는 source와 processed layer에 동일한 zoom/pan/좌표계를 적용해야 한다. | `ImageComparisonViewport` |
| IF-GUI-303 | 비교 상태 증거화 | GUI는 comparison mode, zoom, pan, divider position, source/processed identity를 evidence export에 포함해야 한다. | JSON evidence bundle |
| IF-GUI-304 | 대용량 영상 경계 | Phase 1b GUI는 4096x4096 UInt16 source + processed 비교를 지원하고, 그 이상의 크기는 tile/cache 설계 승인 후 claim한다. | GUI rendering contract |

---

## 7. 검증 요구사항

| ID | 요구사항 | 방법 | 증거 |
|----|---------|------|------|
| VER-FUNC-001 | 모든 FR 검증 | 회귀 테스트 (xpe_display_test.cpp) | TDS-DISPLAY-001 |
| VER-PERF-001 | 성능 요구사항 | Profiling + 시간 측정 | Benchmark report |
| VER-SAFE-001 | 안전 요구사항 | Code review + FEA | SHA-DISPLAY-001 |
| VER-DICOM-001 | DICOM 준수 | DCMTK validator 사용 | DICOM 파일 검증 |
| VER-GUI-001 | GUI source-vs-processed 비교 | 4096x4096 RAW E2E + swipe/zoom/pan automation | GUI evidence report |

---

## 8. 추적성

모든 요구사항은 다음 문서로 추적됩니다:
- **SAD-DISPLAY-001**: 아키텍처 설계
- **RTM-DISPLAY-001**: 요구사항 추적 행렬
- **TDS-DISPLAY-001**: 테스트 데이터 명세
- **SHA-DISPLAY-001**: 위험 분석

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-14 | XPE Display Team | Initial release |
| 1.1 | 2026-04-16 | MoAI | Added GUI Comparison Interface requirements (IF-GUI-301~304). Implemented in XPE-GUI-COMPARE-001 v0.2.0. |

---

*문서 끝 — SRS-DISPLAY-001 v1.0*
