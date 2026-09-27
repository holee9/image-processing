# QA-B-152 (`#130`) — 타당성 측정. **의존은 길이 있고, 모델은 없습니다**

제품 코드는 **한 줄도 바꾸지 않았습니다.**

## 1. 판정 재료 요약

| §3 항목 | 관측 |
|---|---|
| 1 의존을 구할 수 있는가 | **길이 있습니다.** 저장소는 **vcpkg 를 씁니다**(카드 전제와 반대). 고정 baseline 에 `onnxruntime 1.23.2` 포트 실재. 다만 **이 기계에 vcpkg 가 없습니다** |
| 2 x86/x64 | **x64 입니다.** PE 헤더 실측 `machine=0x8664` |
| 3 스텁 해제 시 무엇이 깨지는가 | **아무것도 안 깨집니다 — 그게 문제입니다.** 옵션을 켜도 CMake 가 조용히 스텁으로 되돌리고 `===BUILD=0===` 이 납니다 |
| 4 모델이 있는가 | **없습니다.** 저장소 전체에 `*.onnx` **0건** |

**리더의 셋 중 둘째입니다** — 의존은 되는데 모델이 없습니다. 다만 §5 의 버전
불일치가 하나 더 붙습니다.

## 2. 전제 확인 — 3건 중 2건이 드리프트했습니다

| # | 전제 | 결과 |
|---|---|---|
| 1 | `CMakeLists.txt:26-27` 이 `USE_ONNXRUNTIME=OFF`·`STUB_BUILD=ON` | **참** |
| 2 | `ci-ai` 192/192 가 스텁 경로의 통과다 | **주장은 참, 수치는 낡음** — §4 |
| 3 | `AiIpcBridgeTest` 10건이 IPC 계약을 잰다 | **참** — 10건이고, 전부 **작업자 없는 상태**의 계약입니다 |

### 그리고 카드 §3-1 의 전제가 틀렸습니다

카드는 *"이 저장소는 vcpkg 를 쓰지 않습니다"* 라고 적습니다. **씁니다.**

```
vcpkg.json                     (루트, builtin-baseline b80e0066…, dcmtk·openjpeg 포함)
third_party/vcpkg.json
third_party/common/vcpkg.json
third_party/dicom/vcpkg.json
CMakePresets.json:19,157       CMAKE_TOOLCHAIN_FILE = $env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
```

**다만 좁습니다.** `ci.yml:768` 이 *"vcpkg is used ONLY by coverage-dicom"* 이라 적고,
러너 이미지가 `C:\vcpkg` 로 제공하는 것을 씁니다(`:772-786`). **저장소가 vcpkg 를
동반하지 않습니다.**

## 3. 항목 2 — 아키텍처는 x64 (읽기 아니라 실측)

프리셋을 읽지 않고 **PE 헤더**를 봤습니다:

```
build/ci-post/bin/xpe_display.dll  machine=0x8664  → x64
```

카드가 걱정한 *"과거 x86 빌드라 x64 계측 불가"* 전례는 **지금 빌드에 해당하지
않습니다.** ONNX Runtime x64 Windows 배포본과 아키텍처가 맞습니다
(`supports: "!uwp"`).

## 4. 항목 3 — **옵션을 켜도 스텁이 나옵니다**

요청과 결과가 다릅니다. `-DXPE_AI_USE_ONNXRUNTIME=ON -DXPE_AI_STUB_BUILD=OFF` 로
구성했는데:

```
  ONNX Runtime not found.  Set ONNXRUNTIME_ROOT or install onnxruntime.   ← WARNING 뿐
===CONFIGURE=0===
BUILD_AI:BOOL=ON
XPE_AI_STUB_BUILD:BOOL=ON        ← OFF 로 줬는데 ON
XPE_AI_USE_ONNXRUNTIME:BOOL=OFF  ← ON 으로 줬는데 OFF
===BUILD=0===
```

`CMakeLists.txt:72-74` 가 `set(... CACHE BOOL ... FORCE)` 로 **조용히 되돌립니다.**
경고 한 줄만 나고 **빌드는 성공합니다.**

### 링크까지 확인했습니다 (카드 §4 지시)

`dumpbin` 대신 PE 임포트 테이블을 직접 읽었습니다.

```
build/try-onnx/bin/xpe_ai.dll imports (14):
  xpe_common.dll, spdlog.dll, KERNEL32.dll, MSVCP140.dll, VCRUNTIME140.dll,
  VCRUNTIME140_1.dll, api-ms-win-crt-*.dll (8)
  → onnxruntime 링크: False
```

**"옵션만 켜지고 코드가 안 바뀌었을 수 있다" 가 실제로 그렇습니다.**

> **이것이 `#130` 의 숨은 함정입니다.** 누가 `USE_ONNXRUNTIME=ON` 을 켜고 초록을
> 보면 *"연동됐다"* 로 읽습니다. 빌드 로그의 `WARNING` 한 줄만이 신호이고,
> `===BUILD=0===` 은 아무것도 반박하지 않습니다.
>
> **바꿀 것(리더 소유)**: `USE_ONNXRUNTIME=ON` 인데 런타임을 못 찾으면
> **`FATAL_ERROR`** 로 멈춰야 합니다. 명시적으로 켠 것이 조용히 꺼지는 것은
> 스텁 빌드가 기본인 상황에서 특히 위험합니다.

## 5. 버전이 **세 곳에서 다릅니다**

| 출처 | 버전 | 성격 |
|---|---|---|
| `XPE-SOUP-001` `SOUP-003` 행 (`:22`) | **1.17.x** | SOUP 등록 |
| 같은 문서 이상 대응 (`:61`) | "Version ≥ 1.17 사용" | 감시 계획 |
| **`REQ-AI-006`** (`SPEC-XPE-P3-AI:130`) | **1.20+** | **구속 요구** |
| `modules/ai/CMakeLists.txt:12` 주석 | 1.20+ | 코드 주석 |
| vcpkg 고정 baseline 포트 | **1.23.2** | 실제로 받게 될 것 |

> **SOUP-003 이 등록한 버전(1.17.x)은 그 SOUP 가 뒷받침하는 요구(1.20+)를
> 만족하지 않습니다.** Class B SOUP 기록이 SPEC 이 금지하는 버전을 가리킵니다.
> vcpkg 가 주는 1.23.2 는 요구를 만족하므로, **낡은 것은 SOUP 행입니다.**

문서는 리더 소유라 고치지 않았습니다. `#130` 착수 전에 정리가 필요합니다 —
SOUP 등록 버전은 조달 근거이고, 감사에서 대조되는 자리입니다.

## 6. 항목 1 — 의존의 **크기**

vcpkg `onnxruntime` 포트의 **직접 의존 22개**:

```
abseil, boost-config, boost-mp11, cpuinfo, cxxopts, date, dlpack, eigen3,
flatbuffers(×2: host+target), ms-gsl, nlohmann-json, onnx, optional-lite,
protobuf(×2), re2, safeint, utf8-range, vcpkg-cmake, vcpkg-cmake-config, wil
```

features: `cuda` · `framework` · `openvino` · `tensorrt`.
`SOUP-003` 이 적는 CUDA 12+ 선택은 `cuda` feature 에 대응합니다.

**전이 의존은 세지 않았습니다**(미검증). `abseil`·`protobuf`·`onnx` 는 각각 무거운
쪽이고, vcpkg 로 **소스 빌드**하면 시간이 큽니다. 저장소의 기존 vcpkg 사용
선례(`dcmtk`·`openjpeg`)보다 규모가 다릅니다.

**두 조달 경로가 있습니다**:

| 경로 | 장점 | 단점 |
|---|---|---|
| **vcpkg 포트** | 기존 매니페스트 체계에 들어감. baseline 이 버전을 고정 | 22개 직접 의존 소스 빌드. CI 시간 |
| **공식 사전 빌드 바이너리** | CMake 가 이미 `ONNXRUNTIME_ROOT` 를 지원(`:45-63`). ~200 MB(SOUP 기재와 일치) | 저장소 밖 산출물. 라이선스 파일 동반·버전 고정을 수동 관리 |

CMake 코드는 **후자를 전제로 쓰여 있습니다**(`ONNXRUNTIME_ROOT` 분기가 먼저,
`find_package` 는 뒤). **어느 쪽인지는 리더 판단입니다.**

## 7. 항목 4 — 모델이 없습니다

```
find . -name "*.onnx" -not -path "./build/*"   →  0건
```

`ai_api.h:79` 는 `xpe_ai_init(modelDirPath)` 를 *"Directory containing signed .onnx
model files"* 로 규정하고, `SOUP-001:35` 는 *"Residual U-Net ~50M params"* 를 적습니다.
**그 파일이 저장소에도, 이 워크트리 어디에도 없습니다.**

> **의존을 다 구해도 추론은 못 섭니다.** 그리고 *"signed"* 는 서명 검증 절차까지
> 함의합니다(`REQ-AI-0xx` 계열 미확인 — 미검증으로 둡니다).

## 8. 전제 2 — 스텁 경로임을 **대조군과 함께** 확인

### 부재

추론 API(`recognize` · `denoise` · `bone_suppress` · `stitch` · `infer`)에
`XPE_OK` 를 단언하는 시험 **0건**.

걸린 두 건은 `xpe_stitch_estimate_size`(`test_ai_fallback.cpp:700`,
`test_parameter_dependency.cpp:69`)인데 **기하 계산 헬퍼**이고 추론 경로가
아닙니다(`ai.cpp:480` 부근, 전제 검사 뒤 크기 산술).

### 양성 대조 — **부재가 도구 탓이 아님을 보입니다**

스텁 전용 신호를 단언하는 곳이 4파일 **39건**이고, 이름이 스스로 말합니다:

```
[  OK  ] AiFallbackTest.BodypartRecognizeStubReturnsProcessingFailed
[  OK  ] AiFallbackTest.BodypartRecognizeSetsConfidenceToZero
[  OK  ] AiFallbackTest.KnownDivergence_BodypartRecognizeStubSetsUnknownLabel
```

`ai.cpp:424-432` 의 스텁이 내는 것 그대로입니다 — `confidence = 0`,
라벨 `"UNKNOWN"`, `XPE_ERR_PROCESSING_FAILED`. **시험들이 스텁 경로를 실제로 타고
있고, 그것을 단언합니다.**

### 수치는 낡았습니다

카드의 `192/192` 는 QA-B-32 당시 `build/ci-ai-b20`(프리셋 아님, **임시 빌드
디렉터리**)의 ctest 총계이고 구성은 **XpeCommonTest 58 + DegradedMode 5 + ai 129**
였습니다. 오늘 ai 시험 바이너리 단독은 **156건 전부 통과**입니다(스위트 3개 증가:
`AiDataSizeProbe` · `AiParamDependency` · `AiConfigWarning`).

**`ci-ai` 라는 프리셋은 `CMakePresets.json` 에 없습니다.** 그리고 `ci-post` ·
`ci-fullstack` 은 **`BUILD_AI=OFF`** 입니다 — 제가 지금까지 돌려온 703건에 ai 가
한 건도 없었습니다.

## 9. 바꿀 것 (리더 소유, 목록만)

1. **`USE_ONNXRUNTIME=ON` + 런타임 부재 → `FATAL_ERROR`** (§4). 지금은 조용한 강등
2. **`SOUP-003` 버전을 1.20+ 이상으로 정정** (§5). SPEC 과 모순
3. **조달 경로 결정** — vcpkg 포트 대 사전 빌드 바이너리 (§6)
4. **CI 프리셋** — ai 를 어디서 빌드·시험할지. 지금 `ci-post`·`ci-fullstack` 이
   `BUILD_AI=OFF` 라 ai 시험이 어떤 CI 잡에도 없습니다(§8). **이것 자체가 별건일
   수 있습니다** — 156건이 로컬에서만 돕니다

## 10. 미검증 · 잔여 위험

- **vcpkg 로 실제 빌드해 보지 않았습니다.** 이 기계에 vcpkg 가 없습니다
  (`~/vcpkg`·`C:\vcpkg`·`vcpkg.exe`·`vcpkg_installed` 전부 부재). 포트가
  **존재한다**는 것과 **이 환경에서 선다**는 것은 다릅니다
- **전이 의존 개수·빌드 시간 미측정** (§6)
- **`ONNXRUNTIME_ROOT` 경로도 실제로 시험하지 않았습니다** — 바이너리를 내려받지
  않았습니다. `:45-63` 이 `onnxruntime.lib`/`.dll` 이름을 가정하는데 실제 배포본
  레이아웃과 맞는지 확인 안 됨
- **모델 "서명" 요구를 확인하지 않았습니다** (§7)
- **CUDA EP 는 전혀 보지 않았습니다.** SOUP 가 선택으로 적고 이 기계에 CUDA 유무도
  확인하지 않았습니다
- **156건이 스텁을 탄다는 것은 확인했지만, 그 156건이 무엇을 **덮지 않는지**는
  세지 않았습니다** — 커버리지 측정은 이 카드 범위 밖입니다

## 11. 검증

```
===CONFIGURE=0=== / ===BUILD=0===   (_try_onnx.log — 옵션 무시가 여기 있습니다)
===CONFIGURE=0=== / ===BUILD=0===   (_build_ai.log — BUILD_AI=ON 스텁 빌드)
xpe_ai_tests.exe                    156/156 통과
PE 임포트                            onnxruntime 없음 (§4)
PE machine                           0x8664 = x64 (§3)
vcpkg 포트                            onnxruntime 1.23.2 @ baseline b80e0066 (§6)
*.onnx                               0건 (§7)
```

---

Refs #130
