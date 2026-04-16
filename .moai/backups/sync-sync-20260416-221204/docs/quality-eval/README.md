# FPD 품질 평가 알고리즘 문서 패키지

**모듈**: Python 기반 FPD 측정 알고리즘 라이브러리  
**소유자**: FPD 품질 평가 팀  
**안전 등급**: 해당 없음 (생산 라인 QA 도구, 진단 소프트웨어 아님)  
**문서 버전**: 2.0  
**날짜**: 2026-04-15  
**최종 갱신**: 2026-04-15 (5차 교차검증: SDNR/CNR, 불확도 예산, IEC 62220-1-3 동적 영상 추가)  
**참조 표준**: IEC 62220-1-1:2015, IEC 62220-1-2:2007, IEC 62220-1-3:2008, AAPM TG-116, JCGM 100:2008

---

## 문서 목록

| 파일 | 설명 | 우선순위 |
|------|------|---------|
| [03_측정_알고리즘_명세서.pplx.md](03_측정_알고리즘_명세서.pplx.md) | FPD 품질 파라미터 측정 알고리즘 명세서 (핵심 문서) | 1 |
| [01_Noise_평가_방법론_종합보고서.pplx.md](01_Noise_평가_방법론_종합보고서.pplx.md) | FPD Noise 평가 방법론 종합 보고서 | 2 |
| [02_양산라인_계측방법론_가이드.pplx.md](02_양산라인_계측방법론_가이드.pplx.md) | 생산 라인 QA 계측 방법론 가이드 | 3 |
| [05_체계적_관리방안_문서.pplx.md](05_체계적_관리방안_문서.pplx.md) | 체계적 품질 관리 방안 | 4 |

---

## 알고리즘 개요

### 핵심 측정 알고리즘 (03_측정_알고리즘_명세서.pplx.md)

| # | 알고리즘 | 표준 | 용도 |
|---|---------|------|------|
| 1 | NPS/NNPS | IEC 62220-1-1 §6.4 | 공간 주파수별 노이즈 에너지 분포 |
| 2 | MTF | IEC 62220-1-1 §6.3 | 공간 주파수 응답 (엣지/ESF 방법) |
| 3 | DQE | IEC 62220-1-1 §6.5 | 검출기 SNR 전달 효율 |
| 4 | NEQ | ICRU Report 54 | 유효 노이즈 등가 광자 수 |
| 5 | Dark Noise (DSNU/PRNU) | EMVA 1288 | 암전류 / 광 응답 비균일성 |
| 6 | 결함 화소 검출 | IEC 62220-1-1 §6.6 | Hot/Cold/Stuck/Cluster 픽셀 |
| 7 | STP | IEC 62220-1-1 §6.2 | 신호 전달 선형성 및 응답 |
| 8 | Lag/Ghosting | IEC 62220-1-2 §6.5 | 잔상 및 고스팅 정량화 |
| 9 | **SDNR/CNR** *(신규 v2.0)* | AAPM TG-116, Rose 모델 | 저대비 검출능, Rose 기준 (SDNR≥5) |
| 10 | **불확도 예산** *(신규 v2.0)* | JCGM 100:2008 (GUM) | DQE 확장 불확도 ≈ ±12% |
| 11 | 종합 평가 파이프라인 | 전체 통합 | 원클릭 전체 파라미터 측정 |
| 12 | **동적 영상 NPS/Lag-DQE** *(신규 v2.0)* | IEC 62220-1-3 | 형광투시용 시공간 NPS, Lag 패널티 |

---

## 구현 환경

```
Python        3.10 이상
NumPy         1.24 이상 (배열 연산, FFT)
SciPy         1.10 이상 (신호 처리, 통계, 최적화)
scikit-image  0.21 이상 (이미지 처리 유틸리티)
matplotlib    3.7 이상 (시각화, 선택)
pydicom       2.4 이상 (DICOM 파일 입출력, 선택)
```

---

## v2.0 변경 내역 (2026-04-15)

### 주요 추가 알고리즘 (5차 교차검증 결과)

1. **섹션 10: SDNR/CNR 저대비 검출능 알고리즘**
   - Rose 모델 기반 임상 검출 확률 계산
   - `compute_sdnr()` / `compute_sdnr_vs_dose()` Python 구현 제공
   - 다중 선량 레벨 SDNR 곡선 (Rose slope 분석)

2. **섹션 11: GUM 불확도 예산 분석**
   - JCGM 100:2008 방법론 준수
   - `compute_uncertainty_budget()` / `dqe_uncertainty_budget()` 구현
   - DQE(1 lp/mm) 확장 불확도: k=2, U≈12% (주요 성분: MTF 2%, NNPS 5%)

3. **섹션 13: IEC 62220-1-3 동적 영상 확장**
   - `compute_spatiotemporal_nps()`: 3D 시공간 NPS, Hanning 창 적용
   - `compute_lag_dqe()`: Lag 패널티 인자 계산 (DC vs. Nyquist Lag-DQE)
   - 검증 기준: Lag penalty ≥ 0.70 (최대 30% DQE 저하 허용)

---

## 관련 문서

| 문서 | 경로 | 관계 |
|------|------|------|
| Lag/Ghost 보정 SDD | `docs/ghost-correction/sdd_ghost_correction.md` | Lag 측정 → 보정 알고리즘 |
| FPD 캘리브레이션 SRS | `docs/calibration/SRS-CALIB-001_Software_Requirements_Specification.md` | 측정 조건 정의 |
| Panel Defect SRS | `docs/panel-defect/SRS-DEFECT-001_Software_Requirements_Specification.md` | 결함 화소 검출 연계 |

---

*FPD 품질 평가 문서 패키지 README v2.0 끝*
