# XPE Post-Market Surveillance (PMS) Plan

**Document ID**: XPE-OPS-PMS-001  
**Version**: 0.1.0 (Draft)  
**Date**: 2026-04-22  
**SPEC Reference**: SPEC-XPE-OPS REQ-OPS-001~006  
**Regulatory Basis**: EU MDR Article 83, Annex III; FDA PMS Guidance (AI/ML, 2024)  
**IEC 62304 Class**: B

---

## 1. 목적 및 범위

### 1.1 목적

본 PMS 계획은 EU MDR Article 83에 따른 의무적 판매 후 감시 체계를 수립한다.
XPE가 현장 배포 후 지속적으로 안전하고 의도한 성능을 유지함을 보증한다.

### 1.2 제품 범위

| 제품 | 버전 범위 | 형태 |
|------|-----------|------|
| XPE Image Processing Engine | v1.0.0 이상 | 소프트웨어 라이브러리 (DLL + GUI) |
| 적용 의료기기 | XPE 통합 FPD X-ray 시스템 전반 | 의료기기 소프트웨어 (IEC 62304 Class B) |

---

## 2. PMS 데이터 소스

### 2.1 능동적 수집

| 데이터 소스 | 수집 방법 | 수집 주기 |
|-------------|-----------|-----------|
| 고객 불만 보고 | Support 티켓 시스템 | 실시간 |
| 현장 오류 로그 (익명화) | 자발적 텔레메트리 (opt-in) | 주간 집계 |
| Reject-Analysis 이벤트 | SPEC-XPE-OPS §4.5 OTEL 이벤트 | 실시간 (opt-in) |
| DI/EI Deviation Index 드리프트 | SPEC-XPE-OPS REQ-OPS-050~052 | 주간 집계 |

### 2.2 수동적 수집

| 데이터 소스 | 담당 | 수집 주기 |
|-------------|------|-----------|
| 학술 논문 모니터링 (SPIE, RSNA, AAPM) | Technical Lead | 분기별 |
| 규제 당국 공지 (FDA MAUDE, EUDAMED) | Regulatory Lead | 월간 |
| 경쟁사 유사 제품 불량 이벤트 | Quality Lead | 분기별 |
| 임상 사용자 설문 | Quality Lead | 연 1회 |

---

## 3. PMS 지표 (Indicators)

### 3.1 안전성 지표

| 지표 | 정상 범위 | 경보 임계값 |
|------|-----------|------------|
| 심각 불량 이벤트 (Serious Incident) 발생건수 | 0 | ≥ 1건 즉시 보고 |
| 오진 관련 불만 (진단 영향) | 0 | ≥ 1건 즉시 조사 |
| 크래시/예외 발생률 (1000 연구 기준) | < 0.1% | ≥ 1% |

### 3.2 성능 지표

| 지표 | 정상 범위 | 경보 임계값 |
|------|-----------|------------|
| Phase 1 처리 시간 (3072×3072) | < 3000ms | ≥ 5000ms |
| 피크 메모리 사용 (1000 프레임 연속) | ≤ 190MB | ≥ 250MB |
| DI Deviation Index 드리프트 | ±2σ 이내 | ±3σ 초과 |
| Reject-Analysis 재촬영률 | 현장 기준선 ±20% 이내 | ≥ 기준선 +50% |

### 3.3 보안 지표

| 지표 | 정상 범위 | 경보 임계값 |
|------|-----------|------------|
| 미패치 Critical CVE (CVSS ≥ 9.0) | 0 | ≥ 1건 즉시 조치 |
| SBOM 컴포넌트 지원 만료 | 0 (active) | ≥ 1개 만료 임박 |

---

## 4. 보고 체계

### 4.1 심각 사고 보고 (Serious Incident)

EU MDR Article 2(65) 정의에 해당하는 심각 사고 발생 시:

| 단계 | 대상 | 기한 |
|------|------|------|
| 내부 보고 | Quality Lead | 즉시 |
| 규제 당국 통지 (EU) | 관할 Notified Body | 2일 이내 (사망/악화 포함 시) / 10일 이내 (기타) |
| UK MHRA 통지 | MHRA | MHRA PMS 규정 (2025-06-16 발효) 적용 |
| FDA MAUDE 보고 (해당시) | FDA | MDR 30일 이내 |

긴급 연락처 목록: `docs/operations/competent-authority-contacts.md` (별도 관리)

### 4.2 정기 보고 (PSUR)

| 보고서 종류 | 대상 제품 등급 | 주기 |
|------------|---------------|------|
| Periodic Safety Update Report (PSUR) | Class IIb, III | 연간 |
| Periodic Summary Report (PSR) | Class IIa | 2년마다 |

PSUR 템플릿: `docs/operations/psur-template.md` (v0.1 별도 작성 예정)

---

## 5. 책임 매트릭스

| 활동 | 책임자 | 승인자 |
|------|--------|--------|
| PMS 계획 수립 및 갱신 | Quality Lead | Regulatory Lead |
| 불만 접수 및 분류 (10 영업일 이내) | Support 담당 | Quality Lead |
| 심각 사고 조사 | Technical Lead | Quality Lead + Regulatory Lead |
| 규제 당국 통지 발송 | Regulatory Lead | 경영진 |
| PSUR 작성 | Quality Lead | Regulatory Lead + 경영진 |
| PMS 지표 모니터링 | Quality Lead | — |
| 현장 배포 현황 관리 | Release Manager | — |

---

## 6. 정기 검토 일정

| 검토 항목 | 주기 | 담당 |
|-----------|------|------|
| PMS 지표 검토 | 분기별 | Quality Lead |
| PSUR 발행 | 연간 (IIb/III) | Regulatory Lead |
| PMS 계획 문서 검토 및 갱신 | 연간 또는 규제 중대 변경 시 | Regulatory Lead |
| 현장 안전성 신호 검토 | 월간 | Quality Lead |

---

## 7. 현장 배포 현황 추적

현장 배포 현황은 별도 시스템(`docs/operations/field-deployment-registry.md`)에 관리:

- 설치된 버전별 현장 수
- 운영 중인 기기 모델
- 소프트웨어 업데이트 이력

---

## 8. PCCP (Pre-determined Change Control Plan) 연계

AI 모듈(SPEC-XPE-P3-AI) 배포 시 FDA PCCP 요건에 따라 본 PMS 계획에 추가:

- 성능 모니터링 기준선 (현장 데이터 기반)
- 드리프트 감지 임계값 (SPEC-XPE-OPS REQ-OPS-022)
- 재훈련 트리거 조건 및 승인 절차

현재 AI 모듈은 미착수 상태 (SPEC-XPE-P3-AI 미작성). Phase 3 시 본 섹션 확장.

---

## 9. 변경 이력

| 버전 | 날짜 | 내용 |
|------|------|------|
| 0.1.0 | 2026-04-22 | 초기 작성 — EU MDR Article 83 필수 항목(제품범위·데이터소스·지표·보고체계·책임매트릭스) 기반 초안 |

---

*EU MDR Article 83 + Annex III · FDA AI/ML PMS 가이던스 (2024) · SPEC-XPE-OPS REQ-OPS-001~006*
