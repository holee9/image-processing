# QA-B-82 (#177) 게이트 보고서 — 부위 VOI 프리셋을 DN 임시 전 범위로 · DI 경보 문턱 판독

**카드**: QA-B-82 · **레인**: Lane B (`xpe-post`, `dev/postprocess`)
**병합**: `git merge main` (fast-forward → `83a7c2d`, REQ-DISP-017 개정문 `889635c` 포함 확인)
**커밋**: `a4253be` (`Refs #177`) — display 5파일 · push 없음
**BUILD_EXIT**:
- 세 프리셋 모두 `BUILD=0`
- `AI_EXIT=0` 224/224, `DICOM_EXIT=0` 192/192
- **`POST_EXIT=8` — 534건 중 1건 실패(`T308_PerformanceBudget`, §5).** 재실행해도 같은 1건만 실패

---

## 1. 주장

| # | 항목 | 결과 |
|---|---|---|
| (1)-1 | 네 프리셋 → 32768/65535 | 완료 (`voi_lut.cpp`, case 4개를 하나로 묶음) |
| (1)-2 | 공개 헤더 enum 주석 + `xpe_voi_preset_create` Doxygen | 완료 (DN 영역, 임시값, #151·#177) |
| (1)-3a | `test_voi_lut.cpp` 값 단언 4건 | 32768/65535 로 갱신 |
| (1)-3b | `test_parameter_dependency.cpp` | "다르다" → **"지금은 같다"** 로 뒤집고 이름을 `KnownDivergence_VoiPresetIgnoresBodyPartUntil151` 로 바꿈. 주석에 #151·#177 |
| (1)-3c | `test_display_integration.cpp` BonePreset | **결과가 바뀌었다** — 입력 500 의 출력 127.5 → **1.94**(오차 125.56). 입력을 새 중심 32768 로 옮겨 "중심 → 127.5" 의도를 유지 |
| (1)-3d | 루트 `tests/e2e_post_pipeline` | **단언 결과는 바뀌지 않았다** — 4/4 통과. 이 파일은 BONE 프리셋 출력 화소를 단언하지 않는다(시간·메모리·EI 불일치만). 파이프라인 출력 화소 자체는 바뀌었을 것이나 재지 않았다. 고치지 않았다 |
| (1)-4 | 동작 시험 | `VoiLut.Preset_DnRampIsNotCrushed` |
| (1)-5 | 대조군(수정 전 HEAD) | **네 프리셋 모두 빨강** |
| (1)-5 | 반증(ABDOMEN → 40/400) | **동작 시험 + `Preset_Abdomen` 값 시험, 2건 빨강** — 카드 예측("4번만")과 다름, §3 |
| (2) | `diAlertThreshold` 를 읽는 곳 | **없음** — 컴파일러 확인, 대조군 포함. **enhance_advanced 에는 DI 경보 평가 코드가 없다.** enhance_basic 은 리터럴 `3.0f` 로 평가한다 |
| 부수 | GUI W-07 시나리오 | **Native 모드에서 옛 값 `C=-600`, `W=1600` 을 단언한다** — 이 변경으로 깨질 것으로 보인다(Lane C 소유, 고치지 않음, §4) |

## 2. 동작 시험 — 대조군과 수정 후

DN 0..65535 램프(256×256, 화소 하나당 DN 값 하나)에 프리셋을 적용하고, 출력을 프리셋의 출력 범위에서 256단계로 양자화했다.

단언:
- 서로 다른 단계 수 ≥ 128
- 가장 많은 화소가 몰린 단계의 점유율 ≤ 0.5

**대조군 — 수정 전 값** (`_control_before.log`, `BUILD=0`)

```
BONE    c=500  w=2000 | distinct 8-bit levels=192 | largest level share=0.977158
LUNG    c=-600 w=1600 | distinct 8-bit levels=33  | largest level share=0.996994
ABDOMEN c=40   w=400  | distinct 8-bit levels=154 | largest level share=0.996338
HEAD    c=40   w=80   | distinct 8-bit levels=81  | largest level share=0.998779
[  FAILED  ] VoiLut.Preset_DnRampIsNotCrushed
```

- **네 프리셋 모두 점유율 단언에서 빨강**이다.
- **단계 수만 봤다면 BONE(192)·ABDOMEN(154)은 통과했을 것이다.** 좁은 창도 전 범위 램프에서는 수백 단계를 낸다. 뭉침을 잡는 것은 **한 단계로 몰린 비율**이다. 그래서 두 가지를 함께 단언했다.

**수정 후** (`_after.log`)

```
BONE/LUNG/ABDOMEN/HEAD c=32768 w=65535 | distinct 8-bit levels=256 | largest level share=0.00392151
[       OK ] VoiLut.Preset_DnRampIsNotCrushed
```

## 3. 반증 (`_falsify.log`)

`voi_lut.cpp` 에서 ABDOMEN 만 떼어 40/400 으로 되돌렸다. `BUILD=0`.

```
ABDOMEN c=40 w=400 | distinct 8-bit levels=154 | largest level share=0.996338
[  FAILED  ] VoiLut.Preset_Abdomen
[  FAILED  ] VoiLut.Preset_DnRampIsNotCrushed
PD_EXIT=0   INT_EXIT=0   E2E_EXIT=0
```

- 카드는 "4번 시험만 빨강"을 예측했지만, **값 단언 `Preset_Abdomen` 도 함께 빨갛다.** 같은 값을 상수로도 단언하고 있으니 당연한 결과다.
- 동작 시험 외에 빨개진 것은 그 값 단언 하나뿐이다. `test_parameter_dependency` 는 ABDOMEN 을 보지 않아 초록이다.
- 원복 후 `grep -c 'B82 falsification'` → 0 을 확인했다.

## 4. 소비자 — Lane C 파일 (고치지 않음, 보고)

`clients/ImageProcTest.E2ETests/Scenarios/Workflows/WorkflowScenarios.cs` 의 **W-07**:

```csharp
var (center, width) = _app.BackendMode == "Native"
    ? ("C=-600", "W=1600")      // modules/display/src/voi_lut.cpp XPE_BODY_LUNG (HU)
    : ("C=25000", "W=50000");   // MockXpeBackend.cs XpeBodyPartEnum.Lung
```

- 이 변경 뒤 Native 백엔드의 Lung 프리셋은 `C=32768 W=65535` 다. **Native 모드에서 W-07 이 실패할 것**으로 판독된다. 실행하지는 않았다.
- 모든 부위가 같은 값이 되었으므로, "부위를 고르면 창이 바뀐다"는 이 시나리오의 전제도 Native 에서는 성립하지 않는다.
- `gui/…/RealXpeBackend.cs` 는 네이티브 값을 그대로 읽는다(`:204`). 하드코딩된 값은 없다.

## 5. `T308_PerformanceBudget` 실패

- 검증 로그: `_verify.log`, `_post_rerun.log`, `_t308_isolated.log`
- 문턱: `EXPECT_LT(duration, 100)` ms (`test_edge_enhancement.cpp:636`)

| 측정 | 값 |
|---|---|
| 전체 ctest (2회) | 2회 모두 이 1건만 실패 |
| 단독 1차 3회 | 101 / 101 / 100 ms → 3회 FAILED |
| 단독 2차 5회 | 100 / 98 / 96 / 99 / 99 ms |
| 참고: QA-B-76 때 단독 3회 | 92 / 92 / 93 ms, 통과 |
| 측정 시 CPU 부하 | 14% (`Win32_Processor.LoadPercentage`) |

- 이 카드의 커밋은 `modules/display/` 만 바꿨다.
- (2) 측정 중 `detail/exposure_index.h` 필드 이름을 임시로 바꿨다가 원복했다. 원복은 `git checkout --` 로 했고, `git diff --stat` 이 비어 있음을 확인했다.
- `modules/enhance_advanced` 의 마지막 커밋은 QA-B-80 의 호출처 없는 코드 삭제(`6221931`)다.
- **원인은 밝히지 못했다.** 문턱 100ms 바로 위아래에서 흔들리는 게이트라는 사실만 기록한다.

## 6. (2) `diAlertThreshold` — 판독 + 컴파일러

**컴파일러 확인** (`_rename_c1.log`, `_rename_alert.log`; `cmake --build build\ci-post`, 둘 다 원복)

| 실험 | 결과 |
|---|---|
| **대조군** — `Constants::c1` → `c1_B82` | `exposure_index.cpp(226): error C2039: 'c1'` → **`BUILD=1`** |
| **대상** — `Constants::diAlertThreshold` → `diAlertThreshold_B82` | `exposure_index.cpp.obj`, `xpe_enhance_advanced.cpp.obj` 재컴파일 → **`BUILD=0`** |

→ ci-post 가 빌드하는 타깃 중 이 필드를 읽는 곳은 **없다.**

**DI 경보 평가 코드** — `modules/enhance_basic/src`, `modules/enhance_advanced/src` 에서 `alert|threshold|diOut|deviation` grep 후 판독했다.
- **enhance_advanced: 없음.**
  - `ExposureIndexCalculator::calculate`(`exposure_index.cpp:59-108`)는 EI·DI 를 쓰고 `XPE_OK` 를 돌려줄 뿐, 경보를 올리지 않는다.
  - 이 모듈의 `xpe_alert_push` 호출은 `enhance_advanced_helpers.cpp:131` 하나이고, 미지 config 키 경고다.
- **enhance_basic: 있음.** `modules/enhance_basic/src/exposure_index.cpp:111-116`
  ```cpp
  // REQ-ENH-026: post WARNING alert if |DI| > 3.0
  if (di < -3.0f || di > 3.0f) { ... xpe_alert_push(alertMsg, XPE_ALERT_WARNING); }
  ```
  문턱은 **리터럴 `3.0f`** 이다. 출처는 주석이 인용한 REQ-ENH-026 이다.

**B-77 과의 관계 (사실)**
- B-77 의 CHEST Normal DI −3.98 은 **enhance_advanced**(`xpe_adv_calc_exposure_index`)의 값이다.
- 이 모듈에는 경보 평가가 없으므로, 그 값에 대해 **경보가 올라가는 경로는 코드상 없다.** "`|DI|>3` 경보 조건에 해당한다"는 말은 T504 시험이 자기 안에서 계산한 `qcAlert` 에 대한 것이었다.
- 실행으로 경보 개수를 재지는 않았다.

## 7. 미검증

- W-07 을 실제로 실행하지 않았다(Lane C 영역, 판독만).
- e2e 파이프라인의 출력 화소 변화량은 재지 않았다.
- enhance_advanced 에서 경보가 올라가지 않는다는 것을 실행(`xpe_get_pending_alert_count`)으로 확인하지 않았다. 판독과 컴파일러 확인뿐이다.
- `T308` 이 흔들리는 원인.
- `clients/`·`gui/` 의 다른 프리셋 소비처는 grep 판독만 했다(`RealXpeBackend.cs`, `XpeDisplayInterop.cs`, `XpeDisplayVersionProbe.cs`).

## 8. 잔여 위험

- **부위 선택이 출력을 바꾸지 않는다**(개정문이 스스로 적은 한계). GUI 의 부위 선택은 사실상 효과가 없다.
- W-07 Native 실패가 CI 에 나타날 수 있다.
- `T308` 은 이 기계에서 문턱 경계에 있다.

## 부록 — 증거

`_env.bat`, `_voi.bat`, `_after.bat`, `_build.bat`, `_verify.bat`, `_control_before.log`, `_after.log`, `_falsify.log`, `_rename_c1.log`, `_rename_alert.log`, `_verify.log`, `_post_rerun.log`, `_t308_isolated.log`
