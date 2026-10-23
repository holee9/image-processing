[2026-10-17 17:38 KST] QA-A-259 보고서 — 첫 다크 제외를 생성 도구 한 곳에서 한 번만

이슈 #273. 기반 QA-A-258 `58b40297`. 방법과 예측은 구현 전에 커밋했다(`bd199761`), 구현·시험·반증 설정 `ca16af4c`. 증거는 `.moai/reports/lane-pre/QA-A-259/evidence/`. **이 보고서는 B16 합격 근거가 아니다.**

## 1. 무엇이 바뀌었나

| 단계 | 이전 | 이후 |
|---|---|---|
| 사용자가 넣는 목록 | 첫 장을 뺀 `Dark_08~14` (문서 안내) | 첫 장을 포함한 전체 `Dark_07~14` |
| 첫 장 제외 | 결함 맵 2판의 라이브러리 안에서 한 번 더 (offset·게인은 받은 7장 전부 사용) | 생성 도구가 **한 곳에서 한 번** 뺀다 |
| offset | 받은 목록 전부 | 제외 뒤 같은 목록 |
| 게인 다크 참조 (`--gain-dark-ref mean`) | 받은 목록 전부 | 제외 뒤 같은 목록 |
| 결함 맵 1판·2판 | 1판은 받은 목록 전부, 2판은 한 장 더 제외(6장) | 둘 다 제외 뒤 같은 목록. 2판 호출은 `skip_first_dark = XPE_BPM_V2_SKIP_NONE` |
| 옵션 | 없음 | `--keep-first-dark`: 목록을 그대로 쓴다. 기본은 제외 |
| 다크 1장 | 최소 장 수(5) 검사만 | 첫 장 제외 뒤 0장이라 "first dark" 를 명시한 오류로 거부(종료 코드 10), 출력 폴더 미생성 |
| manifest | `frames.dark` 와 입력 목록 | 추가로 `dark_policy`: `first_dark_excluded`, `one_list_for_all_stages`, `stages`, `given`(경로·sha256), `used`(경로·sha256); 제외된 장은 inputs 에서 role `dark_excluded_first`; 2판은 `dark_frames_used`·`skip_first_dark: false` |

라이브러리의 기본값(첫 장 제외)과 평가 도구의 `--skip-first-dark` 는 바꾸지 않았다. 바뀐 파일은 `modules/preprocess/tools/xpe_calib_fixture_gen.cpp` 와 `modules/preprocess/tests/test_calib_fixture_gen.cpp` 둘이다. 이전 방식으로 만든 세트와 호환되지 않는 점: 같은 입력 목록에 대해 기본 동작의 offset·게인이 이전과 다를 수 있다(이전은 받은 목록 전부를 썼다). 첫 장을 뺀 7장 목록을 직접 주던 호출은 기본에서 한 장이 더 빠지므로 `Dark_08~14` 를 주던 문서의 절차는 바꿔야 한다(§6).

## 2. 시험 (예측 T1~T5)

- `FirstDarkIsExcludedOnceAndAllThreeStagesReadTheSameList`: 첫 장만 다른(수준 +250, 그 장에만 있는 고온 화소) 7장 목록의 기본 실행이, 손으로 첫 장을 뺀 6장 목록을 `--keep-first-dark` 로 준 실행과 offset·gain·defect 세 파일이 **바이트 동일**하다. 결함 맵 1판과 2판 각각. 2판은 manifest 의 `dark_frames_used` 가 6.
- `KeepFirstDarkUsesTheListAsGivenAndEveryStageNoticesTheDifference`: 같은 7장 목록의 기본과 `--keep-first-dark` 는 세 파일이 모두 **다르다**(각 단계가 첫 장에 반응한다는 점 확인 — 위 시험이 맹목이 아니라는 대조).
- `ManifestListsTheGivenAndTheUsedDarks`: `given` 7, `used` 6, 첫 장은 `given` 에만; `--keep-first-dark` 에서는 둘 다 7.
- `OneDarkFrameIsRefusedBecauseTheFirstIsExcluded`: 1장은 종료 코드 10 + 메시지 "first dark" + 출력 폴더 없음; 5장은 "too few dark frames: 4 usable (5 given"; `--keep-first-dark` 5장은 통과.
- 기존 frames 모드 시험은 `--keep-first-dark` 를 붙여 목록 전체 사용이라는 원래 의미를 그대로 시험한다. 처음 돌렸을 때 기존 시험 3개가 깨졌다(manifest 의 sha256 개수를 inputs 밖의 `dark_policy` 까지 센 것 2개, 목록 파일 시험의 인자 한 줄이 옵션 없이 기본 규칙을 탄 것 1개). 시험의 기대를 고쳤고(개수는 inputs 블록 안에서 센다) 제품 동작은 바꾸지 않았다.
- 결과: `CalibFixtureGenTest.*` **46개 통과**(`30_fixture_gen_tests.txt`), 이 도구를 쓰는 `BpmEvalTest` 4개 통과(`31_bpm_eval_tests.txt`).

## 3. 반증 (변이) — 예측과 실제 (`40_mutation.py.txt`, `41_mutation_log.txt`)

| 변이 | 예측 | 실제 |
|---|---|---|
| M1 2판 호출에서 라이브러리가 한 장 더 뺌(옛 이중 제외) | 첫 시험 빨강 | 빨강 (첫 시험) |
| M2 offset 단계만 첫 장 포함 | 첫 시험 빨강 | 빨강 (첫 시험, 그리고 둘째 시험) |
| M3 게인 다크 참조 평균만 첫 장 포함 | 첫 시험 빨강 | 빨강 (첫 시험, 그리고 둘째 시험) |
| M4 manifest `used` 에 첫 장 경로 | manifest 시험 빨강 | 빨강 (manifest 시험) |
| M5 1장 거부 제거 | 1장 시험 빨강 | 빨강 (1장 시험) |
| M6 1판 결함 맵만 첫 장 포함 | 첫 시험 빨강 | 빨강 (첫 시험, 그리고 둘째 시험) |

M2·M3·M6 에서 둘째 시험(`KeepFirstDark…`)도 빨개진 것은 예측에 없던 부수 결과다: 그 변이가 해당 단계에서 "제외"와 "유지"의 차이를 없애기 때문에(제외 실행에서도 첫 장을 쓰므로) 두 실행이 같은 바이트를 낸다. 시험이 단계별 반응을 보고 있다는 뜻이다. 모든 변이는 소스 해시 복원(`03986448d350e513` 시작·종료 동일) 뒤 다시 빌드해 46개 통과를 확인했다. 생존한 변이는 없다.

## 4. 실세트 1회 (측정)

입력: cyan_test 다크 `Dark_07~14` **8장 전체**(목록 `50_darks.txt`), 게인 `Bright_23~28` 6장, 결함 맵 `Bright_17~28`·`31~36` 18장, `--bpm-v2 --bpm-margins 6,2,1,1`, `--expiry-ms 4102444800000`, 기본 규칙. 명령 `53_real_set.ps1.txt`.

| 항목 | 값 | 예측 |
|---|---|---|
| 실제 쓴 다크 | `Dark_08~14` 7장(`dark_policy.used`), 넣은 목록 8장(`given`), 제외 `Dark_07` (role `dark_excluded_first`) | 일치 |
| 세 단계 | `one_list_for_all_stages: true`; 2판의 `dark_frames_used` 7, `skip_first_dark: false` | 일치 |
| offset sha256 | `5569bacf66fb25f10b9137ebf3f466e9ce8d94857c90dc4a7340df362c290c39` | IAP 1.5 의 `5569bacf…` 와 **일치** |
| gain sha256 | `a7ddadbd5b69a350c44c638fd8887096ca4cb448eda322e5d8976575c7520d22` | IAP 1.5 의 `a7ddadbd…` 와 **일치** |
| defect sha256 | `4579e06bcf5be09828935eb48c71bd78d1f51afc9fc97f83e678158dfa2f105c` (결함 화소 2,353) | 바뀜 — IAP 1.5 세트의 `3b6fe9ba…` 는 도구가 결함 맵에서 한 장을 더 빼 6장으로 만든 것(그 manifest: `skip_first_dark: true`) |
| `used` 의 sha256 | IAP 1.5 세트의 `Dark_08~14` 일곱 장의 sha256 과 같은 순서로 동일 | 일치 |

자원 (리더 요청 6): 종료 코드 **0**, 실행 **149.2 초**, 최대 작업 집합 **2,002 MiB**(약 2,099 MB), 입력 파일 **26개**(다크 8 + 게인 6 + 결함 맵 전용 12; 결함 맵 18장 중 6장은 게인과 같은 파일이라 한 번만 읽음), 해상도 **3072 × 3072**. 이 값은 생성 도구 전체의 작업 집합이며 제품 런타임의 PERF-002(200 MB)와 같은 측정량이 아니다. 증거 `55_real_set_resources.txt`, `54_real_set_stdout.txt`, `56_real_set_manifest_check.txt`.

## 5. QA-A-258 보고서의 메모리 정정 (리더 요청 5)

258 보고서 §7 을 고쳤다: 1,903 은 MB 가 아니라 **MiB(약 1,995 MB)**, 새 통계의 추가 배열 264 MB 는 **252 MiB**(28 B × 9,437,184 = 264,241,152 B)이며, 둘 다 평가 도구·생성 도구 전체의 값이라 PERF-002 와 같은 측정량이 아니다. 비어 있던 종료 코드는 측정 스크립트가 프로세스 핸들을 잡지 않아 생긴 결함이어서, 핸들을 잡도록 고친 스크립트로 다시 측정했다: 종료 코드 0, 48.1 초, 1,903 MiB(`QA-A-258/evidence/71_peak_memory.txt`; 첫 측정 파일은 이름을 바꿔 보존).

## 6. IAP 문서 정정 문장 초안 (적용은 리더가 xpe-doc 에 넘김)

대상: `docs/calibration/IAP-CALIB-001_Image_Acquisition_Protocol.md` 의 다크 목록 안내와 검증된 예.

1. 다크 목록 안내 문장 (현재 "첫 장을 제외한 `Dark_08~14` 를 넣는다"는 취지의 문장을 대체):
   > 다크 목록에는 촬영한 **전체 다크 영상을 첫 장까지 포함해** 취득 순서대로 넣는다(예: `Dark_07~14`). 첫 장은 검출기가 안정되기 전에 찍힌 영상이므로 생성 도구가 목록의 첫 장을 **한 번만** 제외하고, 남은 같은 목록을 offset 지도, 게인의 다크 참조, 결함 맵(1판·2판)에 똑같이 쓴다. 사용자가 첫 장을 직접 빼서 넣으면 도구가 한 장을 더 빼므로 빼지 않는다. 이미 안정된 영상만 담은 목록은 `--keep-first-dark` 로 목록 전체를 쓰게 할 수 있다.
2. 검증된 예 (cyan_test) 문장 (현재 "다크 `Dark_07~14`(첫 장 제외 규칙 이전에 만든 세트. 이 규칙을 따르면 `Dark_08~14`)"를 대체):
   > 다크 `Dark_07~14` 8장 전체를 넣는다. 도구가 `Dark_07` 을 제외하고 `Dark_08~14` 7장을 offset·게인·결함 맵에 쓰며, 이때 offset sha256 은 `5569bacf…`, gain 은 `a7ddadbd…` 로 이전 세트(`calib-cyan-v3` 의 `Dark_08~14` 7장)와 같다. 결함 맵은 이전에 한 장을 더 뺀 6장으로 만들었기 때문에 sha256 이 `3b6fe9ba…` 에서 `4579e06b…` 로 바뀐다.
3. 확인 방법 문장 (manifest 확인):
   > 세트 폴더의 `manifest.json` 의 `dark_policy.given`(넣은 목록)과 `dark_policy.used`(실제로 쓴 목록)를 확인한다. `used` 는 `given` 에서 첫 장만 빠진 목록이어야 하고, `one_list_for_all_stages` 가 `true` 여야 한다. 2판 결함 맵의 `defect_format.dark_frames_used` 는 `used` 의 장 수와 같다.
4. 촬영 요청서와 정합: 촬영 요청서의 "첫 장도 저장하되 표시"와 이 규칙은 같은 방향이다(첫 장을 지우지 않고 목록에 넣는다).

## 7. 정리

- 주장: 첫 다크 제외는 생성 도구의 한 곳(트림 한 번)이 되었고 offset·게인 다크 참조·결함 맵(1판·2판)이 같은 목록을 쓴다. QA-A-256 의 6장 관측(0.578)과 QA-A-257 의 7장 대조(0.766) 차이의 원인이던 단계별 다크 불일치가 이 경로에서 사라졌다는 것까지는 증거(바이트 동일 시험, manifest `dark_frames_used`, 실세트의 일곱 장)로 확인했다.
- 증거: 위 파일들과 커밋 `bd199761`(방법·예측), `ca16af4c`(구현·시험·반증 설정).
- 미검증: 새 규칙으로 만든 세트로 다시 잰 잡음 검출률(이 카드의 범위가 아니다). `--keep-first-dark` 를 쓰는 실세트(옵션 경로는 시험에서만 확인). GUI 와 앱 쪽 호출 경로가 문서의 새 절차를 따르는지(레인 C 몫). 실세트는 한 번만 만들었고 반복 변동은 재지 않았다(두 번째 실행의 바이트 동일성은 합성 시험이 확인한다).
- 잔여 위험: 이전 규칙으로 만든 세트와 같은 입력으로 만든 세트의 offset·게인이 다를 수 있다(목록을 첫 장 포함으로 주면 이전과 같은 해시, 첫 장을 뺀 목록을 주면 한 장 더 빠짐). 문서와 앱 안내가 바뀌기 전에 새 도구를 배포하면 `Dark_08~14` 를 넣는 사용자는 6장으로 만든 세트를 받는다.
