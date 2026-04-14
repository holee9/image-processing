# RTM-AI-001 요구사항 추적 행렬 (Requirements Traceability Matrix)

**문서 ID**: RTM-AI-001  
**모듈**: xpe_ai (AI Framework)  
**버전**: 1.0.0  
**날짜**: 2026-04-14  
**IEC 62304 Section**: §5.1.1(c) Traceability  
**관련 문서**: SRS-AI-001, SAD-AI-001, SHA-AI-001

---

## 1. 추적 행렬 개요

RTM-AI-001은 **양방향 추적성(Bidirectional Traceability)**을 제공합니다:
- **SRS → SAD**: 각 요구사항이 아키텍처 컴포넌트로 구현됨
- **SRS → SHA**: 각 요구사항이 위험 분석에서 주소됨
- **SRS → Test**: 각 요구사항이 테스트 케이스로 검증됨

---

## 2. 기능 요구사항 추적 (Functional Requirements)

### FR-AI-100: 워커 시작

| 항목 | 내용 |
|------|------|
| **요구사항 ID** | FR-AI-100 |
| **설명** | 워커 프로세스는 초기화, 크래시 후 자동 재시작, 세션 동안 지속 |
| **출처 문서** | SRS-AI-001 §2.1, PRD §워커 생명주기 |
| **우선순위** | HIGH |
| **상태** | Design |

**아키텍처 매핑 (SAD)**:
- Component: `WorkerLifecycleManager`
- Method: `start_worker()`, `restart_worker()`
- Interface: Public C ABI `xpe_ai_initialize()`

**위험 분석 매핑 (SHA)**:
- HAZ-AI-04: 워커 크래시 (통제: 자동 재시작)
- HAZ-AI-06: IPC 타임아웃 (통제: 재시작 로직)

**테스트 케이스 (TDS)**:
| Test ID | 설명 | 가능성 기준 |
|---------|------|-----------|
| TC-AI-100-01 | 워커 정상 시작 | 시작 후 5초 내 heartbeat |
| TC-AI-100-02 | 재시작 시뮬레이션 | 최대 3회 자동 재시작 |
| TC-AI-100-03 | 과도한 재시작 거부 | 3회 후 AI 비활성화 |

---

### FR-AI-110: 심박 모니터링

| 항목 | 내용 |
|------|------|
| **요구사항 ID** | FR-AI-110 |
| **설명** | 1Hz 간격으로 워커 alive 신호 확인, 3회 미신호 시 재시작 |
| **출처 문서** | SRS-AI-001 §2.1 |
| **우선순위** | HIGH |
| **상태** | Design |

**아키텍처 매핑**:
- Component: `WorkerLifecycleManager`
- Mechanism: Named event `XPE_AI_WORKER_ALIVE`
- Thread: Separate heartbeat monitor thread

**위험 분석 매핑**:
- HAZ-AI-04: 워커 크래시 탐지 (detective control)

**테스트 케이스**:
| Test ID | 설명 |
|---------|------|
| TC-AI-110-01 | Heartbeat 수신 (정상) |
| TC-AI-110-02 | Heartbeat 실패 3회 인식 |
| TC-AI-110-03 | 자동 재시작 트리거 |

---

### FR-AI-120: IPC 타임아웃 및 복구

| 항목 | 내용 |
|------|------|
| **요구사항 ID** | FR-AI-120 |
| **설명** | 5000ms 타임아웃, 3회 연속 시 AI 비활성화 |
| **출처 문서** | SRS-AI-001 §2.1 |
| **우선순위** | CRITICAL |
| **상태** | Design |

**아키텍처 매핑**:
- Component: `SharedMemoryChannel`, `AiProxyClient`
- Timeout: 5000ms (구성 가능)
- Event: `XPE_AI_RESPONSE_READY`

**위험 분석 매핑**:
- HAZ-AI-06: IPC 타임아웃 스타베이션 (preventive + corrective)

**테스트 케이스**:
| Test ID | 설명 |
|---------|------|
| TC-AI-120-01 | 정상 응답 < 5s |
| TC-AI-120-02 | Timeout 정확도 (±50ms) |
| TC-AI-120-03 | 3회 타임아웃 → AI 비활성화 |
| TC-AI-120-04 | 워커 재시작 트리거 |

---

### FR-AI-130: Body Part Classification

| 항목 | 내용 |
|------|------|
| **요구사항 ID** | FR-AI-130 |
| **설명** | 512×512 이미지 분류, confidence 점수, OOD 탐지 |
| **출처 문서** | SRS-AI-001 §2.2, PRD §SWU-2.7 |
| **우선순위** | HIGH |
| **상태** | Testing |

**아키텍처 매핑**:
- Component: `BodyPartClassifier`
- Input: float32 512×512
- Model: MobileNet-v3-Small ONNX
- Output: JSON sidecar

**위험 분석 매핑**:
- HAZ-AI-01: 오분류 (detective control: confidence threshold)

**성능 요구사항 매핑**:
- PERF-AI-100: ≤ 300ms
- Accuracy: ≥ 95%

**테스트 케이스**:
| Test ID | 설명 | 기준 |
|---------|------|------|
| TC-AI-130-01 | Top-1 정확도 | ≥ 95% (15개 부위) |
| TC-AI-130-02 | 레이턴시 | ≤ 300ms |
| TC-AI-130-03 | OOD 탐지 | confidence < 0.70 → "UNKNOWN" |
| TC-AI-130-04 | Sidecar 생성 | JSON 유효성 검사 |

---

### FR-AI-140: Sidecar 관리

| 항목 | 내용 |
|------|------|
| **요구사항 ID** | FR-AI-140 |
| **설명** | JSON sidecar 파일 생성, UTF-8, 원본 이미지와 같은 디렉토리 |
| **출처 문서** | SRS-AI-001 §2.2 |
| **우선순위** | MEDIUM |
| **상태** | Design |

**아키텍처 매핑**:
- Component: Each SWU (BodyPartClassifier, CollimationRefiner, etc.)
- Mechanism: File I/O with error handling

**위험 분석 매핑**:
- HAZ-AI-02: 파생 이미지 오용 (preventive: separate SOP)

**테스트 케이스**:
| Test ID | 설명 |
|---------|------|
| TC-AI-140-01 | Sidecar 파일명 정확성 |
| TC-AI-140-02 | JSON 형식 유효성 |
| TC-AI-140-03 | 파일 생성 성공률 |

---

### FR-AI-150: AI Collimation Refinement

| 항목 | 내용 |
|------|------|
| **요구사항 ID** | FR-AI-150 |
| **설명** | ROI 축소만, confidence ≥ 0.65, 저신뢰도 시 원본 반환 |
| **출처 문서** | SRS-AI-001 §2.3, PRD §SWU-2.8-AI |
| **우선순위** | MEDIUM |
| **상태** | Testing |

**아키텍처 매핑**:
- Component: `CollimationRefiner`
- Model: U-Net edge detection
- Constraint: ROI.width ≤ baseline, ROI.height ≤ baseline

**위험 분석 매핑**:
- HAZ-AI-03: ROI 거짓 음수 (preventive: shrink-only)

**테스트 케이스**:
| Test ID | 설명 | 기준 |
|---------|------|------|
| TC-AI-150-01 | 축소만 정책 강제 | width/height 감소만 |
| TC-AI-150-02 | 저신뢰도 폴백 | confidence < 0.65 → original |
| TC-AI-150-03 | Edge accuracy | IOU ≥ 0.92 |

---

### FR-AI-160: Image Stitching

| 항목 | 내용 |
|------|------|
| **요구사항 ID** | FR-AI-160 |
| **설명** | 2~6개 이미지, 겹침 ≥ 5%, panoramic 출력 |
| **출처 문서** | SRS-AI-001 §2.4, PRD §SWU-2.9 |
| **우선순위** | MEDIUM |
| **상태** | Design |

**아키텍처 매핑**:
- Component: `ImageStitcher`
- Mechanism: Phase correlation + CNN seam

**위험 분석 매핑**:
- HAZ-AI-08: Seam 아티팩트 (preventive: overlap check)

**테스트 케이스**:
| Test ID | 설명 | 기준 |
|---------|------|------|
| TC-AI-160-01 | 겹침 검증 | < 5% → reject |
| TC-AI-160-02 | 정렬 정확도 | ≤ 2px RMS |
| TC-AI-160-03 | Seam 평활성 | SSIM ≥ 0.98 |

---

### FR-AI-170: Bone Suppression

| 항목 | 내용 |
|------|------|
| **요구사항 ID** | FR-AI-170 |
| **설명** | 흉부 이미지, 파생 이미지 출력, confidence ≥ 0.80 |
| **출처 문서** | SRS-AI-001 §2.5, PRD §SWU-2.11 |
| **우선순위** | MEDIUM |
| **상태** | Testing |

**아키텍처 매핑**:
- Component: `BoneSuppressionEngine`
- Model: Residual U-Net
- Output: float32 image (separate from original)

**위험 분석 매핑**:
- HAZ-AI-02: 파생 이미지 오용 (detective: confidence + metadata)
- HAZ-AI-07: 해부학적 구조 제거 (preventive: SSIM gate)

**테스트 케이스**:
| Test ID | 설명 | 기준 |
|---------|------|------|
| TC-AI-170-01 | PSNR | ≥ 33dB |
| TC-AI-170-02 | SSIM | ≥ 0.97 |
| TC-AI-170-03 | 레이턴시 | ≤ 2000ms |
| TC-AI-170-04 | 저신뢰도 플래그 | confidence < 0.80 |

---

### FR-AI-180: Derived Image Management

| 항목 | 내용 |
|------|------|
| **요구사항 ID** | FR-AI-180 |
| **설명** | 별도 DICOM SOP, ReferencedSOPInstanceUID |
| **출처 문서** | SRS-AI-001 §2.5 |
| **우선순위** | HIGH |
| **상태** | Design |

**아키텍처 매핑**:
- Component: DICOM layer (xpe_display.dll 또는 PACS integration)
- Metadata: SOP Instance UID, ReferencedSOPInstanceUID

**위험 분석 매핑**:
- HAZ-AI-02: 파생 이미지 오용 (preventive: separate SOP)

**테스트 케이스**:
| Test ID | 설명 |
|---------|------|
| TC-AI-180-01 | SOP UID 생성 (유니크) |
| TC-AI-180-02 | ReferencedSOPInstanceUID 일관성 |

---

### FR-AI-200: SHA-256 Model Integrity

| 항목 | 내용 |
|------|------|
| **요구사항 ID** | FR-AI-200 |
| **설명** | 시작 시 모든 모델 SHA-256 검증, 실패 시 추론 차단 |
| **출처 문서** | SRS-AI-001 §2.7, PRD §FDA AI/ML |
| **우선순위** | CRITICAL |
| **상태** | Design |

**아키텍처 매핑**:
- Component: `ModelRegistry`
- Method: `verify_model_integrity()`

**위험 분석 매핑**:
- HAZ-AI-05: 모델 손상 (preventive: SHA-256)

**테스트 케이스**:
| Test ID | 설명 | 기준 |
|---------|------|------|
| TC-AI-200-01 | 모델 검증 성공 | 5개 모델 모두 통과 |
| TC-AI-200-02 | 변조 감지 | 1 byte 변경 → 실패 |
| TC-AI-200-03 | 추론 차단 | 검증 실패 → AI OFF |

---

## 3. 안전 요구사항 추적 (Safety Requirements)

| SAF ID | 설명 | SHA 매핑 | 구현 상태 |
|--------|------|----------|----------|
| SAF-AI-100 | AI 출력 격리 (sidecar) | HAZ-AI-02 | ✓ Design |
| SAF-AI-110 | 워커 격리 (Job Object) | HAZ-AI-04 | ✓ Design |
| SAF-AI-120 | 모델 버전 추적 | HAZ-AI-05 | ✓ Design |
| SAF-AI-130 | OOD 탐지 + fallback | HAZ-AI-01 | ✓ Impl |
| SAF-AI-140 | 비블로킹 파이프라인 | HAZ-AI-06 | ✓ Design |

---

## 4. 성능 요구사항 추적 (Performance)

| PERF ID | 목표 | SAD 구현 | 테스트 상태 |
|---------|------|----------|-----------|
| PERF-AI-100 | Body part ≤ 300ms | AiProxyClient | Testing |
| PERF-AI-110 | Collimation ≤ 150ms | CollimationRefiner | Design |
| PERF-AI-120 | Stitching ≤ 1500ms | ImageStitcher | Design |
| PERF-AI-130 | Bone suppress ≤ 2000ms | BoneSuppressionEngine | Testing |
| PERF-AI-140 | Worker cold start ≤ 3s | WorkerLifecycleManager | Design |
| PERF-AI-150 | Memory ≤ 740MB | Job Object config | Design |
| PERF-AI-160 | Throughput ≥ 1 req/sec | Queue design | Design |

---

## 5. 컴포넌트 구현 상태

| 컴포넌트 | 요구사항 | 상태 | 담당 |
|---------|--------|------|------|
| AiProxyClient | FR-AI-100, PERF-AI-100 | Design | xpe_ai developer |
| WorkerLifecycleManager | FR-AI-100, FR-AI-110 | Design | xpe_ai developer |
| SharedMemoryChannel | FR-AI-120 | Design | xpe_ai developer |
| ModelRegistry | FR-AI-200 | Design | xpe_ai developer |
| FallbackController | SAF-AI-100, SAF-AI-130 | Design | xpe_ai developer |
| BodyPartClassifier | FR-AI-130, PERF-AI-100 | Testing | AI team |
| CollimationRefiner | FR-AI-150, PERF-AI-110 | Design | AI team |
| ImageStitcher | FR-AI-160, PERF-AI-120 | Design | AI team |
| BoneSuppressionEngine | FR-AI-170, PERF-AI-130 | Testing | AI team |
| DlDenoiser | FR-AI-190 | Design | Research |

---

## 6. 테스트 커버리지 요약

**총 요구사항**: 30개 (FR 24 + SAF 5 + PERF 7)  
**테스트 케이스**: 45개+ (TDS-AI-001에서 정의)  
**커버리지 목표**: 100% (각 요구사항 최소 1개 테스트)

---

## 7. 추적 검증 체크리스트

- [ ] 모든 FR-AI-* 요구사항이 SAD 컴포넌트에 매핑됨
- [ ] 모든 SAF-AI-* 요구사항이 SHA 위험에 매핑됨
- [ ] 모든 PERF-AI-* 요구사항이 구현 및 테스트 계획됨
- [ ] 모든 SRS 요구사항이 최소 1개 테스트 케이스 보유
- [ ] RTM 문서와 실제 구현 상태 일관성 확인 (월별)

---

*RTM-AI-001 요구사항 추적 행렬 v1.0.0 끝*
