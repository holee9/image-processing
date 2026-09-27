# GUI-C-127 — 세 알림 중 무엇이 띄울 수 있는가 (#198) · **정정판**

## 0. 먼저 — **첫 보고의 (b) 결론이 틀렸다. 내 오류다**

첫 보고에서 *"`#194`·`#196` 알림은 소스에 없다"* 고 적었다. **틀렸다.** 원인은 결함이 아니라 **내가 잰 대상이 낡았던 것**이다.

```
dev/gui 는 main 보다 120 커밋 뒤처져 있었다
git show dev/gui:modules/preprocess/src/nonlinearity_correct.cpp | grep -c xpe_alert_push  → 0
main 의 같은 파일                                                                    → 2
```

**소스 검색과 DLL 문자열 검사 둘 다 낡은 것을 쟀다.** 대조군을 둘이나 붙였는데도 안 걸린 이유는, **대조군이 검증하는 것은 방법이지 대상의 신선도가 아니기** 때문이다 — 그 문자열들은 옛 빌드에도 있었으므로 "방법이 헛돌지 않는다" 는 참이었고, 그 참이 "대상이 최신이다" 를 덮어 주지 못했다.

**조치**: `origin/main` 을 `dev/gui` 에 병합했다(충돌 1건, 아래 §1). 이후 측정은 전부 병합된 트리와 **새로 내려받은 CI 산출물**로 다시 했다.

## 1. 병합

```
git merge origin/main   → CONFLICT (content): gui/ImageProcTest/Models/AppSettings.cs
```

충돌은 **내 문단 하나**였다 — `LaneBGsvgDenoiseK` 의 doc 주석에 C-117 후속(`e77d110`, 옛 키 0.42 가 대응 없이 기본값으로 떨어지는 이유)을 적은 부분이고, main 에는 그 커밋이 아직 없다. **내 쪽을 유지**해 해결했다(사실이 바뀌지 않았고, 그 커밋은 다음 묶음에 올라갈 예정).

```
병합 후: git rev-list --count HEAD..origin/main → 0
```

## 2. (a) 다시 — **세 알림 모두 최신 소스에 있다**

```
modules/preprocess/src/nonlinearity_correct.cpp:88   #196 A  "panel.linear is false but no nonlinearity LUT is loaded ... (#186)"
modules/preprocess/src/nonlinearity_correct.cpp:138  #196 B  "nonlinearity correction did nothing ... (issue #196)"
modules/preprocess/src/gain_correct.cpp:382          #194    "%zu pixel(s) fell outside the gain polynomial's fitted dose range ..."
```

### 스테이징 DLL 의 신선도 — 이것이 첫 보고가 놓친 축

| DLL | 빌드 | #196 A | #196 B | #194 | 게인 다항식 | 대조군 |
|---|---|---|---|---|---|---|
| `build/ci-3f520f8/bin` (첫 보고가 쓴 것) | 2026-09-18 10:39 | FOUND | absent | absent | FOUND | FOUND |
| `build/ci-2ba12f5/bin` (새로 받음, run 35425941186, `2ba12f5`) | 2026-09-19 16:09 | FOUND | FOUND | FOUND | absent | absent |

**두 빌드가 서로 다른 것을 갖고 있다.** 새 빌드에 `XCAL_TYPE_GAIN_POLY` 문자열 자체가 없는데, 최신 소스에서도 그 문구(`"gain polynomial (XCAL_TYPE_GAIN_POLY) is loaded"`)가 사라졌다 — `#187`/QA-A-121 이 *"a loaded polynomial is now APPLIED here"* 로 바꾸면서 그 알림이 없어진 것으로 보인다. **즉 `#198` 이 적은 "게인 다항식 적재됨" 알림은 지금 소스에 없다** — 이번엔 최신 트리에서 확인한 사실이다.

새 산출물을 `build/ci-2ba12f5/bin` 에 스테이징했다(gsvg·display 는 `xpe-ci-post-binaries`, 가상 그리드 표 CSV 는 `modules/gsvg/data` 에서 복사). 기존 네이티브 시험이 그 DLL 로도 초록이다:

```
NativeAlertScenarios : 통과 2, 실패 0   (XPE_NATIVE_DIR=build/ci-2ba12f5/bin)
```

## 3. (b) 띄우는 것을 막는 것 — **둘로 갈린다**

### `#194` 클램프 — **`XCAL_TYPE_GAIN_POLY` 교정 파일이 필요하다**

`gain_correct.cpp:350` 의 조건이 `if (!poly.empty())` 이고, 클램프는 그 안에서만 센다. 즉 **다항식 gain 이 적재돼 있어야** 알림이 나온다.

| 확인 | 결과 |
|---|---|
| 저장소 안의 `.xcal` | 0건 (`build/` 제외) |
| GUI 가 쓰는 `xpe_calib_fixture_gen` | `xpe_calib_generate_gain` → **스칼라** `XCAL_TYPE_GAIN` |
| `xpe_calib_generate_gain_polynomial` 호출부 | **모듈 시험 코드뿐** |

**`modules/preprocess/tools/` 소유(pre)** 이므로 손대지 않았다. 리더가 pre 에 넘긴다고 했다.

### `#196` 비선형 — **GUI 가 그 단계를 아예 호출하지 않는다**

두 알림 모두 `xpe_nonlinearity_*` 안에서 난다. GUI 에는 **P/Invoke 선언도 호출부도 없다**(`gui/ImageProcTest/Services/Native/*.cs` 에 `nonlin` 0건). `GuiPreprocessRunner` 는 offset → gain → defect 세 단계만 돈다.

**이것은 픽스처 문제가 아니라 배선 문제이고, 배선은 이 레인 소유다.** 다만 **그냥 이어서는 안 된다고 본다** — 이유:

- 그 단계는 LUT 가 없으면 **아무것도 하지 않고** 프레임을 그대로 통과시킨다. 즉 **알림을 띄우려고 단계를 하나 더 도는** 모양이 된다.
- 연쇄에 단계를 넣는 것은 **순서·버퍼 형식·무엇이 기본으로 켜지는가**를 정하는 일이다. `#193` 에서 "이을 상대가 없는 입력" 을 뺐던 것과 반대 방향의 결정이라 **리더 판정이 필요하다고 판단**했다.

그래서 **여기서 멈추고 보고한다.** 카드의 *"안 되면 무엇이 막는지 적고 멈추십시오"* 에 해당한다.

## 4. 빌드·시험

```
BUILD_EXIT=0
NativeAlertScenarios (ci-2ba12f5) : 통과 2, 실패 0
Mock  E2E 전체 (병합 후)          : 통과 124, 건너뜀 23, 실패 0 (5m 29s)
IntegrationTests (병합 후)        : 통과 254, 건너뜀 1, 실패 0
소스 변경                          : 병합 해결 1곳(내 주석 유지) 외 없음
```

**120 커밋이 들어온 뒤에도 회귀가 없다.**

## 5. 미검증 (Gaps)

- 새 스테이징(`ci-2ba12f5`)에 `xpe_enhance_*` 는 `xpe-ci-post-binaries` 에서 왔다 — **그 아티팩트가 같은 커밋인지**는 아티팩트 이름만 보고 믿었고, 파일별로 대조하지 않았다.
- `#196` 을 이었을 때 실제로 알림이 뜨는지는 **당연히 미검증**이다(이 카드에서 잇지 않았다).
- 게인 다항식 알림이 "없어졌다" 는 것은 **문자열 부재 + 소스 주석**으로 판단했다. 그 알림을 없앤 커밋을 직접 짚지는 않았다.

## 6. 잔여 위험

- 낡은 워크트리로 남의 모듈을 재는 실수는 **대조군으로 못 잡는다.** 앞으로 다른 레인의 모듈을 측정할 때는 **먼저 main 을 병합하거나 주 체크아웃의 절대경로로 읽는다**(리더 [HARD] 지시, 받았다).
- `build/ci-3f520f8/bin` 이 아직 남아 있다. 다른 카드가 그 경로를 쓰면 다시 낡은 것을 잰다 — 새 경로로 옮겨 쓰는 것이 안전하다.

🗿 MoAI
