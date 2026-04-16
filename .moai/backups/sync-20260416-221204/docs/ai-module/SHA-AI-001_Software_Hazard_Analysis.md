# SHA-AI-001 소프트웨어 위험 분석 (Software Hazard Analysis)

**문서 ID**: SHA-AI-001  
**모듈**: xpe_ai (AI Framework)  
**버전**: 1.0.0  
**날짜**: 2026-04-14  
**IEC 62304 Section**: §5.4 Software Hazard Analysis  
**ISO 14971 준거**: Risk Management Process  
**관련 문서**: SRS-AI-001, SAD-AI-001, RTM-AI-001

---

## 1. 개요

SHA-AI-001은 xpe_ai 모듈의 8개 주요 위험을 식별, 분석, 관리합니다. 각 위험에 대해:
- **원인(Cause)**: 발생 메커니즘
- **효과(Effect)**: 환자/운영에 미치는 영향
- **위험도(Risk)**: 심각도(Severity) × 발생가능성(Probability)
- **통제(Control)**: 위험 경감 전략

---

## 2. 위험 카탈로그 (8 Hazards)

### HAZ-AI-01: 신체 부위 오분류 (Body Part Misclassification)

| 항목 | 설명 |
|------|------|
| **ID** | HAZ-AI-01 |
| **위험 이벤트** | SWU-2.7이 촬영된 신체 부위를 잘못 식별 |
| **원인** | OOD(Out-Of-Distribution) 이미지 또는 모델 성능 저하 |
| **효과** | 잘못된 검사 프로토콜 적용 → 진단 오류 위험 |
| **심각도** | Medium (진단에 미치는 영향이 간접적) |
| **발생가능성** | Low (Top-1 accuracy ≥ 95%) |
| **위험도** | Medium × Low = **MODERATE** |

**탐지**:
- 신뢰도 점수 모니터링
- confidence < 0.70 → "UNKNOWN" 자동 반환

**통제**:
| 통제 | 유형 | 상태 |
|-----|------|------|
| OOD 탐지 임계값 (0.70) | Preventive | Implemented |
| 신뢰도 점수 로깅 | Detective | Implemented |
| 모델 정확도 검증 (≥95%) | Preventive | Testing |

**잔존 위험도**: LOW (ALARP 충족)

---

### HAZ-AI-02: 뼈 억제 이미지를 주 진단 이미지로 오용 (Derived Image Misuse)

| 항목 | 설명 |
|------|------|
| **ID** | HAZ-AI-02 |
| **위험 이벤트** | 파생 이미지(뼈 억제)가 주 진단용으로 사용됨 |
| **원인** | API 오용 또는 호출자 코드 오류 |
| **효과** | 유효하지 않은 이미지로 진단 → 환자 해위험 |
| **심각도** | High (환자 직접 해 가능) |
| **발생가능성** | Low (명확한 API 분리) |
| **위험도** | High × Low = **MODERATE-HIGH** |

**탐지**:
- SOP Instance UID 검증
- ReferencedSOPInstanceUID 추적
- 메타데이터 플래그 검사

**통제**:
| 통제 | 유형 | 상태 |
|-----|------|------|
| Sidecar-only output | Preventive | Design |
| 별도 DICOM SOP Instance | Preventive | Design |
| Confidence threshold (0.80) | Detective | Implemented |
| 메타데이터 "suppression_confidence" | Detective | Implemented |
| 운영 교육 (파생 vs 주 이미지 구분) | Administrative | Required |

**잔존 위험도**: LOW (설계 통제 + 운영 절차)

---

### HAZ-AI-03: AI 조명 ROI 거짓 음수 (AI Collimation False Crop)

| 항목 | 설명 |
|------|------|
| **ID** | HAZ-AI-03 |
| **위험 이벤트** | AI가 정상 해부학적 구조를 ROI에서 제외 (과도한 축소) |
| **원인** | 엣지 감지 오류, 신뢰도 과평가 |
| **효과** | 진단에 필요한 해부학적 영역 손실 → 오진단 위험 |
| **심각도** | High (진단 정보 손실) |
| **발생가능성** | Low (축소만 정책, confidence gate) |
| **위험도** | High × Low = **MODERATE-HIGH** |

**탐지**:
- ROI 변경 크기 모니터링 (logging)
- Confidence < 0.65 시 원본 ROI 유지

**통제**:
| 통제 | 유형 | 상태 |
|------|------|------|
| "축소만(shrink-only)" 정책 강제 | Preventive | Design |
| Confidence threshold (0.65) | Preventive | Implemented |
| ROI 변경 로그 기록 | Detective | Implemented |
| 원본 ROI 캐싱 (fallback용) | Preventive | Design |
| QA: ROI 결과 샘플 리뷰 | Detective | Manual |

**잔존 위험도**: LOW (설계 제약 + 임계값 + 로깅)

---

### HAZ-AI-04: 워커 프로세스 크래시로 인한 NULL/무효 결과

| 항목 | 설명 |
|------|------|
| **ID** | HAZ-AI-04 |
| **위험 이벤트** | xpe_ai_worker.exe 크래시 → NULL/garbage 응답 반환 |
| **원인** | ONNX Runtime 오류, 메모리 오버플로우, SEH 처리 실패 |
| **효과** | 응용 프로그램이 무효한 이미지 처리 → 메모리 손상/크래시 가능 |
| **심각도** | High (시스템 안정성) |
| **발생가능성** | Low (프로세스 격리, Job Object) |
| **위험도** | High × Low = **MODERATE** |

**탐지**:
- 워커 심박 모니터링
- 응답 유효성 검사 (magic number, size check)
- 응답 JSON 파싱 검증

**통제**:
| 통제 | 유형 | 상태 |
|------|------|------|
| 프로세스 격리 (Job Object) | Preventive | Design |
| 심박 모니터링 (1Hz, 3-miss fail) | Detective | Implemented |
| 자동 워커 재시작 (max 3) | Corrective | Implemented |
| 응답 헤더 magic number | Detective | Implemented |
| Response slot 크기 검증 | Detective | Implemented |
| Null pointer 체크 | Preventive | Code review |

**잔존 위험도**: LOW (격리 + 모니터링 + 검증)

---

### HAZ-AI-05: 모델 파일 손상/조작 (Model Integrity Failure)

| 항목 | 설명 |
|------|------|
| **ID** | HAZ-AI-05 |
| **위험 이벤트** | 모델 파일이 손상되거나 의도적으로 변조됨 |
| **원인** | 파일 시스템 오류, 악의적 접근, 전송 오류 |
| **효과** | 잘못된 추론 결과 → 진단 오류 |
| **심각도** | High (진단 정확도 손상) |
| **발생가능성** | Very Low (파일 서명 검증) |
| **위험도** | High × Very Low = **LOW** |

**탐지**:
- SHA-256 해시 검증 (시작 시)
- 런타임 파일 변경 감지 (선택사항)

**통제**:
| 통제 | 유형 | 상태 |
|------|------|------|
| SHA-256 서명 검증 (startup) | Preventive | Implemented |
| 예상 해시 config에 저장 | Preventive | Design |
| 검증 실패 시 추론 차단 | Corrective | Implemented |
| 모델 파일 읽기 전용 권한 | Administrative | Deployment |
| 모델 배포 체크리스트 | Preventive | SOP |

**잔존 위험도**: NEGLIGIBLE (SHA-256 + 차단 로직)

---

### HAZ-AI-06: IPC 타임아웃으로 인한 무한 대기 (IPC Timeout Starvation)

| 항목 | 설명 |
|------|------|
| **ID** | HAZ-AI-06 |
| **위험 이벤트** | IPC 타임아웃이 3회 연속 발동 → 파이프라인 정지 |
| **원인** | 워커 과부하, 교착 상태, 메모리 부족 |
| **효과** | 이미지 처리 시간 초과 → 진단 지연, 워크플로우 중단 |
| **심각도** | Medium (가용성 문제) |
| **발생가능성** | Low (5s 타임아웃 충분함) |
| **위험도** | Medium × Low = **MODERATE** |

**탐지**:
- 타임아웃 카운터 모니터링
- 워커 CPU/메모리 사용량 추적

**통제**:
| 통제 | 유형 | 상태 |
|------|------|------|
| 5s IPC 타임아웃 | Preventive | Implemented |
| 타임아웃 3회 임계값 → AI 비활성화 | Corrective | Implemented |
| 워커 자동 재시작 (전) | Corrective | Implemented |
| 비블로킹 아키텍처 | Preventive | Design |
| 메모리 할당 제한 (Job Object) | Preventive | Design |

**잔존 위험도**: LOW (타임아웃 + 비블로킹 + 모니터링)

---

### HAZ-AI-07: DL 디노이저 해부학적 구조 제거 (Denoiser Anatomy Erasure)

| 항목 | 설명 |
|------|------|
| **ID** | HAZ-AI-07 |
| **위험 이벤트** | DL 디노이저가 과도하게 공격적으로 미세 구조를 제거 |
| **원인** | 모델 과학습, 필터 강도 과설정 |
| **효과** | 작은 병변/미세 구조 손실 → 진단 오류 |
| **심각도** | High (임상 정보 손실) |
| **발생가능성** | Low (SSIM gate, fail-closed) |
| **위험도** | High × Low = **MODERATE** |

**탐지**:
- SSIM 점수 모니터링 (≥ 0.95)
- 고전적 노이즈 감소와 비교
- 전문가 시각 검증

**통제**:
| 통제 | 유형 | 상태 |
|------|------|------|
| SSIM ≥ 0.95 품질 기준 | Preventive | Testing |
| Fail-closed: 오류 시 고전적 방법 | Corrective | Implemented |
| 선택적 활성화 (기본 OFF) | Administrative | Design |
| 모델 검증 (테스트 셋) | Preventive | Testing |
| 임상 리뷰 (샘플 이미지) | Detective | Manual |

**잔존 위험도**: LOW (품질 기준 + fail-closed + optional)

---

### HAZ-AI-08: 이미지 스티칭 Seam 아티팩트 (Stitch Seam Artifact)

| 항목 | 설명 |
|------|------|
| **ID** | HAZ-AI-08 |
| **위험 이벤트** | 스티칭 이음새에서 잘못된 정렬/혼합으로 인한 유사 해부학 생성 |
| **원인** | 위상 상관 오정렬, CNN blending 실패 |
| **효과** | 스티칭 이음새에서 거짓 병변/아티팩트 → 오진단 |
| **심각도** | High (거짓 양성 진단) |
| **발생가능성** | Low (겹침 검증, confidence gate) |
| **위험도** | High × Low = **MODERATE** |

**탐지**:
- Seam 영역 SSIM ≥ 0.98 검증
- 정렬 정확도 ≤ 2px RMS
- 겹침 비율 모니터링

**통제**:
| 통제 | 유형 | 상태 |
|------|------|------|
| 최소 겹침 5% 검증 | Preventive | Implemented |
| 겹침 < 5% → 거부 (XPE_ERR_INSUFFICIENT_OVERLAP) | Preventive | Implemented |
| SSIM ≥ 0.98 blending 기준 | Preventive | Testing |
| 정렬 정확도 ≤ 2px RMS | Preventive | Testing |
| Seam 영역 신뢰도 맵 | Detective | Design |
| 선택적 활성화 (명시적 요청) | Administrative | Design |

**잔존 위험도**: LOW (겹침 검증 + 품질 기준 + 선택적)

---

## 3. 위험 등급 요약 (Risk Summary)

| HAZ ID | 위험 이벤트 | 심각도 | 발생가능성 | 위험도 | 통제 상태 | 잔존 위험 |
|--------|---------|--------|---------|--------|----------|----------|
| HAZ-AI-01 | 신체 부위 오분류 | M | L | MOD | ✓ Impl | LOW |
| HAZ-AI-02 | 파생 이미지 오용 | H | L | MOD-H | ✓ Design | LOW |
| HAZ-AI-03 | ROI 거짓 음수 | H | L | MOD-H | ✓ Impl | LOW |
| HAZ-AI-04 | 워커 크래시 | H | L | MOD | ✓ Impl | LOW |
| HAZ-AI-05 | 모델 손상 | H | VL | LOW | ✓ Impl | NEG |
| HAZ-AI-06 | IPC 타임아웃 | M | L | MOD | ✓ Impl | LOW |
| HAZ-AI-07 | 디노이저 구조 제거 | H | L | MOD | ✓ Impl | LOW |
| HAZ-AI-08 | 스티칭 아티팩트 | H | L | MOD | ✓ Impl | LOW |

**결론**: 모든 식별된 위험이 **ALARP(As Low As Reasonably Practicable)** 수준까지 통제됨. 통제 구현 및 운영 절차 준수 시 허용 가능한 수준.

---

## 4. 위험 통제 이행 현황

### 4.1 설계 단계 통제 (Design Controls)

| 통제 | HAZ | 상태 | 증거 |
|------|-----|------|------|
| Sidecar-only output | AI-02 | ✓ Complete | SAD-AI-001 §2.1 |
| 프로세스 격리 (Job Object) | AI-04 | ✓ Complete | SAD-AI-001 §2.2 |
| 비블로킹 아키텍처 | AI-06 | ✓ Complete | SAD-AI-001 §1.2 |
| 축소만(shrink-only) 정책 | AI-03 | ✓ Complete | SRS-AI-001 §FR-AI-150 |
| 선택적 활성화 (opt-in) | AI-07, AI-08 | ✓ Complete | PRD §SWU-2.12, SWU-2.9 |

### 4.2 구현 단계 통제 (Implementation Controls)

| 통제 | HAZ | 상태 | 증거 |
|------|-----|------|------|
| OOD 탐지 (confidence threshold) | AI-01 | ✓ Implemented | SRS-AI-001 §FR-AI-130 |
| SHA-256 모델 검증 | AI-05 | ✓ Implemented | SRS-AI-001 §FR-AI-200 |
| 심박 모니터링 + 재시작 | AI-04, AI-06 | ✓ Implemented | SAD-AI-001 §3.0 |
| IPC 타임아웃 (5s) | AI-06 | ✓ Implemented | SAD-AI-001 §3.2 |
| Response 유효성 검사 | AI-04 | ✓ Implemented | SAD-AI-001 §6.2 |
| SSIM 품질 기준 | AI-07, AI-08 | Testing | TDS-AI-001 (향후) |

### 4.3 운영 단계 통제 (Operational Controls)

| 통제 | HAZ | 상태 | 책임 |
|------|-----|------|------|
| 모델 파일 권한 관리 (read-only) | AI-05 | ⊙ Planned | DevOps |
| 배포 체크리스트 (SHA-256) | AI-05 | ⊙ Planned | Release Manager |
| 운영 교육 (파생 vs 주 이미지) | AI-02 | ⊙ Planned | Clinical Training |
| 주기적 QA 샘플 리뷰 | AI-01, AI-03, AI-07 | ⊙ Planned | QA Lead |

---

## 5. 변화 분석 (Change Analysis)

향후 모델 업데이트, 기능 추가 시:
- 해당 위험 재평가 필수
- 새로운 위험 식별 (HAZOP 분석)
- 통제 효과성 재검증
- 임상 영향 분석

---

## 6. 참고 문서

- IEC 62304:2006+A1:2015 (Software lifecycle)
- ISO 14971:2019 (Medical device risk management)
- FDA Software as a Medical Device (SaMD) Validation Guidance
- AAMI TIR57:2016 (Guidance on software validation for medical devices)

---

*SHA-AI-001 소프트웨어 위험 분석 v1.0.0 끝*
