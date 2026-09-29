# QA-B-162 (`#130`) — 제 결함입니다. 경로를 컴파일 시점에 박았습니다

## 1. 주장

1. **하나의 결함입니다.** 세 시험이 같이 죽은 것은 **같은 원인**입니다 — 모델 파일을
   못 찾아 `Create` 가 `kInvalidModelPath` 로 실패하고 뒤 단언이 연쇄로 죽습니다. §3
2. **`modules/gsvg` 때문이 아닙니다.** **저장소 루트에서도 빨갰습니다.** 통과하던 곳은
   `modules/ai` **하나뿐**이고, 그건 `ctest` 가 `WORKING_DIRECTORY` 로 넣어 주던
   디렉터리입니다. §2
3. **`ci-ai` 가 통과한 이유**: 그 잡은 **`ctest` 로** 돌려서 `WORKING_DIRECTORY` 가
   적용됩니다. `#162` 단계는 **바이너리를 직접** 돌립니다 — 그 차이가 결함을 가렸습니다. §4
4. **전수에서 8자리**를 찾았습니다(`test_bone_suppress_abi.cpp` 4 + `test_onnx_session.cpp` 4).
   두 파일 모두 고쳤습니다. §5
5. **고친 뒤 네 곳 전부 초록** — 루트·`modules/gsvg`·`modules/ai`·**저장소 밖
   (`C:\Windows`)**. 스텁·풀 양쪽. §2
6. **제 로컬 검증이 왜 못 잡았는지** 적었습니다 — 저는 항상 `ctest` 로 돌렸습니다. §6

---

## 2. §2-① 반증 — cwd 를 바꿔 가며, **고치기 전과 후**

같은 바이너리, cwd 만 다르게. 종료 코드는 파이프 없이 받았습니다.

| cwd | 고치기 전 | 고친 뒤 |
|---|---|---|
| 저장소 루트 `xpe-post` | **1 (빨강)** | **0** |
| `modules/gsvg` (`#162` 단계가 쓰는 곳) | **1 (빨강)** | **0** |
| `modules/ai` (`ctest` 의 `WORKING_DIRECTORY`) | 0 | 0 |
| **저장소 밖** `C:\Windows` | — | **0** |

> **루트도 빨갰다는 것이 핵심입니다.** 카드는 `modules/gsvg` 를 지목했지만, 그건
> 우연히 그 워크플로가 고른 디렉터리였을 뿐입니다. 통과하던 자리가 **하나뿐**이고
> 그것이 `ctest` 가 만들어 주던 자리였다는 것이 진짜 그림입니다 —
> **cwd 에 의존하는 시험은 어느 하네스에서든 깨집니다.**

풀 빌드도 같은 스윕을 했습니다(`XPE_AI_EXPECT_ONNX=1`):
`C:\Windows` **0**, `modules/gsvg` **0**. 그리고 저장소 밖에서도 핵심 반증이
**실제로 실행**됩니다:

```
OK ] OnnxSessionRun.RunningADifferentModelChangesTheOutput (49 ms)
OK ] BoneSuppressAbi.ADifferentModelDirectoryChangesTheOutput (1 ms)
```

건수: 스텁 `179 ran / 168 passed / 11 skipped`, 풀 `179 ran / 177 passed / 2 skipped`.
**건너뛰게 만들어 넘기지 않았습니다** — 카드 §4 대로 경로를 고쳤습니다.

---

## 3. §3 — 세 시험이 같이 죽은 것은 **하나의 결함**

고치기 전 루트 실행의 실패 원문:

```
test_onnx_session.cpp(74): error: Value of: Exists(kScale2)   Actual: false
  tests/data/min_scale2.onnx is missing; regenerate with ...
test_onnx_session.cpp(76): Exists(kScale3)  false
test_onnx_session.cpp(77): Exists(kGarbage) false
test_onnx_session.cpp(88): error: Value of: s.has_value()     Actual: false
test_onnx_session.cpp(188): error: Value of: s.has_value()    Actual: false
```

- `ModelFilesArePresent` 는 **파일 부재를 직접** 단언합니다(`:74`·`:76`·`:77`)
- `StubBuildRefusesToRunAndDoesNotEcho`(`:88`)와
  `EmptyInputIsInvalidInput`(`:188`)는 **`s.has_value()` 에서** 죽습니다 —
  `Create` 가 `kInvalidModelPath` 를 돌려줬기 때문입니다

**연쇄입니다. 원인 하나, 증상 셋.** 별개 결함이 아닙니다.

---

## 4. §2-③ — `ci-ai` 는 왜 통과했나

| | 어떻게 돌리나 | cwd | 결과 |
|---|---|---|---|
| `ai-onnx` 잡 | `ctest --test-dir build/ci-ai …`(`:472`) | `gtest_discover_tests` 의 `WORKING_DIRECTORY` = **`modules/ai`** | 통과 |
| `post-build` `#162` 단계 | 바이너리를 **직접** 실행(`:337-345`, `$_.command[0]`) | `working-directory: modules/gsvg`(`:339`) | **빨강** |

`ctest` 가 각 시험을 등록된 `WORKING_DIRECTORY` 에서 돌려 주므로 상대 경로가
우연히 맞았습니다. `#162` 단계는 그 계층을 **의도적으로 건너뜁니다**(순서 의존
결함을 보려고 한 프로세스로 돌리는 것이 목적) — 그래서 `WORKING_DIRECTORY` 도 같이
사라집니다.

**`gsvg` 도 같은 상대 경로 습관을 갖고 있습니다**(`test_gsvg_result.cpp:26`,
`test_vg_product_table.cpp:28`, `test_virtual_grid.cpp:35`,
`test_thread_determinism.cpp:160` — `"tests/data/virtual_grid_synthetic_table.csv"`).
**그 단계의 cwd 가 `modules/gsvg` 인 이유가 바로 그것**으로 보입니다. 그래서
워크플로를 고치면 gsvg 가 깨집니다 — 카드 §1 의 경고가 맞고, 손대지 않았습니다.

> 다만 **그 습관은 `modules/gsvg` 에 그대로 남아 있습니다.** 그 단계의 cwd 가
> 언젠가 바뀌면 gsvg 가 같은 식으로 깨집니다. 제 소유가 아니라 **보고만** 합니다.

---

## 5. §2-② 전수 — `modules/ai/tests/` 에서 **8자리**

| 파일 | 자리 |
|---|---|
| `test_bone_suppress_abi.cpp` | `kDirX2`·`kDirX3`·`kDirMissing`·`kDirBroken` (4) |
| `test_onnx_session.cpp` | `kScale2`·`kScale3`·`kGarbage` + `there_is_no_such_model.onnx` 인라인 (4) |

`test_bone_suppress_abi.cpp` 는 **아직 CI 를 깨지 않았을 뿐입니다.**
`xpe_ai_init` 은 어떤 디렉터리 문자열도 받고, 못 찾으면
**모델-부재 코드**로 나타납니다 — 그리고 그 파일의 여러 시험이 그 코드를
**정당하게 기대**합니다. 즉 이 결함은 그쪽에서 *"틀린 이유로 통과"* 로
나타났을 것입니다. 잡기 훨씬 어려운 형태라 같이 고쳤습니다.

### 고친 방법

```cmake
target_compile_definitions(xpe_ai_tests PRIVATE
    XPE_AI_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
```

시험 쪽은 `#ifndef XPE_AI_TEST_DATA_DIR` → `#error` 로 **정의가 없으면 컴파일이
멈춥니다.** 조용히 상대 경로로 되돌아가는 길을 막았습니다.

인자로 넘기지 않고 **박은** 이유: 하네스가 아무것도 알 필요가 없고, **틀릴 cwd 가
남지 않습니다.**

---

## 6. 제 로컬 검증은 왜 못 잡았나 — 적어 둡니다

`QA-B-160`·`161` 에서 저는 항상 **`ctest` 로** 돌렸습니다(`ctest -R "^Onnx"`,
`ctest -R "^BoneSuppressAbi"`, `ctest --test-dir …`). `ctest` 가 `WORKING_DIRECTORY`
를 넣어 주므로 **상대 경로가 통과하는 유일한 조건을 제가 매번 만들어 주고
있었습니다.**

`QA-B-151` 에서 **정반대 방향의 같은 함정**을 이미 겪었습니다 — `gsvg_tests.exe` 를
직접 돌리면 거짓 빨강이 나서(phantom/kernel 표를 cwd 상대로 읽으므로) *"ctest 로
재라"* 가 그때의 교훈이었습니다. **이번엔 그 교훈이 결함을 가렸습니다.**

> 둘을 합치면 규칙은 *"ctest 로 재라"* 가 아니라 **"두 방식으로 다 재라"** 입니다 —
> `ctest` 는 등록된 환경을, 직접 실행은 **그 환경 없이도 서는지**를 봅니다.
> `#162` 단계가 존재하는 이유가 바로 후자입니다.

이 카드부터 `modules/ai` 시험은 **cwd 스윕**(루트·다른 모듈·저장소 밖)을 검증에
포함했습니다.

---

## 7. 검증

```
빌드:        ===STUB_BUILD=0===  ===FULL_BUILD=0===
cwd 스윕(스텁): 루트 0 · modules/gsvg 0 · modules/ai 0 · C:\Windows 0
cwd 스윕(풀):   C:\Windows 0 · modules/gsvg 0  (XPE_AI_EXPECT_ONNX=1)
  그중 반증:  RunningADifferentModelChangesTheOutput      OK 49 ms
              ADifferentModelDirectoryChangesTheOutput    OK  1 ms
전체 ctest:  스텁 753/754 · ci-ai 248/248
고치기 전:   루트 1 · modules/gsvg 1  (재현)
```

스텁 전체의 **빨강 1건은 선재**입니다:
`DuplicateExportTest.KnownDivergence_RenamedExportsStillDisagree`
(`enhance_advanced`) — `QA-B-154`·`156`·`160`·`161` 에서 같은 구성에 이미 있던
것이고 `ci-post` 프리셋이 아니기 때문입니다.

## 8. 미검증 · 잔여 위험

- **CI 에서 확인하지 못했습니다.** 푸시가 제 몫이 아니라, `post-build` 의 `#162`
  단계가 실제로 초록이 되는지는 **리더의 푸시 뒤에야** 압니다. 제가 한 것은 그
  단계의 cwd 를 로컬에서 재현한 것입니다
- **`modules/gsvg` 의 같은 습관을 고치지 않았습니다**(4파일 이상). 제 소유가 아니고,
  지금은 그 단계의 cwd 가 마침 `modules/gsvg` 라 동작합니다 — **그 cwd 가 바뀌면
  깨집니다.** 보고만 합니다
- **다른 모듈의 시험은 전수하지 않았습니다** — 범위를 `modules/ai/tests/` 로
  두었습니다. 같은 형태가 `display`·`dicom`·`enhance_*` 에 있는지는 모릅니다
- **`XPE_AI_TEST_DATA_DIR` 는 소스 트리를 가리킵니다.** 빌드 산출물만 배포하는
  환경에서는 그 경로가 없습니다 — 시험은 소스와 함께만 돌리는 물건이라 문제는
  없지만, **설치본에서 시험을 돌리는 방식은 이 카드가 지원하지 않습니다**
- 경로가 박혀 있으므로 **시험 바이너리를 다른 기계로 복사해 돌리면 실패합니다**
  (전엔 cwd 만 맞추면 됐습니다). 재현성 쪽이 낫다고 판단했지만 교환입니다

---

Refs #130
