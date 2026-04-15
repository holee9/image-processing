# Market Requirements Document

**Document ID**: XPE-MRD-001  
**Version**: 1.0.0  
**Date**: 2026-04-15  
**Status**: Controlled Draft  
**Classification**: Internal / Confidential  
**Author**: XPE Program Management  
**Approval**: __________________ Date: __________  
**Regulatory Scope**: IEC 62304, ISO 14971, FDA 21 CFR 820.30, EU MDR 2017/745  
**Canonical Scope**: `docs/project/`  
**Superseded by**: —  
**Derived from**: `docs/project/product.md`, `docs/xray_fpd_tech_classification_final.md`, `docs/post-processing/xpe/XPE-PRD-002_Detailed_Project_Execution_PRD.md`

---

## 1. 목적 (Purpose)

본 문서는 X-ray Image Processing Engine(XPE)의 시장 요구사항을 정의한다. MRD는 문서 계층의 최상위 기준으로, 아래 하위 문서들의 요구사항 우선순위와 비즈니스 정당성을 제공한다.

```
XPE-MRD-001 (Market Requirements)         ← 본 문서
    └── XPE-PRD-SYSTEM-001 (System PRD)
          └── Module PRDs / SRS / SDD / RTM
                └── XPE-SVVP-001 (System V&V Plan)
```

---

## 2. 시장 문제 정의 (Market Problem Statement)

### 2.1 문제 배경

X-ray Flat Panel Detector(FPD) 기반 의료 영상 시스템에서 Raw 센서 데이터를 진단 가능한 DICOM 영상으로 변환하는 영상처리 소프트웨어는 필수 구성요소다. 그러나 FPD 제조사 및 의료기기 OEM은 다음 세 가지 문제에 직면한다.

### 2.2 핵심 시장 문제

**문제 1: 자체 개발 비용과 규제 부담**

FPD 제조사가 영상처리 SW를 자체 개발하려면 IEC 62304 Class B/C 규제 문서 패키지(SDP, SRS, SDD, SAD, RTM, VVP, SHA, SOUP, SRM, SCM 등) 전체를 처음부터 작성해야 한다. 이는 전문 규제 엔지니어, 알고리즘 연구자, SW 개발자 팀을 최소 2년 이상 운영해야 하는 비용을 의미한다. 이 비용은 중소형 FPD 제조사에게 시장 진입 장벽이 되고 있다.

**문제 2: 알고리즘 품질 차별화 어려움**

FPD 시장의 영상처리 품질 경쟁은 점점 심화되고 있다. Lag/Ghost 보정 정확도(14-50x 업계 우위), 결함 픽셀 보정의 ML 기반 고품질 처리(14.2x NMSE 우위), DICOM GSDF 준수 디스플레이 파이프라인 등은 자체 개발 시 막대한 R&D 투자가 필요한 기술이다. 외부 SW 컴포넌트 없이는 이 경쟁 격차를 단기간에 메우기 어렵다.

**문제 3: 하드웨어-소프트웨어 통합 복잡성**

FPGA 기반 HW 보정과 SW 보정 간의 책임 분리, AED(Auto Exposure Detection), 온도 보정, 픽셀 비닝 등 HW 의존적 알고리즘의 SW-first 설계를 지원하지 않으면, 시스템 통합 시 인터페이스 충돌과 의존성 복잡성이 급격히 증가한다.

---

## 3. 목표 고객 세그먼트 (Target Customer Segments)

### 세그먼트 1: FPD 제조사 (OEM 통합)

**설명**: FPD 하드웨어(센서, ASIC, 인터페이스 보드)를 제조하며, 자체 또는 OEM SW 스택으로 의료기기를 구성하는 기업.

**핵심 요구사항**:
- 빠른 시장 출시 (Time-to-Market 단축)
- HW 의존성 최소화 — DLL 교체 없이 FPGA 오프로드 전환 가능
- C ABI 호환 인터페이스 — 기존 SW 스택에 최소 수정으로 통합
- IEC 62304 Class B 규제 패키지 완비 (직접 제출 가능 수준)
- 알고리즘 성능 벤치마크 데이터 제공 (영업 지원)

**성공 지표**: 신규 FPD 모델 시장 출시까지 SW 개발 기간 50% 단축

---

### 세그먼트 2: 의료기기 시스템 통합업체

**설명**: 방사선실 솔루션(RIS, PACS, 워크스테이션)과 FPD를 통합하는 시스템 통합업체 또는 의료기기 OEM.

**핵심 요구사항**:
- DICOM 완전 호환성 (IHE 프로파일 지원)
- 안정적인 API 계약 (하위 호환성 보장)
- 모듈식 DLL 구조 (필요 기능만 선택 가능)
- 다중 FPD 모델 동시 지원
- 포괄적인 통합 문서 및 기술 지원

**성공 지표**: 기존 시스템에 XPE 통합 시 개발 기간 3개월 이내

---

### 세그먼트 3: 병원 IT 및 방사선과

**설명**: 영상처리 SW를 직접 구매하거나 기기 선정 과정에 참여하는 병원 IT 부서 및 방사선과.

**핵심 요구사항**:
- DICOM Conformance Statement (공개 문서)
- 기존 PACS/RIS와의 호환성 검증
- 임상적 영상 품질 보증 (독자 평가 결과)
- 사이버보안 요구사항 충족 (FDA Cybersecurity, EU MDR)
- 업그레이드 호환성 (배포 후 유지보수)

**성공 지표**: 임상 파일럿에서 영상 품질 점수 ≥ 3.5/5 (5점 척도)

---

## 4. 시장 요구사항 (Market Requirements)

### 4.1 기능 요구사항 (MR-FUNC)

| ID | 시장 요구사항 | 우선순위 | 대응 PRD |
|----|------------|---------|---------|
| MR-FUNC-001 | FPD Raw 데이터(14-16bit)에서 Dark/Offset, Gain/Flat-Field, Defective Pixel, Ghost/Lag 보정을 수행하여 Clean 이미지를 생성해야 한다. | Must | PR-FUNC-001~004 |
| MR-FUNC-002 | 보정된 이미지에 Log Transform, Noise Reduction, Contrast Enhancement, Edge Enhancement를 적용하여 진단 가능한 이미지를 생성해야 한다. | Must | PR-FUNC-010~013 |
| MR-FUNC-003 | DICOM Grayscale Display Pipeline(Modality LUT, VOI LUT, Presentation LUT/GSDF)을 준수하는 디스플레이 출력을 제공해야 한다. | Must | PR-FUNC-020~023 |
| MR-FUNC-004 | DICOM 표준을 준수하는 파일 읽기/쓰기 및 네트워크 SCU 기능을 제공해야 한다. | Must | PR-FUNC-030~032 |
| MR-FUNC-005 | 검출기 교정 파라미터(Offset Map, Gain Map, Bad Pixel Map)를 관리하고 SID별로 적용해야 한다. | Must | PR-FUNC-040 |
| MR-FUNC-006 | IEC 62494-1 기준 Exposure Index / Deviation Index를 계산하여 제공해야 한다. | Must | PR-FUNC-041 |
| MR-FUNC-007 | 자동 노출 감지(AED) 이벤트를 처리하고 처리 파이프라인에 전달해야 한다. | Must | PR-FUNC-042 |
| MR-FUNC-008 | 다중 스케일 주파수 처리(≥8 레벨 Laplacian pyramid)로 고급 이미지 품질을 제공해야 한다. (Phase 2) | Should | PR-FUNC-014 |
| MR-FUNC-009 | Grid Suppression / Virtual Grid 기능으로 산란선을 억제해야 한다. (Phase 2) | Should | PR-FUNC-050 |
| MR-FUNC-010 | CNN 기반 신체 부위 인식(≥15 카테고리, ≥95% 정확도)으로 처리 파라미터를 자동 선택해야 한다. (Phase 3) | Could | PR-FUNC-016 |
| MR-FUNC-011 | DL 기반 Bone Suppression(PSNR≥33dB, SSIM≥0.97)을 제공해야 한다. (Phase 3) | Could | PR-FUNC-018 |
| MR-FUNC-012 | AI 기능 실패 시 결정론적 처리 결과로 자동 폴백해야 한다. (Phase 3 필수 안전 요구사항) | Must (for Phase 3) | PR-SAFE-010 |

### 4.2 성능 요구사항 (MR-PERF)

| ID | 시장 요구사항 | 우선순위 | 대응 PRD |
|----|------------|---------|---------|
| MR-PERF-001 | 3072×3072 이미지의 Pre-processing(Phase 1a)을 500ms 이내에 완료해야 한다. | Must | PR-PERF-001 |
| MR-PERF-002 | 전체 파이프라인(Pre→Core→Display)을 3초 이내에 완료해야 한다. | Must | PR-PERF-002 |
| MR-PERF-003 | Window/Level 인터랙티브 조정 응답이 16ms(60fps) 이내여야 한다. | Must | PR-PERF-003 |
| MR-PERF-004 | 최대 메모리 사용량이 4096×4096 이미지 처리 시 2GB 이하여야 한다. | Must | PR-PERF-004 |
| MR-PERF-005 | 1000회 연속 이미지 처리 후 메모리 누수 없이 안정적으로 동작해야 한다. | Must | PR-PERF-005 |

### 4.3 규제 및 인증 요구사항 (MR-REG)

| ID | 시장 요구사항 | 우선순위 | 대응 PRD |
|----|------------|---------|---------|
| MR-REG-001 | IEC 62304 Class B 소프트웨어 수명주기 규제 패키지를 완비해야 한다. (SDP, SRS, SDD, SAD, RTM, VVP, SHA, SOUP, SRM, SCM, SPR, SMP, SRP 포함) | Must | PR-REG-001 |
| MR-REG-002 | ISO 14971 기반 소프트웨어 위험 관리 파일(SRM)을 작성해야 한다. | Must | PR-REG-002 |
| MR-REG-003 | FDA 21 CFR 820.30 Design Controls 요구사항을 충족해야 한다. | Must | PR-REG-003 |
| MR-REG-004 | EU MDR 2017/745 Annex I 필수 성능 요구사항을 충족해야 한다. | Must | PR-REG-004 |
| MR-REG-005 | AI 기능(Phase 3)은 임상 의사 결정 보조 도구임을 명시하고, AI 처리 이미지에 명시적 표시를 제공해야 한다. | Must (for Phase 3) | PR-SAFE-008 |

### 4.4 호환성 요구사항 (MR-COMPAT)

| ID | 시장 요구사항 | 우선순위 | 대응 PRD |
|----|------------|---------|---------|
| MR-COMPAT-001 | DICOM 3.0 표준을 완전히 준수해야 한다. (DVTk 도구로 검증 가능) | Must | PR-FUNC-030 |
| MR-COMPAT-002 | C ABI(C89 호환) 인터페이스를 통해 C, C++, C#, Python 등 다양한 언어에서 호출 가능해야 한다. | Must | PR-COMPAT-001 |
| MR-COMPAT-003 | Windows 10/11 64-bit 환경에서 동작해야 한다. 추후 Linux 지원 확장 고려. | Must | PR-COMPAT-002 |

### 4.5 사업 요구사항 (MR-BUSI)

| ID | 시장 요구사항 | 우선순위 | 대응 PRD |
|----|------------|---------|---------|
| MR-BUSI-001 | 알고리즘 모듈 단위로 라이선스 선택이 가능해야 한다. (Must-Have vs 차별화 기능 분리 판매) | Should | PR-BUSI-001 |
| MR-BUSI-002 | 신규 FPD 모델 추가 시 교정 파라미터 갱신만으로 지원이 가능해야 한다. (SW 재컴파일 없이) | Must | PR-BUSI-002 |
| MR-BUSI-003 | 규제 인증 제출용 문서 패키지를 고객에게 제공할 수 있어야 한다. | Should | PR-BUSI-003 |

---

## 5. 사업 성공 지표 (Business KPIs)

### 5.1 기술적 성과 지표

| KPI | 목표 | 측정 방법 |
|-----|------|---------|
| Pre-processing 처리 시간 | ≤ 500ms (3072×3072) | 자동화 성능 테스트 |
| 전체 파이프라인 처리 시간 | ≤ 3초 | 자동화 성능 테스트 |
| Ghost 보정 정확도 (Tier 1) | ≥ 90% ghost removal | 합성 데이터 테스트 |
| 결함 픽셀 보정 PSNR | ≥ 60dB (대비 참조 이미지) | 자동화 품질 테스트 |
| DICOM GSDF 준수 | Δ JND ≤ 1% | DVTk 검증 |

### 5.2 규제 인증 달성 지표

| KPI | 목표 | 측정 방법 |
|-----|------|---------|
| IEC 62304 Class B 문서 패키지 완비 | Phase 1b 출시 전 | 문서 체크리스트 감사 |
| Unit Test 커버리지 | Statement ≥ 80% per SWU | gcov + lcov |
| 메모리 누수 | Zero (ASan clean) | AddressSanitizer |
| Static Analysis 위반 | Zero critical/high | cppcheck + clang-tidy |

### 5.3 고객 수용 지표

| KPI | 목표 | 측정 방법 |
|-----|------|---------|
| 임상 이미지 품질 점수 | ≥ 3.5/5 (5점 척도) | 방사선사 독자 평가 (N=50) |
| Phase 1b 고객 통합 기간 | ≤ 3개월 | 고객 피드백 |
| DICOM DVTk Full Pass | 100% | DVTk 자동 검증 |

---

## 6. MR → PR 추적성 테이블

다음 표는 시장 요구사항(MR-xxx)과 시스템 PRD 요구사항(PR-xxx) 간의 상향 추적성을 제공한다.

| MR ID | MR 설명 (요약) | 대응 PR-xxx |
|-------|-------------|-----------|
| MR-FUNC-001 | Pre-processing 4종 보정 | PR-FUNC-001, 002, 003, 004 |
| MR-FUNC-002 | Core 처리 (Log, NR, CE, EE) | PR-FUNC-010, 011, 012, 013 |
| MR-FUNC-003 | DICOM Display Pipeline | PR-FUNC-020, 021, 022, 023 |
| MR-FUNC-004 | DICOM 파일/네트워크 | PR-FUNC-030, 031, 032 |
| MR-FUNC-005 | 교정 파라미터 관리 | PR-FUNC-040 |
| MR-FUNC-006 | EI / DI 계산 | PR-FUNC-041 |
| MR-FUNC-007 | AED 이벤트 처리 | PR-FUNC-042 |
| MR-FUNC-008 | 멀티스케일 주파수 처리 | PR-FUNC-014, 015 |
| MR-FUNC-009 | Grid Suppression/Virtual Grid | PR-FUNC-050 |
| MR-FUNC-010 | CNN 신체 부위 인식 | PR-FUNC-016 |
| MR-FUNC-011 | DL Bone Suppression | PR-FUNC-018 |
| MR-FUNC-012 | AI 폴백 | PR-SAFE-010 |
| MR-PERF-001 | Pre-processing 시간 | PR-PERF-001 |
| MR-PERF-002 | 전체 파이프라인 시간 | PR-PERF-002 |
| MR-PERF-003 | W/L 응답 시간 | PR-PERF-003 |
| MR-PERF-004 | 메모리 제한 | PR-PERF-004 |
| MR-PERF-005 | 장기 안정성 | PR-PERF-005 |
| MR-REG-001 | IEC 62304 문서 패키지 | PR-REG-001 |
| MR-REG-002 | ISO 14971 SRM | PR-REG-002 |
| MR-REG-003 | FDA 21 CFR 820.30 | PR-REG-003 |
| MR-REG-004 | EU MDR 2017/745 | PR-REG-004 |
| MR-REG-005 | AI 임상 표시 | PR-SAFE-008 |
| MR-COMPAT-001 | DICOM 3.0 준수 | PR-FUNC-030 |
| MR-COMPAT-002 | C ABI 인터페이스 | PR-COMPAT-001 |
| MR-COMPAT-003 | Windows 10/11 64-bit | PR-COMPAT-002 |
| MR-BUSI-001 | 모듈별 라이선스 | PR-BUSI-001 |
| MR-BUSI-002 | 파라미터 기반 FPD 추가 | PR-BUSI-002 |
| MR-BUSI-003 | 규제 문서 고객 제공 | PR-BUSI-003 |

---

## 7. 경쟁 포지션 요약

XPE의 핵심 차별화 기술은 다음과 같다:

| 기술 | 차별화 지표 | 적용 Phase |
|------|-----------|----------|
| Lag 보정 (NLCSC 비선형) | 14-50x 업계 우위 | Phase 1a (선택) |
| 결함 픽셀 보정 (ML/ViT AE) | 14.2x NMSE 우위 | Phase 3 |
| Virtual Grid (GSVG) | 산란선 억제 + 격자 제거 | Phase 2 |
| Bone Suppression (DL) | PSNR≥33dB, SSIM≥0.97 | Phase 3 |

Must-Have 기능(MR-FUNC-001~007, 전체 MR-PERF, MR-REG)은 시장 진입 기준이며, 차별화 기능(MR-FUNC-008~012)은 Phase 2~3에서 경쟁 우위를 제공한다.

---

## 8. 제약 조건 및 전제

- 본 MRD는 XPE가 의료기기에 통합되는 **SW 컴포넌트**임을 전제한다. XPE 자체는 독립적인 의료기기가 아니다.
- Safety Class B 결정은 IEC 62304 Class B working assumption으로 사용되며, 시스템 Hazard Analysis(ISO 14971) 완료 후 최종 확정된다.
- 경쟁사 데이터는 내부 R&D 벤치마크 기준이며, 공개 발표 전 검증이 필요하다.

---

## 9. 개정 이력

| Rev | 날짜 | 저자 | 설명 |
|-----|------|------|------|
| 1.0.0 | 2026-04-15 | MoAI (SPEC-DOC-001 구현) | 초안 작성 — 교차검증 Round 5 결과 반영 |

---

*Document End — XPE-MRD-001 v1.0.0*
