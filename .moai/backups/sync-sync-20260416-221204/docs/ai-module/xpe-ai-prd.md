# X-ray FPD AI 모듈 (xpe_ai) 제품 요구사항 문서 (PRD)

**모듈**: `xpe_ai.dll` + `xpe_ai_worker.exe` (Layer 1, Phase 3)  
**소유자 DLL**: `xpe_ai.dll` (IPC 클라이언트), `xpe_ai_worker.exe` (샌드박스 워커)  
**의존성**: `xpe_enhance_advanced.dll` (선행 처리), `xpe_common.dll` (Layer 0)  
**안전 등급**: IEC 62304 Class B  
**문서 버전**: 1.0.0  
**날짜**: 2026-04-14  
**규범 사양**: [ALG-SPEC-001 v3.0.0-ds2](../../.moai/specs/xpe-algorithm-spec-deepsync.md)

---

## 개요

`xpe_ai` 모듈은 X-ray FPD 이미지 처리 파이프라인의 **보조적(assistive)** AI 기능을 제공합니다. 핵심 설계 원칙:

- **IPC 프록시 아키텍처**: `xpe_ai.dll`은 **프록시만** 담당하며, 실제 ONNX Runtime 추론은 샌드박스된 `xpe_ai_worker.exe`에서 실행
- **비블로킹 파이프라인**: AI 결과는 보조적이며, 파이프라인은 AI 워커의 완료를 기다리지 않음
- **Graceful Degradation**: AI 워커 오류/타임아웃 시 고전적(classical) 폴백 경로로 자동 전환
- **IEC 62304 준수**: 모든 AI 출력은 sidecar JSON으로 저장되며, 주 진단 이미지를 절대 변경하지 않음

---

## 5개 AI 함수 명세

### SWU-2.7 신체 부위 인식 (Body Part Recognition)

**목적**: X-ray 영상에서 촬영된 신체 부위를 자동 분류

**모델**: MobileNet-v3-Small (ONNX, INT8 양자화, < 5MB)

**입력**:
- 512×512 그레이스케일 float32 이미지
- 로그 도메인 개선된 이미지 (xpe_enhance_advanced.dll 출력)

**출력**: JSON sidecar `{image_path}.bodypart.json`
```json
{
  "body_part": "chest",
  "confidence": 0.92,
  "model_version": "bodypart-mobilenet-v3-20260414",
  "timestamp_ms": 1713052800000,
  "top3": [
    { "label": "chest", "score": 0.92 },
    { "label": "abdomen", "score": 0.06 },
    { "label": "spine", "score": 0.02 }
  ]
}
```

**성능 목표**:
- Top-1 정확도 ≥ 95% (≥ 15개 해부학적 부위)
- 레이턴시: ≤ 300ms (3072×3072 이미지 기준, 포함: 리사이즈 + 추론)
- OOD(Out-Of-Distribution) 탐지: 최대 신뢰도 < 0.70 → `body_part = "UNKNOWN"` 반환

**안전 제약**:
- SWU-2.7 출력은 **JSON sidecar only**. XpeImageMetadata.bodyPart를 직접 변경하지 않음
- 신뢰도 < 0.70인 경우 자동으로 "UNKNOWN" 반환
- 모델 버전은 모든 출력에 포함되어야 함 (추적 가능성)

**지원 부위 목록** (최소 15개 범주):
1. Chest (흉부)
2. Abdomen (복부)
3. Spine (척추)
4. Pelvis (골반)
5. Extremity-Arm (상지)
6. Extremity-Leg (하지)
7. Hand (손)
8. Foot (발)
9. Skull (두개골)
10. Facial (안면)
11. Shoulder (어깨)
12. Knee (무릎)
13. Ankle (발목)
14. Wrist (손목)
15. Cervical (경추)

---

### SWU-2.8-AI AI 기반 조명 ROI 정제 (AI Collimation Refinement)

**목적**: SWU-2.8 (Hough 기반 조명 검출)의 기본 ROI를 AI로 정제

**모델**: 경량 U-Net 엣지 검출기 (ONNX, ≤ 2MB)

**입력**:
- Baseline collimation ROI (SWU-2.8에서)
- 로그 도메인 이미지

**출력**: JSON sidecar `{image_path}.collimation-refined.json`
```json
{
  "roi_original": { "x": 100, "y": 100, "width": 2900, "height": 3000 },
  "roi_refined": { "x": 120, "y": 110, "width": 2860, "height": 2980 },
  "confidence": 0.78,
  "mode": "shrink_only",
  "model_version": "collimation-unet-20260414"
}
```

**핵심 규칙**:
- ROI는 **축소만 가능** (확대 금지) — 오진단 방지
- 엣지 신뢰도 < 0.65 → 원본 ROI 반환, 변경 없음
- 모드: "shrink_only" (불변)

**성능 목표**:
- 엣지 정확도: IOU ≥ 0.92 (검증 데이터)
- 레이턴시: ≤ 150ms
- 거짓 양성 (FP) 제거율: ≥ 90%

---

### SWU-2.9 이미지 스티칭 (Image Stitching)

**목적**: 장척장 방사선 촬영(척추, 다리) 또는 다중 노출 스티칭을 위한 파노라마 이미지 생성

**모델**: 위상 상관(Phase Correlation) 정렬 + CNN 기반 seam blending (ONNX)

**입력**:
- 2~6개의 겹치는 검출기 이미지 (float32)
- 최소 5% 겹침 요구

**출력**: 스티칭된 panoramic float32 이미지
```
[image 1][overlap][image 2][overlap][image 3]
```

**성능 목표**:
- 정렬 정확도: ≤ 2 픽셀 RMS 오류
- Seam 아티팩트: SSIM ≥ 0.98 (경계 평활성)
- 레이턴시: 2-frame 기준 ≤ 1500ms

**조건부 실행**:
- 호출자가 명시적으로 스티칭 모드 요청 시에만 실행
- 겹침 < 5% → 거부 및 오류 반환

---

### SWU-2.11 뼈 억제 (Bone Suppression)

**목적**: 흉부 X-ray에서 갈비뼈, 척추뼈를 억제하여 폐실질(lung parenchyma) 병변 가시화 향상

**모델**: Residual U-Net (ONNX, DRR 증강 훈련)

**입력**: 로그 도메인 개선 흉부 이미지 (float32)

**출력**: **파생(Derived) 이미지만** — 주 진단 이미지는 절대 변경하지 않음
```json
{
  "derived_image_uid": "1.2.840.113619.2.55.4.{instance_uid}",
  "suppression_confidence": 0.91,
  "quality_metrics": {
    "psnr": 34.2,
    "ssim": 0.974
  },
  "model_version": "bone-suppression-resnet-20260414"
}
```

**IEC 62304 주의사항**:
- 파생 이미지는 별도 DICOM SOP Instance로 저장되어야 함
- 원본 이미지를 명시적으로 참조하는 ReferencedSOPInstanceUID 포함

**성능 목표**:
- PSNR ≥ 33dB (vs ground truth)
- SSIM ≥ 0.97 (해부학적 구조 보존)
- 레이턴시: ≤ 2000ms (2048×2048)

**안전 제약**:
- 출력은 **derived image로만** 취급
- 신뢰도 < 0.80 → 사용 권고 안 함 (진단용 보조 도구)

---

### SWU-2.12 DL 디노이저 (Deep Learning Denoiser) — 연구 경로

**목적**: DnCNN 기반 심화 노이즈 감소 (선택적 활성화)

**모델**: DnCNN variant (ONNX, INT8, ≤ 8MB)

**상태**: **연구 경로만** — 기본 파이프라인에서 비활성화

**입력**: 로그 도메인 이미지

**출력**: 노이즈 감소된 float32 이미지 또는 고전적(classical) 감소 출력 (폴백)

**성능 목표**:
- SSIM ≥ 0.95 (vs 고전적 노이즈 감소)
- 레이턴시: ≤ 800ms

**Fail-Closed 로직**:
- 추론 실패 또는 품질 기준 미달 → 고전적 노이즈 감소 출력으로 자동 전환 (무성 폴백, 오류 로그만 기록)

---

## 워커 프로세스 생명주기 및 성능 예산

### 워커 시작 및 심박 (Heartbeat)

```
xpe_ai.dll (메인 프로세스)
    |
    | CreateProcess()
    v
xpe_ai_worker.exe (샌드박스)
    |
    | Named event: XPE_AI_WORKER_ALIVE
    | 심박 주기: 1Hz
    |
```

**심박 검증**:
- 1Hz 간격으로 `XPE_AI_WORKER_ALIVE` 신호 확인
- 3회 연속 미신호 → 워커 사망 선언
- 즉시 워커 재시작 (최대 3회 재시작 제한)

### IPC 타임아웃

```
요청 전송
    |
    | Named event: XPE_AI_REQUEST_READY
    |
    v (wait)
응답 대기 (5000ms 타임아웃)
    |
    | Named event: XPE_AI_RESPONSE_READY
    |
    v
응답 수신 또는 타임아웃 발동
```

**타임아웃 처리**:
- IPC 타임아웃: 5000ms
- 워커 재시작 트리거
- 3회 연속 타임아웃 후 AI 기능 비활성화 (세션 종료까지)

### 공유 메모리 레이아웃

```
┌─────────────────────────────────────┐
│  Request Header (64 bytes)          │
│  ├─ function_id (uint32)            │
│  ├─ width, height (uint32 x 2)      │
│  ├─ data_size (uint32)              │
│  └─ timeout_ms (uint32)             │
├─────────────────────────────────────┤
│  Image Data (variable)              │
│  (최대 ~5MB 이미지)                 │
├─────────────────────────────────────┤
│  Response Slot (1KB)                │
│  ├─ status (uint32)                 │
│  ├─ error_code (uint32)             │
│  └─ result_json (char[960])         │
└─────────────────────────────────────┘
```

**성능 예산 (Phase 3)**:

| 항목 | 할당 (ms) | 목표 (ms) |
|-----|:---------:|:---------:|
| 총 AI 처리 시간 (3072×3072) | 3000 | 2500 |
| 워커 냉 시작 (startup) | 1000 | 800 |
| 신체 부위 인식 | 300 | 250 |
| 조명 정제 | 150 | 120 |
| 스티칭 (2-frame) | 1500 | 1200 |
| 뼈 억제 | 2000 | 1600 |
| 최대 메모리 (워커 heap) | 740MB | 600MB |

---

## FDA AI/ML SaMD 고려사항

### 1. OOD(Out-Of-Distribution) 탐지

**필수**: 모든 추론 출력에 OOD 감지 메커니즘

- 최대 신뢰도 임계값: 0.70 (SWU-2.7)
- 신뢰도 점수는 모든 sidecar에 필수 포함
- OOD 감지 → fallback 또는 "UNKNOWN" 응답

### 2. 신뢰도 점수 및 추적성

**필수**: 모든 AI 출력에 신뢰도(confidence) 스코어 포함

```json
{
  "prediction": "...",
  "confidence": 0.92,
  "model_version": "...",
  "timestamp_ms": 1713052800000
}
```

### 3. 모델 무결성 검증

**필수**: 시작 시 SHA-256 해시 검증

```c
// 모델 로드 전
computed_hash = SHA256(model_file)
if (computed_hash != expected_hash) {
    ERROR: Model integrity check failed
    block_inference()
}
```

### 4. 감사 추적 (Audit Trail)

**필수**: 모든 AI 추론 결과를 로깅

```json
{
  "timestamp": "2026-04-14T12:00:00Z",
  "function": "body_part_recognition",
  "model_version": "bodypart-mobilenet-v3-20260414",
  "input_size": "512x512",
  "confidence": 0.92,
  "result": "chest",
  "worker_uptime_ms": 125340
}
```

---

## 안전 설계 원칙

### 1. Assistive & Degradable (보조적 및 저하 가능)

- AI 기능은 **선택적(optional)**, 주 진단 파이프라인에 필수 아님
- 워커 오류 → 자동 fallback (고전적 경로)
- 파이프라인은 AI 완료를 기다리지 않음 (비블로킹)

### 2. Process Isolation (프로세스 격리)

- `xpe_ai_worker.exe`는 Job Object로 메모리 제한 (740MB)
- 워커 크래시 → xpe_ai.dll 또는 주 프로세스 영향 없음
- 격리된 힙 메모리, 스택 오버플로우 격리

### 3. Sidecar-Only Output (사이드카 전용 출력)

- AI 결과는 **JSON sidecar files**로만 저장
- 주 진단 이미지(XpeImageData, XpeImageMetadata) 절대 변경 금지
- 파생 이미지는 별도 DICOM SOP Instance

### 4. Version Locking (버전 고정)

- 모델 파일: SHA-256 해시로 고정
- 모델 버전 문자열: 모든 출력에 포함
- 모델 업데이트 → 별도 배포 사이클

---

## 비활성화 및 폴백 정책

**AI 완전히 비활성화 조건**:
1. 워커 3회 연속 시작 실패
2. 타임아웃 3회 연속 발동
3. SHA-256 모델 무결성 검사 실패
4. 메모리 할당 실패

**폴백 동작**:
- SWU-2.7 (신체 부위): 기본 classify() 반환 없음 (sidecar 미생성)
- SWU-2.8-AI (조명 정제): 원본 ROI 그대로 사용
- SWU-2.9 (스티칭): 스티칭 거부, 단일 이미지 반환
- SWU-2.11 (뼈 억제): 파생 이미지 미생성, 원본 사용
- SWU-2.12 (DL 디노이저): 고전적 노이즈 감소 출력

---

## 문서 참조 구조

```
xpe-ai-prd.md (이 문서 - 요구사항 원본)
    |
    └─▶ SRS-AI-001 (기능/안전/성능 요구사항)
    └─▶ SAD-AI-001 (아키텍처 설계)
    └─▶ SHA-AI-001 (위험 분석)
    └─▶ RTM-AI-001 (추적성 행렬)
```

---

*xpe_ai 제품 요구사항 문서 v1.0.0 끝*
