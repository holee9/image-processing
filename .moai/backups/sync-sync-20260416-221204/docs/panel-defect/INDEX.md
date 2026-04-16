# Panel Defect Correction Module - Documentation Index

**Module**: `xpe_preprocess.dll` (Stage 3, Layer 1)  
**Safety Classification**: IEC 62304 Class B  
**Document Package Version**: 1.0  
**Date**: 2026-04-14  

---

## 개요

이 디렉토리는 Panel Defect Correction Module의 완전한 IEC 62304 의료기기 규격 문서 패키지를 포함합니다.

## 문서 네비게이션

### 1. 요구사항 및 설계 문서

#### 시작점: [README.md](README.md) (기술 개요)
- 모듈 목적 및 파이프라인 개요
- ANN 아키텍처 및 Min/Normal/Max 프로필
- API 레퍼런스 및 성능 예산
- **읽어야 할 사람**: 모든 개발자, QA 엔지니어

#### 세부 요구사항: [xray-panel-defect-prd.md](xray-panel-defect-prd.md)
- Product Requirements Document (PRD)
- 알고리즘 수학 명세서 (RMM, ANN, DWT/DCT)
- 성능 목표 및 참고문헌
- **읽어야 할 사람**: 알고리즘 엔지니어, 아키텍트

#### 소프트웨어 요구사항: [SRS-DEFECT-001](SRS-DEFECT-001_Software_Requirements_Specification.md)
- 45 functional + 9 safety + 8 performance 요구사항
- 각 요구사항별 우선순위 및 검증 방법
- **읽어야 할 사람**: 소프트웨어 개발자, QA

#### 소프트웨어 아키텍처: [SAD-DEFECT-001](SAD-DEFECT-001_Software_Architecture_Document.md)
- 11개 Software Units (SWU) 분해
- Data flow, memory layout, API 명세
- Anti-spaghetti 규칙 및 성능 예산
- **읽어야 할 사람**: 소프트웨어 설계자, 개발 팀장

### 2. 안전 및 규정 준수 문서

#### 위험 분석: [SHA-DEFECT-001](SHA-DEFECT-001_Software_Hazard_Analysis.md)
- ISO 14971:2019 준수
- 8개 hazard 식별 (HAZ-DEFECT-001~008)
- 각 hazard별 severity/probability 평가
- 위험 통제 조치 (SRS 링크)
- **읽어야 할 사람**: 안전 담당자, 규제 담당자

#### 추적성 행렬: [RTM-DEFECT-001](RTM-DEFECT-001_Requirements_Traceability_Matrix.md)
- 양방향 추적성: SRS ↔ SAD ↔ SHA ↔ Tests
- 모든 요구사항 ↔ 테스트 사례 매핑
- IEC 62304 §5.1.1c 준수
- **읽어야 할 사람**: 규제 담당자, QA

### 3. 테스트 및 검증 문서

#### 캘리브레이션 프로토콜: [IAP-DEFECT-001](IAP-DEFECT-001_Image_Acquisition_Protocol.md)
- Static BPM 생성 절차
- Dark frame, flat-field, flickering, grid 취득
- 온도별 캘리브레이션 (20/30/40°C)
- 수용 기준 (<0.1% hot/cold, <5 lines)
- **읽어야 할 사람**: 캘리브레이션 엔지니어, QA

#### 테스트 데이터셋: [TDS-DEFECT-001](TDS-DEFECT-001_Test_Dataset_Specification.md)
- 합성 BPM 데이터셋 (known defects)
- ANN 검증 데이터셋 (NMSE 메트릭)
- 라인/그리드 억제 데이터셋
- Golden references (SHA-256 lock)
- **읽어야 할 사람**: QA, 테스트 엔지니어

---

## 역할별 읽기 가이드

### Software Developer
1. 먼저: [README.md](README.md) (파이프라인 개요)
2. 다음: [SAD-DEFECT-001](SAD-DEFECT-001_Software_Architecture_Document.md) (SWU 명세)
3. 참고: [SRS-DEFECT-001](SRS-DEFECT-001_Software_Requirements_Specification.md) (요구사항)
4. 검증: [RTM-DEFECT-001](RTM-DEFECT-001_Requirements_Traceability_Matrix.md) (테스트 케이스)

### QA / Test Engineer
1. 먼저: [README.md](README.md)
2. 다음: [TDS-DEFECT-001](TDS-DEFECT-001_Test_Dataset_Specification.md) (테스트 데이터)
3. 다음: [IAP-DEFECT-001](IAP-DEFECT-001_Image_Acquisition_Protocol.md) (캘리브레이션)
4. 검증: [RTM-DEFECT-001](RTM-DEFECT-001_Requirements_Traceability_Matrix.md)
5. 기준: [SRS-DEFECT-001](SRS-DEFECT-001_Software_Requirements_Specification.md)

### Safety / Regulatory Officer
1. 먼저: [SHA-DEFECT-001](SHA-DEFECT-001_Software_Hazard_Analysis.md) (위험 분석)
2. 다음: [RTM-DEFECT-001](RTM-DEFECT-001_Requirements_Traceability_Matrix.md) (추적성)
3. 다음: [SRS-DEFECT-001](SRS-DEFECT-001_Software_Requirements_Specification.md) (요구사항)
4. 참고: [SAD-DEFECT-001](SAD-DEFECT-001_Software_Architecture_Document.md)

### Calibration Engineer
1. 먼저: [IAP-DEFECT-001](IAP-DEFECT-001_Image_Acquisition_Protocol.md) (취득 프로토콜)
2. 다음: [README.md](README.md) (BPM 포맷)
3. 참고: [TDS-DEFECT-001](TDS-DEFECT-001_Test_Dataset_Specification.md) (검증)

### Algorithm Researcher
1. 먼저: [xray-panel-defect-prd.md](xray-panel-defect-prd.md) (알고리즘 명세)
2. 다음: [README.md](README.md) (아키텍처)
3. 검증: [TDS-DEFECT-001](TDS-DEFECT-001_Test_Dataset_Specification.md) (메트릭)

---

## 문서 구조 및 상호 참조

```
┌─────────────────────────────────────────────────────┐
│         xray-panel-defect-prd.md (PRD)              │
│  알고리즘 요구사항 원본 (RMM, ANN, DWT/DCT)        │
└──────────────────┬──────────────────────────────────┘
                   │ 파생
      ┌────────────┼──────────────┐
      │            │              │
      ▼            ▼              ▼
┌──────────┐  ┌──────────┐  ┌──────────────┐
│ SRS-001  │  │ SAD-001  │  │ SHA-001      │
│ (필수)   │  │(설계)    │  │ (위험분석)   │
└──────┬───┘  └────┬─────┘  └──────┬───────┘
       │           │               │
       └───────────┼───────────────┘
                   │ 추적
                   ▼
           ┌──────────────┐
           │ RTM-001      │
           │ (추적성)     │
           └──────┬───────┘
                  │ 테스트입력
     ┌────────────┴─────────────┐
     ▼                          ▼
┌──────────┐            ┌──────────────┐
│IAP-001   │            │TDS-001       │
│(캘리브) │            │(테스트 데이터)│
└──────────┘            └──────────────┘

README.md: 모든 문서의 기술 개요 및 네비게이션 역할
```

---

## 문서 특성

### 언어
- **사용자 대면 내용**: Korean (한국어)
- **기술 명세 및 식별자**: English (논문/표준 호환성)

### 규정 준수
- **IEC 62304:2006 (amended 2015)**: Class B 의료기기 소프트웨어 생명주기
- **ISO 14971:2019**: 의료기기 위험 관리
- **IEC 62220-1-1:2015**: Detective Quantum Efficiency 이미지 품질

### 크기 및 범위
- **총 문서 수**: 8 (+ INDEX.md)
- **총 분량**: ~130 KB
- **총 요구사항**: 62 (45 FR + 9 SAF + 8 PERF)
- **총 SWU**: 11 software units
- **총 Hazard**: 8 (모두 control 후 Low risk)
- **총 Test Case**: 100+ (UT + IT + ST)

---

## 버전 관리

### 현재 버전: 1.0 (2026-04-14)

모든 문서는 다음 필드를 포함합니다:
- Document ID (예: SRS-DEFECT-001 v1.0)
- Version number
- Date of creation/revision
- Author/Owner

### 업데이트 정책

문서 변경 시:
1. Version number 증가 (minor: 1.1, major: 2.0)
2. "Last Updated" 날짜 갱신
3. Change log 또는 revision history 추가 (권장)

---

## 상호 참조 규칙

### 문서 간 링크
```markdown
[SRS-DEFECT-001](SRS-DEFECT-001_Software_Requirements_Specification.md)
[SHA hazards](SHA-DEFECT-001_Software_Hazard_Analysis.md#위험-식별-테이블)
```

### 요구사항 참조 형식
- **FR-101**: Functional requirement 101
- **SAF-101**: Safety requirement 101
- **PERF-101**: Performance requirement 101
- **SWU-3.1**: Software Unit 3.1
- **HAZ-DEFECT-001**: Hazard 1

---

## 자주 찾는 정보

### "어떤 요구사항이 이 hazard를 통제하는가?"
→ [SHA-DEFECT-001](SHA-DEFECT-001_Software_Hazard_Analysis.md) 참조 → "통제 조치 (SRS)" 컬럼

### "이 ANN 아키텍처의 성능 목표는?"
→ [xray-panel-defect-prd.md](xray-panel-defect-prd.md) §성능 목표 또는 [SRS-DEFECT-001](SRS-DEFECT-001_Software_Requirements_Specification.md) FR-301/302

### "테스트 데이터셋을 어떻게 생성하는가?"
→ [TDS-DEFECT-001](TDS-DEFECT-001_Test_Dataset_Specification.md)

### "캘리브레이션을 어떻게 수행하는가?"
→ [IAP-DEFECT-001](IAP-DEFECT-001_Image_Acquisition_Protocol.md)

### "소프트웨어 아키텍처는 어떻게 되는가?"
→ [SAD-DEFECT-001](SAD-DEFECT-001_Software_Architecture_Document.md)

### "현재 요구사항이 모두 테스트되는가?"
→ [RTM-DEFECT-001](RTM-DEFECT-001_Requirements_Traceability_Matrix.md)

---

## 규제 제출 체크리스트

의료기기 규제 제출 전 확인 사항:

- [ ] IEC 62304 compliance: SRS + SAD + SHA + RTM 검토
- [ ] All 62 requirements traced to tests
- [ ] All 8 hazards have LOW residual risk after controls
- [ ] 45+ unit tests + integration tests documented (SRS 기반)
- [ ] ANN performance (NMSE < 0.14/0.20) validated with TDS
- [ ] Calibration protocol (IAP) executed per specification
- [ ] Audit trail and logging implemented (SAF-201, SAF-202)
- [ ] MD5/SHA-256 integrity checks on calibration files (SAF-203)
- [ ] 100% traceability matrix complete

---

**Document Package Version**: 1.0  
**Status**: Complete and Ready for Implementation  
**Last Updated**: 2026-04-14  
**Next Step**: Code implementation following SAD SWU specifications
