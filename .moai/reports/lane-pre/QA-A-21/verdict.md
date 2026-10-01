# QA-A-21 — 미등록 테스트 3파일 + 헤더 알고리즘 주석 정정 (#120, #125)

**레인**: Lane A (`xpe-pre`, dev/preprocess)   **발행**: 2026-09-10   **Refs**: #120 #125
**baseline**: 로컬 `main` 병합 → `ba05efa`

## 1. 주장 (Claim)

**3항(헤더 주석 정정) 완료.** 1·2항(테스트 등록)은 **등록하지 않았다** — 세 파일 + 판정
대상 1파일 모두, 남은 장벽이 "호출 형태를 맞추는 일"이 아니라 **없어진 계약을 무엇으로
대체할지 정하는 일**이기 때문이다. 사유는 실측과 함께 CMake 주석·아래에 남겼다.

## 2. 증거 (Evidence)

### 2.1 3항 — `xpe_defect_correct` 주석 정정 (완료)

leader 의 #125 판독을 옮기지 않고 구현을 직접 읽어 확인했다:

| 실제 동작 | 근거 |
|---|---|
| 고립 결함 → 유효 4-이웃(N/S/E/W) **평균** | `helpers.cpp:22-36` |
| 4-이웃이 전부 결함 → 체비셰프 링 `r = 1..3` | `helpers.cpp:38-48` |
| 클러스터(4-연결 2+) → 3×3 유효 이웃 **중앙값** | `defect_correct.cpp:70-72` |

"edge-aware bilinear" 도 "5x5 neighborhood" 도 구현에 없다 — leader 판독이 맞다.
주석을 실제 동작 + 근거 파일·줄로 교체했다.

**범위 안에서 1건 추가**: 반환 코드 목록에 A-20 이 도입한 `XPE_ERR_CALIB_NOT_LOADED` 가
빠져 있어 넣고, `NOT_INITIALIZED` 설명도 "모듈 초기화 여부"로 정확히 했다.

### 2.2 1항 — 3파일 등록 시 컴파일 오류 110건 (`a21-b1.log`)

```
50  test_calibration_roundtrip.cpp
44  test_gain_correct_reciprocal_fma.cpp
16  test_golden_reference.cpp
```

전부 은퇴한 맵-인자 API 계열: `xpe_calib_save`(4인자), `xpe_calib_load_offset/gain`(2인자),
`xpe_calib_generate_offset`(4인자), `xpe_calib_check_expiry`(2인자), `xpe_offset_correct`
(2인자), `xpe_gain_correct`(2인자), `xpe_validate_readout_artifact`(인자 형태).

### 2.3 등록하지 않은 이유 — 두 건은 "없어진 계약"이다

`test_golden_reference.cpp` 20 케이스 중 **12개(Ghost/Temp/Binning)는 그대로 컴파일된다.**
막히는 8개가 문제다:

**(a) `GoldenGainTest` 2건 — 게인 규약이 반대다.**

```
테스트(golden 16개):  output[i] = raw[i] * gain[i]      (예: 1000 × 1.5 = 1500.0)
구현(gain_correct.cpp:296): reciprocal[i] = 1.0f / gainmap[i]; output = raw × reciprocal
```

저장된 맵이 게인 G 인지 역게인 R=1/G 인지에 따라 어느 쪽이든 맞을 수 있다.
`spec.md:145` 는 "multiply each pixel by the corresponding gain factor" 라 하고, 같은 절의
Algorithm 줄은 "production path uses precomputed reciprocal gain map R = 1/G" 라 한다 —
**문서 안에서도 두 서술이 갈린다.** 테스트를 고치면 규약을 내가 정하는 셈이다.

`ZeroGainProducesZero` 도 같은 뿌리다: 구현은 `is_valid_gain` 으로 0 을 걸러
`XPE_ERR_CONFIG_INVALID` 를 낸다(0 으로 나눌 수 없으므로). 테스트는 결과 0.0f 를 기대한다.

**(b) `GoldenReadoutTest` 4건 — 출력 모델 자체가 다르다.**

```
테스트:  xpe_validate_readout_artifact(&img, &score, msg, sizeof(msg))  → 0..100 점수
구현:    xpe_validate_readout_artifact(img, metadata, bool* dropped, bool* nonuniform)
```

점수라는 관측 대상이 없다. "migrate" 가 아니라 **새로 쓰는 것**이다.

`test_gain_correct_reciprocal_fma.cpp`(44) 와 `test_calibration_roundtrip.cpp`(50)는
같은 게인 규약과 은퇴한 저장/로드 API 위에 서 있다 — (a) 가 정해지기 전에는 같은 문제다.

### 2.4 2항 판정 — `test_xpe_preprocess.cpp`(51 케이스)

**기존 CMake 주석의 절반은 낡았다.** "uses legacy 3-arg API" 라 적혀 있으나 현재 API 가
3-인자이고, 이 파일의 2-인자 보정 호출은 **0건**이다(`grep -c` 실측).

등록 프로브 결과 오류 **14건**, 전부 구조체 필드다(`a21-probe.log`). 다만 **단순 개명이
아니다**:

| 옛 필드 | 현재 | 성격 |
|---|---|---|
| `XpeImageBuffer::stride` | 없음 | 2곳, `width` 로 유도 가능 — 기계적 |
| `kvp` → `kVp`, `sid_mm` → `SID_mm` | 있음 | 개명 |
| `ma` → `mAs` | 있음 | **mA vs mA·s — 다른 물리량** |
| `acquisition_time_s` → `acquisitionTime` | 있음 | **초 vs epoch 밀리초** |
| `temperature_c`, `integration_time_ms` | **없음** | 대응 필드 자체가 없음 |

마지막 셋은 해당 케이스가 무엇을 주장하는지를 바꾼다. 온도 보상 테스트
(`:771`, `:1170`)는 온도를 metadata 로 주는 전제 위에 있는데 그 전제가 ABI 에 없다.

**판정: 등록 가능하나 계약 결정이 선행되어야 한다.** 편집 작업이 아니다.

### 2.5 재실측 (`a21-ctest2.log`)

```
100% tests passed, 0 tests failed out of 404
```

등록을 보류했으므로 케이스 수는 A-19 와 동일하다.

## 3. baseline 귀속

로컬 `main` 병합 트리 `ba05efa`. 오류 건수는 세 파일을 실제로 등록해 빌드한 결과
(`a21-b1.log`), 14건은 `test_xpe_preprocess.cpp` 를 등록해 빌드한 결과(`a21-probe.log`).
두 등록 모두 측정 후 원복했다.

## 4. Gaps (미검증)

- **세 파일을 마이그레이션하지 않았다.** §2.3 (a) 게인 규약이 정해지면
  `test_golden_reference` 의 나머지 8건과 다른 두 파일이 함께 풀린다 — 한 카드로 묶는 편이
  낫다고 본다.
- `GoldenReadoutTest` 4건은 규약이 정해져도 **새로 작성**해야 한다(점수 모델 부재).
- `test_xpe_preprocess.cpp` 의 51 케이스 중 몇 개가 사라진 필드에 실제로 의존하는지
  세지 않았다 — 오류가 난 지점만 봤다.
- 헤더 주석 정정은 **주석이므로 동작 검증 대상이 아니지만**, 정정 내용이 모든 입력에서
  성립하는지(예: 링 r=3 에서도 유효 이웃이 없을 때)는 코드를 읽어 판단했을 뿐 실행으로
  확인하지 않았다.

**세션 진행 관련(leader 요청)**: 이 세션이 "idle" 로 보이는 것은 도구 타임아웃이나 긴 빌드
때문이 아니다. 이 레인은 **대화형 세션**이라 한 턴이 사용자에게 보고를 돌려주는 지점에서
끝나고, 다음 사용자 입력까지 대기한다. 카드 크기와 무관한 구조적 특성이다 — 다만 한 턴에
담기는 작업량은 유한하므로, **카드를 "한 번에 커밋 가능한 단위"로 쪼개면** 미커밋 상태로
대기하는 구간이 줄어든다.

## 5. 잔여 위험

- 게인 규약을 잘못 정하면 **모든 게인 골든값이 조용히 반대로 고정된다.** 지금은 테스트가
  없어서 아무도 틀렸다고 말해주지 않는 상태이고, 잘못된 골든은 그 침묵을 확정으로 바꾼다.
- 미등록 상태가 길어질수록 세 파일은 계속 표류한다 — 이번에 110건이었고, A-15 때
  36건이었다.
