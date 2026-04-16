# DICOM I/O 모듈 — 소프트웨어 위험 분석

**문서 ID**: SHA-DICOM-001  
**버전**: 1.0.0  
**날짜**: 2026-04-14  
**IEC 62304 절**: 5.2.2 — Software Hazard Analysis  
**ISO 14971 준수**: Yes  
**안전 등급**: Class B  

---

## 목차

1. [위험 분석 개요](#1-위험-분석-개요)
2. [식별된 위험 (8개)](#2-식별된-위험-8개)
3. [위험 평가](#3-위험-평가)
4. [위험 통제](#4-위험-통제)
5. [잔존 위험 평가](#5-잔존-위험-평가)

---

## 1. 위험 분석 개요

### 1.1 방법론

- **표준**: ISO 14971:2019 (Medical Device Risk Management)
- **위험 정의**: Hazard + Probability + Consequence
- **심각도**: 1 (최소) ~ 5 (치명적)
- **발생확률**: 1 (극히 드물게) ~ 5 (매우 자주)
- **위험도 = 심각도 × 발생확률**
- **수용 기준**: 위험도 ≤ 5 (합리적 저감 후)

### 1.2 분석 범위

- xpe_dicom.dll 모듈
- DICOM 파일 I/O, 네트워크 통신
- 픽셀 데이터 및 메타데이터 무결성
- 환자 안전에 직접 영향

---

## 2. 식별된 위험 (8개)

### HAZ-DCM-001: 손실 압축이 적용된 진단 이미지

**설명**: 손실 JPEG 2000 (Irreversible) 압축이 적용되어 진단 정보 손실

**실패 모드**:
- DicomWriter에서 user parameter로 JPEG 2000 Irreversible TS 선택
- 압축 검증 로직 실패
- 손상된 이미지가 PACS에 저장됨

**결과 (Consequence)**:
- 진단 정보 손실
- 임상 의사가 손상된 이미지 기반 진단
- 환자 치료 오류
- **심각도**: 5 (치명적 — 환자 해)

**발생 확률 (Probability)**: 2 (설계에 의해 방지, user error 가능)
- API 문서에 명시적 거부 명시
- Hard constraint로 구현
- User가 의도적으로 거부 무시 시도 가능성: 낮음

**초기 위험도**: 5 × 2 = **10 (High)**

**통제 방안** (Section 4):
1. Transfer Syntax 검증 (다중 층)
2. 파라미터 검증 (호출 함수)
3. 자동 거부 (hard constraint, bypass 불가)
4. 명확한 에러 메시지
5. 감시 로깅

**잔존 위험도**: 1 × 1 = **1 (Acceptable)**

---

### HAZ-DCM-002: 파일 손상으로 인한 부분 데이터 반환

**설명**: DICOM 파일이 손상되었으나 부분적으로 판독되어 불완전한 이미지 반환

**실패 모드**:
- xpe_dicom_read() 호출
- DICOM parser가 손상된 헤더 건너뜀
- 일부 메타데이터 누락, 픽셀 데이터 불완전
- 호출자에게 "성공" 반환

**결과**:
- 불완전한 영상 처리
- 메타데이터 누락으로 환자 추적 오류 가능
- 품질 저하된 진단 이미지
- **심각도**: 4 (주요 — 진단 신뢰성 영향)

**발생 확률**: 2 (네트워크 전송 오류, 디스크 섹터 손상)

**초기 위험도**: 4 × 2 = **8 (High)**

**통제 방안**:
1. Preamble 검증 (DICM 시그니처)
2. VR/길이 필드 일관성 확인
3. 손상 감지 시 즉시 에러 반환
4. Partial data 반환 금지 (NULL 포인터)
5. 손상 파일 alert 로깅

**잔존 위험도**: 1 × 2 = **2 (Acceptable)**

---

### HAZ-DCM-003: 환자 ID 불일치 (메타데이터 누락)

**설명**: DICOM 파일의 Patient ID가 처리 메타데이터와 불일치하여 잘못된 환자 이미지 처리

**실패 모드**:
- DICOM 파일에서 Patient ID "12345" 읽음
- 처리 메타데이터에 Patient ID "12346" 저장
- 시스템이 불일치 미감지
- 이미지가 잘못된 환자에게 연결됨

**결과**:
- 환자 혼동 (critical safety issue)
- 잘못된 진단, 치료
- **심각도**: 5 (치명적 — 환자 해)

**발생 확률**: 1 (네트워크 전송 오류, 매우 드물게)

**초기 위험도**: 5 × 1 = **5 (High)**

**통제 방안**:
1. DICOM 파일 읽을 때 Patient ID 추출 및 저장
2. XpeImage 구조체와 비교
3. 불일치 → XPE_ERR_PATIENT_ID_MISMATCH 반환
4. Alert 로깅 (상세 정보 포함)
5. 쓰기 전 재검증

**잔존 위험도**: 1 × 1 = **1 (Acceptable)**

---

### HAZ-DCM-004: C-STORE 네트워크 장애로 인한 데이터 손실

**설명**: PACS로의 C-STORE 전송 중 네트워크 끊김으로 이미지가 PACS에 도착하지 않음

**실패 모드**:
- xpe_dicom_cstore() 호출
- 네트워크 연결 끊김 (WAN 불안정)
- C-STORE 응답 수신 전 연결 종료
- 호출자는 "전송 성공" 가정
- 이미지가 PACS에 없음 (임상의가 알 수 없음)

**결과**:
- PACS에 이미지 부재
- 진단 지연 또는 누락
- 환자 치료 지연
- **심각도**: 4 (주요 — 진단 지연)

**발생 확률**: 3 (WAN 환경에서 흔함)

**초기 위험도**: 4 × 3 = **12 (High)**

**통제 방안**:
1. Automatic retry with exponential backoff (3회)
2. Timeout 설정 (30초 Association, 120초 DIMSE)
3. 명확한 Status code 반환 (성공/경고/실패)
4. 로컬 파일은 보호 (PACS 전송 실패해도 보존)
5. 실패 alert 로깅
6. User가 retry 또는 manual 전송 선택 가능

**잔존 위험도**: 2 × 2 = **4 (Acceptable)**

---

### HAZ-DCM-005: MWL 쿼리 결과에서 잘못된 환자 자동 선택

**설명**: C-FIND MWL 쿼리 결과 중 잘못된 환자 정보를 자동으로 선택하여 처리

**실패 모드**:
- xpe_dicom_cfind_mwl(patient_id="12345")
- RIS 오류로 인해 다른 환자 정보 반환 (예: patient_id="12346")
- 호출자가 첫 번째 결과를 자동 선택
- 이미지가 잘못된 환자에게 연결됨

**결과**:
- 환자 혼동
- 진단 오류
- **심각도**: 5 (치명적)

**발생 확률**: 1 (RIS 오류, 매우 드물게)

**초기 위험도**: 5 × 1 = **5 (High)**

**통제 방안**:
1. 반환된 Patient ID를 쿼리된 값과 비교
2. 불일치 → Alert 발행, automatic selection 금지
3. User가 명시적으로 결과 선택
4. 선택 전에 Patient Name, DOB 확인 UI
5. 로깅: 쿼리 키, 반환 결과, user selection

**잔존 위험도**: 1 × 1 = **1 (Acceptable)**

---

### HAZ-DCM-006: GSPS 참조 불일치 (원본 이미지 손상)

**설명**: GSPS (Presentation State)가 원본 DX 이미지와 다른 이미지를 참조하여 window/level 설정 오적용

**실패 모드**:
- GSPS 생성: Primary DX image A 참조
- 임상의가 GSPS를 이미지 B에 적용
- GSPS의 window/level이 B의 특성과 맞지 않음
- 진단 이미지 화질 저하

**결과**:
- 진단 이미지 품질 저하
- 미묘한 이상 징후 놓칠 가능성
- **심각도**: 3 (중간 — 진단 신뢰성 저하)

**발생 확률**: 2 (Manual UI error, user interface 불명확)

**초기 위험도**: 3 × 2 = **6 (High)**

**통제 방안**:
1. GSPS에 Referenced Series UID 명시적 저장
2. Presentation State 적용 전 참조 검증
3. 불일치 → Warning 또는 error
4. UI에 Referenced Image 정보 표시
5. 로깅: GSPS 생성 시 참조 이미지 정보

**잔존 위험도**: 1 × 1 = **1 (Acceptable)**

---

### HAZ-DCM-007: Unsupported Transfer Syntax 미감지

**설명**: 지원되지 않는 Transfer Syntax가 DICOM 파일에 있으나 감지되지 않아 잘못된 데이터 해석

**실패 모드**:
- DICOM 파일: Transfer Syntax = MPEG-2 (1.2.840.10008.1.2.4.100)
- xpe_dicom_read()가 미지원 TS 감지 실패
- Fallback으로 Implicit VR LE로 가정하여 파싱
- 픽셀 데이터 완전히 왜곡

**결과**:
- 왜곡된 이미지
- 진단 불가능
- **심각도**: 4 (주요)

**발생 확률**: 1 (외부 시스템이 미지원 TS 전송 — 드문 경우)

**초기 위험도**: 4 × 1 = **4 (Acceptable)**

**통제 방안**:
1. Transfer Syntax UID 명시적 읽기
2. 지원 목록과 비교
3. Unsupported TS → XPE_ERR_DICOM_UNSUPPORTED_TRANSFER_SYNTAX
4. Parser fallback 금지 (error-first 원칙)
5. 로깅: Unsupported TS 감지 시 alert

**잔존 위험도**: 1 × 1 = **1 (Acceptable)**

---

### HAZ-DCM-008: Private Tag 충돌 (XPE block)

**설명**: XPE private block (0019,xx00)이 다른 vendor의 private data와 충돌하여 메타데이터 손상

**실패 모드**:
- DICOM 파일에 다른 vendor의 private block (0019,0010) = "VENDOR_X" 존재
- xpe_dicom_write()가 XPE private block (0019,1001) 덮어쓰기
- 원본 vendor data 손실

**결과**:
- 외부 시스템의 vendor-specific 정보 손실
- 상호운용성 문제
- **심각도**: 2 (경미 — 데이터 손실이지만 진단에 직접 영향 아님)

**발생 확률**: 2 (다중 vendor system integration)

**초기 위험도**: 2 × 2 = **4 (Acceptable)**

**통제 방안**:
1. Private block read 전에 기존 data 확인
2. 다른 vendor의 block 발견 → preserve
3. XPE data를 별도 group에 저장 (예: 0019,2000 from 0010)
4. Collision detection 로깅
5. 문서화: Private block 사용 범위 제한

**잔존 위험도**: 1 × 1 = **1 (Acceptable)**

---

## 3. 위험 평가

### 3.1 위험 매트릭스 (초기)

```
        Probability
         1    2    3    4    5
S  5    5   10   15   20   25   (Catastrophic)
e  4    4    8   12   16   20   (Critical)
v  3    3    6    9   12   15   (Major)
e  2    2    4    6    8   10   (Minor)
r  1    1    2    3    4    5   (Negligible)
i
t
y
```

**초기 위험도 분포:**
- High (≥6): HAZ-001, HAZ-002, HAZ-003, HAZ-004, HAZ-005, HAZ-006
- Acceptable (≤5): HAZ-007, HAZ-008

### 3.2 위험 수용 기준 (Acceptance Criteria)

| 위험도 | 분류 | 조치 |
|--------|------|------|
| ≥ 10 | **Critical** | 반드시 저감, 또는 제품 출시 불가 |
| 6 ~ 9 | **High** | 저감 필수, 잔존 위험 ≤ 5 필요 |
| ≤ 5 | **Acceptable** | 저감 권장, 문서화 |

---

## 4. 위험 통제

### 4.1 HAZ-DCM-001 통제 (Lossy Compression)

**통제 방안:**

| ID | 통제 | 유형 | 구현 | 검증 |
|----|------|------|------|------|
| C-001-A | Transfer Syntax 검증 | 설계 | Hard constraint: JPEG2K Irreversible 자동 거부 | Unit test |
| C-001-B | 파라미터 검증 | 코드 | xpe_dicom_write()에서 호출 시 검사 | Code review |
| C-001-C | 에러 메시지 | 문서 | 명확한 거부 메시지: "Lossy compression not allowed" | User guide |
| C-001-D | 감시 로깅 | 동작 | CRITICAL 수준 로그 | Log review |

**효과성 평가:**
- 설계 수준 통제 (bypass 불가능)
- 효과성: 95% 이상
- 잔존 위험도: 1 × 1 = 1 (Acceptable)

---

### 4.2 HAZ-DCM-002 통제 (파일 손상)

**통제 방안:**

| ID | 통제 | 유형 | 구현 |
|----|------|------|------|
| C-002-A | Preamble 검증 | 설계 | "DICM" signature 확인 |
| C-002-B | VR/길이 필드 검증 | 코드 | DCMTK parser 검증 |
| C-002-C | 손상 감지 → 에러 | 코드 | XPE_ERR_DICOM_CORRUPTED 반환 |
| C-002-D | Partial data 금지 | 설계 | NULL 포인터 반환 (정부분 데이터 아님) |

**효과성 평가:**
- 잔존 위험도: 1 × 2 = 2 (Acceptable)

---

### 4.3 HAZ-DCM-003 통제 (환자 ID 불일치)

**통제 방안:**

| ID | 통제 | 유형 | 구현 |
|----|------|------|------|
| C-003-A | Patient ID 추출 | 코드 | DICOM (0010,0020) 읽기 |
| C-003-B | Patient ID 검증 | 코드 | 메타데이터와 비교 |
| C-003-C | 불일치 검출 → 에러 | 설계 | XPE_ERR_PATIENT_ID_MISMATCH |
| C-003-D | Alert 로깅 | 동작 | 상세 정보 포함 로그 |

**효과성 평가:**
- 잔존 위험도: 1 × 1 = 1 (Acceptable)

---

### 4.4 HAZ-DCM-004 통제 (C-STORE 네트워크 실패)

**통제 방안:**

| ID | 통제 | 유형 | 구현 |
|----|------|------|------|
| C-004-A | Automatic Retry | 코드 | 최대 3회 재시도 |
| C-004-B | Exponential Backoff | 코드 | 2s, 4s, 8s delay |
| C-004-C | Timeout 설정 | 구성 | 30초 Association, 120초 DIMSE |
| C-004-D | Status Code 반환 | API | 명확한 성공/경고/실패 코드 |
| C-004-E | 로컬 파일 보호 | 설계 | PACS 전송 실패해도 로컬 보존 |
| C-004-F | User Interaction | UI | Retry 또는 manual 전송 옵션 |

**효과성 평가:**
- 재시도로 성공률 95% 이상
- 잔존 위험도: 2 × 2 = 4 (Acceptable)

---

### 4.5 HAZ-DCM-005 통제 (MWL 환자 혼동)

**통제 방안:**

| ID | 통제 | 유형 | 구현 |
|----|------|------|------|
| C-005-A | Patient ID 검증 | 코드 | 반환 결과 vs 쿼리 키 비교 |
| C-005-B | 불일치 시 Alert | 설계 | Automatic selection 금지 |
| C-005-C | User 확인 UI | UI | 결과 선택 전 Patient Info 표시 |
| C-005-D | 로깅 | 동작 | 쿼리, 결과, user selection 기록 |

**효과성 평가:**
- 잔존 위험도: 1 × 1 = 1 (Acceptable)

---

### 4.6 HAZ-DCM-006 통제 (GSPS 참조 불일치)

**통제 방안:**

| ID | 통제 | 유형 | 구현 |
|----|------|------|------|
| C-006-A | Referenced UID 저장 | 설계 | GSPS에 원본 Series UID 포함 |
| C-006-B | 적용 전 검증 | 코드 | Referenced Series UID 확인 |
| C-006-C | 불일치 시 경고 | 설계 | Warning 또는 error |
| C-006-D | UI 정보 표시 | UI | Referenced Image 정보 시각화 |

**효과성 평가:**
- 잔존 위험도: 1 × 1 = 1 (Acceptable)

---

### 4.7 HAZ-DCM-007 통제 (Unsupported TS)

**통제 방안:**

| ID | 통제 | 유형 | 구현 |
|----|------|------|------|
| C-007-A | TS UID 명시적 읽기 | 코드 | (0002,0010) 파싱 |
| C-007-B | 지원 목록 검증 | 설계 | Allowlist 기반 검증 |
| C-007-C | Unsupported TS → 에러 | 코드 | XPE_ERR_DICOM_UNSUPPORTED_TRANSFER_SYNTAX |
| C-007-D | Fallback 금지 | 설계 | Assumption 없음, error-first |

**효과성 평가:**
- 잔존 위험도: 1 × 1 = 1 (Acceptable)

---

### 4.8 HAZ-DCM-008 통제 (Private Tag 충돌)

**통제 방안:**

| ID | 통제 | 유형 | 구현 |
|----|------|------|------|
| C-008-A | Collision Detection | 코드 | Private block read 전에 기존 data 확인 |
| C-008-B | Data Preservation | 설계 | 다른 vendor data 보호 |
| C-008-C | Separate Allocation | 설계 | XPE data를 별도 group에 저장 |
| C-008-D | 로깅 | 동작 | Collision 감지 시 로깅 |

**효과성 평가:**
- 잔존 위험도: 1 × 1 = 1 (Acceptable)

---

## 5. 잔존 위험 평가

### 5.1 위험 저감 후 요약

| HAZ ID | 초기 위험도 | 통제 | 잔존 위험도 | 상태 |
|--------|:----------:|------|:----------:|---------|
| HAZ-001 | 10 | 다층 검증 + 하드 거부 | 1 | ✓ Accept |
| HAZ-002 | 8 | Preamble + VR 검증 | 2 | ✓ Accept |
| HAZ-003 | 5 | Patient ID 비교 | 1 | ✓ Accept |
| HAZ-004 | 12 | 재시도 + timeout | 4 | ✓ Accept |
| HAZ-005 | 5 | Patient ID 비교 | 1 | ✓ Accept |
| HAZ-006 | 6 | Referenced UID 저장 | 1 | ✓ Accept |
| HAZ-007 | 4 | TS 검증 + error-first | 1 | ✓ Accept |
| HAZ-008 | 4 | Collision detection | 1 | ✓ Accept |

**모든 위험: 잔존 위험도 ≤ 5 (Acceptable)**

### 5.2 위험 저감 효과성 검증

| 방법 | 검증 계획 |
|------|---------|
| **Unit Testing** | 각 위험당 1개 이상 test case |
| **Integration Testing** | 실제 DICOM 파일, PACS 시뮬레이션 |
| **Stress Testing** | 네트워크 불안정, 파일 손상 시뮬레이션 |
| **Code Review** | 에러 처리, 로깅 검증 |
| **Documentation** | User guide, API documentation |

### 5.3 모니터링 및 추적

**운영 중 모니터링:**
- 에러 로그 분석 (월간)
- 환자 안전 incident report (real-time)
- Complaint 분석 (분기별)
- 통제 효과성 재평가 (연간)

**문제 발견 시 조치:**
- 즉시 root cause analysis
- 통제 강화 또는 추가 도입
- 출시 후 software update (if needed)

---

**문서 끝: SHA-DICOM-001 v1.0.0**
