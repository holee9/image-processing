[2026-10-28 23:09 KST] [xpe-leader 대행 QA-A-316] 방법·입력 규칙 (결과 전에 커밋)

# QA-A-316 방법과 입력 규칙 (Refs #278)

코드 기준: `dev/preprocess` HEAD `e6568b17`, `build/ci-preprocess/bin/xpe_preprocess.dll` sha256 `0f15bda3…92d6`
(QA-A-315 `36_dll_sha_final.txt` 와 같음). 입력은 `batch-20261022-1230/manifest.json` 의 `cal_source` 6곳(NAS, 읽기만).

## 1. 결과 전에 확인한 입력 사실 (근거 파일)

- `11_facts.txt` (`10_facts.py.txt`): 세트별 파일 목록·평균·0/65535 개수·결함 수·BrightAvg 위치.
- `15_value_hist.txt` (`14_value_hist.py.txt`): 6세트 모두 밝은 프레임과 `BrightAvg.raw` 에 [20000, 32767] 값이 0개(s6 Bright01 만 1개)인데 32768 이상은 수만 개다.
- `17_signed_probe.txt` (`16_signed_probe.py.txt`): 밝은 프레임과 `BrightAvg.raw` 를 부호 있는 int16 으로 읽으면
  `BrightAvg == rint(mean(Bright01..10))` 가 6세트 모두 화소 100.0000 % 일치(불일치 0). uint16 으로 읽으면 96.47~98.12 %.
  → caleng `BrightAvg` 의 0·65535 는 별도 처리 규칙이 아니라 int16 값 0 과 −1 이다(측정 사실). 반올림은 rint(짝수 쪽) — 반올림 올림(half-up)은 94.9~95.0 %.
- 밝은 프레임은 offset 이 빠진 값이다: int16 으로 읽은 Bright01..10 에 음수가 0.35~1.66 % 있다(`17_…`). 다크 원값(`dark.raw`)에는 그런 값 구간 분포가 없다(`15_…`).
- `dark.raw` 는 세트마다 한 파일. 한 장인지 여러 장 평균인지는 파일만으로 **미확인**.

## 2. PRE 이득 맵 생성 — 입력 규칙

생성기: `xpe_calib_generate_gain` (`modules/preprocess/src/xpe_calib_generate_gain.cpp:216`), 공개 API 를 직접 부르는
임시 하니스(`qa316_gen.cpp`, 임시 폴더, 모듈·시험·도구 코드는 바꾸지 않음).

| 규칙 | 근거 |
|---|---|
| 프레임 = Bright01..10 (세트마다 10장) | 이 10장은 한 선량: 파일 이름 OC 최대/최소 1.0056~1.0320, 중심 ±200행 평균 비 1.0118~1.0494 (`11_facts.txt`). 생성기는 한 조건의 프레임을 요구한다(`preprocess_api.h:523-527`). 선량 묶음 기준 1.25(`xpe_bpm_v2.cpp:56`, `xpe_calib_fixture_gen.cpp` QA-A-250b) 안쪽. Bright11..20 은 선량이 섞여 이득에 쓰지 않는다. |
| `dark_reference = NULL` | 밝은 프레임은 offset 이 빠진 값(§1). 2G CalData 의 offset 이 빠진 평탄 프레임에 다크 기준 없이 이득을 만드는 선례: `tools/xpe_calib_fixture_gen.cpp:27-30`. |
| int16 값 v 를 uint16 max(0, v) 로 바꿔 넘긴다 | 생성기는 UINT16 만 받는다(`xpe_calib_generate_gain.cpp:287`). 원래 생성기는 (밝은 − 다크) 의 음수를 0 으로 바닥 처리한다(`:297-299`). 다크 기준이 NULL 이면 그 바닥 처리가 일어나지 않으므로, 같은 규칙(음수 → 0)을 호출 전에 적용한다. int16 −1 을 uint16 65535 로 넘기면 신호로 계산되어 생성기의 정의와 다르다. |
| `metadata_json = NULL` | 기본값(포화 검출 켬, `:369-372`). |

PRE 이득 정의(코드): G(x,y) = mean_i max(0, f_i − d) / 전 화소 평균 (`:296-322`). 다크 기준 NULL 이면 d = 0.

caleng 쪽 비교값(같은 정의로 유도): G_c = BrightAvg_int16 / mean(전 화소 BrightAvg_int16). caleng 은 음수를 바닥 처리하지 않고(int16 평균, §1) 정수로 반올림해 저장하므로 두 값의 차이는 (1) 프레임별 음수 바닥 처리 (2) 정수 반올림 (3) 정규화 평균의 차이에서 온다. 화소별 차이 G − G_c 의 중앙값·p1·p99·최대(절댓값)와 피어슨 상관을 전 화소와 BPMap 결함이 아닌 화소에서 각각 낸다. 우열 판정은 하지 않는다.

교차 확인: 생성된 `gain.xcal` 의 값이 numpy 로 같은 식을 계산한 값과 일치하는지(최대 절대 차) 함께 낸다 — 하니스 입력이 규칙대로 들어갔는지 확인하는 대조군.

## 3. PRE 결함 화소 맵 2판(`xpe_bpm_generate_v2`) — 입력 조건 확인

- 다크 프레임: 첫 장을 건너뛴 뒤(`xpe_bpm_v2.cpp:53`) 최소 5장(`:54`), 설정으로 낮춰도 최소 3장(`:70`), 부족하면 `XPE_ERR_INVALID_INPUT`(`:696`). 세트마다 다크는 1장이다.
- 다크·밝은 프레임 모두 offset 이 빠지지 않은 원값이어야 한다(`preprocess_api.h:2139-2141`). 2G 밝은 프레임은 offset 이 빠진 int16 값이다(§1).
- 같은 다크를 여러 번 넣거나 0 프레임을 다크로 넣는 것은 없는 측정값을 만드는 것이라 하지 않는다.
- 따라서 결함 맵 2판의 생성 비교는 **측정 불가**로 적는다. 하니스는 호출만 하여 반환 코드를 기록한다: (a) 기본 설정, 다크 1장 (b) `skip_first_dark = NONE`, `min_frames_dark = 3`, 다크 1장. 밝은 프레임은 Bright11..20 (다선량, 파일 그대로).
- 결함 맵에서 할 수 있는 사실 확인(caleng 쪽만): 결함 수, 전폭 가로줄 위치(`11_facts.txt`).

## 4. offset 생성 비교

`dark.raw` 한 장뿐이고 다크 여러 장이 없으므로 offset 생성(`xpe_calib_generate_offset`) 비교는 **측정 불가**.

## 5. 임시 폴더·증거

임시 폴더 `D:/workspace-github/xpe-data/qa-316-tmp` 하나. 생성한 `gain.xcal` 은 SHA-256 만 남기고 지운다. 하니스 소스는 증거로 `.txt` 로 남긴다.
