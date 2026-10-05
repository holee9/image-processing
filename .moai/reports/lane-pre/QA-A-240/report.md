# QA-A-240 — 1단계 체크리스트 B1~B10, 실영상 기준 현재 수준 측정 (고스트 끔)

기준 커밋: fe5d1791 (dev/preprocess) + 이 카드의 측정 하니스. 측정일 2026-10-05, 로컬 (Windows 11, i7-12700, NVMe SSD), 제품 코드 변경 없음.
체크리스트 원문: 메인 저장소 `.moai/project/milestone-1-pre-basic.md`. 기준 문구는 `docs/calibration/SRS-CALIB-001_Software_Requirements_Specification.md`(이하 SRS)와 `docs/project/Preprocessing-E2E-Automated-Evaluation-Protocol.md`(이하 프로토콜)에서 인용했다.

## 1. 결과

**통과 7 / 실패 3 / 미구현·기준 없음 0** (항목 단위). 항목 안의 세부 판정에는 "기준 없음"이 몇 개 있다 (§3).

| # | 항목 | 판정 | 한 줄 요약 | 증거 |
|---|---|---|---|---|
| B1 | offset 맵 적재 + 치수 검증 | **통과** | 실제 맵 적재 40 ms. 치수 불일치는 적용 시 `XPE_ERR_INVALID_INPUT`. SRS 문구와 다른 점은 §3 | `21_B1_*.txt` |
| B2 | gain 맵 적재 + 치수·값 범위 검증 | **실패** | 범위 밖 gain 을 SRS 는 **오류**로 거부하라는데 구현은 **경고 + 결함 분류**로 받는다. 실제 gain 에 42,026화소가 해당 | `22_B2_*.txt` |
| B3 | 결함 맵 적재 + 무결성 | **통과** | 실제 BPMap 적재 10 ms, 손상 7종 모두 거부, RLE 적재. 오류 코드가 SRS 와 다름, 5% 한도는 어디서도 안 검사 (§3) | `23_B3_*.txt`, `23b_B3b_*.txt` |
| B4 | 맵 없음·손상·만료 시 안전 동작 | **실패** | 맵 없음·손상은 안전. 그러나 ① 적재 뒤 만료 미검출 ② 만료 0 경고 없음 ③ `bypassOffset/bypassGain` 무경고 허용 ④ `pipeline_ex` 가 호출자 버퍼를 덮어씀 | `24_B4_*.txt` |
| B5 | 실영상 offset → gain → 결함 순서 | **통과** | 독립 계산과 비마스크 9,385,641화소 **전부 비트 일치**, 고립 결함 803개 전부 일치 | `25_B5_*.txt` |
| B6 | 보정 품질 | **실패** | 평탄 잔여가 6장 모두 Phase 1 한도 1.0% 초과 (최소 1.18%, 최대 3.34%). 암전류는 통과(순환), 결함은 기준 없음 | `26_B6_*.txt` |
| B7 | 프레임 처리 ≤ 500 ms | **통과** | 중앙값 55.9 ms (최대 62.2 ms), 고스트 끔 | `27_B7_*.txt` |
| B8 | 최대 메모리 ≤ 200 MB, 100프레임 누수 없음 | **통과** | 모듈 최대 118.2 MiB (123.9 MB), 100프레임 동안 비공개 사용량 118.7 → 118.8 MiB | `28_B8_*.txt` |
| B9 | 맵 적재 ≤ 200 ms | **통과** | 세 파일 합계 중앙값 97.9 ms (94.7~100.3) | `29_B9_*.txt` |
| B10 | 기준 영상 고정 | **통과** | 출력 float32 파일 + SHA-256, 두 프로세스·맵 재생성 모두 바이트 동일 | `30_`~`33_`, `32_hashes.txt` |

## 2. 먼저: 실영상은 어떤 영상인가

`gui/ImageProcTest/fixtures/gui-s0/raw/wrist_lat_3072x3072.raw` (`10_wrist_provenance.txt`)

- **출처 문서는 저장소에 없다.** `fixture-manifest.json` 은 크기·형식(`UInt16LE`)·SHA-256 만 적고, 검출기·처리 단계·선량은 없다. 파일을 넣은 커밋(977df225)의 메시지는 "[GUI] 대용량 비교 뷰어 및 wrist fixture 구현" 하나뿐이다.
- 영상에서 읽은 사실: 18,874,368 B = 3072² uint16, 값 범위 2481~15451 (14비트 안), 0 도 포화도 없음. **오프셋 대좌(약 2500)가 남아 있다**: p0.1 = 2517, `dark.raw` 평균 2503. 즉 offset 보정 전 raw 다 (CalData_6 의 `bright*` 는 README 대로 이미 보정된 값이라 다르다).
- `CalData_6` 와 같은 검출기인가: **그럴 가능성이 높으나 메타데이터로는 확인 못 했다.** 근거 둘: (a) 행 단위 고정패턴(고역 통과한 행 평균 프로파일)의 `dark.raw` 와의 상관이 0.895 (열 방향은 0.46: 서로 다른 선량의 `bright02` 와 `bright03` 사이 대조값 0.49 와 비슷한 수준. 행 방향 대조는 재지 않았다), (b) `BPMap.map` 의 결함 화소 23,505개에서 옆 화소와의 차이가 평균 28.1, 나머지 화소는 13.4 (결함 위치가 이 영상에서도 튀는 것). 같은 검출기라는 증명은 아니다.
- 입력 해시 전체는 `32_hashes.txt`.

## 3. 항목별 상세

### B1 offset 맵 적재 + 치수 검증 — 통과 (SRS 문구와 다른 점 있음)

기준: SRS-CALIB-FUNC-001 "load offset calibration file ... XPE_ERR_IO_FAILED 로 손상 거부", FUNC-006 은 체크리스트가 같이 적었지만 본문은 비선형 보정이다 (체크리스트의 번호 어긋남, §6).

| 실행 | 결과 |
|---|---|
| 실제 offset 맵(`dark.raw` 에서 제품 생성기 `xpe_calib_generate_offset` 로 생성) 적재 | `XPE_OK`, 40.4 ms |
| `xpe_offset_correct` 를 `dark.raw` 에 적용 | 평균 0.0000 ADU (맵이 이 프레임이라 순환) |
| 1024² 맵을 적재, 3072² 프레임에 적용 | 적재 `XPE_OK`, 적용 `XPE_ERR_INVALID_INPUT` (치수 검사는 **적용 시점**) |
| gain·결함 파일을 offset 적재기로 | `XPE_ERR_CONFIG_INVALID` |
| 없는 경로 / null | `XPE_ERR_IO_FAILED` / `XPE_ERR_INVALID_INPUT` |

SRS 와 다른 점: 페이로드가 **float32 (화소당 4 B, 파일 37.7 MB)** 다. SRS FUNC-001 은 uint16 (3072×3072×2 B). SRS 의 "파일 형식 정정" 블록(2026-09-18)은 헤더 크기와 SHA-256 만 고쳤고 이 차이는 적지 않았다.
기존 시험: `CalibLoadTest` (20개, `LoadOffset_HappyPath`, `_WrongType`, `_TamperedPayload`, `_ExpiredFile` 등), `test_xcal_reader/validator/writer`. CI 잡 `preprocess-tests`, `asan-tests`. 모두 합성 소형 맵이고 실제 맵으로 도는 시험은 없다.

### B2 gain 맵 적재 + 치수·값 범위 검증 — 실패

기준: SRS-CALIB-FUNC-002 "Values shall be in range [0.1, 10.0]; out-of-range values shall trigger `XPE_ERR_INVALID_CALIB_DATA` error" (체크리스트가 같이 적은 FUNC-007·008 은 결함 보정 모드와 온도 보상이다, §6).

| 실행 | 결과 |
|---|---|
| 실제 gain 맵(`bright01~06.raw` 6장에서 제품 생성기 `xpe_calib_generate_gain`) 적재 | `XPE_OK`, 47.0 ms. 맵 통계: 평균 1.0000, **최소 0.0000, 최대 15.4126**, 0.1 미만 42,024화소, 10 초과 2화소 |
| 같은 적재가 낸 알림 | `XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT: 42026 of 9437184 pixel(s) (0.445%) have a gain outside [0.1, 10.0] and are marked defective: gain 1.0 is used ...` |
| 64² 맵에 화소 하나를 0.05, 20.0, 0.0, NaN, −1.0 으로 | 5개 모두 `XPE_OK` + 같은 경고. 그 화소의 gain 단계 결과는 gain 1.0 으로 계산 (1000 → 1000) |
| 1024² 맵을 적재, 3072² 프레임 | 적재 OK, 적용 `XPE_ERR_INVALID_INPUT` |

기준과 다른 점: SRS 는 거부(`XPE_ERR_INVALID_CALIB_DATA`)이고 구현은 **수용 + 경고 + 결함으로 분류해 결함 단계가 채움**이다. 이것은 코드(QA-A-211, #233)에 의도로 적힌 대안 동작이다. SRS 의 "파일 형식 정정" 블록은 이 검사를 "구현 없음 (별도 이슈)" 로 적어 두었다. SRS 문구대로면 **제품 생성기가 만든 이 실제 gain 맵 자체가 적재 거부 대상**이다. 어느 쪽이 맞는지는 결정 사항이다.
치수는 B1 과 같이 적용 시점에 검사한다.
기존 시험: `test_gain_defect_classify.cpp`, `CalibLoadTest`.

### B3 결함 맵 적재 + 무결성 — 통과 (SRS 문구와 다른 점 있음)

기준: SRS-CALIB-FUNC-003 (uint8, 0 = 정상, 1~255 = 결함 종류, RLE 지원, 최대 5% 밀도 허용), SAFE-003 (손상 파일 거부, 부분 보정 없음).

| 실행 | 결과 |
|---|---|
| 실제 BPMap(`BPMap.map` 을 XCal 로 감쌈) 적재 | `XPE_OK`, 10.2 ms. 23,505화소 = 0.249%, 값은 전부 255 (SRS 가 말한 1~4 종류 값이 아니다, 허용 범위 1~255 안) |
| 페이로드 1비트, 헤더 1바이트(폭), 저장된 SHA-256 한 바이트, 매직 바이트 변경 | 4개 모두 거부, **`XPE_ERR_CONFIG_INVALID`** |
| 파일 절반 / 빈 파일 | `XPE_ERR_IO_FAILED` |
| RLE 로 쓴 같은 맵 (14,330 B) | 적재 `XPE_OK` |
| 밀도 4%, 6%, 30% (`23b_`) | 적재와 파이프라인 모두 `XPE_OK`, **알림 0건** |
| 1024² 맵을 적재, 3072² 프레임 | 적재 OK, 적용 `XPE_ERR_INVALID_INPUT` |

SRS 와 다른 점 셋: ① 손상 거부의 오류 코드가 SRS(`XPE_ERR_IO_FAILED`)와 다르다. 거부 자체는 맞고, 기존 시험 `LoadOffset_TamperedPayload_ReturnsConfigInvalid` 가 이 동작을 고정한다. ② 5% 한도는 적재에서도 파이프라인에서도 검사·경고가 없다 (알림 0건). 결함 단계의 `XPE_WARN_DEFECT_UNION_OVER_LIMIT` 은 코드상 gain 이 분류한 화소가 있을 때만 난다. ③ 값 255 만 쓰는 실제 맵.
그리고 제 하니스를 만들다가 우연히 본 사실 하나: 세션 ID 가 다른 맵끼리(내 작성기와 픽스처 생성기) 적재하자 결함 맵이 `XPE_ERR_CONFIG_INVALID` 로 거부됐다. 그 실행의 출력은 덮어써서 증거로 남기지 못했다.
기존 시험: `CalibLoadTest`, `test_xcal_compression.cpp` (RLE).

### B4 맵 없음·손상·만료 시 안전 동작 — 실패

기준: SRS-CALIB-SAFE-001~004, FUNC-009 (체크리스트는 FUNC-012 로 적었으나 그 번호는 빈닝이고 만료는 FUNC-009 다, §6).

| 실행 (실제 맵, 실영상 프레임) | 결과 | SRS 기준 대비 |
|---|---|---|
| 모듈을 초기화하지 않음 | `XPE_ERR_NOT_INITIALIZED` | 맞음 |
| 초기화했으나 맵 없음 / offset 만 / gain 만 / 결함 맵 없이 결함 단계 켬 | 전부 **`XPE_ERR_CALIB_NOT_LOADED`**, 호출자 버퍼 그대로 | SAFE-001 은 `XPE_ERR_NOT_INITIALIZED`. 코드 이름은 다르나 안전하게 멈춘다 (#117 결정으로 둘을 가름) |
| 손상 offset 을 적재 | `XPE_ERR_CONFIG_INVALID`. 정상 세트가 올라가 있으면 그 세트가 유지되어 파이프라인이 계속 돈다. 처음 적재가 손상이면 파이프라인은 `XPE_ERR_CALIB_NOT_LOADED` | SAFE-003 "all or none" 에서 "정상 세트 유지"는 어느 쪽인지 문구가 없다 |
| 만료(어제) offset 을 적재 | 적재가 `XPE_ERR_CALIBRATION_EXPIRED` 로 거부, 파이프라인은 `XPE_ERR_CALIB_NOT_LOADED` | SAFE-002 는 파이프라인이 `XPE_ERR_CALIBRATION_EXPIRED` 를 내라고 한다. 만료 맵으로 처리되지는 않으니 안전 |
| **적재 시점 6초 뒤 만료되는 맵**을 올리고 8초 뒤 파이프라인 | **`XPE_OK`로 계속 처리** (경고도 없음) | **SAFE-002 위반**: "Pipeline shall abort ... if any loaded calibration file has expired". 만료는 적재 때만 검사 |
| 만료 0 (만료 없음) 맵을 적재 | 정상 적재, **`XPE_WARN_NO_EXPIRY` 알림 없음** | **FUNC-009 위반**: "shall log a warning XPE_WARN_NO_EXPIRY" |
| 세 맵을 모두 올리고 `bypassOffset=true` / `bypassGain=true` / 둘 다 | 전부 `XPE_OK`, 알림 없음 | **SAFE-001 위반**: offset·gain 은 "mandatory and non-bypassable" |
| `xpe_preprocess_pipeline_ex` 후 호출자 버퍼 | **덮어씀** (float 결과를 같은 버퍼에 씀) | **SAFE-004 위반**: "Calibration correction shall never modify the input image buffer" (프로토콜 5.7 `InputPreserved = true`). 단계 API `xpe_offset_correct`, `xpe_gain_correct` 를 별도 출력 버퍼로 쓰면 입력은 그대로 |

실패 이유는 위 굵은 네 줄이다. 맵이 없거나 손상됐을 때 처리를 안 하고 멈추는 핵심 동작은 안전하게 작동한다.
기존 시험: `CheckExpiryTest` (8), `CalibLoadTest` 의 만료·변조 사례, `test_global_state_hygiene.cpp`. 이 시험들은 **적재 시점의 동작**만 고정한다. 네 위반 중 적재 뒤 만료·만료 0 경고·입력 보존을 단언하는 시험은 찾지 못했다 (`modules/preprocess/tests` 에서 `NO_EXPIRY`, `InputPreserved`, 입력 불변 문구를 검색). **우회는 오히려 정상 기능으로 시험된다**: `test_pipeline_ex.cpp:266` 이 `bypassOffset`, `bypassGain` 을 포함해 모두 true 로 둔 설정을 쓰며, 여러 시험이 이 플래그를 쓴다. 즉 SAFE-001 과 어긋나는 동작이 의도된 기능으로 굳어 있다.

### B5 실영상 offset → gain → 결함 순서 — 통과

기준: PR-FUNC-002 (정준 순서: 판독 검증, 온도 보상, offset, 비선형, gain, 빈닝, 결함, 고스트), SRS-CALIB-FUNC-004·005.
방법: 실영상에 파이프라인(기본 설정과 `bypassTemp/Nonlinearity/Binning` 설정)을 돌리고, **맵 파일의 값과 SRS 공식만으로 제 하니스에서 따로 계산**한 기대값과 화소 단위로 비교했다 (offset 은 `src − off` 를 0 미만 0 으로 막고 +0.5 반올림 후 uint16, gain 은 `x × (1/g)`, 범위 밖 gain 화소는 gain 1.0).

| 확인 | 결과 |
|---|---|
| 마스크 화소 (맵 23,505 + gain 분류 42,026 − 겹침 = 합집합) | 51,543 |
| 마스크되지 않은 9,385,641화소가 기대값과 같은 화소 수 | **9,385,641 (전부, 최대 |차| 0)** |
| 결함 단계를 끈 출력이 기대값과 같은 화소 수 | 9,437,184 / 9,437,184 |
| 순서가 gain → offset 이었다면 | 비마스크 9,365,928화소가 0.5 ADU 넘게 달라지고 최대 22,603 ADU (평균 175.5 ADU) → 이 출력은 offset → gain 순서 |
| 결함 단계가 바꾼 마스크 화소 | 51,543 중 51,541 (2개는 그대로) |
| 고립 마스크 화소 803개 | 유효한 4이웃 평균과 **803개 전부 일치** (최대 |차| 0) |
| 군집 마스크 화소 50,740개 | 가장 가까운 유효 고리의 값 범위 안: **50,740개 전부** |
| NaN/Inf | 0. 출력 평균 2190.996 (원본 평균 4705.535) |
| 기본 설정 출력 = `bypassTemp/Nonlinearity/Binning` 설정 출력 | 바이트 동일 (플래그만 0x203c 대 0x201c) |

기존 시험: `PipelineStageValueTest` (7), `test_pipeline_ex.cpp`, `test_pipeline_stages.cpp`. 합성 입력이다.
미검증: 군집 화소의 정확한 중앙값 계산은 값 범위 확인까지만 했다 (제품의 중앙값 산식을 독립 재구현하지 않았다).

### B6 보정 품질 — 실패 (평탄 잔여)

기준: SRS-CALIB-FUNC-016 (`abs(DarkBias) ≤ 5 ADU` 또는 `DarkReduction_dB ≥ 10`), FUNC-017 (`FlatResidualPct ≤ 1.0%`, 릴리스 하드닝 목표 `≤ 0.5%`), 프로토콜 5.3 (`FPN_Reduction_dB ≥ 10`, 평탄 ROI는 gain 이 유효한 모든 화소), FUNC-019 (합성 BPM 오라클에서 재현율 100%, `GoodPixelDeltaP99 ≤ 1 ADU`). `FlatResidualPct = 100 × std / mean`.

**암전류 (통과, 순환)**: 맵이 `dark.raw` 한 프레임에서 만들어져서 그 프레임을 보정하면 DarkBias 0.0000, DSNU 0.0000, ClampRate (R−D<0) 0.0000% 이 당연하다. 독립된 두 번째 암전류 프레임이 없어서 **독립 검증은 못 했다**. `xpe_verify_offset` 도 `overall_pass = 1`.

**평탄 (실패)**: 실제 평탄 6장을 `dark + bright_k` 로 만든 raw 로 파이프라인에 넣었다 (맵의 gain 은 이 6장의 평균에서 만든 것).

| 평탄 | 평균 ADU | 보정 전 | gain 후 (전 화소) | gain 후 (마스크 제외) | 결함 단계 후 (전 화소) | FPN 감소 dB | 화소 잡음 CV | 64×64 타일 평균 CV | 한 장을 뺀 gain 으로 (마스크 제외) |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 2883.2 | 11.50% | 6.68% | 1.99% | **2.06%** | 13.39 | 0.30% | 1.82% | 2.58% |
| 2 | 5014.6 | 11.12% | 6.47% | 1.17% | **1.18%** | 17.53 | 0.21% | 1.12% | 1.94% |
| 3 | 350.6 | 16.72% | 10.20% | 3.31% | **3.34%** | 8.49 | 1.12% | 2.91% | 3.41% |
| 4 | 867.4 | 11.13% | 7.14% | 2.41% | **2.46%** | 10.74 | 0.68% | 2.15% | 2.59% |
| 5 | 1395.7 | 10.88% | 6.93% | 2.34% | **2.39%** | 10.97 | 0.52% | 2.15% | 2.63% |
| 6 | 2196.2 | 10.73% | 6.79% | 2.30% | **2.34%** | 11.12 | 0.40% | 2.15% | 2.78% |

- 최종 출력(결함 단계 뒤) 기준 `FlatResidualPct` 가 **6장 모두 1.0% 초과**: 최소 1.18%(평탄 2), 최대 3.34%(평탄 3). 한 장을 뺀 gain 으로는 1.94~3.41%.
- "gain 후 (전 화소)" 6.5~10.2% 는 결함 화소(평탄 영상에서 0 또는 65533 근처)가 평균과 표준편차를 끌어서 큰 값이다. 프로토콜 5.3 의 정의대로(gain 이 유효한 모든 화소, 결함 포함)면 이 값이 `FlatResidualPct` 라서 제품 자체 검증 `xpe_verify_gain` 도 평탄 2 에서 `prnu_after 6.47%`, `overall_pass = 0` 을 낸다. 결함 단계까지 거친 값(굵은 글씨)이 의미 있는 값이다.
- `FPN_Reduction_dB ≥ 10` 은 평탄 3 만 미달 (8.49).
- 잔여의 성격: 화소 잡음(0.2~1.1%)이 아니라 **큰 규모의 불균일**(타일 평균 CV 1.1~2.9%)이다. 6장 평균으로 만든 gain 이 화소 단위 감도는 맞췄으나 영상 전체의 완만한 명암 차이는 남는다. 원인은 확인하지 못했다. 가설 둘(검증하지 않음): ① 6장의 선량이 달라 응답이 선량에 비선형이거나 선량마다 음영이 다르다 (비선형 보정과 다점 gain 은 이 마일스톤 범위 밖), ② 데이터셋 `bright*` 가 이미 다른 처리를 거친 값이라 평탄하지 않다.
- 미측정: `LineArtifactScore` (프로토콜 5.3, "gain 전 대비 10% 넘게 늘지 않음") 은 재지 않았다.

**결함**: 실영상에는 정답(어느 화소가 결함인지의 오라클)이 없다. `DefectRecall`, `DefectFPR` 의 합격선은 합성 오라클 전용이라 **기준 없음**이다. 값으로는: `GoodPixelDeltaP99 = 0.000000 ADU` (결함 단계가 비마스크 화소 9,385,641개 중 하나도 바꾸지 않았다, 기준 ≤ 1 ADU 충족), `DefectResidualADU` (마스크 화소와 유효 4이웃 평균의 평균 절대 차): 결함 단계 후 11.23 ADU, 전 2202.28 ADU (평탄 2, 7,207화소; 평균 5014 ADU 의 0.22%). `xpe_verify_defect`: `correction_error 4.49`, `overall_pass = 1`.
기존 시험: `VerifyMetricsTest` (35) 는 합성 오라클이다. 실제 평탄으로 `FlatResidualPct` 를 재는 시험은 없고 이 측정이 처음이다.

### B7 프레임 처리 시간 — 통과

기준: SRS-CALIB-PERF-001 "Total preprocessing pipeline execution time shall not exceed 500 ms per 3072×3072 float32 frame on Intel Core i7". 실영상, 실제 맵, 고스트 끔, 40프레임 연속 (`27_`).

| 설정 | 첫 프레임 | 이후 39프레임 최소 / 중앙값 / p95 / 최대 |
|---|---|---|
| 기본 설정 | 60.0 ms | 53.0 / **55.9** / 62.0 / 62.2 ms |
| 기본 + `bypassTemp/Nonlinearity/Binning` | 50.4 ms | 47.3 / 52.1 / 88.5 / 95.8 ms |

단계별 시간은 이번에 따로 재지 않았다 (기존 `PipelinePerformance3072` 가 단계별로 출력하지만 합성 맵이고 고스트를 포함한다). 고스트 끔이라 SRS 의 고스트 단계 예산은 해당 없음.
기존 시험: `PipelinePerformance3072.TheWholeFrameWithCalibrationLoadedFitsSrsPerf001` (500 ms 단언, CI `preprocess-tests`, `asan-tests` 에서는 제외).

### B8 최대 메모리와 100프레임 누수 — 통과

기준: SRS-CALIB-PERF-002 "Peak memory allocation shall not exceed 200 MB ... No memory leaks during 100-frame batch processing". 방법은 237g 와 같은 프로브: 실영상 100프레임, 실제 맵, **고스트 핸들 없음**, 단계 경계 측정 (`28_`).

| 항목 | 값 |
|---|---|
| 정상 상태 (맵 적재 후) | 81.4 MiB |
| 모듈 최대 | **118.2 MiB (123.9 MB)** = 한계 200 MB 의 62% |
| 단계 최대 | 온도 단계 +35.8 (작업 버퍼) → 117.5, 결함 단계 직후 +0.7 → 118.2 |
| 100프레임 비공개 사용량 (프레임 0 / 9 / 49 / 99 뒤) | 118.7 / 118.7 / 118.7 / 118.8 MiB (증가 0.1 MiB) |
| 누적 최대 커밋 (프레임 0 뒤 / 99 뒤) | 155.5 / 155.8 MiB 절대값 (하니스 바탕 37.3 MiB 포함) |

100프레임에서 증가는 측정 해상도(0.1 MiB) 안의 0.1 MiB 이고 프레임마다 자라는 추세는 없다. 기존 누수 시험(`EnduranceTest` 6, `XpePreprocessEndurance` 2)도 일반 빌드에서 통과한다 (`40_`, 합성 입력, CRT 힙 기반).
SRS 의 190 MB 내역은 offset 맵을 18.9 MB (uint16) 로 가정하고 고스트 150 MB 를 포함한 것이다. 실제 offset 맵은 float32 37.7 MB 이고 이번 측정은 고스트를 뺐다.
미검증: 메모리 최대의 CI 단언은 없다 (237 시리즈의 프로브는 `DISABLED_`). 한계의 해석(10진 MB)은 237g 와 같다.

### B9 맵 적재 시간 — 통과

기준: SRS-CALIB-PERF-003 "not exceed 200 ms for all three files on SSD". 세 파일을 차례로 12번 (프로세스 안에서 모듈을 매번 초기화), `29_`.

| 파일 | 크기 | 중앙값 | 범위 |
|---|---|---|---|
| offset | 37,748,950 B | 40.3 ms | 39.4~43.6 |
| gain | 37,749,092 B | 46.4 ms | 45.0~48.5 |
| 결함 | 9,437,336 B | 10.1 ms | 9.8~11.4 |
| **합계** | | **97.9 ms** | 94.7~100.3 (첫 번째 100.3) |

저장장치: 이 기계의 디스크 둘 다 NVMe SSD (D: 가 어느 쪽인지는 확인하지 않았다). **파일을 방금 써서 OS 캐시에 있을 가능성이 크다. 콜드 캐시 적재는 재지 않았다.** SRS 는 "incremental CRC" 라고 하나 실제는 SHA-256 스트리밍이다 (SRS 정정 블록).
기존 시험: `PipelinePerformance3072` 가 적재 시간을 **출력만 하고 단언하지 않는다** ("not asserted").

### B10 기준 영상 고정 — 통과

출력: `wrist_lat_3072x3072_corrected_f32le.raw` (float32 리틀엔디언, 3072², 37,748,736 B), 설정은 고스트 핸들 없음 + `{"bypassTemp":true,"bypassNonlinearity":true,"bypassBinning":true}` (기본 설정과 바이트 동일, B5).

| 항목 | 값 |
|---|---|
| SHA-256 | `42e78facd6cd75edbb59d26b9be1cca313d431d07900e3064cd472c068d1ef02` |
| FNV-64 | `3e0aa952d2fc9252` |
| 같은 프로세스에서 두 번 | 바이트 동일 |
| 다른 프로세스에서 다시 | 바이트 동일 |
| **맵을 처음부터 다시 생성**해서 (헤더의 생성 시각만 다르고 헤더 뒤 내용은 동일) | 출력 바이트 동일 (`33_`) |

재현 절차(맵 생성 포함, 증거 `32_hashes.txt` 에 입력 해시): 입력 `wrist_lat_3072x3072.raw`, `CalData_6/dark.raw` (→ `xpe_calib_generate_offset`, 프레임 1장·100 ms·25 °C), `bright01~06.raw` (→ `xpe_calib_generate_gain`, 6장, dark 참조 없음), `BPMap.map` (→ XCal 결함 맵, 값 유지). 실행: `xpe_preprocess_tests --gtest_also_run_disabled_tests --gtest_filter=A240.DISABLED_B10_*` 에 환경 변수 `XPE_A240_CAL`, `XPE_A240_WRIST`, `XPE_A240_OUT`.
**파일 위치**: `build/a240/baseline/` 이다. **37.7 MB 라서 커밋하지 않았다.** 어디에 둘지(예: 메인 저장소의 `tests/test_data/`)는 리더 결정이다.
주의: ① 이 기준 영상의 gain 은 저장소의 실제 평탄 영상에서 제품 생성기로 만든 것이고 운영 gain 이 아니다 (분류 화소 42,026개 포함). ② 제품이 의도해서 출력을 바꾸는 변경(예: gain 범위 검사 구현으로 맵이 거부되면 이 기준 영상 자체가 달라진다, B2)은 기준 영상을 다시 고정해야 한다.

## 4. 측정 하니스

`modules/preprocess/tests/test_zz_a240_checklist.cpp` (`DISABLED_`, 값을 출력만 하고 거의 단언하지 않는다), `CMakeLists.txt` 에 등록. B8 용으로 `test_zz_a237b_mem.cpp` 프로브에 `XPE_A240_NOGHOST`, `XPE_A240_WRIST` 선택지를 더했다. 제품 코드 변경은 없다. 증거 파일은 `evidence/` 에 `[a240]` 줄로 출력되어 있다.

## 5. 미검증 · 잔여 위험

- 실영상의 출처와 검출기 일치는 메타데이터로 확인하지 못했다 (§2).
- gain·offset 맵은 저장소의 실제 평탄·암 영상에서 제품 생성기로 만든 것이고 운영 맵이 아니다. B6 의 평탄 잔여 값은 이 gain 에 대한 것이다.
- 암전류 보정 품질은 독립 검증이 없다 (같은 프레임으로 만든 맵을 같은 프레임에 적용). 
- B6 의 `LineArtifactScore` 와 B7 의 단계별 시간은 재지 않았다.
- B9 는 콜드 캐시를 재지 않았다.
- B4 의 만료 시험은 6초·8초라는 짧은 시간으로 했다. 파이프라인이 만료를 프레임마다 재검사하지 않는다는 사실은 이 한 번의 실행으로 확인했다 (코드는 읽지 않았다).
- 단일 기계, 단일 실행 위주다. 흔들림은 B7(40프레임)과 B9(12회)에서만 봤다.

## 6. 체크리스트와 SRS 의 번호 어긋남 (확인 요청)

체크리스트(`milestone-1-pre-basic.md`)의 기준 출처 번호가 SRS 본문과 맞지 않는 곳이 있다. 이 보고서는 **본문의 내용**으로 판정했다.

| 체크리스트 | 적힌 번호 | SRS 본문 |
|---|---|---|
| B1 | FUNC-001·006 | 006 은 비선형 보정 (offset 적재는 001, 적용은 004) |
| B2 | FUNC-002·007·008 | 007 은 결함 보정 모드, 008 은 온도 보상 (gain 적용은 005) |
| B4 | FUNC-012 | 012 는 빈닝 보상, 만료 검사는 FUNC-009 |
| B6 | "SRS-CALIB 의 정량 기준" | FUNC-016·017·019 와 프로토콜 5.2~5.5 에 있다 |

## 7. SRS 와 구현이 어긋난 곳 (판정 근거 모음)

| 항목 | SRS | 구현 |
|---|---|---|
| offset 맵 페이로드 | uint16, 18.9 MB | float32, 37.7 MB |
| 범위 밖 gain | 거부 `XPE_ERR_INVALID_CALIB_DATA` | 수용, 경고, 결함으로 분류 |
| 손상 파일 거부 코드 | `XPE_ERR_IO_FAILED` (CRC-32) | `XPE_ERR_CONFIG_INVALID` (SHA-256) |
| 맵 없음 | `XPE_ERR_NOT_INITIALIZED` | `XPE_ERR_CALIB_NOT_LOADED` |
| 만료 | 파이프라인이 `XPE_ERR_CALIBRATION_EXPIRED` 로 중단, 적재 뒤에도 | 적재만 거부, 적재 뒤 만료는 무시 |
| 만료 0 | `XPE_WARN_NO_EXPIRY` 경고 | 경고 없음 |
| offset·gain 우회 | 불가 | `bypassOffset/bypassGain` 허용, 무경고 |
| 입력 버퍼 | 수정 금지 | `pipeline_ex` 는 제자리 결과 |
| 결함 밀도 5% | "최대 5% 허용" | 검사·경고 없음 |

## Card Cross-Check

| milestone | card |
|---|---|
| B1~B10 측정 | QA-A-240 (이 보고서) |
