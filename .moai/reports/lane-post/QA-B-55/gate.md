# QA-B-55 게이트 보고서 — `xpe_calc_exposure_index` 심볼 충돌 해소

**카드**: QA-B-55 (#153 #142)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-55/`
**커밋 1건**: `bb288f8`
**선행**: `git merge origin/main` — 이미 최신 (B-54 병합 `ef5a83c` 포함)

---

## 1. 이름 선택

**`xpe_adv_calc_exposure_index`**

| 후보 | 판단 |
|---|---|
| **`xpe_adv_calc_exposure_index`** | **채택.** 모듈이 왼쪽에서 바로 드러나고, 호출부 한 줄만 봐도 어느 구현인지 읽힌다 |
| `xpe_enhance_advanced_calc_exposure_index` | 기각. 이 모듈에서 **전체 모듈명 접두어는 생명주기 함수**(`_init`/`_shutdown`/`_version`)의 표식이고, 처리 계열은 짧은 이름을 쓴다(`xpe_multiscale_process`, `xpe_fractional_process`, `xpe_detect_collimation`). 처리 함수에 생명주기 접두어를 붙이면 그 구분이 흐려진다 |
| `xpe_calc_exposure_index_adv` | 기각. 이름은 **왼쪽부터** 읽힌다. 접미사를 쓰면 두 이름이 앞 24자가 같아, export 목록을 훑을 때 구분되지 않는다 — 구분하려고 바꾸는 이름이 구분을 어렵게 하면 안 된다 |

---

## 2. 헤더 주의문 — 지우지 않고 갱신

기존 주의문(`:163-167`)은 **정확했다**: "두 DLL 의 별개 구현이고 계약이 다르니 둘 다 로드하는
소비자는 이름만으로가 아니라 명시적으로 해결해야 한다."

**그리고 막지 못했다.** 이름이 하나이면 C++ 호출자에게 **"명시적으로 해결할" 대상 자체가
없다.** 문서로 못 막는 것을 문서로 적어 둔 상태였다.

갱신본이 남기는 것: ① 한때 이름이 같았다는 사실, ② 왜 advanced 만 바꿨는지(C# 바인딩이 전부
basic 을 가리킨다 — 돌아가는 소비자를 깨뜨려 C++ 위험을 고칠 수는 없다), ③ **값은 여전히
다르다**는 것과 어느 쪽이 요구를 만족하는지는 별개 질문이라는 것.

---

## 3. 가드 — 그리고 내 주장 하나가 반증됐다

### 3.1 반증된 주장

e2e 주석에 처음 이렇게 적었다: *"재충돌하면 두 호출이 하나의 모호한 이름이 되어 이 파일이
컴파일되지 않는다."*

**틀렸다.** 옛 이름을 advanced 헤더에 되돌려 놓고 재 봤다(`_falsify_header.log`):

```
===BUILD=0===          <- 빌드 통과. 컴파일러는 아무 말도 하지 않는다
[  OK  ] DuplicateExportTest.KnownDivergence_RenamedExportsStillDisagree
[  OK  ] FullPipelineE2E.BothExposureIndexExportsAreCallableAndDisagree
```

같은 이름·같은 시그니처의 **재선언은 C++ 에서 합법**이다. 두 헤더가 한 이름에 합의하면
진단이 없다 — **원래 충돌이 오래 눈에 띄지 않은 이유가 정확히 그것이다.**
주석을 측정된 사실로 고쳤다. B-50 에서 남의 주석에 적용한 기준을 내 주석에 적용한 것이다.

### 3.2 그래서 가드는 이렇게 갈린다

| 재충돌 형태 | 잡히는가 | 근거 |
|---|---|---|
| **advanced DLL 이 옛 이름을 다시 export** | **잡힌다** — `test_duplicate_export.cpp` 가 각 DLL 에 상대의 이름을 묻는다 | `_falsify.log`: 별칭 재추가 → **BUILD=0 에서 테스트 실패** |
| **헤더에 옛 선언만 되돌림** | **잡히지 않는다** — 컴파일러에 진단이 없다 | `_falsify_header.log`: BUILD=0, 두 가드 모두 통과 |

실제 재충돌은 export 를 동반하므로 첫 줄이 실효 가드다. 두 번째 줄은 **가드의 한계**이고,
산문이 아니라 측정으로 적어 둔다.

### 3.3 e2e 가 기여하는 것

컴파일 실패는 아니지만 빈손도 아니다 — 두 헤더를 보는 유일한 번역 단위에서 **양쪽을 각각
명시적으로 호출**하므로, 파이프라인이 쓰는 구현이 **호출부 한 줄에 적힌다.** 이전에는
링크 순서가 정하고 소스 어디에도 적혀 있지 않았다.

B-54 의 **"둘이 다르다" 단언은 유지**했다(카드 2항). 개명은 선택을 보이게 만들 뿐 답을
같게 만들지 않는다.

---

## 4. export 전후 대조 (`_exports_after.log` vs B-54 `_exports.log`)

| | 이전 | 이후 | 판정 |
|---|---|---|---|
| `xpe_enhance_basic.dll` | 8개 | **8개, 이름·서수 동일** | **완전 불변** |
| `xpe_enhance_advanced.dll` | 7개 (`xpe_calc_exposure_index` 포함) | 7개 (`xpe_adv_calc_exposure_index` 로 교체) | 개수 불변, 이름 1개 교체 |

**A-39 교훈**: 이름 목록이 같다고 ABI 가 같은 것은 아니다. 여기서 의미 있는 것은 반대
방향이다 — **basic 이 전혀 바뀌지 않은 것**이 C# 무영향의 근거이고, advanced 의 이름 변경은
**의도한 ABI 변경**이다.

---

## 5. C# 무영향 확인

| | 변경 전 | 변경 후 |
|---|---|---|
| `grep xpe_calc_exposure_index clients/ gui/ --include=*.cs` | **5건** | **5건** (동일) |
| `grep xpe_adv_calc_exposure_index clients/ gui/ --include=*.cs` | — | **0건** |

5건의 내역: `XpeEnhanceBasicReadinessProbe.cs` ×2, `NativeEnhanceBasicPreviewService.cs` ×1,
`PipelineOrchestrator.cs` ×2(`RequiredExports` 목록 + TODO 주석). **전부 basic 을 가리킨다**
— 파일 이름과 `DllName = "xpe_enhance_basic.dll"` 이 그것을 말한다. 변경 없음.

---

## 6. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 개명 범위 | 15파일 (헤더 1·구현 3·테스트 11) | `git show --stat` |
| GREEN | BUILD=0, 두 가드 통과 | `_green.log` / `_green2.log` |
| 반증 (DLL) | BUILD=0, 가드 **실패** = 재현됨 | `_falsify.log` |
| 반증 (헤더) | BUILD=0, 가드 **통과** = **내 주장 반증** | `_falsify_header.log` |
| export basic | 8개, 이름·서수 동일 | `_exports_after.log` |
| export advanced | 7개, 이름 1개 교체 | 같은 로그 |
| C# | 5건 → 5건, 새 이름 0건 | `_csharp_before.log` + 재grep |
| 이전 ctest | 475 / 211 / 173 | QA-B-54 `_verify.log` |
| 현재 ctest | **476 / 211 / 173** (e2e +1) | `_verify.log` |
| 빌드 경고 | 0 | `_verify.log` |

---

## 7. 미검증 (Gaps)

- **어느 쪽이 REQ-ENH-030 / REQ-ADV-013 을 만족하는지 판정하지 않았다** — 카드가 범위 밖으로
  명시했고, 요구 문구 대조가 필요하다. 값이 다르다는 사실만 고정돼 있다.
- **C# 을 실행해 확인하지 않았다.** grep 결과가 같다는 것과 basic 이 불변이라는 것이 근거이고,
  **실제 로드·P/Invoke 는 돌려 보지 않았다**(Lane C 영역). GUI 의 `RequiredExports` 검사는
  이름 목록 대조이므로 basic 불변이면 통과할 것으로 **판독**한다.
- **advanced 의 새 이름을 쓰는 외부 호출자는 없다** — 문서에도 아직 없다(leader 처리 대기).
- **다른 중복 export 는 찾지 않았다.** B-54 는 `xpe_calc_exposure_index` 한 건을 잡았을 뿐,
  6모듈 45개 이름 전체의 교차 중복을 전수하지 않았다.

---

## 8. 잔여 위험 (Residual-risk)

- **advanced 의 ABI 가 깨진다.** 옛 이름으로 바인딩한 소비자는 로드에 실패한다. C# 3곳과
  GUI 목록은 전부 basic 이라 영향이 없다고 판단했지만, **레포 밖 소비자는 확인할 수 없다.**
- **헤더 재선언은 여전히 잡히지 않는다**(§3.2). 실제 재충돌은 export 를 동반하므로 실효
  가드가 있지만, 선언만 되돌리고 export 는 두지 않는 중간 상태는 조용하다.
- **값 차이는 그대로다.** 이 카드는 **선택을 보이게** 만들었을 뿐, 두 답 중 어느 것이 옳은지는
  열려 있다. 임상 수치가 두 가지인 상태가 유지된다.
- **개명 15파일 중 대부분이 기계 치환이다.** 빌드와 476건 통과가 근거이고, `test_duplicate_export.cpp`
  처럼 **양쪽 이름을 모두 다뤄야 하는 파일**은 손으로 되돌렸다(기계 치환이 basic 쪽 조회까지
  바꿔 놓았다 — 전수 치환의 전형적 사고 경로다).
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` | 환경 + 세 프리셋 재빌드·전체 ctest |
| `_b55.bat` | 두 가드 빌드·실행 |
| `_green.log` / `_green2.log` | GREEN — BUILD=0, 두 가드 통과 |
| `_falsify.log` | DLL 별칭 재추가 — BUILD=0, 가드 **실패**(재현됨) |
| `_falsify_header.log` | 헤더 선언 재추가 — BUILD=0, **통과 = 내 주장 반증** |
| `_exports.bat` / `_exports_after.log` | dumpbin 이후 상태 (B-54 `_exports.log` 와 대조) |
| `_csharp_before.log` | C# 5건 목록 (변경 전) |
| `_verify.log` | 최종 476 / 211 / 173, 경고 0 |
