# 다음 세션 준비: Preprocessing Calibration 알고리즘 보강 구현

**Version**: 1.0.0 | **Created**: 2026-04-19 | **Status**: Ready for Next Session

---

## 1. 현재 상태 정리

### 1.1 SPEC-XPE-P1A 상태
- **Version**: 1.3.0
- **Status**: M2 Complete (SUP-01 + M2 algorithms implemented)
- **완료된 작업**:
  - SUP-01 (Calibration Management): 89/90 tests pass
  - M2 algorithms (REQ-P1A-010~013): Offset, Gain, Defect correction
  - 657 lines added across 7 files
  - Integration tests 추가됨

### 1.2 누락된 업데이트
- **acceptance.md**: Version 1.2.0, Status "M2 pending" → **업데이트 필요**
- **progress.md**: Phase 1 SUP-01 Complete → **M2 완료 상태로 업데이트 필요**

---

## 2. Calibration 기술 정의 (확인 완료)

### 2.1 Calibration (SUP-01)
**Detector 고유의 보정 데이터(Offset, Gain, Defect Map)를 관리하는 기능**

- Research ID: SUP-01
- SWU 매핑: SWU-1.5 (Calibration Manager), SWU-5.6 (Calibration Parameter Management)
- 분류: Support 기술 (필수)
- 목적: Detector 특성별 보정 데이터의 생성, 저장, 로딩, 검증

### 2.2 Preprocessing Algorithms (PRE-01~09)
**Raw detector 데이터를 보정된 이미지로 변환하는 9가지 전처리 기술**

- Research ID: PRE-01~PRE-09
- SWU 매핑: SWU-1.1~1.9
- 분류: Pre-processing 기술 (필수)
- 목적: Raw detector 이미지의 각종 아티팩트 제거 및 신호 정규화

### 2.3 관계
Calibration에서 생성된 보정 데이터를 Preprocessing Algorithms가 사용

---

## 3. 다음 세션 작업 계획

### 3.1 업데이트된 계획 (Updated Plan)

#### Phase 1: 문서 업데이트
1. **acceptance.md 업데이트** (v1.2.0 → v1.3.0)
   - M2 Complete 상태 반영
   - 완료된 M2 acceptance criteria 마킹
   - SIMD parity acceptance criteria 추가

2. **progress.md 업데이트**
   - M2 algorithms 완료 상태 반영
   - 테스트 커버리지 업데이트
   - 다음 Phase (M3) 준비 상태 명시

#### Phase 2: 사양서 리뷰 (SPEC Review)

**검토 대상 문서**:
1. **SPEC-XPE-P1A/spec.md** (v1.3.0)
   - M2 algorithms 구현 완료 확인
   - REQ-P1A-010~013 충족 여부 검토
   - 다음 Phase 요구사항 확인

2. **acceptance.md** (v1.2.0 → v1.3.0)
   - M2 관련 acceptance criteria 통과 확인
   - 누락된 테스트 시나리오 확인
   - IEC 62304 compliance 검토

3. **simd-parity-harness.md**
   - SIMD parity 테스트 프로토콜 검토
   - Scalar ↔ AVX2 동등성 검증 기준 확인

#### Phase 3: 보강 구현 준비 (Reinforcement Implementation Planning)

### 3.2 보강 구현 영역 (Reinforcement Areas)

#### 영역 1: Calibration Data Management (SUP-01) 보강

**현재 상태**: 기본 기능 완료 (89/90 tests pass)

**보강 항목**:
1. **다중 오프셋 생성 알고리즘**
   - 현재: 단순 평균 (mean)
   - 보강: Median, Robust mean (outlier 제거)
   - 이유: 노이즈에 강한 보정 데이터 생성

2. **XCal v1 포맷 최적화**
   - 압축 지원 (Defect map run-length encoding)
   - 메타데이터 확장 (detector 모델, 일련번호)
   - 이유: 저장 공간 절약, 추적성 향상

3. **자동 보정 로드 (Temperature-based)**
   - 온도별 calibration map 자동 선택
   - Interpolation between temperature points
   - 이유: 정확도 향상, 사용자 편의성

#### 영역 2: Calibration-Preprocessing 통합 보강

**현재 상태**: 개별 함수 호출 방식

**보강 항목**:
1. **통합 파이프라인 최적화**
   - `xpe_preprocess_pipeline()` 성능 향상
   - 메모리 할당 최소화
   - 병렬 처리 지원
   - 이유: 실시간 처리 성능 확보

2. **Calibration 데이터 캐싱**
   - 자주 사용하는 map 메모리 캐시
   - LRU 캐시 전략
   - 이유: 반복 로드 오버헤드 제거

3. **배치 처리 지원**
   - 다중 프레임 동일 calibration 적용
   - SIMD 병렬 처리 확장
   - 이유: 고프레임 처리 성능

#### 영역 3: 품질 및 검증 보강

**현재 상태**: 기본 단위/통합 테스트 완료

**보강 항목**:
1. **SIMD Parity 테스트 확장**
   - 현재: Scalar ↔ AVX2
   - 확장: AVX-512, NEON (ARM64)
   - 이유: 다중 플랫폼 지원

2. **성능 벤치마크 확장**
   - 현재: BP-01~05
   - 확장: Clinical 데이터셋 기반
   - 이유: 실제 환경 성능 검증

3. **메모리 누수 탐지 강화**
   - Valgrind/AddressSanitizer 통합
   - 장기 실행 테스트 (1000 프레임 → 10000 프레임)
   - 이유: 프로덕션 안정성 확보

---

## 4. 구현 우선순위 (Implementation Priority)

### Priority 1 (높음): 문서 업데이트 및 검증
- acceptance.md v1.3.0 업데이트
- progress.md 업데이트
- M2 algorithms 검증 완료 확인

### Priority 2 (중간): Calibration-Preprocessing 통합
- 통합 파이프라인 최적화
- Calibration 데이터 캐싱
- 배치 처리 지원

### Priority 3 (낮음): 고급 기능
- 다중 오프셋 생성 알고리즘
- XCal v1 포맷 최적화
- SIMD Parity 테스트 확장

---

## 5. 다음 세션 시작 절차 (Next Session Checklist)

1. **문서 업데이트 완료 확인**
   - [ ] acceptance.md v1.3.0 반영
   - [ ] progress.md M2 Complete 상태 반영
   - [ ] SPEC-XPE-P1A/spec.md v1.3.0 최종 확인

2. **코드베이스 상태 확인**
   - [ ] M2 algorithms 구현 코드 리뷰
   - [ ] 테스트 커버리지 확인 (85%+ 목표)
   - [ ] SIMD parity 테스트 통과 확인

3. **다음 Phase 준비**
   - [ ] M3 (SIMD optimization) 요구사항 확인
   - [ ] Performance benchmark 계획 수립
   - [ ] IEC 62304 Class B 준수 검토

4. **보강 구현 계획 수립**
   - [ ] Priority 1~3 항목 명세화
   - [ ] 작업 예상 시간 추정
   - [ ] 필요한 리소스 식별

---

## 6. 레퍼런스 (References)

### 6.1 문서
- `.moai/specs/SPEC-XPE-P1A/spec.md` (v1.3.0)
- `.moai/specs/SPEC-XPE-P1A/acceptance.md` (v1.2.0 → v1.3.0)
- `.moai/specs/SPEC-XPE-P1A/progress.md`
- `.moai/project/codemaps/calibration-vs-preprocessing.md`

### 6.2 코드
- `modules/preprocess/src/` (M2 algorithms)
- `modules/preprocess/tests/` (Test suites)
- `modules/preprocess/include/xpe/preprocess/xpe_preprocess_api.h`

### 6.3 SPEC
- **SPEC-XPE-P1A**: Pre-processing Module (Gain/Offset/Defect Correction)
- **SUP-01**: Calibration Parameter Management
- **REQ-P1A-010~013**: M2 algorithms requirements

---

## 7. 부록: 세션 기억 요약

### 7.1 학습 내용 (Lessons Learned)
1. **Calibration 정의**: SUP-01은 보정 데이터 관리, PRE-01~09는 전처리 알고리즘
2. **기술 분류 명확화**: Calibration vs Preprocessing 관계 문서화 완료
3. **구현 상태 파악**: M2 Complete, 다음은 M3 및 보강 구현

### 7.2 다음 세션 초점
1. 문서 업데이트 (acceptance.md, progress.md)
2. 사양서 리뷰 (M2 완료 검증)
3. 보강 구현 계획 수립 (Priority 1~3)

---

**Status**: ✅ 준비 완료 - 다음 세션에서 바로 시작 가능
