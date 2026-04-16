# SRS-AI-001 소프트웨어 요구사항 명세서 (Software Requirements Specification)

**문서 ID**: SRS-AI-001  
**모듈**: xpe_ai (AI Framework)  
**버전**: 1.0.0  
**날짜**: 2026-04-14  
**IEC 62304 Section**: §5.2 Software Requirements  
**관련 문서**: xpe-ai-prd.md, SAD-AI-001, SHA-AI-001, RTM-AI-001

---

## 1. 개요

SRS-AI-001은 xpe_ai 모듈의 **기능 요구사항(FR)**, **안전 요구사항(SAF)**, **성능 요구사항(PERF)**을 IEC 62304 §5.2 형식으로 정의합니다.

---

## 2. 기능 요구사항 (Functional Requirements, FR)

### 2.1 워커 프로세스 생명주기

#### FR-AI-100: 워커 프로세스 시작
**상태**: MANDATORY  
**우선순위**: HIGH  
**출처**: PRD §워커 생명주기

워커 프로세스는 다음 조건에서 시작되어야 한다:
1. xpe_ai.dll 초기화 시 최초 1회 (startup)
2. 워커 크래시 감지 후 자동 재시작 (최대 3회)
3. 세션 동안 계속 실행

**가능성 기준 (Acceptance Criteria)**:
- 워커 시작 후 5초 내 `XPE_AI_WORKER_ALIVE` 신호 생성
- 신호 실패 → `XPE_ERR_WORKER_STARTUP_FAILED` 반환
- 최대 3회 재시작 제한; 3회 실패 후 AI 기능 비활성화

#### FR-AI-110: 심박 모니터링 (Heartbeat)
**상태**: MANDATORY  
**우선순위**: HIGH

xpe_ai.dll은 워커 상태를 1Hz 간격으로 모니터링해야 한다:
- Named event `XPE_AI_WORKER_ALIVE` 검사
- 3회 연속 미신호 → 워커 사망 선언
- 즉시 재시작 로직 트리거

**가능성 기준**:
- 심박 체크 주기: 1000ms ± 100ms
- 메모리 오버헤드: < 2MB (모니터 스레드)

#### FR-AI-120: IPC 타임아웃 및 오류 복구
**상태**: MANDATORY  
**우선순위**: CRITICAL

모든 IPC 요청은 5000ms 타임아웃을 적용해야 한다:
```
xpe_ai.dll: 요청 전송
            ↓ (wait 5000ms)
xpe_ai_worker.exe: 응답 반환 또는 타임아웃

if (timeout):
    increment_timeout_counter()
    if (timeout_counter >= 3):
        set_AI_UNAVAILABLE()
    else:
        trigger_worker_restart()
```

**가능성 기준**:
- 타임아웃 정확도: 5000ms ± 50ms
- 타임아웃 3회 연속 → AI 기능 전체 비활성화

---

### 2.2 신체 부위 인식 인터페이스 (SWU-2.7)

#### FR-AI-130: Body Part Classification 함수
**상태**: MANDATORY  
**우선순위**: HIGH

```c
int xpe_ai_body_part_classify(
    const float* image,      // 512x512 float32
    int width, int height,
    char* output_json,       // sidecar JSON
    int max_json_len
);
```

**동작**:
- 입력: 512×512 float32 그레이스케일
- 처리: MobileNet-v3-Small ONNX 추론
- 출력: JSON sidecar (body_part, confidence, top3, model_version)
- 신뢰도 < 0.70 → "UNKNOWN"

**가능성 기준**:
- 정확도: Top-1 ≥ 95% (15개 부위)
- 레이턴시: ≤ 300ms (포함: 리사이즈 + 추론)
- OOD 감지: confidence < 0.70 시 자동 "UNKNOWN" 반환

#### FR-AI-140: 출력 Sidecar 관리
**상태**: MANDATORY  
**우선순위**: MEDIUM

신체 부위 결과는 JSON sidecar로 저장:
- 파일명: `{image_path}.bodypart.json`
- 내용: body_part, confidence, model_version, timestamp_ms, top3
- 위치: 원본 이미지와 동일 디렉토리

**가능성 기준**:
- Sidecar 생성 성공률: 100% (오류 시 로그만 기록)
- JSON 형식: UTF-8, 유효한 JSON

---

### 2.3 AI 조명 ROI 정제 인터페이스 (SWU-2.8-AI)

#### FR-AI-150: AI Collimation Refinement 함수
**상태**: MANDATORY  
**우선순위**: MEDIUM

```c
int xpe_ai_refine_collimation(
    const float* image,
    int width, int height,
    const XpeROI* baseline_roi,
    XpeROI* refined_roi_out,
    char* output_json,
    int max_json_len
);
```

**동작**:
- 입력: 로그 도메인 이미지 + baseline ROI
- 처리: U-Net 엣지 감지
- 출력: 정제된 ROI (축소만 가능)
- 신뢰도 < 0.65 → 원본 ROI 반환

**가능성 기준**:
- ROI 수정: 축소만 (확대 금지)
- 엣지 정확도: IOU ≥ 0.92
- 레이턴시: ≤ 150ms

---

### 2.4 이미지 스티칭 인터페이스 (SWU-2.9)

#### FR-AI-160: Image Stitching 함수
**상태**: CONDITIONAL  
**우선순위**: MEDIUM

```c
int xpe_ai_stitch_images(
    const float** images,    // 이미지 배열
    int num_images,          // 2-6개
    int width, int height,
    float* stitched_out,     // 출력 panoramic
    int* out_width, int* out_height,
    char* diagnostic_json,   // 정렬 신뢰도
    int max_json_len
);
```

**동작**:
- 입력: 2~6개 겹치는 이미지 (겹침 ≥ 5%)
- 처리: 위상 상관 정렬 + CNN seam blending
- 출력: Panoramic float32 이미지
- 겹침 < 5% → 거부 (XPE_ERR_INSUFFICIENT_OVERLAP)

**가능성 기준**:
- 정렬 정확도: ≤ 2 픽셀 RMS
- Seam 평활성: SSIM ≥ 0.98
- 레이턴시: 2-frame ≤ 1500ms

---

### 2.5 뼈 억제 인터페이스 (SWU-2.11)

#### FR-AI-170: Bone Suppression 함수
**상태**: MANDATORY  
**우선순위**: MEDIUM

```c
int xpe_ai_suppress_bones(
    const float* image,           // 로그 도메인
    int width, int height,
    float* suppressed_image_out,  // 파생 이미지
    char* output_json,            // 신뢰도 + 품질
    int max_json_len
);
```

**동작**:
- 입력: 로그 도메인 흉부 이미지
- 처리: Residual U-Net 추론
- 출력: 파생 이미지 (float32) + sidecar
- 신뢰도 < 0.80 → 사용 권고 안 함 표기

**가능성 기준**:
- 품질: PSNR ≥ 33dB, SSIM ≥ 0.97
- 레이턴시: ≤ 2000ms (2048×2048)
- 파생 이미지: 원본과 분리된 DICOM SOP Instance

#### FR-AI-180: Derived Image 관리
**상태**: MANDATORY  
**우선순위**: HIGH

파생 이미지(뼈 억제)는 다음을 만족해야 한다:
- 별도 DICOM SOP Instance UID 생성
- ReferencedSOPInstanceUID로 원본 참조
- 메타데이터: suppression_confidence, quality_metrics (PSNR, SSIM)

**가능성 기준**:
- SOP UID 생성: 유니크, ISO/IEC 8824-1 준수
- 참조 관계: 원본 ↔ 파생 양방향 추적 가능

---

### 2.6 DL 디노이저 인터페이스 (SWU-2.12)

#### FR-AI-190: DL Denoiser 함수 (Research Path)
**상태**: OPTIONAL  
**우선순위**: LOW

```c
int xpe_ai_denoise_dl(
    const float* image,
    int width, int height,
    float* denoised_out,    // 또는 classical 폴백
    char* quality_json,     // 신뢰도
    int max_json_len
);
```

**동작**:
- 입력: 로그 도메인 이미지
- 처리: DnCNN 추론
- 출력: 노이즈 감소된 이미지 또는 고전적 폴백
- Fail-Closed: 추론 실패 → 고전적 감소 (무성 폴백)

**가능성 기준**:
- SSIM ≥ 0.95 vs 고전적 방법
- 레이턴시: ≤ 800ms
- 오류 → 고전적 경로로 자동 전환

---

### 2.7 모델 무결성 검증

#### FR-AI-200: SHA-256 Model Integrity Check
**상태**: MANDATORY  
**우선순위**: CRITICAL

모든 ONNX 모델은 시작 시 SHA-256 해시로 검증되어야 한다:
```
expected_hash = config.model_hashes[model_id]
computed_hash = SHA256(model_file)

if (computed_hash != expected_hash):
    log("Model integrity check failed: " + model_id)
    block_all_inference()
    return XPE_ERR_MODEL_INTEGRITY_FAILED
```

**가능성 기준**:
- 모든 모델 파일(5개)에 SHA-256 검증 적용
- 검증 실패 시 모든 추론 차단
- 해시 불일치 → 로그 기록 + 진단 JSON

---

## 3. 안전 요구사항 (Safety Requirements, SAF)

### SAF-AI-100: AI 출력 격리
**상태**: MANDATORY  
**우선순위**: CRITICAL

**규칙**: AI 결과는 **JSON sidecar**로만 저장되며, 주 진단 이미지(`XpeImageData`, `XpeImageMetadata`) 절대 변경 금지

**설명**: 
- SWU-2.7 (신체 부위) → `{image}.bodypart.json`
- SWU-2.8-AI (ROI 정제) → `{image}.collimation-refined.json`
- SWU-2.11 (뼈 억제) → 파생 DICOM SOP (별도)
- SWU-2.12 (DL 디노이저) → Classical fallback

**검증 방법**: 코드 리뷰 + 단위 테스트 (메타데이터 변경 감지)

### SAF-AI-110: 워커 격리
**상태**: MANDATORY  
**우선순위**: CRITICAL

**규칙**: `xpe_ai_worker.exe`는 Job Object로 격리되어야 하며, 워커 크래시가 xpe_ai.dll 또는 주 프로세스에 영향을 주지 않아야 함

**설정**:
- Job Object: 메모리 제한 740MB, CPU 선호도 설정
- Crash Handler: SEH로 캡처, 로그 기록, 자동 재시작
- Heap 오버플로우: 스택 가드 페이지

**검증 방법**: 스트레스 테스트 (의도적 크래시 주입)

### SAF-AI-120: 모델 버전 추적
**상태**: MANDATORY  
**우선순위**: HIGH

**규칙**: 모든 AI 출력에 모델 버전 문자열 및 SHA-256 해시 포함

**포맷**:
```json
{
  "model_version": "bodypart-mobilenet-v3-20260414",
  "model_hash": "abc123def456...",
  "timestamp_ms": 1713052800000,
  "prediction": "..."
}
```

**목적**: FDA SaMD 감사 추적, 모델 업데이트 추적성

### SAF-AI-130: OOD 탐지 및 Fallback
**상state**: MANDATORY  
**우선순위**: HIGH

**규칙**: 모든 추론의 신뢰도 점수가 임계값 이상이어야 함; 미달 시 자동 fallback

**임계값**:
- SWU-2.7 (신체 부위): confidence ≥ 0.70
- SWU-2.8-AI (ROI): confidence ≥ 0.65
- SWU-2.11 (뼈 억제): confidence ≥ 0.80

**Fallback 동작**:
- 임계값 미달 → "UNKNOWN" 또는 원본 경로 반환
- 워커 timeout → classical 경로
- 모델 무결성 실패 → AI 기능 비활성화

### SAF-AI-140: 비블로킹 파이프라인
**상태**: MANDATORY  
**우선순위**: HIGH

**규칙**: 메인 이미지 처리 파이프라인은 AI 워커 완료를 기다리지 않아야 함

**구현**:
- AI 요청은 비동기(async) 큐에 배치
- 응답 수집은 별도 스레드에서 처리
- 메인 파이프라인: AI 완료와 무관하게 계속 진행

**검증 방법**: 타이밍 분석, 파이프라인 latency 측정

---

## 4. 성능 요구사항 (Performance Requirements, PERF)

### PERF-AI-100: 신체 부위 인식 레이턴시
**상태**: MANDATORY  
**우선순위**: HIGH

- **목표**: ≤ 300ms (3072×3072 이미지)
- **포함**: 리사이즈 (3072→512) + 추론 + JSON 쓰기
- **측정**: xpe_ai_worker.exe 내 벽시간(wall time)

**가능성 기준**:
- 10회 실행 평균 ≤ 300ms
- 95분위수 ≤ 350ms
- 99분위수 ≤ 400ms

### PERF-AI-110: 조명 ROI 정제 레이턴시
**상태**: MANDATORY  
**우선순위**: MEDIUM

- **목표**: ≤ 150ms
- **포함**: U-Net 엣지 감지 + ROI 정제
- **측정**: 벽시간

### PERF-AI-120: 이미지 스티칭 레이턴시
**상태**: CONDITIONAL  
**우선순위**: MEDIUM

- **목표**: 2-frame ≤ 1500ms
- **포함**: 정렬 + seam blending
- **확장**: N-frame 시간 = 1500 + (N-2)×500ms 추정

### PERF-AI-130: 뼈 억제 레이턴시
**상태**: MANDATORY  
**우선순위**: MEDIUM

- **목표**: ≤ 2000ms (2048×2048)
- **포함**: U-Net 추론 + 메모리 할당
- **측정**: 벽시간

### PERF-AI-140: 워커 시작 시간 (Cold Start)
**상태**: MANDATORY  
**우선순위**: MEDIUM

- **목표**: ≤ 3000ms (DLL 로드 + 모델 로드)
- **세부**:
  - DLL 로드: ≤ 500ms
  - 모델 로드 및 메모리 할당: ≤ 2000ms
  - 초기화 완료 (심박 신호): ≤ 3000ms

### PERF-AI-150: 최대 메모리 사용
**상state**: MANDATORY  
**우선순위**: HIGH

- **워커 프로세스**: ≤ 740MB
  - 모델 로드: ~200-400MB (ONNX Runtime + weights)
  - 워킹 메모리: ~100-200MB (임시 버퍼)
  - Headroom: ~40-140MB (안전 여유)
- **메인 프로세스 (xpe_ai.dll)**: ≤ 10MB

### PERF-AI-160: IPC 처리량
**상态**: MANDATORY  
**우선순위**: MEDIUM

- **요청/응답 처리량**: 최소 1 req/sec (비블로킹이므로 큐에 쌓임)
- **공유 메모리 크기**: ≤ 10MB (5MB 이미지 + 1KB response + headroom)

---

## 5. 구성 및 비활성화 정책

### FR-AI-300: AI 기능 구성
**상태**: MANDATORY  
**우선순위**: MEDIUM

AI 기능은 config JSON으로 런타임에 제어:
```json
{
  "ai": {
    "enabled": true,
    "body_part_recognition": { "enabled": true, "confidence_threshold": 0.70 },
    "collimation_refinement": { "enabled": true },
    "bone_suppression": { "enabled": true },
    "stitch_images": { "enabled": false },
    "dl_denoiser": { "enabled": false }
  }
}
```

### FR-AI-310: AI 비활성화 조건
**상태**: MANDATORY  
**우선순위**: CRITICAL

다음 조건 발생 시 AI 전체 비활성화:
1. 워커 3회 연속 시작 실패
2. IPC 타임아웃 3회 연속
3. 모든 모델 SHA-256 검증 실패
4. 메모리 할당 실패 (Job Object 상한)

**동작**: 세션 종료까지 AI 기능 비활성화, classical fallback 사용

---

## 6. 추적 행렬 (RTM)

SRS-AI-001의 모든 요구사항(FR, SAF, PERF)은 RTM-AI-001에서 다음으로 추적됨:
- 아키텍처 컴포넌트 (SAD-AI-001)
- 테스트 케이스
- 위험 분석 (SHA-AI-001)

---

*SRS-AI-001 소프트웨어 요구사항 명세서 v1.0.0 끝*
