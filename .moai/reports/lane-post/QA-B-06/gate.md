# QA-B-06 — /WX 전환 (Lane B 6개 모듈, #106)

**레인**: Lane B (`dev/postprocess`)
**헬퍼**: `cmake/CompilerWarnings.cmake` `xpe_target_warnings_as_errors()` (main 4c90a1d, 이미 트리에 있음)
**참조**: Lane A `modules/preprocess/CMakeLists.txt:79,205` 호출 패턴

---

## 0. 선행 조건에 대한 기록 — `git merge main` 미수행

leader 가 착수 전 `git merge main (ae62ff6)` 을 지시했으나 **수행하지 않았다.**

- 이 워크트리의 지시 파일(`CLAUDE.local.md`)에 `git merge main` 은 **"사용자 승인 대기"** 로 걸려 있다.
  피어 세션의 지시는 사용자 승인이 아니므로 이 항목을 해제하지 못한다.
- 실무상 문제가 없음을 확인했다: 이 카드가 요구하는 헬퍼
  `xpe_target_warnings_as_errors()` 는 **이미 이 트리에 존재한다**
  (`cmake/CompilerWarnings.cmake`, 루트 `CMakeLists.txt:31` 에서 include).
  f7ecd33 merge 시점에 들어왔다. 머지 없이 카드 전량을 수행했다.

머지가 필요해지면 사용자 승인 후 별건으로 처리한다.

---

## 1. 주장 (Claim)

1. Lane B 소유 6개 모듈의 경고 실측: **dicom 6건, 나머지 5개 모듈 0건**.
2. dicom 6건은 **전부 우리 코드 유래** — 서드파티 헤더 유래 0건. 6건 모두 해소.
3. `/WX-` 15곳 + 무조건 `/WX` 1곳을 `xpe_target_warnings_as_errors()` 로 전환.
4. 전환한 타깃은 `XPE_WARNINGS_AS_ERRORS=ON` 에서 **실제로 경고에 빌드가 실패한다** (관측).
5. `xpe_ai_tests` 의 `/wd4996` 억제를 **제거**했다 — 덮은 것이 아니라 원인을 고쳤다.
6. ctest 무회귀.

---

## 2. 1단계 측정 — 이전 측정을 재확인했다

카드 지시대로 "0" 을 재측정했다. 이전 0 은 dicom 이 빌드 불가하던 시점의 것이라
그대로 쓸 수 없다. 각 빌드 디렉터리의 모듈 오브젝트를 지우고 **강제 재컴파일**했다
(증분 빌드는 경고를 다시 출력하지 않으므로 재측정이 되지 않는다).

| 빌드 | 대상 모듈 | 컴파일/링크 스텝 | warning | 로그 |
|---|---|---|---|---|
| `ci-post` | gsvg, enhance_basic, enhance_advanced, display, e2e | 59 | **0** | `_build_post.log` |
| `ci-dicom` | dicom | 15 | **6** | `_build_dicom.log` |
| `ci-ai` | ai | 15 | **0** | `_build_ai.log` |

`ci-dicom` 6건 + `BUILD_EXIT=0` — **경고가 있는데 빌드가 통과한다**는 #106 의 증상이
이 카드 시점에도 그대로 재현됐다. `/WX-` 가 옵션을 무력화하고 있었다는 직접 증거다.

### 2.1 첫 dicom 측정이 실패한 원인 (기록)

첫 시도는 `BUILD_EXIT=1` 로 끝났다. 컴파일 실패가 아니라
`gtest_discover_tests` 가 빌드 시점에 테스트 exe 를 실행하는데 DCMTK/OpenJPEG
런타임 DLL 이 PATH 에 없어 실행이 실패한 것이다. QA-B-02 의 `_run.bat` 처럼
`vcpkg_installed/x64-windows/bin` 을 PATH 앞에 넣어 해소했다
(`_build_dicom.bat`). **경고 6건은 두 측정에서 동일**하다.

---

## 3. 2단계 — dicom 경고 6건 분류와 해소

카드 지시대로 **유래를 먼저 확인**했다. 서드파티(DCMTK/OpenJPEG) 헤더 유래는 **0건**이었다.
따라서 `SYSTEM` include 처리 대상은 없고, 6건 모두 우리 코드다.

| # | 위치 | 코드 | 분류 | 처리 |
|---|---|---|---|---|
| 1 | `src/dicom.cpp:10` | C4005 `XPE_DLL_EXPORT` 매크로 재정의 | 우리 코드 · 관례 이탈 | `#ifndef` 가드 추가 |
| 2 | `src/DicomValidator.cpp:46` | C4189 `parseOk` 미사용 지역변수 | 우리 코드 · 죽은 변수 | 선언 제거 |
| 3-6 | `tests/test_dicom_{reader,writer,validator,network_scu}.cpp` | C4996 `strncpy` deprecated | 우리 테스트 코드 | `std::snprintf` 로 교체 |

### 3.1 #1 — 왜 dicom 만 경고가 났는가

`XPE_DLL_EXPORT` 를 소스에서 `#define` 하는 모듈은 4개다. 그런데 셋은 경고가 없다:

```
modules/ai/src/ai.cpp:23              #ifndef XPE_DLL_EXPORT / #define / #endif
modules/common/src/xpe_common.cpp:11  (동일)
modules/enhance_basic/src/enhance_basic.cpp:5  (동일)
modules/dicom/src/dicom.cpp:10        #define  (가드 없음)  <- 유일한 예외
```

루트 `CMakeLists.txt:14` 가 `add_compile_definitions(XPE_DLL_EXPORT)` 로
이미 정의(`=1`)하므로, 가드 없는 빈 `#define` 은 값이 달라 C4005 가 된다.
**`#define` 을 지우는 것이 아니라 나머지 3개와 같은 `#ifndef` 가드를 씌웠다** —
지웠다면 CMake 가 정의를 주지 않는 단독 빌드에서 export 가 깨진다.

### 3.2 #2 — 미사용인가, 빠진 분기의 흔적인가

`parseOk` 는 선언만 있고 참조가 0건이다(`grep -n parseOk` → 46행 1건).
"설정을 빠뜨린 것" 인지 판단하기 위해 `validate()` 전체를 읽었다: 파싱 실패 /
메타정보 없음 / 데이터셋 없음 3개 경로가 **모두 그 자리에서 early return** 한다.
플래그가 전달할 상태가 없다 — 제어 흐름이 이미 그 정보를 표현하고 있다.
빠진 분기의 흔적이 아니라 잔재이므로 선언을 제거했다.

### 3.3 #3-6 — 억제가 아니라 교체

라이브러리 쪽 `DicomValidator.cpp` 도 `std::strncpy` 를 쓰는데 경고가 나지 않는다
(DCMTK 를 링크하며 따라오는 정의 때문으로 보인다). 즉 이 4건은 **테스트 타깃에만
전파가 없어서 드러난 것**이다. 이때 `_CRT_SECURE_NO_WARNINGS` 를 테스트에 붙이면
"전환이 아니라 은폐" 가 된다. 대신 리포지토리에 이미 있는 관례
(`enhance_basic` 테스트의 `set_body_part()` → `std::snprintf`) 로 교체했다.

**결과: dicom warning 6 → 0** (`_build_dicom_after.log`, `BUILD_EXIT=0`).

---

## 4. 3·4단계 — 전환 목록

패턴은 Lane A 와 동일하다: `if(MSVC)` 블록에서 `/WX-` 만 떼고 `/W4` 는 유지,
`endif()` 뒤에 `xpe_target_warnings_as_errors(<target>)` 를 둔다
(헬퍼가 비-MSVC 의 `-Werror` 도 처리하므로 `if(MSVC)` 안에 두면 안 된다).

### 4.1 전환함 (16곳)

| 파일 | 타깃 | 종류 |
|---|---|---|
| `modules/dicom/CMakeLists.txt` | `xpe_dicom` | 라이브러리 |
| `modules/display/CMakeLists.txt` | `xpe_display` | 라이브러리 |
| `modules/gsvg/CMakeLists.txt` | `gsvg` | 라이브러리 |
| `modules/enhance_advanced/CMakeLists.txt` | `${MODULE_NAME}` (`xpe_enhance_advanced`) | 라이브러리 · §5 판단 |
| `modules/dicom/CMakeLists.txt` | `test_dicom_{reader,writer,validator,network_scu}` | 테스트 ×4 |
| `modules/display/CMakeLists.txt` | `test_display_{modality_lut,voi_lut,presentation_lut,integration}` | 테스트 ×4 |
| `modules/gsvg/CMakeLists.txt` | `gsvg_tests` | 테스트 |
| `modules/enhance_basic/CMakeLists.txt` | `xpe_enhance_basic_tests` | 테스트 |
| `modules/enhance_advanced/CMakeLists.txt` | `test_${MODULE_NAME}` | 테스트 · §6 판단 |
| `modules/ai/CMakeLists.txt` | `xpe_ai_tests` | 테스트 · §7 |
| `tests/e2e_post_pipeline/CMakeLists.txt` | `test_e2e_post_pipeline` | 테스트 · 카드 목록 밖 |

카드가 준 13곳에 더해 2곳이 있었다:

- `modules/enhance_advanced:53` — `/WX-` 가 아니라 **무조건 `/WX`**. 카드가 판단을 요구한 건(§5).
- `tests/e2e_post_pipeline/CMakeLists.txt:50` — 카드 목록에 없던 `/WX-`.
  `ci-post` 에서 실제로 빌드되고 warning 0 이라 같은 기준으로 전환했다.

카드의 행 번호와 실제 행 번호는 어긋난다(예: 카드 `dicom:64` → 실제 72).
QA-B-02/QA-B-05 이후 파일이 바뀌었기 때문이며, 타깃 이름으로 특정해 전환했다.

### 4.2 전환하지 않음 (1곳) — 사유

| 파일 | 타깃 | 사유 |
|---|---|---|
| `tests/ai_tests/CMakeLists.txt:54` | `xpe_ai_tests` (중복 정의) | **어느 빌드에서도 빌드되지 않는다.** `modules/ai:259` 와 같은 타깃명이 양쪽에 정의돼 있고(#113 / QA-B-07 의 주제), 현재 살아 있는 정의는 `modules/ai` 쪽이다. 빌드되지 않는 타깃을 "경고 0 이므로 전환" 한다고 주장하면 **측정하지 않은 것을 측정했다고 하는 것**이 된다. 정본 판정(QA-B-07)이 끝난 뒤 남는 쪽을 전환하는 것이 맞다. |

**주의**: `modules/ai` 의 `xpe_ai_tests` 는 소스를 `tests/ai_tests/*.cpp` 에서 가져온다.
즉 두 CMakeLists 가 **같은 소스를 두 타깃명으로 다투는** 구조다 — QA-B-07 에 넘긴다.

### 4.3 범위 밖으로 남긴 관측 (lead 판단용)

`modules/enhance_basic/CMakeLists.txt:43` 의 `xpe_enhance_basic` **라이브러리**는
`/W4` 만 있고 `/WX-` 도 헬퍼 호출도 없다. 카드의 13곳 목록에 없어 손대지 않았으나,
결과적으로 이 라이브러리만 경고-오류화 대상 밖에 남는다. 후속 카드 후보.

---

## 5. `enhance_advanced:53` 판단 — 전환한다

카드가 답을 정해두지 않은 건이다. **전환을 택했고**, 근거는 다음과 같다.

### 5.1 무엇이 걸려 있었나

```cmake
if(MSVC)
    target_compile_options(${MODULE_NAME} PRIVATE /W4 /WX   # 옵션과 무관하게 항상
        /wd4100 /wd4127 /wd4244 /wd4245 /wd5054 /wd4365)    # Eigen 소음 억제
else()
    target_compile_options(${MODULE_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror ...)
endif()
```

MSVC 의 `/WX` 와 GCC/Clang 의 `-Werror` **양쪽 다** 무조건이었다. 전환하면
`XPE_WARNINGS_AS_ERRORS=OFF` 일 때 현재보다 느슨해진다 — 카드가 지적한 그대로다.

### 5.2 느슨해지는 범위를 실측했다

느슨해지는 것은 **옵션이 OFF 인 빌드에 한한다.** 옵션이 ON 인 빌드에서는 동작이 동일하다.
그래서 "OFF 인 빌드가 무엇인가" 를 확인했다:

```
CMakePresets.json:  XPE_WARNINGS_AS_ERRORS": "ON"  x 4  (ci-common / ci-post / ci-preprocess / ci-fullstack)
.github/workflows/: 모든 빌드가 위 preset 을 경유 (ci.yml, codeql.yml, benchmark-regression.yml,
                    windows-common-build.yml — 전부 `cmake --preset ci-*`)
```

**CI 는 전부 ON 이다.** 즉 머지를 막는 관문에서의 보증은 전환 전후가 같다.
느슨해지는 것은 개발자 로컬의 기본 빌드뿐이고, 거기서 `/WX` 가 하는 일은
"새 컴파일러가 낸 새 경고 때문에 빌드가 안 되는 것" 이다 — 안전이 아니라 마찰이다.

### 5.3 반대 방향의 비용

이 모듈만 프로젝트 스위치를 무시한다는 것은, #106 이 지적한 문제
("옵션이 이름값을 못 한다")의 **거울상**이다. 한쪽은 옵션을 켜도 안 걸리고,
다른 한쪽은 옵션을 꺼도 걸린다. 어느 쪽이든 옵션을 읽고 동작을 예측할 수 없다.
스위치 하나가 진실이 되는 편이 낫다.

`/wd*` 억제 6종은 그대로 뒀다 — Eigen 3.4.0 유래이고 사유가 행마다 적혀 있으며,
경고-오류화 여부와 독립된 축이다.

---

## 6. `enhance_advanced:124` 사유 판단 — 사유가 만료됐다

주석에 적힌 사유:

> tests include xpe_types.h which has int→uint32_t assignments;
> keeping /WX on tests adds noise without safety gain

카드 지시대로 **사유가 여전히 유효한지 확인**했다. `ci-post` 에서
`test_xpe_enhance_advanced` 를 강제 재컴파일한 결과 **warning 0건**이다
(`_build_post.log`, 59스텝 중 해당 타깃 링크 포함, 경고 매칭 0).
`/WX-` 상태였으므로 경고가 있었다면 출력됐어야 한다 — 출력이 없으니 사유가
말하는 소음은 현재 존재하지 않는다.

사유가 만료됐으므로 전환하고, 주석은 만료 사실을 적는 문구로 교체했다.
**주석을 지우지 않고 남긴 이유**: 나중에 이 자리를 보는 사람이 "왜 여기만 사유가
있었다가 사라졌나" 를 다시 조사하지 않도록.

---

## 7. `xpe_ai_tests` — 억제를 제거했다

원래: `target_compile_options(xpe_ai_tests PRIVATE /W4 /WX- /wd4996)`

`/wd4996` 이 아직 필요한지 실측했다. 제거하고 빌드하니 **필요했다**:

```
tests\ai_tests\test_ai_fallback.cpp(152):        warning C4996  -> error C2220
tests\ai_tests\test_ai_worker_isolation.cpp(108): warning C4996 -> error C2220
===BUILD_EXIT=1===
```
(`_probe_ai_no4996.log`)

내용은 `std::strcpy(meta.bodyPart, "CHEST")` — 길이 상한이 없는 진짜 위험 함수다.
억제를 되돌리는 대신 dicom 과 같은 방식으로 `std::snprintf` 로 교체했다.
재빌드 결과 `/wd4996` 없이 warning 0, `BUILD_EXIT=0` (`_build_ai_after.log`).

**부수 소득**: 이 프로브가 전환의 실효성을 먼저 증명했다 — `/WX` 가 걸린 타깃에서
경고가 `error C2220` 으로 승격돼 빌드가 죽었다.

---

## 8. 합격 조건 #1 검증 — 전환이 실제로 무는가 (관측)

§7 은 우연히 얻은 관측이므로, **의도한 프로브**를 라이브러리·테스트 각각에 하나씩
넣어 별도로 확인했다. 주입한 것은 `static void xpe_wx_probe_unreferenced() {}`
(미참조 정적 함수 → C4505, /W4 레벨).

```
gsvg.cpp(236):             error C2220 / warning C4505     <- 라이브러리 타깃 gsvg
test_log_transform.cpp(183): error C2220 / warning C4505   <- 테스트 타깃 xpe_enhance_basic_tests
FAILED x2 ,  exit=1
```
(`_probe_wx.log`)

프로브는 원복했고 (`grep -c xpe_wx_probe` → 두 파일 모두 0), 최종 빌드는
프로브 없는 상태에서 warning 0 / exit 0 이다 (`_build_post_after.log`).

**첫 프로브는 실패했다는 기록**: 처음엔 `static int xpe_wx_probe_unused = 0;`
(파일 스코프 미사용 변수)를 썼는데 빌드가 통과했다. MSVC /W4 는 이것을 경고하지
않는다. "프로브를 넣었는데 통과했다" 를 "전환이 안 됐다" 로 읽지 않고
**프로브가 약했다**로 진단한 것이 맞았다 — 프로브를 바꾸자 즉시 실패했다.

---

## 9. baseline 귀속 — ctest 무회귀

측정 범위가 다르면 숫자가 달라지므로, **카드가 준 기준과 같은 필터**로 맞췄다.

| 대상 | baseline (출처) | 현재 | 판정 |
|---|---|---|---|
| `ci-post` 전체 | 281 (`QA-B-01/_ctest_post.log`) | **282** | +1 = QA-B-05 신규 테스트 1건. 회귀 아님 |
| `ci-dicom -R "Dicom"` | 43 (카드, `QA-B-02`) | **43** | 동일 |
| `ci-ai -R "Ai\|AI"` | 108 (`QA-B-01/_gate_ai.log`) | **108** | 동일 |
| `ci-dicom` 전체 | — | 100 passed / 0 failed | 신규 측정(공통 테스트 포함) |
| `ci-ai` 전체 | — | 166 passed / 0 failed | 신규 측정(공통 테스트 포함) |

실행 로그: `_ctest_all.log` (`POST_EXIT=0`, `DICOM_EXIT=0`, `AI_EXIT=0`),
카운트 로그: `_count.bat` / `_count2.bat` 출력.

> 카드의 "ai 108/108, dicom 43/43" 은 **필터를 건 부분 집계**였다. 전체 디렉터리
> 집계(166 / 100)와 혼동하면 회귀로 오독된다. 실제로 처음 전체 집계를 보고
> 회귀를 의심했고, `-N` 으로 필터별 카운트를 재봐서 일치를 확인했다.

빌드 경고: 세 빌드 모두 최종 상태 warning 0 / error 0.

---

## 10. 미검증 (Gaps)

- **비-MSVC 경로 미실행.** 전환은 `-Werror` 도 함께 옮기지만 Windows 에서만 측정했다.
  GCC/Clang 에서 `xpe_target_warnings_as_errors()` 가 기존 `-Werror` 와 같게
  동작하는지는 확인하지 않았다. 특히 `enhance_advanced` 는 무조건 `-Werror` 를
  잃으므로, Linux CI 가 옵션 ON 으로 도는지 별도 확인이 필요하다.
- **`XPE_WARNINGS_AS_ERRORS=OFF` 빌드 미실행.** OFF 에서 헬퍼가 아무것도 하지 않는
  경로는 코드로만 확인했고 빌드로 관측하지 않았다.
- **단독(standalone) 모듈 빌드 미실행.** 각 모듈 CMakeLists 는 이제 루트가 include 하는
  `xpe_target_warnings_as_errors()` 를 호출한다. 모듈 디렉터리를 단독 `project()` 로
  구성하면 함수가 없어 configure 가 실패할 수 있다. Lane A 의 preprocess 도 같은
  구조이므로 새 노출은 아니지만, 어느 쪽도 검증되지 않았다.
- **`tests/ai_tests/CMakeLists.txt:54` 미전환** (§4.2). 빌드되지 않아 측정 불가.
- **`xpe_enhance_basic` 라이브러리 미전환** (§4.3). 카드 범위 밖.
- **`/wd*` 억제 잔존**: `enhance_advanced` 6종(Eigen), `modules/ai:135,196` 의
  `/wd4996 /wd4150`. 이번 카드에서 필요성을 실측한 것은 `xpe_ai_tests` 의 `/wd4996`
  하나뿐이다. 나머지는 유효성 미확인.
- **`git merge main` 미수행** (§0). ae62ff6 이후 main 변경이 이 전환과 충돌하는지 미확인.

## 11. 잔여 위험 (Residual risk)

- 전환된 타깃은 이제 **새 컴파일러/새 라이브러리 버전이 낸 경고 하나로 CI 가 막힌다.**
  이것이 의도한 동작이지만, 툴체인 업그레이드 시 비용이 발생한다.
- `enhance_advanced` 는 옵션 OFF 로컬 빌드에서 보증이 없어졌다(§5). CI 가 ON 인 것에
  의존하는 구조이므로, 누군가 preset 에서 옵션을 끄면 조용히 보증이 사라진다.
- C4996 교체는 `snprintf` 로 잘림 동작이 바뀔 수 있으나, 대상이 전부 `"CHEST"` /
  `"HAND"` 같은 짧은 리터럴이라 잘림이 발생하지 않는다. 테스트 166/100/282 통과가 이를 뒷받침한다.

---

## 12. 합격 조건 대조

| 조건 | 결과 | 근거 |
|---|---|---|
| 전환한 타깃이 ON 에서 실제로 경고에 실패 (관측) | PASS | §8 (의도한 프로브, 라이브러리+테스트 각 1), §7 (우연 관측) |
| ctest 무회귀 (ci-post 281/281, ai 108/108, dicom 43/43) | PASS | §9 — 282(+1 신규) / 108 / 43 |
| 전환하지 않은 타깃마다 사유가 gate.md 에 | PASS | §4.2 (1곳), §4.3 (범위 밖 관측 1곳) |
| `enhance_advanced:53` 판단에 근거 | PASS | §5 |
