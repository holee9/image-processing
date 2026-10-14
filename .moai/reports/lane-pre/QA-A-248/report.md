[2026-10-14 14:00 KST] [xpe-pre]

# QA-A-248 — CI 에서 보정 세트를 만드는 경로 (#262)

## 1. 결과

| 항목 | 결과 |
|---|---|
| 카드의 전제 | **틀렸다.** `tests/test_data/CalData_6` 에서 추적되는 파일은 `README.md`, `fixture.json` 두 개뿐이다. 입력 프레임은 저장소에도 CI 에도 없다 (§2) |
| 입력과 출력 위치를 인자로 받는 명령 | `xpe_calib_fixture_gen --out <디렉터리> --caldata <CalData_6 폴더> [--expiry-ms N]` (기존 도구에 모드 추가) |
| calib-real-v2 와의 관계 | 세 파일 모두 **페이로드가 바이트 동일**. 헤더만 다르다 (§3) |
| 앱 경로 대조 | 이 명령으로 만든 세트를 앱 호출 순서로 돌리면 출력이 기준 영상 v2 와 바이트 동일 (5회 모두, `22_`) |
| 결정성 | 실데이터 모드: 같은 입력 2회 생성 시 세 파일과 매니페스트 모두 바이트 동일. 합성 3072² 도 2회 동일 |
| 시간 | 실데이터 모드 **1.7초** (1752, 1707 ms). 합성 3072² 는 **약 6분** (352, 356, 354 s) |
| 크기 | 산출 84.9 MB (offset 37.7 + gain 37.7 + defect 9.4). 실데이터 모드가 읽는 입력 5개도 84.9 MB. 합성은 입력이 없고 최대 작업 메모리 375 MB |
| 스텝 초안 | `evidence/40_..._A_real_caldata.yml.txt`, `evidence/41b_..._B_synthetic.yml.txt` (둘 다 `ci.yml` 미수정) |

**리더가 정할 것 두 가지.** (1) 실데이터로 가려면 입력 5개(84.9 MB)를 CI 러너에 어떻게 올릴지. (2) W14 는 앱이 저장한 영상을 1단계 기준 영상과 바이트 단위로 대조하는데, 그 기준 영상(37.7 MB)도 저장소 밖이다 (§6). 합성 세트로는 W14 의 이 단언이 통과할 수 없다.

## 2. 카드 전제의 정정

이슈 #262 와 카드는 "CI 가 저장소 안 입력 `tests/test_data/CalData_6` 으로 보정 세트를 만든다"고 적었다. 확인한 사실:

- `git ls-files` 의 `CalData_6` 항목은 `README.md`, `fixture.json` 이다.
- `dark.raw`, `bright01~06.raw` 는 `.gitignore:290` (`tests/test_data/**/*.raw`) 에, `BPMap.map` 은 `.gitignore:292` (`**/*.map`) 에 걸린다. 로컬 `main` 체크아웃에는 파일이 있고, 저장소에는 없다.
- `ci.yml` 에 `CalData`, `test_data` 참조가 없고 LFS 도 쓰지 않는다 (`git lfs ls-files` 출력 없음).
- 반대로 손목 영상 `gui/ImageProcTest/fixtures/gui-s0/raw/wrist_lat_3072x3072.raw` (18,874,368 B) 는 추적된다.

리더가 13:08 에 이 정정을 확인했다.

## 3. 기존 경로와 새 모드, 같은 점과 다른 점

기존 경로: `modules/preprocess/tests/test_zz_a241_measure.cpp` 의 `A241Measure.DISABLED_A245_ExportAndAppPath` (1346행 시작). 오프셋은 `xpe_calib_generate_offset(&darkBuf, 1, 100.0f, 25.0f, ...)` (1392행), 게인은 `xpe_calib_generate_gain(three, 3, nullptr, ...)` (1395행, 보정 영상 `bright04~06`, 암전류 참조 없음), 결함 맵은 `BPMap.map` 을 `write_xcal_file` 로 그대로 쓴다.

새 모드는 같은 생성기를 같은 인자로 부른다. 코드는 `modules/preprocess/tools/xpe_calib_fixture_gen.cpp` 에 있고, 입력을 먼저 전부 읽은 뒤에 쓰기 시작한다.

| 항목 | calib-real-v2 (`DISABLED_A245_…`) | `--caldata` 모드 | 같은가 |
|---|---|---|---|
| offset 페이로드 | 37,748,736 B | 37,748,736 B | **바이트 동일** (`21_`) |
| gain 페이로드 | 37,748,736 B | 37,748,736 B | **바이트 동일** |
| defect 페이로드 | 9,437,184 B | 9,437,184 B | **바이트 동일** |
| 파일 크기 | offset 37,748,950 / gain 37,749,092 / defect 9,437,336 B | 37,748,888 / 37,748,888 / 9,437,336 B | offset −62, gain −204 (config JSON 이 없다) |
| `created_epoch_ms` | 생성 시각 (offset 1791357017482, gain …610) | 0 (고정) | 다름 — 결정성을 위해 |
| `expiry_epoch_ms` | 1822893017654 (생성 +1년) | `--expiry-ms` 값, 기본 0 | 다름 |
| session id | `generated` | `fixture` | 다름 |
| config JSON | offset 62 B, gain 204 B (생성기가 넣는 메타) | 없음 | 다름 |

필드별 값은 `evidence/24_header_fields_ours_vs_calib_real_v2.txt`. config JSON 을 읽는 코드는 압축 메타데이터 용도만 확인했고 (`xcal_reader.cpp:8`, `:195`), 모든 경로를 추적하지는 않았다.

**헤더 차이가 앱에서 보이는 경고를 바꾼다.** 같은 손목 영상에 두 세트를 적재하고 앱의 단계 순서로 돌렸을 때 모듈이 올리는 경고 (`evidence/23_`, `32_`):

| 세트 | 경고 |
|---|---|
| calib-real-v2 | `XPE_WARN_CALIB_SESSION_UNSPECIFIED`, `XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT` (44053 화소, 0.467 %), 비선형성 미적용 — 3개 |
| `--caldata`, 만료일 0 (기본) | `XPE_WARN_NO_EXPIRY` ×3, `GAIN_PIXELS_CLASSIFIED_DEFECT`, 비선형성 미적용 — 5개 |
| `--caldata --expiry-ms 4102444800000` | `GAIN_PIXELS_CLASSIFIED_DEFECT`, 비선형성 미적용 — 2개 |

그래서 CI 스텝에는 `--expiry-ms 4102444800000` (2100-01-01) 을 권한다. 고정 값이라 파일이 매번 바이트 동일하고, "지금 + 1년" 처럼 시간이 지나면 터지는 값이 아니다 (QA-A-244 와 같은 종류의 함정). 생성본의 session id 는 `fixture` 라서 `SESSION_UNSPECIFIED` 경고가 나지 않는다. 그래서 calib-real-v2 쪽에만 경고가 하나 더 있다.

## 4. CI 한 스텝에서 부르는 명령

```
xpe_calib_fixture_gen --out <출력 디렉터리> --caldata <CalData_6 폴더> --expiry-ms 4102444800000
```

- `--caldata` 폴더에서 읽는 파일: `dark.raw`, `bright04.raw`, `bright05.raw`, `bright06.raw`, `BPMap.map`. `--width/--height` 를 주지 않으면 3072 × 3072 로 본다. 크기가 맞지 않으면 쓰기 전에 종료 코드 9 로 끝난다.
- `--gain-poly` 와 같이 쓰면 거부한다 (종료 코드 2, 실행해 확인). 입력 폴더나 파일이 없어도 종료 코드 9 (실행해 확인) 이고 `.xcal` 은 생기지 않는다.
- 매니페스트에 `"source": "caldata"` 와 입력 5개의 sha-256 이 추가된다. 합성 실행의 매니페스트는 바이트 동일이다 (변경 전후 비교, `10_` ↔ `11_`).
- 입력은 `bright04~06` 으로 고정했다. `CalData_6` README 의 정정에 따라 `bright01~06` 의 번호와 선량 순서가 다르고, 1단계 기준 영상 v2 가 이 셋으로 만들어졌기 때문이다.

**결정성** (`20_`, `23_`): 같은 입력으로 2회 생성했을 때 offset, gain, defect, 매니페스트 모두 바이트 동일. 만료일 0 과 2100 두 경우 모두. 복원한 빌드(반증 검사 뒤)로 다시 만든 세트도 이전 바이트와 같았다.

## 5. 시간과 크기

| 경로 | 시간 | 산출 | 입력 | 메모리 |
|---|---|---|---|---|
| 실데이터 `--caldata` | 1752, 1707 ms (프로세스 전체, 2회) | 84.9 MB | 84.9 MB (5개 파일) | 측정 안 함 |
| 합성 3072² | 352080, 356462, 353967 ms (3회. 셋째는 파일 쓰기 같은 가벼운 작업과 겹쳤고 값은 같은 수준) | 84.9 MB | 없음 | 최대 375 MB (둘째 실행 중 관측) |

`ci.yml` 의 `gui-e2e-native` 는 제한 45분이고 최근 약 25분이다. 합성 생성이 약 6분 더해진다. 러너에서의 시간은 재지 않았다 (이 기계의 값이다). 합성 생성 시간이 어느 단계에 쓰이는지도 재지 않았다.

## 6. 두 갈래의 비교 — 합성 세트로 충분한가의 근거

두 갈래의 스텝 초안을 만들었다. 어느 쪽이 맞는지는 리더가 판단한다. 판단에 필요한 사실:

- **W15 (U8)**: 중앙값 ≤ 3000 ms 단언만 한다 (`QuickStartWalkthroughScenarios.cs`, dev/gui `64c9a89e`). 보정 세트의 내용이 판정에 들어가지 않는다. 합성 세트에서도 돌 수 있다. 다만 시간이 세트 내용과 무관한지는 확인하지 않았다.
- **W14 (U2)**: 마지막에 앱이 저장한 float 영상의 sha256 을 기준 영상과 비교한다 (같은 파일 965~972행 부근). 기준 영상은 `XPE_C231_BASELINE_F32` 또는 보정 폴더의 상위에 있는 `m1-baseline-v2/wrist_lat_3072x3072_corrected_v2_f32le.raw` 다. 이 37.7 MB 파일도 저장소에 없다. **합성 세트로는 이 단언이 통과할 수 없다.**
- 실데이터 세트는 모듈 쪽에서 이 기준 영상을 바이트 단위로 재현한다 (`22_`). 그래서 기준 영상을 CI 에서 모듈로 만들어도 되지만, 그러면 모듈에 대해서는 순환 비교가 되고 앱이 화소를 바꾸지 않았는지만 확인한다.
- 앱 화면이 보는 경고도 다르다. 실데이터 세트는 `GAIN_PIXELS_CLASSIFIED_DEFECT` 를 올리고 합성 세트는 올리지 않는다 (`32_`). W14/W15 의 단언 목록에는 이 경고 줄을 확인하는 것이 보이지 않았다. 다만 앱 HUD 의 "Calib:" 줄 내용은 확인하지 못했다.

## 7. 변경과 검증

변경: `modules/preprocess/tools/xpe_calib_fixture_gen.cpp` (모드 추가), `modules/preprocess/tests/test_calib_fixture_gen.cpp` (시험 4개). 제품 코드 변경 없음. 합성 경로는 바이트 동일 (`10_` ↔ `11_`: 기본 설정의 세 파일과 매니페스트 동일, `--gain-poly` 의 `gain_poly.xcal` 은 변경 전후가 헤더 시각 3바이트 (24~26번) 만 다르다. 패치된 실행 파일을 두 번 돌려도 같은 3바이트가 달라지므로 제품 생성기의 시각 도장 때문으로 보지만, 변경 전 실행 파일을 두 번 돌려 확인하지는 않았다. 나머지 바이트는 같다).

새 시험 (`CalibFixtureGenTest`, 실데이터 없이 256×256 모의 폴더로 돈다):

1. `CalDataModeBuildsTheSetFromTheGivenFramesAndIsByteIdenticalBetweenRuns` — 2회 바이트 동일, defect.xcal 의 끝이 `BPMap.map` 과 같음, 매니페스트 키, 모듈이 세 파일을 적재함
2. `CalDataModeRefusesAMissingOrWronglySizedInputAndWritesNoMap` — 없는 파일, 1바이트 긴 파일, 짧은 맵, 기본 크기(3072) 기대. 네 경우 모두 종료 코드가 0 이 아니고, 이유를 stderr 에 말하고, `.xcal` 이 생기지 않음
3. `CalDataModeCannotBeCombinedWithAGainPolynomial`
4. `TheSyntheticManifestCarriesNoCalDataKeys`

**반증** (`51_mutation_log.txt`, 예측을 먼저 기록, 변형마다 빌드·시험, 끝에 `touch` 후 재빌드, 소스 해시 `2cc55a37…` 가 처음과 같음):

| 변형 | 예측 | 결과 |
|---|---|---|
| M1 결함 맵을 0 으로 덮어씀 | 1번 빨강 | 1번 빨강, 나머지 8개 초록 |
| M2 크기 검사 `!=` → `<` | 2번 빨강 | 2번 빨강 |
| M3 기본 크기 3072 → 1024 | 2번 빨강 (메시지의 기대 바이트 수) | 2번 빨강 |
| M4 `--gain-poly` 거부 해제 | 3번 빨강 | 3번 빨강 |
| M5 매니페스트 키가 합성에도 나옴 | 4번 빨강 | 4번 빨강 |

M3 은 처음 설계에서는 잡히지 않는 시험이었다. 기본값이 1024 여도 256² 프레임을 똑같이 거부하기 때문이다. 오류 메시지의 기대 바이트 수 (`expected 18874368`) 를 단언하도록 고쳐서 잡게 했다.

| 검증 | 결과 | 증거 |
|---|---|---|
| `CalibFixtureGenTest.*` | 9 통과 | `50_` |
| `xpe_preprocess_tests` 단일 프로세스 | 1048 통과 (이전 1044 + 4) | `30_` |
| ctest 순차 | 1290 개 중 실패 0, 100 % | `31_ctest_summary.txt` |
| clang-tidy 기준선 게이트 (main 의 `check_clang_tidy.py`) | `GATE_EXIT=0` | `41_` |
| 도구 파일에 직접 clang-tidy | 변경 전 14건, 변경 후 14건 (줄 번호를 빼면 같은 지적) | `42_` |

게이트 `GATE_EXIT=0` 은 `modules/<모듈>/src/*.cpp` 만 보므로 `tools/` 와 `tests/` 의 변경에 대해서는 말하지 않는다. 그래서 도구 파일에는 따로 돌렸다. 처음에는 새 분기가 `--out` 분기와 같은 모양이라 `bugprone-branch-clone` 이 하나 늘었고, 두 분기를 합쳐서 없앴다. 시험 파일에는 clang-tidy 를 돌리지 않았다. doxygen 은 헤더 변경이 없어 돌리지 않았다. cppcheck 는 이 기계에 없다.

## 8. 못 본 것과 남는 위험

- **러너에서의 실행.** 시간, 메모리, 해시 모두 이 기계의 값이다. 초안의 해시 고정 가드는 러너의 CPU·컴파일러에서 float 마지막 비트가 같은지 확인하지 않은 채 쓰였다. 첫 CI 실행이 이것을 정한다.
- **`xpe-ci-preprocess-binaries` 아티팩트에 생성기 실행 파일이 들어 있는지.** 아티팩트 경로는 `build/ci-preprocess/bin/` 이고 로컬에서는 그 폴더에 있다. CI 아티팩트 내용은 보지 못했다.
- **입력 공급 방식.** 입력 5개를 러너에 올리는 방법은 정하지 않았다 (리더 결정).
- **합성 생성 시간의 구성.** 6분이 어느 단계인지 재지 않았다.
- **W14/W15 를 실제로 돌린 결과.** gui 소유라 돌리지 않았다. §6 은 코드를 읽은 것이다.
- **config JSON 이 없는 파일의 다른 소비처.** 읽는 코드 일부만 확인했다 (§3).

## 9. 변경 파일

`modules/preprocess/tools/xpe_calib_fixture_gen.cpp`, `modules/preprocess/tests/test_calib_fixture_gen.cpp`. 증거: `.moai/reports/lane-pre/QA-A-248/`.

## Card Cross-Check

| milestone | card |
|---|---|
| #262 닫는 조건 — CI 가 입력으로 보정 세트를 만들어 U2·U8 시험을 실제로 돌림 | QA-A-248 (생성 경로와 초안까지. 입력 공급 방식과 W14 기준 영상은 리더 결정으로 남음) |
