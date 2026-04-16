# SHA-DISPLAY-001: 소프트웨어 위험 분석

**문서 ID**: SHA-DISPLAY-001  
**IEC 62304 절**: 5.3.5 위험 통제  
**ISO 14971 연계**: Risk Management  
**안전 분류**: Class B  
**모듈**: `xpe_display.dll`  
**버전**: 1.0  
**날짜**: 2026-04-14  
**작성자**: XPE 안전팀  
**승인**: __________________ 날짜: __________

---

## 1. 목적

`xpe_display.dll`의 소프트웨어 위험을 식별하고, 각 위험에 대한 통제 방법을 정의합니다. 이 분석은 IEC 62304 §5.3.5 위험 통제 및 ISO 14971 위험 관리를 따릅니다.

---

## 2. 위험 평가 방법

### 2.1 위험도 계산

```
위험도 = 심각성 × 발생 확률

심각성 레벨:
  Critical (5): 사망 또는 심각한 부상 가능
  Major (4): 진단 오류, 심각한 이미지 손상
  Moderate (3): 임상적 영향, 재촬영 필요
  Minor (2): 경미한 불편, 해결 가능
  Trivial (1): 무시할 수 있는 영향

발생 확률:
  Very High (5): > 50% (거의 확실)
  High (4): 20~50% (가능성 높음)
  Medium (3): 5~20% (중간)
  Low (2): 1~5% (낮음)
  Very Low (1): < 1% (매우 낮음)

위험도:
  ≥ 12: Unacceptable Risk (통제 필수)
  8~11: High Risk (강력한 통제)
  5~7: Moderate Risk (일반 통제)
  < 5: Acceptable Risk (모니터링)
```

### 2.2 통제 전략

| 통제 유형 | 설명 | 예시 |
|----------|------|------|
| **회피** | 위험 활동 제거 | 불필요한 기능 삭제 |
| **완화** | 설계 변경으로 위험 감소 | 입력 검증, 범위 제한 |
| **격리** | 실패 영역 격리 | Error handling, fallback |
| **감지** | 위험 상황 감지 및 알림 | 플래그, 경고 메시지 |
| **사용자 교육** | 정관 절차 수립 | 임상 가이드, 매뉴얼 |

---

## 3. 위험 식별 및 분석

### 위험 #1: GSDF 비준수 — 광도 매핑 오류

#### 3.1.1 위험 설명

**위험**: GSDF 역함수 수식이 부정확하여 광도 매핑이 DICOM PS3.14 표준과 다릅니다.

**근인**: 
- Barten 1999 공식의 잘못된 구현
- 계수 a, c, e, g, m 오류
- 수치 역계산(Newton-Raphson) 수렴 실패

**영향**:
- 모니터 광도가 의료 표준을 벗어남
- 진단 신뢰도 감소
- 환자 선량 최적화 불가

#### 3.1.2 위험도 평가

| 항목 | 점수 | 근거 |
|------|------|------|
| 심각성 | 5 (Critical) | 진단 오류 가능 |
| 발생 확률 | 2 (Low) | 구현 검증 테스트 수행 |
| **위험도** | **10 (High Risk)** | 5 × 2 |

#### 3.1.3 통제

**완화**:
1. Barten 1999 논문 참고 공식으로 구현
2. Golden Reference display와 비교 검증 (±1% 정확도)
3. 알려진 luminance 값(50, 100, 200, 500 cd/m²)에 대한 회귀 테스트

**감지**:
1. `xpe_gsdf_check_display_capability()` 함수로 display 검증
2. 편차 > 10% 시 경고 알림 발행
3. 메타데이터에 GSDF 적용 플래그 기록

**증거**:
- TDS-DISPLAY-001 (회귀 테스트, Golden Reference)
- 논문 검증 보고서

#### 3.1.4 검증

```
테스트 케이스:
  · j=0 → L=0.05 cd/m²
  · j=256 → L≈25 cd/m²
  · j=512 → L≈100 cd/m²
  · j=1023 → L=4000 cd/m²
  
합격 기준: 각 테스트 ±1% 정확도
```

---

### 위험 #2: Window/Level 클리핑 — 진단 정보 손실

#### 3.2.1 위험 설명

**위험**: Window/Level 매핑 시 클리핑으로 진단 정보(tissue contrast)가 손실됩니다.

**근인**:
- 부적절한 Window Center/Width 선택
- 자동 window 계산 오류
- 사용자 실수 (WW=0, negative)

**영향**:
- 미묘한 병변(결절, 낭종) 미감지
- 재촬영 필요 → 환자 선량 증가
- 진단 지연

#### 3.2.2 위험도 평가

| 항목 | 점수 | 근거 |
|------|------|------|
| 심각성 | 4 (Major) | 병변 미감지, 재촬영 필요 |
| 발생 확률 | 3 (Medium) | 사용자 실수 가능 |
| **위험도** | **12 (Unacceptable)** | 4 × 3 |

#### 3.2.3 통제

**회피 + 완화**:
1. 기본 Window 프리셋 제공 (7가지 신체 부위)
2. WW > 0 검증 (범위 검사)
3. 자동 window 계산 (EI-derived 통계)

**격리**:
1. 실패한 window는 기본값으로 fallback
2. 임상의가 수정 가능하도록 UI 제공

**감지**:
1. Clipping 발생 감지 → 플래그 설정 (XPE_FLAG_CLIPPED)
2. 메타데이터에 적용된 WC/WW 기록
3. 경고: "이미지가 부분 클리핑됨. 수동 window 조정 권장"

**증거**:
- 기본 preset 임상 검증 (SAD-DISPLAY-001 §4)
- 자동 window 알고리즘 검증 (TDS-DISPLAY-001)

#### 3.2.4 검증

```
테스트 케이스:
  · WW=0 → XPE_ERR_INVALID_PARAM
  · WW < 0 → XPE_ERR_INVALID_PARAM
  · WC=5000 (범위 초과) → 경고
  · 자동 window (histogram) → ±20% 오차 이내
```

---

### 위험 #3: Format Boundary 정확도 손실 — float32 → uint16 변환

#### 3.3.1 위험 설명

**위험**: float32 (4095 레벨)를 uint16 (65535 레벨)로 변환할 때 quantization 오차 발생.

**근인**:
- 부동소수점 반올림 오류
- 정밀도 손실 (precision floor)

**영향**:
- 임상적 영향: 미미 (±1 LSB/65535 = ±0.0015% 오차)
- 그러나 반복 연산(재계산) 시 누적 가능

#### 3.3.2 위험도 평가

| 항목 | 점수 | 근거 |
|------|------|------|
| 심각성 | 2 (Minor) | 의료 display 표준 범위 내 |
| 발생 확률 | 5 (Very High) | 매번 발생, 하지만 인지 불가 |
| **위험도** | **10 (High Risk)** | 2 × 5 |

#### 3.3.3 통제

**완화**:
1. 부동소수점 round-to-nearest 사용 (표준 IEEE 754)
2. Deterministic conversion: `uint16 = (uint16_t)round(p_value / 4095.0 * 65535.0)`

**감지**:
1. Unit test: 1000개 임의값 입력 → 역계산 검증
2. 에러 ≤ 1 LSB 확인

**사용자 교육**:
1. 기술 가이드: "Format conversion은 의료 display 표준 범위 내 정확도 보증"

**증거**:
- Format conversion test (TDS-DISPLAY-001 §5.3)

#### 3.3.4 검증

```
테스트:
  · p_value=0 → uint16=0
  · p_value=2047.5 → uint16=32767 (또는 32768, ±1 LSB)
  · p_value=4095 → uint16=65535
  · 역변환: uint16 → p_value 검증 (오차 ≤ 1/65535)
```

---

### 위험 #4: 잘못된 Preset 선택 — 신체 부위 오분류

#### 3.4.1 위험 설명

**위험**: 신체 부위 자동 선택 오류로 부적절한 Window/Level 적용.

**근인**:
- DICOM (0018,0015) BodyPartExamined 태그 오류/누락
- Metadata 전달 오류
- 자동 선택 로직 오류

**영향**:
- 잘못된 window로 인한 병변 미감지
- 예: Chest → Extremity window (너무 좁은 WW)

#### 3.4.2 위험도 평가

| 항목 | 점수 | 근거 |
|------|------|------|
| 심각성 | 4 (Major) | 진단 오류 가능 |
| 발생 확률 | 2 (Low) | DICOM 표준 태그 사용 |
| **위험도** | **8 (High Risk)** | 4 × 2 |

#### 3.4.3 통제

**완화**:
1. Fallback: body_part 미제공 시 기본값 "chest" 사용
2. 유효성 검사: body_part ∈ {chest, extremity, spine, abdomen, pediatric}
3. 무효값 → XPE_ERR_INVALID_PARAM

**격리**:
1. 자동 선택 실패 → 사용자가 수동 선택
2. 임상의 검토 프로세스 (QA)

**감지**:
1. 선택된 preset ID를 메타데이터에 기록
2. 감사 로그: "Applied preset: chest_pa for body_part=chest"

**사용자 교육**:
1. 매뉴얼: "자동 선택은 신뢰도 높지만, 최종 검증은 임상의가 담당"

**증례**:
- Integration test: 5가지 body_part → 자동 선택 검증 (TDS-DISPLAY-001 §6.2)

#### 3.4.4 검증

```
테스트 케이스:
  · body_part="chest" → chest_pa selected
  · body_part="CHEST" (대문자) → 거부 또는 자동 소문자 변환
  · body_part="invalid" → XPE_ERR_INVALID_PARAM
  · body_part=NULL → fallback "chest" 또는 에러
```

---

### 위험 #5: DICOM 태그 파싱 오류 — Slope/Intercept 오독

#### 3.5.1 위험 설명

**위험**: DICOM (0028,1053) Rescale Slope 또는 (0028,1052) Intercept 파싱 오류.

**근인**:
- dcmtk 라이브러리 오류
- 태그 데이터 형식 오류 (String vs Float)
- 태그 누락

**영향**:
- 잘못된 선형 변환 → 진단 오류
- 예: slope=1.5 but read 0.15 → 진단값 10배 오차

#### 3.5.2 위험도 평가

| 항목 | 점수 | 근거 |
|------|------|------|
| 심각성 | 5 (Critical) | 정량 진단 오류 |
| 발생 확률 | 1 (Very Low) | dcmtk 라이브러리 검증됨 |
| **위험도** | **5 (Moderate)** | 5 × 1 |

#### 3.5.3 통제

**완화**:
1. DICOM 파일 파싱 검증 (dcmtk 사용)
2. Slope/Intercept 범위 검사: slope > 0, |intercept| ≤ 1000
3. Fallback: 태그 누락 → slope=1.0, intercept=0.0 (identity transform)

**감지**:
1. Slope=0 → XPE_ERR_INVALID_PARAM
2. 의심 범위 (slope < 0.5 또는 > 2.0) → 경고 로그
3. 메타데이터: applied_slope, applied_intercept 기록

**사용자 교육**:
1. DICOM 검증 도구 제공 (별도 유틸리티)
2. 임상의 검수 프로세스

**증거**:
- DICOM 파일 샘플 테스트 (TDS-DISPLAY-001 §5.1)
- XPE-SOUP-001 (dcmtk SOUP 검증)

#### 3.5.4 검증

```
테스트 케이스:
  · slope=1.0, intercept=-1000 (CT) → 정확 파싱
  · slope=0.5, intercept=0 → 정확 파싱
  · slope=2.0, intercept=500 → 경고, 사용자 확인
  · slope=0 (invalid) → XPE_ERR_INVALID_PARAM
  · 태그 누락 → fallback (1.0, 0.0)
```

---

### 위험 #6: LUT Preset 손상 — JSON 저장소 오류

#### 3.6.1 위험 설명

**위험**: JSON 파일 손상 또는 권한 오류로 preset을 읽을 수 없음.

**근인**:
- 디스크 쓰기 실패
- 파일 시스템 오류
- 권한 부족 (read-only filesystem)

**영향**:
- Preset 로드 실패 → 기본값 사용 (fallback)
- 사용자 정의 preset 손실

#### 3.6.2 위험도 평가

| 항목 | 점수 | 근거 |
|------|------|------|
| 심각성 | 2 (Minor) | Fallback 있음 |
| 발생 확률 | 1 (Very Low) | 파일 시스템 표준 |
| **위험도** | **2 (Acceptable)** | 2 × 1 |

#### 3.6.3 통제

**격리**:
1. Preset 읽기 실패 → 기본 Factory Preset 사용
2. Preset 쓰기 실패 → 경고 로그, 현재 세션에서만 사용 (메모리)

**감지**:
1. 파일 I/O 오류 → XPE_ERR_FILE_IO
2. 로그: "Failed to load preset 'user/my_preset.json'. Using 'factory/chest_pa.json'"

**사용자 교육**:
1. 디렉토리 권한: `~/.xpe/luts/user/` write permission 필요
2. Preset 백업 권장

**증거**:
- File I/O test (TDS-DISPLAY-001 §6.3)

#### 3.6.4 검증

```
테스트 케이스:
  · 파일 누락 → Factory fallback
  · 파일 손상 (invalid JSON) → 에러 + fallback
  · 권한 없음 (read-only) → 경고 + fallback
  · 메모리 preset (임시) → 저장 불가, 메모리만 사용
```

---

### 위험 #7: NaN/Inf 입력 — 부동소수점 이상값

#### 3.7.1 위험 설명

**위험**: 입력 이미지에 NaN 또는 Infinity 값이 포함되어 계산 오류 발생.

**근인**:
- Upstream 모듈 오류 (xpe_enhance_advanced.dll)
- 0으로 나누기 (0/0 = NaN)
- 오버플로우 (1e308 * 2 = Inf)

**영향**:
- 출력 이미지 손상
- float → uint16 변환 undefined behavior

#### 3.7.2 위험도 평가

| 항목 | 점수 | 근거 |
|------|------|------|
| 심각성 | 3 (Moderate) | 이미지 부분 손상 |
| 발생 확률 | 2 (Low) | Upstream 검증됨 |
| **위험도** | **6 (Moderate)** | 3 × 2 |

#### 3.7.3 통제

**완화**:
1. 모든 입력 픽셀 검사: `if (isnan(x) || isinf(x))`
2. NaN/Inf → 0으로 대체 또는 에러 반환
3. 범위 검사: `x ∈ [0.0, 4095.0]`

**감지**:
1. NaN/Inf 감지 → 플래그 설정 (XPE_FLAG_INVALID_FLOAT)
2. 경고 로그: "NaN detected in 5 pixels. Replaced with 0."

**증거**:
- Boundary test (TDS-DISPLAY-001 §5.4)

#### 3.7.4 검증

```
테스트 케이스:
  · input[100] = NaN → 플래그 설정, output[100] = 0
  · input[200] = Inf → 플래그 설정, output[200] = 65535
  · input[300] = -1.5 → clamp to 0
  · input[400] = 5000 → clamp to 4095
```

---

## 4. 위험 요약 테이블

| # | 위험 | 심각성 | 확률 | 위험도 | 통제 | 검증 |
|----|------|-----:|-----:|-----:|------|------|
| 1 | GSDF 비준수 | 5 | 2 | 10 | 완화 + 감지 | TDS §3.1 |
| 2 | Window 클리핑 | 4 | 3 | 12 | 회피 + 완화 + 격리 | TDS §3.2 |
| 3 | Format 정확도 | 2 | 5 | 10 | 완화 + 감지 | TDS §5.3 |
| 4 | Preset 오선택 | 4 | 2 | 8 | 완화 + 격리 | TDS §6.2 |
| 5 | 태그 파싱 오류 | 5 | 1 | 5 | 완화 + 감지 | TDS §5.1 |
| 6 | JSON 손상 | 2 | 1 | 2 | 격리 + 감지 | TDS §6.3 |
| 7 | NaN/Inf 입력 | 3 | 2 | 6 | 완화 + 감지 | TDS §5.4 |

---

## 5. 통합 위험 평가

### 5.1 전체 위험도

```
총 위험도 = GSDF(10) + Window(12) + Format(10) + Preset(8) + Tag(5) + JSON(2) + NaN(6)
         = 53 / 7 = 7.6 (평균 Moderate)

최대 단일 위험: Window 클리핑 (12, Unacceptable)
  → 강력한 통제 필수: Preset, Validation, Fallback
```

### 5.2 통제 효과

| 통제 | 위험도 감소 | 최종 위험도 |
|------|:-----------:|:-----------:|
| Preset 자동 선택 | Window: 12→8 | 8 (High) |
| WW > 0 검증 | Window: 8→4 | 4 (Moderate) |
| Clipping 감지/알림 | Window: 4→2 | 2 (Acceptable) |

**결론**: 모든 위험이 Acceptable 또는 High Risk 수준으로 감소. Class B 안전 등급 충족.

---

## 6. 연속 모니터링

| 항목 | 방법 | 빈도 |
|------|------|------|
| GSDF 정확도 | Golden Reference display 비교 | 분기마다 |
| Window 임상 효과 | 임상의 피드백 수집 | 월간 |
| Preset 사용율 | 감사 로그 분석 | 분기마다 |
| 오류율 | 에러 로그 집계 | 주간 |

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-14 | XPE Safety Team | Initial release |

---

*문서 끝 — SHA-DISPLAY-001 v1.0*
