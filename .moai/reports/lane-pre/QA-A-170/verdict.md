# QA-A-170 — Doxygen 초록 복구. **원인은 "옛 이름" 이 아니라 "고아가 된 블록"** 입니다

카드 §2 의 진단을 **한 가지 고쳐야** 합니다. 그리고 §5 의 답은 **두 겹**입니다.

---

## 1. 정정 — `@param` 이름은 낡지 않았습니다. **블록이 제 선언을 잃었습니다**

카드는 `BlendReferenceAt` 의 `@param` 셋(`img`·`windowValues`·`deviations`)을 *"시그니처가 바뀌었는데 주석이 안 따라간 것"* 으로 읽었습니다. 실제는 다릅니다 — **그 셋은 `BlendReferenceAt` 의 주석이 아니었습니다.**

`DetectDefectivePixel` 의 주석 블록이 `820`행에 있었고, **`QA-A-164` 가 새 함수 셋을 그 블록과 선언(`1013`행) 사이에 끼워 넣었습니다.** Doxygen 은 선언 바로 위 블록을 그 선언의 것으로 읽으므로, `820` 의 블록을 **바로 아래에 있는 `BlendReferenceAt`** 의 것으로 붙였습니다. 그래서 한 번의 삽입이 **두 종류의 오류를 동시에** 냈습니다:

| 오류 | 실제 원인 |
|---|---|
| `argument 'img' … not found in … BlendReferenceAt` ×3 | 남의 블록을 물려받음 |
| `Member DetectDefectivePixel … is not documented` | 자기 블록을 190행 위에 두고 왔음 |

**증거** — 수정 전 실행 로그가 두 줄을 한 실행에서 같이 냅니다:

```
runtime_detection.h:1013: error: Member DetectDefectivePixel(...) is not documented.
runtime_detection.h:848:  error: argument 'img' of command @param is not found in
                                 the argument list of ... BlendReferenceAt(...)
```

`848` 은 `BlendReferenceAt` 의 선언 줄이고 `1013` 은 `DetectDefectivePixel` 의 선언 줄입니다. **한 블록이 두 자리에서 동시에 어긋난 것**입니다.

> 왜 중요한가: 카드의 진단대로 `BlendReferenceAt` 의 `@param` 셋을 *"실제 인자로 교체"* 했다면, `DetectDefectivePixel` 의 문서가 **영구히 사라집니다**. 오류 셋은 없어지고 `1013` 의 오류는 남아, 그다음 손이 그것마저 새로 써서 메웠을 것입니다 — 이 저장소가 *"틀린 보증을 걷어내면 공백이 남는다"* 로 적어 둔 형태입니다.

**그래서 고친 방법은 "이름 교체" 가 아니라 "블록 되돌리기"** 입니다:

| 함수 | 한 일 |
|---|---|
| `DetectDefectivePixel` | 블록을 **선언 바로 위로 이동**(내용 그대로). 다시 벌어지지 않도록 *왜 인접해야 하는지* 를 블록 안에 적음 |
| `BlendReferenceAt` | `@param` 3 + `@return` **신규 작성** |
| `ResolveSigma` | `@param` 4 + `@return` **신규 작성** — `w=0.10`·tile128 이 `QA-A-164` 의 측정 결과이고 자유 손잡이가 아니라는 것을 `@return` 에 적음 |
| `BuildFrameConfig` | `@param` 은 이미 맞았음. **`@return` 만 추가** — `globalSigmaFloor`/`Cap` 을 0 으로 두는 이유(둘째 전면 통과를 건너뛰는 근거)까지 |

**주석을 지운 곳은 없습니다.** `#143` 에서 쌓인 판단(왜 타일인지, `QA-A-62` 가 왜 복사본을 지웠는지)은 전부 그대로입니다.

---

## 2. §4 반증 — **로컬이 CI 와 같은 설정으로 돌았습니다**

`doxygen` 이 **설치돼 있지 않았습니다**(`doxygen: command not found`). `winget` 설치는 UAC 승격 때문에 실패했으므로, CI 가 핀한 **같은 버전의 portable 압축본**을 받아 썼습니다.

| 항목 | CI (`docs-generate.yml`) | 로컬 |
|---|---|---|
| 버전 | `ssciwr/doxygen-install@v2`, `version: "1.12.0"` | **`1.12.0`** (`c73f5d30f9e8`) |
| 명령 | `doxygen Doxyfile`, cwd `docs/help/doxygen` | 같음 |
| 설정 | 저장소의 `Doxyfile` (`WARN_AS_ERROR = FAIL_ON_WARNINGS`, `WARN_IF_UNDOCUMENTED = YES`) | **같은 파일** |
| `doxygen-awesome` | 워크플로가 clone | 같은 저장소를 clone |

### 세 실행

| # | 대상 | `EXIT` | `error:` |
|---|---|---|---|
| **① 양성 대조군** | 수정 **전** 헤더 (= `271ba54` 이후의 main) | **1** | **11** |
| **② 수정 후** | 이 커밋 | **0** | **0** |
| **③ 반증 주입** | ② 에서 `@param mad` → `@param madZZZ` 한 글자 | **1** | **4** |

①의 11건은 **카드 §2 가 옮긴 CI 오류 목록과 같습니다** — 로컬이 CI 의 실패를 재현합니다.

③은 `argument 'madZZZ' … is not found in the argument list of … ResolveSigma` 와 `The following parameter of … ResolveSigma is not documented` 를 냈습니다. **되돌리면 다시 0건**입니다(네 번째 실행으로 확인).

> **그래서 ②의 0건은 의미가 있습니다.** 카드가 지적한 대로 *"고쳤을 것"* 이 아니라, 같은 도구·같은 버전·같은 설정이 (a) 원래 실패를 재현하고 (b) 새로 주입한 손상을 잡아내는 것을 본 뒤의 0건입니다.

### 이 반증이 잡아낸 제 실수

②를 처음 돌렸을 때 **오류가 2건 남았습니다.** 원인은 제가 `DetectDefectivePixel` 블록에 적은 설명 문장이었습니다 — 산문 안에 쓴 `@param` 이라는 낱말을 Doxygen 이 **명령으로 파싱**해 `argument 'names' … is not found` 를 냈습니다. 문구를 바꿔 해결했습니다(`attached the parameter list below to that function`).

**반증을 안 돌렸으면 11건을 2건으로 줄이고 초록이라 보고했을 것입니다.**

---

## 3. §5 의 답 — **두 겹의 공백이었습니다**

### (a) 레인 검증에 문서 생성이 없습니다 — 카드의 추측이 맞습니다

`271ba54`(`QA-A-164`)를 올릴 때 돌린 것은 전부입니다:

```
cmake --build build\ci-preprocess --config RelWithDebInfo        → BUILD_EXIT
ctest  --test-dir build\ci-preprocess -C RelWithDebInfo          → CTEST_EXIT
```

그리고 단일 프로세스 4순서(`--gtest_shuffle` 3종 + 기본). **doxygen 단계는 없습니다** — 명령도 없고, 그때 이 기계에 **doxygen 자체가 설치돼 있지 않았습니다**(이 카드에서 처음 받았습니다). 그러니 *"돌렸는데 못 잡았다"* 가 아니라 **돌릴 수 없었습니다.**

이것은 이미 이 저장소가 적어 둔 형태입니다 — *"빌드 스크립트가 한 타깃만 짓는다"*. 레인 검증은 **컴파일과 런타임만** 보고, **문서는 보지 않습니다.**

### (b) 그 공백을 메우려고 만든 검사기가 이 함수들을 **구조적으로 못 봅니다**

`#159` 가 바로 이 사고를 막으려고 `tools/docs/check_header_docs.py` 를 넣었고, `ci.yml:70-74` 가 그 이유를 이렇게 적습니다:

> *"Doxygen runs with WARN_AS_ERROR in a separate workflow, and lane sessions have no doxygen installed, so a mismatched @param is found after the merge."*

**진단은 정확했습니다. 그런데 이번 결함을 못 잡았습니다.**

| 실행 | 대상 | 결과 |
|---|---|---|
| `python tools/docs/check_header_docs.py` | main (`f112ccf`, **깨진 헤더 포함**) | `20 headers, 0 declarations skipped, 0 findings` / `EXIT=0` |
| `doxygen Doxyfile` | 같은 트리 | `EXIT=1`, **11건** |

이유는 그 스크립트의 선언 정규식입니다:

```python
r"(?:extern\s+\"C\"\s+)?XPE_API\s+...\b(?P<name>xpe_[a-z0-9_]+)\s*\(..."
```

**`XPE_API` 로 내보내는 `xpe_*` C ABI 함수만** 봅니다. 이번에 깨진 넷은 전부 `xpe::preprocess::internal` 안의 **`inline` C++ 함수**라 이 정규식에 걸리지 않습니다. `0 declarations skipped` 가 그 증거입니다 — **건너뛴 것이 아니라 애초에 후보가 아니었습니다.** 게다가 스크립트가 세는 세 가지 부류(문서 없음 / 없는 인자 이름 / 중복 이름)에는 **`@return` 누락**과 **블록–선언 인접성**이 아예 없습니다.

> 그래서 `0 findings` 는 "문서가 맞다" 가 아니라 **"이 도구가 볼 수 있는 범위에 문제가 없다"** 였고, 그 차이가 5회 푸시 동안 보이지 않았습니다. 이 저장소의 *"부재 단언에는 대조군이 필요하다"* 와 같은 형태입니다 — 여기서는 대조군이 있었더라도(`XPE_API` 함수 하나를 망가뜨리면 잡힙니다) **판별 범위가 대상을 포함하지 않았습니다.**

### 보고 (메우는 것은 리더 소유)

`.github/workflows/` 와 `tools/` 는 제 소유가 아니므로 **관측만** 적습니다. 리더가 고를 수 있는 선택지는 최소 셋입니다:

1. **`ci.yml` 의 text-lint 에 doxygen 을 넣는다** — 가장 확실하지만 러너에 doxygen 설치 시간이 듭니다. 지금 `docs-generate.yml` 이 별도 워크플로인 이유가 그것으로 보입니다
2. **`check_header_docs.py` 의 범위를 넓힌다** — `XPE_API xpe_*` 외에 `inline` 선언까지. 다만 `@return` 누락과 블록 인접성은 새 규칙이라 **doxygen 을 다시 구현하는 방향**입니다
3. **`docs-generate.yml` 을 게이트에 넣는다** — 반나절 못 읽힌 원인이 "목록에 없었다" 이므로, 게이트가 되면 읽을지 여부가 문제가 아니게 됩니다

**어느 것도 제가 고르지 않았습니다.** 다만 `2` 는 `1`·`3` 의 대체가 아니라는 점만 적어 둡니다 — 이번 결함 중 **`@return` 누락 3건과 블록 인접성 1건**은 `2` 를 그대로 확장해도 잡히지 않습니다.

---

## 4. 검증

| 항목 | 명령 | 결과 |
|---|---|---|
| Doxygen (CI 와 동일 버전·설정) | `doxygen Doxyfile` @ `docs/help/doxygen` | **`EXIT=0`, error 0, warning 0** |
| 빌드 | `cmake --build build\ci-preprocess --config RelWithDebInfo` | **`BUILD_EXIT=0`** |
| 전체 시험 | `ctest --test-dir build\ci-preprocess -C RelWithDebInfo` | **`100% tests passed, 0 failed out of 775`** |
| **동작 변경** | — | **없음.** 주석 전용 변경 |

## 5. 하지 않은 것 (카드 §6)

- 주석 **삭제 없음** — 옮기고 더했습니다
- `runtime_detection.h` 의 **동작 손대지 않음**(주석 외 실행 코드 0줄)
- **워크플로·`tools/` 수정 없음** — 리더 소유
- **새 이슈 없음**

## 6. 미검증 / 잔여 위험

- **`docs-generate.yml` 의 나머지 잡은 돌리지 않았습니다.** `doxygen-native` 만 재현했고, `docfx-managed` 와 번들 잡은 손대지 않았습니다 — CI 가 그 둘도 실패하고 있었는지 **확인하지 않았습니다.** 카드가 준 오류 목록은 전부 `doxygen-native` 것입니다
- **다른 모듈 헤더도 이 형태일 수 있는지 훑지 않았습니다.** ②의 0건은 `Doxyfile` 의 `INPUT` 전체(20 헤더)를 돈 결과이니 **지금은 없습니다**. 다만 같은 사고(선언과 블록 사이에 삽입)를 막는 것은 여전히 사람 손입니다
- **`1.12.0` 은 CI 핀이지 최신이 아닙니다.** `winget` 이 제시한 것은 `1.18.0` 이었습니다. 버전이 올라가면 오류 집합이 달라질 수 있고, 그것은 확인하지 않았습니다
- **CI 에서 실제로 초록이 되는 것은 아직 보지 못했습니다** — 푸시가 리더 몫이므로, 이 보고의 초록은 **로컬 재현**입니다. `#144` 형태(로컬값을 CI 결론으로 읽는 것)를 피하기 위해 명시합니다
- 내려받은 doxygen 은 `%TEMP%\dox1120\` 에 있고 **설치하지 않았습니다** — PATH 에도 없으므로, 다른 세션이 같은 반증을 하려면 같은 절차가 필요합니다

🗿 MoAI
