# QA-B-161 (`#130`) — C ABI 한 함수가 실제 세션에 닿습니다. **그리고 `ai-onnx` 잡이 깨집니다**

## 1. 주장

1. **`xpe_bone_suppress` 를 골랐습니다** — float 영상 in, float 영상 out 이라
   모델의 모양 그대로이고, **별도 출력 버퍼**라 입력이 대조군으로 남습니다. §2
2. **반증이 C ABI 층에서 섭니다.** 모델 디렉터리만 바꾸면 같은 화소가 다른 수로
   나옵니다(×2 / ×3). `40 ms` 동안 실제로 돌았습니다. §3
3. **세 원인이 세 코드로 갈라집니다** — 모델 없음 `IO_FAILED`, 모델 깨짐
   `CONFIG_INVALID`, 추론 실패 `PROCESSING_FAILED`. §3
4. **세션 소유권을 헤더에 적었습니다**(`FUNC-038`) — 모듈이 소유, 첫 호출에 적재,
   `xpe_ai_shutdown` 이 해제, **호출자는 보지도 해제하지도 않습니다.** §4
5. **`ai-onnx` 잡이 첫 실행에서 깨졌을 것입니다 — 제 코드 때문에.** 로컬에서
   레시피를 그대로 돌려 잡았습니다. 원인은 **제가 드러낸 선재 결함**이고
   고쳤습니다. **워크플로 자체는 정상입니다.** §5
6. **§4 CI 관측은 여전히 불가능합니다 — 이유가 달라졌습니다.** 잡이
   `origin/main` 에 **없습니다**(미푸시). 관측이 아니라 측정된 부재입니다. §6

---

## 2. §1 — 왜 `xpe_bone_suppress` 인가

| 후보 | 왜 아닌가 |
|---|---|
| `xpe_dl_denoise` | **제자리 수정**이라 입력이 파괴됩니다 — *"출력이 입력과 같지 않다"* 는 대조군을 쓸 수 없습니다 |
| `xpe_bodypart_recognize` | float → 라벨 규칙을 **제가 발명해야** 합니다. 모델이 주지 않는 두 번째 미검증 물건이 생깁니다 |
| `xpe_stitch_*` | 다입력 + 기하. 움직이는 부분이 여럿 |
| **`xpe_bone_suppress`** | float 영상 in → float 영상 out. **모델이 이미 그 모양**이라 `QA-B-160` 의 x2/x3 단언이 **그대로 올라옵니다.** 출력이 별도 버퍼라 입력이 살아 있습니다 |

---

## 3. §3 반증 — 셋 다

### (a) 모델을 바꾸면 C ABI 출력이 바뀌는가 → **예**

```
[ OK ] BoneSuppressAbi.ADifferentModelDirectoryChangesTheOutput (40 ms)
```

`OnnxSession` 이 완벽해도 `xpe_bone_suppress` 가 그것을 무시하면 하위 시험은
아무것도 모릅니다 — **그것이 `#205` 였습니다.** 그래서 이 단언이 C ABI 층에
있어야 합니다. 단언 내용: 화소마다 `x*2`·`x*3` 을 **수치로**, 두 출력이 서로
다름, **출력 ≠ 입력**(메아리 배제), 그리고 별도로 *"다른 화소 → 다른 출력"*
(입력 무시 배제).

모델은 `<modelDir>/bone_suppress.onnx` 로 찾고, 시험은 **디렉터리를 바꿔** 모델을
바꿉니다 — 두 번째 탐색 규칙을 발명하지 않으려고요.

영상은 크기가 고정이 아니므로 **동적 길이 모델**을 새로 만들었습니다. 모듈에
넣기 전에 프로브로 확인했습니다 — 선언된 축이 `-1` 이고, 모델 파일이 말한 적 없는
길이 **9 와 6** 을 둘 다 받습니다.

### (b) 스텁 빌드에서 무엇을 반환하는가 → **`PROCESSING_FAILED`, 메아리 아님**

```
[ OK ] BoneSuppressAbi.StubBuildFailsAndLeavesTheOutputAlone (stub 빌드에서)
```

출력 버퍼가 **호출 전과 같음**을 단언합니다 — 반쯤 쓰다 만 화소도, 입력 복사도
없습니다.

### (c) 세 원인, 세 코드

| 상황 | 코드 | 왜 갈라야 하나 |
|---|---|---|
| 모델 파일 없음 | `XPE_ERR_IO_FAILED` | *"모델을 설치하라"* |
| 파일은 있으나 모델이 아님 | `XPE_ERR_CONFIG_INVALID` | *"설치한 모델이 깨졌다"* |
| 추론 자체 실패 | `XPE_ERR_PROCESSING_FAILED` | *"다시 시도/대체 경로"* |
| float32 아님 | `XPE_ERR_UNSUPPORTED_FORMAT` | 16비트를 float 로 읽으면 **오류 대신 숫자**가 나옵니다 |

`MissingModelAndBrokenModelGiveDifferentCodes` 가 앞의 둘을 **서로 다름**까지
단언합니다.

---

## 4. §2 경계와 수명 (`FUNC-038`)

**C++ 타입이 헤더로 새지 않습니다.** `OnnxSession` 은 `ai.cpp` 안에서만 쓰이고,
`ai_api.h` 는 그대로 C ABI 입니다. 세션 핸들은 **ABI 를 건너지 않습니다.**

헤더에 적은 계약:

> 모델은 `<modelDir>/bone_suppress.onnx` 에서 읽는다. 그 세션은 **이 모듈이
> 소유**한다 — **첫 호출**에 만들어지고(초기화 때가 아니라), 이후 호출이 재사용하고,
> `xpe_ai_init` 이 다른 디렉터리로 다시 불리면 다시 만들고, `xpe_ai_shutdown` 이
> 해제한다. **호출자는 받지도, 보관하지도, 해제하지도 않는다.**

**왜 `init` 이 아니라 첫 호출인가**: 추론을 안 쓰는 호출자에게 `init` 이 비싸지면
안 되고, 모델이 없을 때 **그것이 필요했던 함수가** 보고하는 편이 낫습니다.

시험으로 고정했습니다 — `ReInitWithADifferentDirectorySwitchesTheModel`(같은
디렉터리면 재사용, 바뀌면 교체), `ShutdownThenUseIsRejectedNotCrashed`(해제 뒤
호출은 거부, `shutdown` 두 번도 안전).

---

## 5. **`ai-onnx` 잡이 깨졌을 것입니다 — 제 코드 때문에**

카드가 *"깨지면 고치지 말고 증상만 보고하라(워크플로는 리더 소유)"* 라고 했습니다.
**깨진 것은 워크플로가 아니라 제 코드였고, 그래서 고쳤습니다.**

잡 레시피를 로컬에서 그대로 돌렸습니다(`cmake --preset ci-ai` +
`-DONNXRUNTIME_ROOT` + `XPE_AI_EXPECT_ONNX=1`):

```
1차:  ===PRESET_CONFIGURE=0===  ===PRESET_BUILD=1===   ← 빌드 실패
      ai_onnx_session.h(230): warning C4150 → error C2220
```

### 원인 — 제가 드러낸 **선재 결함**

`C4150`: *"불완전한 형식의 포인터를 삭제했습니다. 소멸자가 호출되지 않습니다."*

`OnnxSession` 은 PIMPL 인데 **소멸자가 `struct Impl` 정의보다 앞에**, 이동 대입은
아예 **헤더에** 있었습니다. 둘 다 `delete pimpl_` 을 합니다 — **`Impl` 이 소유한
것(풀 빌드에서는 ONNX 세션과 env)이 해제되지 않는 진짜 누수**이지 스타일 문제가
아닙니다.

**왜 지금까지 안 보였나**: `modules/ai/CMakeLists.txt` 가 제품을 `/wd4150` 으로
컴파일합니다. 그 억제가 없는 첫 타깃이 **이 카드의 시험**이었고, `ci-ai` 프리셋이
경고를 오류로 올립니다.

**고친 방법**: 억제를 추가하지 않고 **정의를 옮겼습니다** — 소멸자와 이동 대입을
`Impl` 정의 **뒤**로. `/wd4150` 은 그대로 뒀고 **그것이 해결책이 아님**을 주석에
적었습니다.

> `.cpp` 에 있는 것만으로는 부족했습니다 — **순서가 문제**였습니다. 소멸자는 이미
> `.cpp` 에 있었고 주석도 *"must be in .cpp for PIMPL"* 이라고 적혀 있었지만,
> `Impl` 앞에 있어서 아무 효과가 없었습니다.

### 그다음 — 기존 시험 3건이 풀 빌드에서 빨강

```
2차:  ===PRESET_BUILD=0===  ===PRESET_CTEST=8===   99% (3/248 실패)
```

`xpe_bone_suppress` 의 동작이 **실제로 바뀌었기 때문**입니다. 약화시키지 않고
각각 바로잡았습니다:

| 시험 | 전 | 후 | 이유 |
|---|---|---|---|
| `AiFallbackTest.BoneSuppressStub…` | `PROCESSING_FAILED` | **`UNSUPPORTED_FORMAT`** | UINT16 화소. 형식은 이제 **전제 조건**이고 전제는 자원보다 먼저 검사합니다(`#119` 우선순위) |
| `AiWorkerIsolationTest.StubMode…` | 같음 | 같음 | 같음 |
| `AiParamDependency.InferenceEntry…` | `PROCESSING_FAILED` | **`IO_FAILED`** | float 화소라 형식은 통과, `dummy_model_dir` 에 모델이 없음 |

**두 번 틀렸고 두 번 다 측정이 고쳤습니다:**

1. UINT16 건을 *"스텁은 그대로, 풀만 바뀜"* 으로 분기했는데 — **형식 검사가 양쪽
   모두에 적용**되므로 스텁도 `UNSUPPORTED_FORMAT` 입니다. 분기를 없앴습니다.
2. float 건도 분기했는데 — 스텁도 **`-9`(`IO_FAILED`)** 였습니다.
   **파일이 있는지는 ONNX 가 답하는 질문이 아니라서** `OnnxSession::Create` 가 두
   가지 **앞에서** 확인하기 때문입니다. 이쪽이 더 쓸모 있는 사실이라 주석에
   적었습니다.

> 두 번 다 *"이럴 것이다"* 로 썼다가 **실행이 반박**했습니다. 분기를 지운 자리가
> 둘 다 더 강한 단언이 됐습니다 — 빌드와 무관하게 같은 답을 요구하니까요.

### 결과

```
3차:  ===PRESET_CONFIGURE=0===  ===PRESET_BUILD=0===  ===PRESET_CTEST=0===
      100% tests passed, 0 tests failed out of 248
```

**`ai-onnx` 잡의 레시피가 이제 완주합니다.** 워크플로·프리셋은 **한 줄도 고치지
않았습니다.**

---

## 6. §4 CI 관측 — **아직 불가능하고, 이유가 바뀌었습니다**

`QA-B-160` 에서 제가 남긴 미검증: *"CI 에서 신규 시험을 관측하지 못했습니다."*
카드는 *"잡이 생겼으니 관측으로 바꾸라"* 고 했습니다. **바꾸지 못했습니다.**

| 확인 | 결과 |
|---|---|
| `origin/main` 의 `ci.yml` 에 `ai-onnx` | **0건** |
| `5215e0f` 를 담은 **원격** 브랜치 | **0개** |
| `ai-onnx` 를 포함한 CI 런 | **없음** |

**`ai-onnx` 잡은 로컬 `main` 에만 있고 푸시되지 않았습니다.** 그래서 CI 에서 한
번도 돈 적이 없습니다. 이것은 추론이 아니라 **측정된 부재**입니다 — `git branch -r
--contains` 와 `git show origin/main:` 로 확인했습니다.

**제가 할 수 있는 가장 가까운 것**이 §5 의 레시피 로컬 재현이었고, 그것이 첫 CI
실행이 빨갛게 났을 원인을 **미리 잡았습니다.** 다만 **관측은 여전히 리더의 푸시
뒤에야 가능합니다.**

---

## 7. 검증

```
프로브(모듈 전, 동적 모델):  선언 축 -1, 길이 9·6 모두 수용, x2/x3 다름
                             models_broken → Protobuf parsing failed (code 7)
ci-ai 레시피 (잡과 동일):    ===PRESET_CONFIGURE=0=== ===PRESET_BUILD=0===
                             ===PRESET_CTEST=0===  248/248
  그중 반증:                  [ OK ] ADifferentModelDirectoryChangesTheOutput (40 ms)
스텁 전체:                   ===STUB_BUILD=0===  753/754
가드 반증(스텁+주장):        ===STUB_CLAIMING_ONNX=1===  (빨강 — 옳음)
BoneSuppressAbi 12건:        풀 11 실행 + 1 건너뜀 / 스텁 7 실행 + 5 건너뜀
```

스텁 전체의 **빨강 1건은 선재**입니다:
`DuplicateExportTest.KnownDivergence_RenamedExportsStillDisagree`
(`enhance_advanced`) — `QA-B-154`·`156`·`160` 에서 같은 구성에 이미 있던 것이고
`ci-post` 프리셋이 아니기 때문입니다. **`ci-ai` 에서는 나지 않습니다**(그 모듈을
안 지음).

종료 코드는 전부 파이프 없이 받았습니다.

## 8. 미검증 · 잔여 위험

- **CI 에서 아무것도 관측하지 못했습니다**(§6). `ai-onnx` 가 미푸시입니다
- **`/wd4150` 이 아직 제품에 걸려 있습니다.** 이번 `C4150` 은 고쳤지만 **같은 종류의
   다른 결함이 제품 코드에 더 있는지는 안 봤습니다** — 억제를 떼어 보는 것이
   확인 방법이고, 이 카드 범위 밖으로 뒀습니다
- **`REQ-AI-050`·`051`(U-Net, 결절 민감도 +16.8 %)은 전혀 검증하지 않았고 충족되지
   않습니다.** 모델이 장난감 배율입니다. 이 카드는 **배선**이지 임상 주장이 아닙니다
- **나머지 C ABI 는 그대로 스텁입니다** — `xpe_bodypart_recognize`·`xpe_dl_denoise`·
  `xpe_stitch_images`
- **동시 호출을 시험하지 않았습니다.** 모듈 뮤텍스로 직렬화된다고 헤더에 적었지만
  **여러 스레드에서 실제로 부르지는 않았습니다**
- **모델 메타데이터(`REQ-AI-008`)를 `bone_suppress` 경로에 연결하지 않았습니다** —
  `OnnxSession` 이 `.json` 을 읽지만 C ABI 는 그것을 노출하지 않습니다
- Linux 경로 미실행

---

Refs #130
