# QA-A-06 — /WX 전환 (Lane A: common, preprocess)

- **브랜치**: `dev/preprocess` · **Refs**: #106 · **선행**: main `4c90a1d` (헬퍼 제공)
- **결과**: Lane A 4개 타깃 **전부 전환 완료**. `/WX-` 잔여 0.
  전환이 실제로 작동함을 **고의 경고 관측으로 확인**했다.

## 0단계 — 헬퍼 확보

`xpe_target_warnings_as_errors()` 는 main 에만 있고 `dev/preprocess` 에는 없었다.
`cmake/` 는 main 소유라 직접 작성할 수 없으므로 `git merge main` 으로 가져왔다
(병합 커밋 `9f3d3b7`, 헬퍼 커밋 `4c90a1d` 포함).

이 병합은 lead 가 나중에 "하지 말라"고 한 병합과 **다른 것이다**. 시점상 `e909ece`
(`.claude/settings.json` 권한 변경) 이전이며, 실측으로 확인했다:

```
git merge-base --is-ancestor e909ece HEAD  →  포함 안 됨
git show 9f3d3b7 --name-only | grep settings  →  포함 안 됨
```

## 1단계 — 측정 (변경 없이)

`build/ci-preprocess` 를 지우고 클린 리빌드해 **87 TU 전부 재컴파일**한 상태에서 측정했다
(증분 빌드는 재컴파일하지 않아 경고가 보이지 않는다 — 측정으로 쓸 수 없다).

| 타깃 | 종류 | TU | 경고 |
|---|---|---:|---:|
| `xpe_common` | 라이브러리 | 5 | **0** |
| `xpe_preprocess` | 라이브러리 | 30 | **0** |
| `test_xpe_common` | 테스트 | 2 | **0** |
| `xpe_preprocess_tests` | 테스트 | 36 | **0** |

전체 0건. QA-A-02 에서 해소한 3건이 재발하지 않았음을 관측으로 확인했다.
원문: `1-baseline-build.log`

## 2단계 — 라이브러리 전환

```diff
 modules/common/CMakeLists.txt:82
-    target_compile_options(xpe_common PRIVATE /W4 /WX-)
+    target_compile_options(xpe_common PRIVATE /W4)
 endif()
+xpe_target_warnings_as_errors(xpe_common)

 modules/preprocess/CMakeLists.txt:75 부근
 endif()
+
+xpe_target_warnings_as_errors(xpe_preprocess)
```

`xpe_preprocess` 는 `/WX` 도 `/WX-` 도 없어 무방비였다(카드 지적대로). 헬퍼 호출만 추가했다.
빌드 결과: `BUILD_EXIT=0`, 경고 0. 원문: `2-libs-build.log`

## 3단계 — 테스트 전환

전환 전 두 `/WX-` 에 **사유 주석이 있는지 확인했고, 없었다.**
(Lane B `enhance_advanced:124` 에는 "tests include xpe_types.h which has int→uint32_t"
라는 사유가 있으나 Lane A 쪽에는 그런 기록이 없다. 카드가 요구한 확인 항목.)

```diff
 modules/common/CMakeLists.txt:129
-        target_compile_options(test_xpe_common PRIVATE /W4 /WX-)
+        target_compile_options(test_xpe_common PRIVATE /W4)
     endif()
+    xpe_target_warnings_as_errors(test_xpe_common)

 modules/preprocess/CMakeLists.txt:199
-        target_compile_options(xpe_preprocess_tests PRIVATE /W4 /WX-)
+        target_compile_options(xpe_preprocess_tests PRIVATE /W4)
     endif()
+    xpe_target_warnings_as_errors(xpe_preprocess_tests)
```

`grep -n "WX-" modules/common/CMakeLists.txt modules/preprocess/CMakeLists.txt` → **없음**

## 합격 조건 — "켰다"가 아니라 "켜졌다"를 관측

빌드가 통과했다는 사실은 전환이 작동한다는 증거가 **아니다**(경고가 0이므로 꺼져 있어도
통과한다). 고의로 경고를 주입해 빌드가 깨지는지 관측했다.

### 첫 시도 — 실패한 관측 (기록해 둔다)

`modules/preprocess/src/xpe_preprocess.cpp` 에 C4244 를 주입했으나
`BUILD_EXIT=0`, 경고 0, **컴파일된 TU 0개**였다.

원인: 그 파일은 `XPE_PREPROCESS_SOURCES`(CMakeLists:17-53) 에 **없다.** 빌드 대상이 아닌
파일이라 수정해도 아무 일도 일어나지 않는다. "빌드가 안 깨졌다"를 "전환이 안 됐다"로
읽었다면 오진이었을 것이다 — 컴파일 TU 수를 함께 본 것이 오진을 막았다.

**부수 발견**: `modules/preprocess/src/xpe_preprocess.cpp` 는 어느 타깃에도 속하지 않는
사실상 죽은 파일이다. 처분은 본 카드 범위 밖이므로 보고만 한다.

### 라이브러리 관측 (`helpers.cpp`, 실제 소스)

```
BUILD_EXIT=1   컴파일 TU=1
helpers.cpp(107): error C2220: 경고가 오류로 처리됩니다
helpers.cpp(107): warning C4244: '=': 'double'에서 'unsigned __int64'로 변환하면서 데이터가 손실될 수 있습니다
```

### 테스트 관측 (`test_xpe_common.cpp`)

```
BUILD_EXIT=1   컴파일 TU=2
test_xpe_common.cpp(606): error C2220: 경고가 오류로 처리됩니다
test_xpe_common.cpp(606): warning C4244: ...
test_xpe_common.cpp(606): warning C4505: 참조되지 않는 함수 제거됨
```

라이브러리·테스트 양쪽에서 `C2220`(경고→오류)이 발생하고 빌드가 실패했다.
**전환이 실제로 작동한다.** 프로브는 즉시 원복했고 `git status` 로 소스 무변경을 확인했다.
원문: `4-probe-lib.log`, `5-probe-test.log`

## 최종 검증 (프로브 원복 후 클린 리빌드)

```
BUILD_EXIT=0   컴파일 TU=87   warning C####: 0건
ctest --output-on-failure  →  100% tests passed, 0 failed out of 345   CTEST_EXIT=0
```

원문: `6-final-build.log`, `tests.log`

## 전환하지 않은 타깃

없다. Lane A 소유 4개 타깃 전부 전환했다.

## 지키지 않은 것 / 하지 않은 것

- `#pragma warning(disable)` 미사용 — 은폐가 아니라 전환이어야 한다는 원칙대로
- 다른 레인 모듈, `cmake/`, `.moai/specs/` 미수정
- 고아 테스트 트리 삭제, `tests/CMakeLists.txt` 주석 미수정 (#109 대기)
- **`.claude/settings.json` 은 커밋하지 않았다.** 내 변경이 아니다 —
  lead 가 사용자 지시로 수정했다고 통보받았고, 워크트리에 `M` 로 남아 있다.
  권한 설정은 사용자 소유이므로 되돌리지도, 커밋에 포함하지도 않았다.
  (`.claude/settings.json.bak-rmask` 도 lead 가 만든 백업이라 그대로 둔다.)

## 미검증 (Gaps)

- **비MSVC 경로 미검증.** 헬퍼의 `-Werror` 분기는 이 환경에서 실행되지 않았다
  (CI 전 잡이 `windows-2025`).
- `XPE_WARNINGS_AS_ERRORS=OFF` 로 빌드해 `/WX` 가 **붙지 않는지**는 확인하지 않았다.
  ON 경로만 관측했다. 헬퍼가 `if(NOT XPE_WARNINGS_AS_ERRORS) return()` 를 갖고 있으나
  실측은 아니다.
- 다른 프리셋(`ci-common`, `ci-fullstack` 등)에서의 영향은 측정하지 않았다.
  `ci-preprocess` 한 구성에서만 검증했다.
- `xpe_preprocess.cpp` 가 왜 소스 목록에 없는지(의도적 제외인지 누락인지) 조사하지 않았다.
