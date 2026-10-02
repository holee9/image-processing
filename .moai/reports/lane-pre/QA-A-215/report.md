# QA-A-215 — 비닝·런타임 결함 검출이 비유한 프레임을 입구에서 거부한다 (Refs #233)

기준: dev/preprocess `76b8ac4b` 위의 작업 트리. 코드: `helpers.cpp`, `xpe_preprocess_internal.h`, `binning_correct.cpp`, `runtime_detection.cpp`, `preprocess_api.h`(문서), `CMakeLists.txt`, 시험 `test_nonfinite_inputs.cpp`(8건). 증거: `evidence/`. 푸시하지 않음. 211~214b 와 분리된 별도 커밋.

## 결론

- `xpe_binning_correct`: NaN·±무한대가 든 프레임은 **쓰기 전에** `XPE_ERR_INVALID_INPUT` 으로 거부한다. 버퍼 바이트 불변. 이전에는 `PROCESSING_FAILED` 를 돌려주면서 앞쪽 화소가 이미 나뉘어 있었다(실측: 입력 `2 2 2 nan 8 8` 의 앞 세 화소가 바뀐 채 rc=-3).
- `xpe_defect_detect_runtime`: 같은 원칙으로 입구에서 거부한다. 결함 맵도 쓰지 않는다(이전: 맵을 먼저 비우고 씀).
- 유한 프레임은 두 함수 모두 **출력이 비트 동일**(옛 빌드 `bin_pre214b` 와 11개 조합 해시 일치). 파이프라인 결과 불변(비닝의 입력은 게인 단계의 유한 출력).
- 고스트·`xpe_verify_gain` 은 고치지 않고 판정만 적는다(아래).

## 1. 비닝 — 비유한 결과가 유한 입력에서 나올 수 있는가

없다. 인수는 1/4 또는 1/16 이다. 유한 float 에 곱하면 크기가 줄 뿐(정규수 → 비정규수 → 0, 모두 유한) 최대 float 를 넘지 않는다. 그래서 비유한 결과의 원천은 비유한 입력뿐이고, 검사를 입력에 둬도 옛 사후 검사와 같은 프레임을 같은 사유로 거른다. 시험이 극단 유한값(FLT_MAX, lowest, min, denorm_min, 1e-40, ±0, …)이 `OK` 에 곱셈 결과와 비트 동일임을 단언한다.

순서: 널·형식·모드(1 은 무동작, 잘못된 모드 `CONFIG_INVALID`) 가 이전과 같은 우선순위로 먼저 나오고, 그 뒤에 화소 검사, 그 뒤에 쓰기. 시험 고정.

알림(`XPE_ALERT_ERROR`, 레인 간 계약):
```
XPE_WARN_BINNING_INPUT_NOT_FINITE: <N> pixel(s) of the input frame are NaN or infinite (first: index <I>, x=<X>, y=<Y>); the frame was not binned and the buffer was not changed
```

## 2. 런타임 결함 검출 — 비유한 입력 때 동작(검사 전 실측)

| 입력 | 검사 전 동작 |
|---|---|
| NaN 화소 1개 | 플래그 **0**(깨끗한 프레임과 같음): 모든 비교가 거짓이라 가장 명백한 이상 화소가 "정상" |
| ±무한대 1개 | 플래그 1(그 화소만) |
| 이웃 화소 | 영향 없음(거울 프레임 200쌍 중 차이 0) |
| 맵 | 비유한 프레임에도 먼저 비우고 씀 |

즉 손상은 통계 오염이 아니라 "조용하고 한쪽으로만 틀림"이다. 입구 거부가 맞다. 알림:
```
XPE_WARN_RUNTIME_DETECT_INPUT_NOT_FINITE: <N> pixel(s) of the input frame are NaN or infinite (first: index <I>, x=<X>, y=<Y>); no detection was run and the defect map was not written
```
검사 위치: 버퍼 검증 뒤, 파라미터 구성·맵 쓰기 앞(`BUFFER_TOO_SMALL` 등이 먼저 나옴, 시험 고정).

### 호출자 표 (`grep -rn`, clients/gui/modules, 시험 제외)

| 함수 | 호출 | 위치 |
|---|---|---|
| `xpe_binning_correct` | 파이프라인 단계 5 | `pipeline.cpp:354` (입력은 게인 단계 출력) |
| 〃 | 클라이언트·GUI 직접 호출 | **없음** |
| `xpe_defect_detect_runtime` | 이름만 등록(준비 확인 목록) | `ReadinessProbe.cs:26`, `XpePreprocessNative.cs:82` — 호출 아님 |
| 〃 | 실험 도구 | `modules/preprocess/tools/xpe_detect_experiment.cpp:326, 1144, 1699` (실영상, 유한) |
| 〃 | GUI 호출 | **없음** |

## 3. 판정만 (고치지 않음)

| 함수 | 판정 | 근거 |
|---|---|---|
| 고스트 `xpe_ghost_correct` | **제안 있음(별도 카드)**: 입구 거부로 통일하는 것이 맞다. 지금은 화소를 제자리에서 쓰다가 중간에 비유한 `raw`/`corrected` 를 만나면 `PROCESSING_FAILED` — 비닝과 같은 "쓴 뒤 거부, 출력 불변 위반 + 코드 불일치"다. 단 평균·이웃 평균은 비유한을 건너뛰는 설계(`:97`, `:200`)라 입력 의미가 더 복잡해, 입구 거부로 바꿀 때 "건너뜀"과 "거부" 중 무엇이 요구인지 SRS 확인이 먼저 | `ghost_correct.cpp:97,121,125,151,155,180,200,217` |
| `xpe_verify_gain` | **제안 없음(현행 유지)**: 게인 맵의 비유한·0 이하 항목을 건너뛰고 `invalid_gain_count` 로 센다 — 측정 지표 함수라 세어서 보고하는 것이 옳고, 쓰는 출력이 없다. 남는 구멍은 `after` 프레임의 비유한이 통계를 오염시킬 수 있는지인데 이번에 측정하지 않았다(Gaps) | `xpe_verify_metrics.cpp:537` |

## 시험 (`test_nonfinite_inputs.cpp`, 8건)

비닝 4건(NaN/+inf/−inf 가 처음·중간·끝에 있을 때 모드 2·4 모두 바이트 불변·알림 존재, 알림 전체 문구, 극단 유한값 비트 동일, 기존 오류 우선순위) + 런타임 4건(거부·맵 77 패턴 불변, 알림 전체 문구, 유한 프레임이 핫픽셀 60000 을 여전히 1개만 플래그·알림 0, BUFFER_TOO_SMALL 우선).

### 반증 (한 번에 하나, 전체 빌드, `evidence/30_arm_h*.txt`; 마지막에 복원 + 다시 빌드)

| 손상 | 빨강이 된 시험 |
|---|---|
| h1 비닝 사전 검사 무력화 | 비닝 거부 시험(바이트 불변) + 알림 시험 |
| h2 비닝 오류 코드를 `PROCESSING_FAILED` 로 | 비닝 거부 시험 |
| h3 비닝 알림 제거 | 비닝 거부·알림 시험 |
| h4 런타임 입구 검사 무력화 | 런타임 거부·알림 시험 |
| h5 도우미가 NaN 만 봄(무한대 통과) | 비닝·런타임 거부 시험(−inf·+inf 케이스) |
| h6 런타임이 검사 전에 맵을 비움 | 런타임 거부 시험(맵 패턴 77 불변) |

## 이전/이후 (`evidence/50_e2e_old.txt` / `50_e2e_new.txt`, 3072², old = `bin_pre214b` DLL)

11개 조합(비닝 모드 2·4 × 4개 프레임 + 런타임 3개 프레임: CalData_6, cyan_test, 합성, 극단값) **출력 sha256 전부 일치**. 시간(ms, 단일 실행, 순서 번갈아 2회 중 1회분): 비닝은 오히려 빨라짐(7.6~10 → 3.5~4.6; 화소별 `isfinite` 분기가 사라지고 사전 검사 한 번이 벡터화됨), 런타임 검출은 차이 없음(110.9 vs 111.6, 검사 비용은 약 +1~2 ms 로 잡음 속).

## 검증

`evidence/verify/`: 빌드 `BUILD_EXIT=0`, preprocess 885 중 877 통과·8 건너뜀(이전과 같은 CalibSave/CheckExpiry/Load* 8건, 셔플 877 동일), 할당 실패 60, common 69·12, ctest 총 1067, 공개 export 변화 없음(diff 0, common 16), 헤더 문서 0건, 프리셋 일치 12/12.

## 미검증 (Gaps)

- `xpe_verify_gain` 의 `after` 프레임이 비유한일 때의 동작.
- 고스트를 입구 거부로 바꿨을 때 SRS 요구(건너뜀 vs 거부) — 카드 범위 밖.
- 시간 수치는 한 기계·단일 실행. 비닝 속도 향상의 크기는 일반화하지 않는다.
- 런타임 검출 입력의 유한성을 호출 실험 도구 외 외부 호출자가 보장하는지(호출자 없음 확인까지만).

## 잔여 위험

- 이미 비유한 프레임을 넘기던 직접 호출자가 있다면 `INVALID_INPUT` 을 받는다(grep 으로는 없음).
- 알림 문구 두 개가 새 레인 간 계약이다(정규식 앵커 시험이 clients/ 에 생기면 영향).
