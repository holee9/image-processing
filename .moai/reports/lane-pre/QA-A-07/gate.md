# QA-A-07 — 컴파일되지 않는 소스 파일 조사

- **Refs**: #112 · **브랜치**: `dev/preprocess` · **조사만 — 삭제·목록 추가 없음**
- **결론**: **가설 (b) 대체 후 잔해.** 근거는 아래 4가지가 모두 일치한다.

## 1. 언제 빠졌는가 — `e9b8ed4` (2026-04-20)

파일은 생성 시점부터 목록에 **있었다**. 각 시점 실측:

```
dada6af 2026-04-16  있음   ← 파일 생성 커밋
bc22093 2026-04-16  있음
46e99bf 2026-04-17  있음
3e01b37 2026-04-17  있음
8a0d16d 2026-04-18  있음
e9b8ed4 2026-04-20  없음   ← 여기서 빠짐
HEAD                없음
```

`e9b8ed4` 는 목록 형식 자체를 바꿨다. 이전에는 `add_library(xpe_preprocess SHARED ...)`
안에 인라인으로 나열했고, 이 커밋이 `set(XPE_PREPROCESS_SOURCES ...)` 변수로 재작성했다.
재작성 과정에서 4개 파일이 새 목록에 옮겨지지 않았다.

> `git log -S "xpe_preprocess.cpp"` 는 이 커밋 하나만 반환하는데, 실제로는 테스트 파일명
> `test_xpe_preprocess.cpp` 가 부분 문자열로 걸린 것이다. 목록 멤버십은 커밋별 직접
> 조회로 확인했다.

## 2. 같은 시점에 무엇이 함께 빠졌는가 — 3개는 삭제, 1개만 남았다

`e9b8ed4` 에서 구 목록에는 있고 신 목록에는 없는 파일 4개:

| 파일 | 현재 디스크 |
|---|---|
| `src/xpe_offset.cpp` | **삭제됨** |
| `src/xpe_gain.cpp` | **삭제됨** |
| `src/xpe_defect.cpp` | **삭제됨** |
| `src/xpe_preprocess.cpp` | **존재 (9,765 bytes)** |

같은 마이그레이션에서 정리된 4개 중 3개는 파일까지 지워졌고 이것만 남았다.
**일괄 정리에서 한 건이 누락된 형태**다.

## 3. 내용 중복 — 심볼 단위 대조

죽은 파일이 정의하는 6개 심볼 중 **5개가 현재 빌드되는 소스에도 정의돼 있다.**

| 심볼 | 죽은 파일 | 살아있는 정의 | DLL export |
|---|---|---|---|
| `xpe_preprocess_version` | ✓ | `src/preprocess.cpp` | 됨 |
| `xpe_preprocess_init` | ✓ | `src/preprocess.cpp` | 됨 |
| `xpe_preprocess_shutdown` | ✓ | `src/preprocess.cpp` | 됨 |
| `xpe_preprocess_get_param_range` | ✓ | `src/preprocess.cpp` | 됨 |
| `xpe_defect_detect_runtime` | ✓ | `src/runtime_detection.cpp` | 됨 |
| `xpe_preprocess_is_initialized` | ✓ | (없음 — 이 파일에만) | 안 됨 |

5개는 `extern "C" XPE_API` 정의다. **목록에 추가하면 LNK2005(중복 정의)로 링크가 깨진다.**

`xpe_preprocess_is_initialized` 만 이 파일에 유일하나 `XPE_API` 가 아니고 DLL export 도
되지 않는다. 즉 외부에서 쓸 수 없는 심볼이라 유실 우려가 없다.

## 4. 사유가 기록돼 있는가 — 커밋 메시지에 간접 증거

`e9b8ed4` 커밋 메시지:

```
feat(preprocess): SPEC-XPE-P1A M2 캘리브레이션 API 마이그레이션 + 테스트 202개 전체 통과
- 중복 main() 제거 (LNK2005 해소)
- LNK2005 / LNK2019 빌드 오류 전체 해소
```

**LNK2005(중복 정의) 해소가 이 커밋의 명시적 목표**였다. 위 3번의 중복 심볼 5개와
정확히 맞물린다. 즉 이 파일이 목록에서 빠진 것은 실수가 아니라 **중복 정의를 없애기
위한 조치**였고, 파일 삭제만 누락됐다.

다만 파일 자체나 CMakeLists 에는 사유 주석이 **남아 있지 않다.** `tests/CMakeLists.txt:16`
("Temporarily disabled due to GTest conflicts")처럼 다음 사람에게 남긴 기록이 없다.

## 세 가설 판정

| 가설 | 판정 | 근거 |
|---|---|---|
| (a) 목록 누락 실수 — 추가해야 함 | **기각** | 추가하면 중복 정의 5건으로 LNK2005. 커밋 목표가 바로 그 해소였다 |
| (b) 대체 후 잔해 — 처분 대상 | **채택** | 대체 정의가 `preprocess.cpp`·`runtime_detection.cpp` 에 존재하고 DLL 이 그쪽을 export 한다. 동반 3개 파일은 이미 삭제됨 |
| (c) 의도적 보류 | **기각** | 보류를 뜻하는 기록이 파일·CMakeLists 어디에도 없다. 동반 파일 3개가 삭제된 것은 보류가 아니라 정리 의도를 보여준다 |

## 다른 모듈의 동종 사례 — 목록만, 조치 없음

디스크의 `src/*.cpp` 중 해당 모듈 CMakeLists 소스 목록에 없는 파일:

```
preprocess         mode_selector.cpp   simd_dispatch.cpp   xpe_preprocess.cpp
enhance_advanced   enhance_advanced.cpp   xpe_collimation_detect.cpp
common, enhance_basic, ai, display, dicom, gsvg   (없음)
```

**본 카드 대상은 `xpe_preprocess.cpp` 하나다.** 나머지 4건은 조사하지 않았고 손대지도
않았다. `enhance_advanced` 2건은 Lane B 소유다. 전수 조사는 별도 카드 소관.

> 주의: 단순 파일명 grep 은 오탐을 낸다. `xpe_preprocess.cpp` 는 CMakeLists 주석의
> `tests/test_xpe_preprocess.cpp` 에 부분 문자열로 걸려 "목록에 있음"으로 보인다.
> 위 결과는 `src/<파일명>` 형태로 앵커를 걸어 재조사한 것이다.

## 판정 요청

lead 판정 사항 — 파일 처분(삭제) 여부. 본 카드는 삭제하지 않았다.
`xpe_preprocess_is_initialized` 가 이 파일에만 있으나 export 되지 않아 외부 영향은 없다.

## 미검증 (Gaps)

- 5개 중복 심볼의 **구현이 동등한지는 대조하지 않았다.** 심볼명·시그니처 수준까지만 봤다.
  죽은 파일 쪽에만 있는 로직이 있는지는 확인하지 않았다 (처분 판정 시 필요하면 추가 조사).
- `mode_selector.cpp`, `simd_dispatch.cpp` 는 헤더 전용 패턴일 수도 있고 같은 잔해일
  수도 있다. 판별하지 않았다.
- `e9b8ed4` 는 squash merge 커밋이라 그 안의 개별 결정 이력은 남아 있지 않다.

---

# 판정 수령 (2026-09-01, lead) — PASS, 가설 (b) 채택

목록 추가 시 LNK2005 라는 근거(살아있는 소스에 중복 정의된 5개 심볼이 전부
`extern "C" XPE_API`)가 결정적이라고 평가됐다. 커밋 메시지의 "LNK2005 해소" 기록과
맞춘 것도 채택. "파일명 grep 은 오탐" 주의도 채택됐다.

동종 사례 4건은 목록만 접수. `enhance_advanced` 2건(`enhance_advanced.cpp`,
`xpe_collimation_detect.cpp`)은 **Lane B 소유라 그쪽 카드로 넘어갔다.**
`preprocess` 2건(`mode_selector.cpp`, `simd_dispatch.cpp`)은 후속 카드 대기.

파일 처분(삭제) 실행은 별도 카드로 나온다. 본 카드에서는 삭제하지 않았다.
