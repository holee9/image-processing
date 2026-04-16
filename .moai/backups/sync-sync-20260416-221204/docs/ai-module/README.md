# X-ray FPD AI 모듈 (xpe_ai) 문서 패키지

**모듈**: `xpe_ai.dll` + `xpe_ai_worker.exe` (Layer 1, Phase 3)  
**안전 등급**: IEC 62304 Class B  
**문서 버전**: 1.0.0  
**날짜**: 2026-04-14  
**규범 사양**: [ALG-SPEC-001 v3.0.0-ds2](../../.moai/specs/xpe-algorithm-spec-deepsync.md)

---

## 개요

`xpe_ai` 모듈은 X-ray FPD 이미지 처리 파이프라인에 **보조적(assistive) AI 기능**을 제공합니다. 핵심 특징:

- **IPC 프록시 아키텍처**: `xpe_ai.dll`은 클라이언트, `xpe_ai_worker.exe`는 샌드박스 워커
- **5개 AI 함수**: 신체 부위 인식, ROI 정제, 이미지 스티칭, 뼈 억제, DL 디노이저
- **Graceful Degradation**: 워커 오류 시 고전적(classical) 경로로 자동 전환
- **IEC 62304 Class B 준수**: 모든 출력은 JSON sidecar, 진단 이미지 미변경

---

## 문서 빠른 참조

이 README는 7개의 상호 연관된 AI 모듈 문서 중 하나입니다. 역할에 따라 바로 이동하세요:

| 역할 | 읽어야 할 문서 | 목적 |
|------|--------------|------|
| **소프트웨어 개발자** | 이 README → SRS → SAD | 파이프라인 구조, API, 알고리즘 이해 |
| **AI/ML 엔지니어** | xpe-ai-prd.md → SRS | AI 함수 명세, 성능 목표, 모델 요구사항 |
| **QA / 테스트 엔지니어** | TDS-AI-001 → RTM | 테스트 데이터 구성, 합격 기준 |
| **안전/위험 담당자** | SHA-AI-001 → RTM | 위험 식별, 통제, 잔존 위험도 |
| **의료기기 규제 담당자** | SRS → RTM → SHA → SAD | IEC 62304 추적성 패키지 |
| **시스템 아키텍트** | SAD-AI-001 (전체) | 컴포넌트, IPC, 메모리 레이아웃 |
| **프로젝트 관리자** | README (이 파일) | 상태, 일정, 산출물 |

---

## 문서 생태계 구조

```
┌────────────────────────────────────────────────────────────┐
│         AI 모듈 문서 패키지 (IEC 62304 Class B)           │
│                                                            │
│  ┌──────────────────────────────────────────────────┐    │
│  │  xpe-ai-prd.md  (PRD)                            │    │
│  │  5개 AI 함수 · 워커 아키텍처 · FDA SaMD 원칙   │    │
│  └──────┬───────────────────────────────────────────┘    │
│         │ 파생                                            │
│   ┌─────┼──────────────────────┐                        │
│   │     │                      │                        │
│   v     v                      v                        │
│ ┌──────┐  ┌──────┐  ┌────────────────────┐             │
│ │SRS-  │  │SAD-  │  │SHA-AI-001          │             │
│ │AI-001│  │AI-001│  │위험 분석            │             │
│ │기능/ │  │아키  │  │(8개 위험, ISO)      │             │
│ │안전/ │  │텍처  │  │                    │             │
│ │성능  │  │설계  │  │                    │             │
│ └──┬──┘  └──┬──┘  └──────────┬───────────┘             │
│    │        │                 │                         │
│    └────────┼─────────────────┘                         │
│             │ 추적                                      │
│             v                                          │
│    ┌──────────────────────┐                           │
│    │  RTM-AI-001          │                           │
│    │  요구사항 추적        │                           │
│    │  (SRS↔SAD↔SHA↔Test) │                           │
│    └────────┬─────────────┘                           │
│             │ 입력                                     │
│      ┌──────┴──────────────┐                          │
│      v                     v                          │
│  ┌────────────┐      ┌──────────────┐                 │
│  │TDS-AI-001  │      │(향후)         │                 │
│  │테스트 데이 │      │실장 계획서    │                 │
│  │터 명세서   │      │              │                 │
│  │(개발 중)   │      │              │                 │
│  └────────────┘      └──────────────┘                 │
│                                                        │
│ ▶ 이 파일 (README.md) = AI 모듈 개요 및 네비게이션   │
└────────────────────────────────────────────────────────┘
```

---

## 1. 아키텍처 개요

### 1.1 계층 위치

```
┌─────────────────────────────────────────┐
│ Layer 2: ImageProcTest.exe (C# WPF)    │
│ (파이프라인 오케스트레이터)              │
└────────────────┬────────────────────────┘
                 │
        P/Invoke (C ABI)
                 │
┌────────────────▼────────────────────────┐
│ Layer 1: xpe_ai.dll (IPC 클라이언트)   │
│ · AiProxyClient                        │
│ · WorkerLifecycleManager               │
│ · SharedMemoryChannel                  │
│ · ModelRegistry                        │
│ · FallbackController                   │
└────────────────┬────────────────────────┘
                 │
      IPC (Named Memory + Events)
                 │
┌────────────────▼──────────────────────────────┐
│ Process: xpe_ai_worker.exe (Sandboxed)       │
│ · IpcServer                                  │
│ · OnnxInferenceEngine                        │
│ · BodyPartClassifier (SWU-2.7)              │
│ · CollimationRefiner (SWU-2.8-AI)           │
│ · ImageStitcher (SWU-2.9)                   │
│ · BoneSuppressionEngine (SWU-2.11)          │
│ · DlDenoiser (SWU-2.12, optional)           │
└────────────────┬──────────────────────────────┘
                 │
        링크 의존성
                 │
┌────────────────▼────────────────────────┐
│ Layer 0: xpe_common.dll                 │
│ (타입, 메모리, 구성, 에러, 알림)        │
└─────────────────────────────────────────┘
```

### 1.2 5개 AI 함수 (SWU)

#### SWU-2.7: 신체 부위 인식 (Body Part Recognition)

**모델**: MobileNet-v3-Small (ONNX, INT8, < 5MB)  
**입력**: 512×512 float32  
**출력**: JSON sidecar `{body_part, confidence, top3}`  
**성능**: ≤ 300ms, accuracy ≥ 95%  

#### SWU-2.8-AI: AI 기반 조명 ROI 정제 (AI Collimation Refinement)

**모델**: 경량 U-Net (ONNX, ≤ 2MB)  
**입력**: 로그 도메인 이미지 + baseline ROI  
**출력**: 정제된 ROI (축소만)  
**성능**: ≤ 150ms, confidence ≥ 0.65  

#### SWU-2.9: 이미지 스티칭 (Image Stitching)

**모델**: 위상 상관 정렬 + CNN seam blending  
**입력**: 2~6개 겹치는 이미지 (≥ 5% 겹침)  
**출력**: Panoramic float32 이미지  
**성능**: ≤ 1500ms (2-frame), SSIM ≥ 0.98  

#### SWU-2.11: 뼈 억제 (Bone Suppression)

**모델**: Residual U-Net (ONNX, 훈련 DRR 증강)  
**입력**: 로그 도메인 흉부 이미지  
**출력**: 파생 이미지 (별도 DICOM SOP)  
**성능**: ≤ 2000ms, PSNR ≥ 33dB, SSIM ≥ 0.97  

#### SWU-2.12: DL 디노이저 (Deep Learning Denoiser) — 연구 경로

**모델**: DnCNN variant (ONNX, INT8, ≤ 8MB)  
**입력**: 로그 도메인 이미지  
**출력**: 노이즈 감소 또는 고전적 폴백  
**상태**: 기본 파이프라인에서 비활성화, 연구 경로만  

---

## 2. 워커 프로세스 생명주기

### 2.1 상태 머신

```
초기 상태
    │
    v
[STOPPED]
    │ start_worker()
    v
[STARTING] (CreateProcess)
    │ 5s timeout
    v
[WAITING_HEARTBEAT] (첫 신호 대기)
    │ heartbeat 수신
    v
[RUNNING] (정상 운영)
    │ 1Hz heartbeat
    │ 3회 연속 미신호 시
    v
[DEAD] (워커 사망)
    │ 자동 재시작
    v
[RESTARTING] (retry N, N < 3)
    │ 100ms × N backoff
    v
    └─→ [STARTING]
    
    또는 (N ≥ 3)
    
    [UNAVAILABLE] (이번 세션 동안)
```

### 2.2 성능 예산

| 항목 | 할당 | 목표 |
|-----|:----:|:----:|
| 총 AI 처리 (3072×3072) | 3000ms | 2500ms |
| 워커 냉 시작 | 1000ms | 800ms |
| 신체 부위 | 300ms | 250ms |
| ROI 정제 | 150ms | 120ms |
| 스티칭 (2-frame) | 1500ms | 1200ms |
| 뼈 억제 | 2000ms | 1600ms |
| **최대 메모리** | **740MB** | **600MB** |

---

## 3. IPC 프로토콜

### 3.1 공유 메모리 레이아웃

```
┌────────────────────────────┐
│ Request Header (64 bytes)  │
│ · function_id              │
│ · width, height            │
│ · data_size                │
│ · timeout_ms               │
├────────────────────────────┤
│ Image Data (max 5MB)       │
├────────────────────────────┤
│ Response Slot (1KB)        │
│ · status, error_code       │
│ · result_json (UTF-8)      │
└────────────────────────────┘
```

### 3.2 Named Events 신호

```
XPE_AI_REQUEST_READY   (xpe_ai.dll → worker)
XPE_AI_RESPONSE_READY  (worker → xpe_ai.dll)
XPE_AI_WORKER_ALIVE    (worker heartbeat, 1Hz)
```

---

## 4. 안전 설계 원칙

### 4.1 Assistive & Degradable

- AI는 선택적(optional), 주 진단 파이프라인에 필수 아님
- 워커 오류 → 자동 fallback
- 파이프라인은 AI 완료를 기다리지 않음 (비블로킹)

### 4.2 Process Isolation

- 워커는 Job Object로 격리 (메모리 ≤ 740MB)
- 워커 크래시 → xpe_ai.dll 영향 없음
- 격리된 heap, stack overflow 보호

### 4.3 Sidecar-Only Output

- AI 결과는 JSON sidecar로만 저장
- 주 진단 이미지 절대 변경 금지
- 파생 이미지는 별도 DICOM SOP Instance

### 4.4 Version Locking

- 모델: SHA-256 해시로 고정
- 모델 버전 문자열: 모든 출력에 포함
- 모델 업데이트 → 별도 배포 사이클

---

## 5. FDA AI/ML SaMD 고려사항

### 5.1 OOD 탐지 (필수)

모든 추론에 신뢰도(confidence) 점수:
- SWU-2.7: confidence ≥ 0.70
- SWU-2.8-AI: confidence ≥ 0.65
- SWU-2.11: confidence ≥ 0.80
- 미달 → "UNKNOWN" 또는 fallback

### 5.2 감사 추적 (필수)

```json
{
  "timestamp": "2026-04-14T12:00:00Z",
  "function": "body_part_recognition",
  "model_version": "bodypart-mobilenet-v3-20260414",
  "confidence": 0.92,
  "result": "chest",
  "worker_uptime_ms": 125340
}
```

### 5.3 모델 무결성 (필수)

시작 시 SHA-256 검증:
```c
if (SHA256(model_file) != expected_hash) {
    block_all_inference();
    return XPE_ERR_MODEL_INTEGRITY_FAILED;
}
```

---

## 6. API 레퍼런스

### 6.1 Public C ABI (xpe_ai.dll)

```c
// 초기화
int xpe_ai_initialize(const char* config_json);
int xpe_ai_shutdown();

// 신체 부위
int xpe_ai_body_part_classify(
    const float* image, int w, int h,
    char* json_out, int max_len
);

// 조명 ROI
int xpe_ai_refine_collimation(
    const float* image, int w, int h,
    const XpeROI* baseline, XpeROI* refined_out,
    char* json_out, int max_len
);

// 스티칭
int xpe_ai_stitch_images(
    const float** images, int num_images,
    int w, int h, float* stitched_out,
    int* out_w, int* out_h,
    char* diag_json, int max_len
);

// 뼈 억제
int xpe_ai_suppress_bones(
    const float* image, int w, int h,
    float* suppressed_out,
    char* json_out, int max_len
);

// 상태
bool xpe_ai_is_available();
int xpe_ai_get_last_error();
```

---

## 7. 위험 분석 요약

**8개 주요 위험 식별, 모두 ALARP 수준까지 통제됨**:

| 위험 | 심각도 | 발생가능성 | 위험도 | 통제 |
|-----|--------|---------|--------|------|
| 신체 부위 오분류 | M | L | MOD | confidence ≥ 0.70 |
| 파생 이미지 오용 | H | L | MOD | 별도 SOP + metadata |
| ROI 거짓 음수 | H | L | MOD | shrink-only |
| 워커 크래시 | H | L | MOD | 격리 + 재시작 |
| 모델 손상 | H | VL | LOW | SHA-256 검증 |
| IPC 타임아웃 | M | L | MOD | 5s timeout + restart |
| 디노이저 구조 제거 | H | L | MOD | SSIM gate |
| 스티칭 아티팩트 | H | L | MOD | 겹침 검증 + SSIM |

자세한 내용은 **SHA-AI-001** 참조.

---

## 8. 문서 상태 및 일정

| 문서 | 상태 | 담당 | 마감 |
|-----|------|------|------|
| xpe-ai-prd.md | ✓ Complete | AI Lead | 2026-04-14 |
| SRS-AI-001 | ✓ Complete | SW Lead | 2026-04-14 |
| SAD-AI-001 | ✓ Complete | Architect | 2026-04-14 |
| SHA-AI-001 | ✓ Complete | Safety | 2026-04-14 |
| RTM-AI-001 | ✓ Complete | QA | 2026-04-14 |
| README.md | ✓ Complete | PM | 2026-04-14 |
| TDS-AI-001 | ⊙ In Progress | QA | 2026-05-15 |
| 구현 (xpe_ai.dll) | ⊙ In Progress | SW Team | 2026-06-30 |
| 구현 (xpe_ai_worker.exe) | ⊙ In Progress | AI Team | 2026-06-30 |
| 테스트 및 검증 | ◐ Planned | QA | 2026-07-31 |
| 규제 제출 | ◐ Planned | Regulatory | 2026-08-31 |

---

## 9. 다운스트림 의존성

이 모듈은 다음 모듈과 통합:

| 모듈 | 인터페이스 | 의존성 |
|-----|-----------|--------|
| `xpe_enhance_advanced.dll` | Output image (float32) | 입력 데이터 제공 |
| `xpe_common.dll` | Shared types, error codes | 기본 정의 |
| `xpe_display.dll` | Derived DICOM SOP | 파생 이미지 처리 |
| ImageProcTest.exe | P/Invoke API | AI 함수 호출 |

---

## 10. 참고문헌

### 표준

| 표준 | 관련성 |
|-----|--------|
| IEC 62304:2006+A1:2015 | 소프트웨어 생명주기 |
| ISO 14971:2019 | 위험 관리 프로세스 |
| IEC 62220-1-1:2015 | DQE 측정 (모델 검증) |
| FDA SaMD Guidance | AI/ML 의료기기 |

### 프로젝트 문서

- [xpe-ai-prd.md](xpe-ai-prd.md) — AI 함수 명세 (원본)
- [SRS-AI-001](SRS-AI-001_Software_Requirements_Specification.md) — 기능/안전/성능 요구사항
- [SAD-AI-001](SAD-AI-001_Software_Architecture_Document.md) — 컴포넌트 및 IPC 설계
- [SHA-AI-001](SHA-AI-001_Software_Hazard_Analysis.md) — 위험 분석 (8개 위험)
- [RTM-AI-001](RTM-AI-001_Requirements_Traceability_Matrix.md) — 추적성 행렬 (SRS↔SAD↔SHA)
- [ALG-SPEC-001 v3.0.0-ds2](../../.moai/specs/xpe-algorithm-spec-deepsync.md) — 규범 알고리즘 사양

---

## 11. 연락 정보

| 역할 | 담당자 | 연락처 |
|-----|--------|--------|
| Project Manager | [PM Name] | [Email] |
| AI Lead | [AI Lead Name] | [Email] |
| Software Lead | [SW Lead Name] | [Email] |
| Safety Officer | [Safety Name] | [Email] |
| QA Lead | [QA Lead Name] | [Email] |

---

## 12. 체크리스트 및 게이트

### Phase 3 Go-Live 게이트 (2026-07-31)

- [ ] SRS-AI-001 리뷰 완료 (개발 + QA)
- [ ] SAD-AI-001 리뷰 완료 (아키텍트 + 보안)
- [ ] SHA-AI-001 리뷰 완료 (위험 담당자)
- [ ] RTM-AI-001 확인 (모든 요구사항 추적)
- [ ] TDS-AI-001 완성 및 리뷰
- [ ] xpe_ai.dll 구현 완료
- [ ] xpe_ai_worker.exe 구현 완료
- [ ] 통합 테스트 통과 (45개 테스트 케이스)
- [ ] 보안 검토 완료
- [ ] 규제 검토 완료 (법무)

---

*xpe_ai 모듈 README v1.0.0 끝 (2026-04-14)*
