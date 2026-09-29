# QA-B-160 (`#130`) — `xpe_ai` 가 **실제로 모델을 돌립니다**

## 1. 주장

1. **§2 SOUP: 고칠 것은 요구가 아니라 등록입니다.** 최신 ONNX Runtime 은 **v1.30.0**,
   MIT, `onnxruntime-win-x64` 자산 실재 — `REQ-AI-006`(1.20+)을 만족합니다.
   **그래서 멈추지 않았습니다.** §2
2. **카드의 전제 하나가 낡았습니다.** `SOUP-003` 등록은 이미 **미확정**입니다
   (리더가 `adde13d`). 다만 **다른 문서 4곳에 `1.17` 이 그대로 남아 있습니다.** §2
3. **세로 한 줄이 통과합니다.** 모델 적재 → 입력 텐서 → 세션 실행 → 출력 해석 →
   `XpeErrorCode`. `Ort::` 0건이었던 모듈이 이제 실제 세션을 돕니다. §3
4. **반증이 섭니다.** 배율만 다른 두 모델이 같은 입력에서 **다른 수**를 냅니다
   (×2 → 2,4,6,8 / ×3 → 3,6,9,12). 메아리·항등·스텁은 통과할 수 없습니다. §4
5. **§4 CI: 지금 이대로면 이 구현의 핵심 단언은 CI 에서 "건너뜀" 으로 초록입니다.**
   그것을 잡는 가드를 넣었고, 러너에 런타임을 넣는 방법을 보고합니다. §5
6. 검증: 풀 **167/167**, 스텁 전체 **741/742**(선재 실패 1건). §6

---

## 2. §2 — SOUP: **요구가 아니라 등록을 고칩니다**

### 카드 전제부터 확인했습니다

카드: *"`SOUP-003` 등록 버전이 `1.17.x`"*. **지금은 아닙니다:**

```
docs/post-processing/xpe/xpe-iec62304-class-b-package.md:733
| SOUP-003 | ONNX Runtime | **미확정** (`XPE-SOUP-001` 주 참조) | ... | MIT | B |
```

리더가 `QA-B-155` 무렵 미확정으로 되돌려 둔 상태입니다(카드 `QA-B-155` §5 에 그렇게
적혀 있었습니다). **그러나 다른 문서 4곳은 여전히 `1.17` 입니다** — `docs/` 는 리더
소유라 손대지 않았고 보고만 합니다:

| 파일 | 내용 |
|---|---|
| `XPE-SAD-001_Software_Architecture_Document.md:180` | `ONNX Runtime 1.17` |
| `XPE-SCM-001_Configuration_Management_Plan.md:120` | `onnxruntime: 1.17.x` |
| `XPE-SOUP-001_SOUP_Analysis.md:69` | 알려진 문제 대응: *"Version ≥ 1.17 사용"* |
| `docs/project/sprint-plan.md:295` | *"check 1.17+ availability"* |

`XPE-SOUP-001_SOUP_Analysis.md:26` 에 리더가 남긴 불일치 기록은 살아 있습니다.
**즉 "미확정" 은 표 하나에만 반영됐습니다.**

### 세 질문에 대한 답 — 전부 측정

**(1) 실제로 연동 가능한 버전이 무엇인가**

| 항목 | 값 | 근거 |
|---|---|---|
| 최신 릴리스 | **v1.30.0** | `github.com/microsoft/onnxruntime/releases/latest` 가 `v1.30.0` 으로 302 |
| 배포 형태 | `onnxruntime-win-x64-1.30.0.zip` (CPU), `...-gpu_cuda12/13-...zip` | 릴리스 자산 목록 |
| 라이선스 | **MIT** | 저장소 `license.spdx_id` |
| x64 | **예** | 자산 이름이 `win-x64` |
| 내용 | `include/onnxruntime_cxx_api.h` · `lib/onnxruntime.lib` · `lib/onnxruntime.dll` | 받아서 확인 |

곁가지 관측: 이 기계의 bun 캐시에 `onnxruntime-node@1.24.3` 이 있고 그 안의
`win32/x64/onnxruntime.dll` 은 `FileVersion 1.24.20260304.5` 였습니다. **헤더도
`.lib` 도 없어 C++ 링크에는 못 씁니다** — 참고용 관측입니다.

**(2) `REQ-AI-006`(1.20+)을 만족하는가 → 예.** 1.30.0 은 1.20 보다 높고, 실제로
그 버전으로 빌드·실행했습니다(§3).

**(3) 무엇을 고쳐야 하는가 → SOUP 등록입니다. 요구가 아닙니다.**
따라서 카드 §2 의 *"요구를 고친다 이면 멈추라"* 에 해당하지 않아 진행했습니다.
등록에 적을 값: **`1.30.0` · MIT · 사전 빌드 win-x64 · 위험등급 B**.
`docs/` 소유가 리더이므로 **제가 고치지 않았습니다.**

### 덤 — `QA-B-153` 의 미검증 두 건이 닫혔습니다

`QA-B-153` 이 *"런타임이 있는 경우의 성공 경로를 시험하지 않았다"* 와
*"`:90-95` 의 파일명 가정이 실제 배포본과 맞는지 확인 안 됨"* 을 남겼습니다.
**둘 다 확인됐습니다** — 배포본 레이아웃이 `lib/onnxruntime.lib`·`lib/onnxruntime.dll`
그대로이고, `ONNXRUNTIME_ROOT` 분기가 `CONFIGURE=0` 으로 섭니다:

```
-- ONNX Runtime: using pre-built from D:/workspace-github/_deps/onnxruntime-win-x64-1.30.0
-- xpe_ai: FULL build with ONNX Runtime
```

---

## 3. §3 — 세로 한 줄

### 착수 전에 런타임과 모델을 따로 검증했습니다

모듈을 건드리기 전에 **독립 프로브**(`_probe.cpp`, ORT 에 직접 링크)로 확인했습니다 —
안 그러면 제가 손으로 만든 protobuf 를 모듈 빌드 안에서 디버깅하게 됩니다:

```
ORT version: 1.30.0
min_scale2.onnx   in=X out=Y nIn=1 nOut=1   ===RUN2=0=== y = 2.0 4.0 6.0 8.0
min_scale3.onnx   in=X out=Y nIn=1 nOut=1   ===RUN3=0=== y = 3.0 6.0 9.0 12.0
garbage file      ORT exception: Protobuf parsing failed. (code 7)   ===GARBAGE=1===
```

### 모델 — `onnx` 패키지 없이 손으로

`modules/ai/tests/data/make_min_models.py` 가 stdlib 만으로 protobuf 를 씁니다.
116 바이트짜리 `Y = Mul(X, scale)`, float32 `[4]`. **두 개**를 만드는 것이 요점입니다 —
`scale` 만 다릅니다. 하나로는 §4 를 단언할 수 없습니다.

### 구현 (`modules/ai/src/ai_onnx_session.cpp`)

| 단계 | 전 | 후 |
|---|---|---|
| 모델 적재 | `#else` 가지가 *"For now, use stub implementation even in full build"* | `Ort::Env` + `Ort::Session`, 실패는 `kModelLoadFailed`/`kSessionCreationFailed` 로 반환하고 **`is_valid` 를 세우지 않음** |
| 입출력 메타 | `inputs.clear(); outputs.clear();` | 그래프에서 이름·형상을 읽음 |
| 세션 실행 | **없음** (클래스에 실행 메서드가 없었음) | `Run()` 추가 |
| 출력 해석 | — | 출력 텐서를 `std::vector<float>` 로 |
| EP 목록 | `kCuda`·`kTensorRt`·`kDirectMl` **무조건 push** | `Ort::GetAvailableProviders()` **조회** |

`Impl` 이 PIMPL 이라 ONNX 타입이 헤더로 새지 않습니다 — **스텁 빌드 소비자가 계속
컴파일됩니다**(대조군으로 확인, §6).

**스텁 가지는 메아리를 돌려주지 않습니다.** `Run()` 이 `kModelLoadFailed` 를 냅니다 —
메아리면 스텁이 동작하는 추론 경로로 읽히고, 그것이 `#205` 였습니다.

### 시험이 DLL 대신 구현을 직접 컴파일합니다

`OnnxSession` 은 C++ 클래스이고 `xpe_ai.dll` 은 **C ABI 만** 내보냅니다(링크 오류로
확인). IPC 브리지와 같은 처방을 썼습니다 — 시험 타깃이 `ai_onnx_session.cpp` 를 직접
컴파일합니다. **C++ 클래스를 DLL 경계로 내보내면 `std::vector`·`std::string` 이
모듈의 공개 계약에 들어가는데, C ABI 는 바로 그것을 막으려고 있습니다.**

### 범위 밖으로 남긴 것 — 주석에 적었습니다

CPU EP 만 등록 · 입력/출력 각 1개 float32 만 · 워커 격리(`REQ-AI-003`)는 이 경로를
안 지남 · `enable_profiling` 은 받되 무시(쓰기 경로가 `REQ-AI-093` 정책 문제라
여기서 정할 것이 아님).

---

## 4. §5 — 반증: **모델이 답을 정하는가**

`RunningADifferentModelChangesTheOutput` 이 이 카드의 하중을 받는 시험입니다.

```
[ OK ] OnnxSessionRun.RunningADifferentModelChangesTheOutput (45 ms)
```

단언하는 것:

- `x*2` 와 `x*3` 을 **수치로** (`EXPECT_FLOAT_EQ`) — "다르다" 가 아니라 무엇인지
- 두 모델의 출력이 서로 다름 — **항등·no-op 로는 불가능**
- 출력이 입력과 같지 않음(`EXPECT_NE(x, y2)`) — **메아리 배제**
- `OutputIsNotAConstantIndependentOfInput`: 같은 세션, 다른 입력 → 다른 출력 —
  **입력 무시 상수 배제**

`#154`·`#155` 에서 *"계산되고 실행되는데 답에서 약분되는"* 모델이 두 번 나왔습니다.
**약분되면 이 네 단언 중 최소 둘이 빨강입니다.**

실패 경로도 코드로 나옵니다: 모델 없음 → `kInvalidModelPath`, 쓰레기 파일 →
`kModelLoadFailed`(+ `value == nullptr`), 길이 불일치·빈 입력 → `kInvalidInput`.

---

## 5. §4 — CI: **지금 이대로면 이 구현은 조용히 초록입니다**

### 관측: `ai` 시험은 이제 CI 에서 돕니다

`#205` 이후 상태를 **워크플로가 아니라 로그에서** 확인했습니다
(`XPE CI Pipeline`, main, 성공 런):

```
post-build   408/827 Test #408: AiAbi.VersionReturnsNonNull ... Passed
```

`Test #408` 이 실재하므로 **인용만 되는 초록이 아니라 실제로 실행됩니다.** 총 827건.

### 그러나 CI 는 **스텁**입니다

`ci-post` 는 `BUILD_AI=ON` 이지만 `XPE_AI_USE_ONNXRUNTIME` 은 **설정하지 않습니다.**
따라서 CI 에서:

| 시험 | CI(스텁) | 의미 |
|---|---|---|
| `RunningADifferentModelChangesTheOutput` | **SKIPPED** | **이 카드의 핵심 단언이 안 돕니다** |
| `OutputIsNotAConstantIndependentOfInput` | SKIPPED | |
| `WrongInputLengthIsInvalidInput` | SKIPPED | |
| `GarbageFileIsModelLoadFailed` | SKIPPED | |
| `InputAndOutputAreReadFromTheModel` | SKIPPED | |
| 나머지 4건 + 가드 | 실행 | 스텁 계약을 고정 |

**`ctest` 는 SKIPPED 를 통과로 셉니다.** 즉 아무것도 안 바꾸면 **`#205` 와 같은
모양이 한 겹 안쪽에서 재현됩니다** — 초록인데 보장이 없습니다.

### 그래서 가드를 넣었습니다 — 건너뜀을 빨강으로 바꿀 수 있게

`OnnxSessionBuildMode.AFullBuildIsProvableWhenTheCallerSaysItExpectedOne`:
`XPE_AI_EXPECT_ONNX=1` 이 설정된 잡에서 스텁 빌드면 **실패**합니다.
시험 자신은 의도된 건너뜀인지 알 수 없으므로 **호출자가 선언**하게 했습니다.

양방향으로 쟀습니다:

```
스텁 + XPE_AI_EXPECT_ONNX=1   ===STUB_CLAIMING_ONNX=1===   [ FAILED ]
풀   + XPE_AI_EXPECT_ONNX=1   ===FULL_CLAIMING_ONNX=0===   [   OK   ]
스텁 + (선언 없음)             ===STUB_NO_CLAIM=0===        대조군: 초록 유지
```

대조군이 있어야 *"다 죽이는 가드"* 가 아님이 확인됩니다.

### 러너에 런타임을 넣는 방법 — **보고만**(파일이 리더 소유)

`.github/workflows/`·`CMakePresets.json` 은 안 고쳤습니다. 필요한 변경:

1. `windows-2025` 러너에서 **다운로드 + 캐시**:
   `onnxruntime-win-x64-1.30.0.zip`(**82,645,522 B**, 실측) 를
   `actions/cache` 로 버전 키에 묶으면 재다운로드가 없습니다.
2. 그 경로로 **별도 구성**:
   `-DXPE_AI_USE_ONNXRUNTIME=ON -DXPE_AI_STUB_BUILD=OFF -DONNXRUNTIME_ROOT=<추출경로>`
3. 그 스텝에 **`XPE_AI_EXPECT_ONNX=1`** 을 넣어 가드가 서게 합니다.

**새 잡 대 기존 잡 확장** — `QA-B-154` 에서 쟀던 대로 ai 의 추가 비용은 잡음대와
같은 크기였지만, 여기서는 성격이 다릅니다:

| 선택 | 득 | 실 |
|---|---|---|
| `ci-post` 를 통째로 풀 빌드로 | 잡 추가 없음 | **ONNX 다운로드가 post 잡 전체의 선행 조건**이 됩니다 — 받기 실패하면 post 가 통째로 빨강 |
| **작은 `ai-onnx` 잡 신설**(권고) | post 가 ONNX 에 묶이지 않음. 실패가 국소적 | 러너 슬롯 1개. 다만 `ctest -R "^Onnx"` 만 돌면 짧습니다 |

로컬 측정으로는 풀 빌드의 `Ai|Onnx` 167건이 **`FULL_CTEST=0`** 으로 끝났고, 다운로드는
캐시되면 1회입니다. **CI 러너에서는 재지 못했습니다**(§8).

조건부 건너뛰기를 **골랐습니다** — 사유는 시험 안의 `GTEST_SKIP()` 문구에 적혀
있습니다(`#214` 형태). 문구가 *"이 건너뜀이 `#205` 의 축소판"* 이라고 말합니다.

---

## 6. 검증

```
독립 프로브(모듈 건드리기 전):  ===COMPILE=0===  RUN2=0 RUN3=0 GARBAGE=1
풀 구성/빌드:                  ===FULL_CONFIGURE=0===  ===FULL_BUILD=0===
풀 시험 (Ai + Onnx):            ===FULL_CTEST=0===  167/167 통과
  그중 반증:                    [ OK ] RunningADifferentModelChangesTheOutput (45 ms)
  건너뜀:                       1건(스텁 계약 시험) — 정확
스텁 대조군(빌드가 여전히 선다): ===STUB_BUILD_ALL=0===
스텁 전체 ctest:                741/742 (742 = 731 + 신규 11)
  건너뜀:                       5건(풀 전용) — 정확
가드 반증:                      스텁+주장 1 / 풀+주장 0 / 스텁+무주장 0
```

스텁 전체의 **빨강 1건은 선재**입니다:
`DuplicateExportTest.KnownDivergence_RenamedExportsStillDisagree`(`enhance_advanced`).
`QA-B-154`·`QA-B-156` 에서 **이 카드 이전에** 같은 구성에서 똑같이 났습니다 —
`ci-post` 프리셋이 아니기 때문입니다.

종료 코드는 전부 파이프 없이 받았습니다.

## 7. 같은 턴에 고친 낡은 주석

이 구현이 **제가 `QA-B-155`·`B-156` 에서 쓴 주석을 거짓으로 만듭니다.** 같이 고쳤습니다:

- `CMakeLists.txt:8` *"REQUIREMENT, NOT CURRENT STATE. No build of this module
  links ONNX Runtime today"* → **"PARTLY MET as of QA-B-160"** + 무엇이 아직 아닌지
- `ai_onnx_session.cpp` 머리말 *"THERE IS NO FULL MODE YET"* → 무엇이 되고 무엇이
  안 되는지로 교체(이력은 남김 — 두 가지가 여전히 있고 독자는 자기 빌드가 어느
  쪽인지 알아야 합니다)
- 제 시험 파일 머리말의 *"스텁에서도 전부 돈다"* → **5건은 건너뛴다**로 정정
  (쓰자마자 틀린 문장이었습니다)

## 8. 미검증 · 잔여 위험

- **CI 에서 아무것도 관측하지 못했습니다** — 신규 시험은 main 에 없고 저는 푸시하지
  않습니다. §5 의 CI 서술 중 **관측된 것은 "`ai` 시험이 돈다"(`Test #408`)까지**이고,
  *"신규 시험이 CI 에서 건너뛴다"* 는 **프리셋과 로컬 스텁 실행에서 추론한 것**입니다
- **CI 러너에서 다운로드·캐시를 시험하지 않았습니다.** 82.6 MB 는 로컬 실측이고,
  러너의 소요 시간·캐시 적중은 미지입니다
- **CPU EP 만 등록합니다.** `GetAvailableExecutionProviders()` 가 이제 조회를 하지만,
  CUDA/DirectML 을 **세션에 등록하지는 않습니다** — 목록에 뜨는 것과 쓸 수 있는 것은
  다릅니다. 이 카드 범위 밖이라 주석에 적었습니다
- **`Run()` 은 입력 1·출력 1·float32 만** 다룹니다. 그 밖은 `kInvalidInput` 입니다
- **모델이 장난감입니다.** `Y = X * k` 는 추론 경로가 도는 것을 보이지 **성능·정확도에
  대해 아무 말도 하지 않습니다.** `REQ-AI-002`(결정론적 fallback)·`REQ-AI-061`·
  `REQ-AI-092` 는 여전히 미구현입니다
- **`xpe_ai.dll` 의 C ABI 는 그대로입니다** — `xpe_bodypart_recognize` 등은 아직
  스텁입니다. 이 카드는 `OnnxSession` 까지이고, C ABI 를 그 위에 얹는 것은 다음입니다
- **Linux 경로는 안 돌렸습니다**(`libonnxruntime.so` 분기)
- `docs/` 의 `1.17` 4곳과 SOUP 등록값은 **보고만** 했습니다

---

Refs #130
