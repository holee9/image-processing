# QA-B-168 (`#130`) — `SRS-ALERT-004`. **음성 단언이 위치를 고정합니다**

## 1. 주장

1. **DL 이 실제로 적용됐을 때만 `SRS-ALERT-004`(Info, `"AI-processed"`) 를 냅니다.**
   `xpe_bone_suppress` 의 **단 하나의 성공 출구**, `memcpy` 뒤에 있습니다. §2
2. **양방향으로 단언했습니다** — 적용 경로에서 **난다** 3건, 모델이 손대지 않은
   영상을 돌려준 경로에서 **안 난다** 5건. §3
3. **두 구성 결과가 다릅니다.** 스텁 7통과/4건너뜀, 풀 10통과/1건너뜀 —
   `AiIpcBridgeTest` 가 두 구성에서 바이트 동일해 실경로를 못 단언했던 함정을
   피했습니다. §4
4. **반증 둘 다 터집니다.** 알림을 지우면 **3건 빨강**, 함수 입구로 올리면
   **8건 빨강**(음성 5 전부 포함). §5
5. **카드가 든 음성 경로 셋은 이 함수에 없습니다** — `enabled=0`·non-chest
   skip·저신뢰. 없는 코드에 단언하지 않고, **있는 경로 전부**를 덮었습니다. §3.1
6. 전체 `ci-post` **705/705 통과**. §6

---

## 2. 무엇을 넣었나

`modules/ai/src/ai.cpp`, `xpe_bone_suppress` 의 마지막 두 줄:

```cpp
    std::memcpy(softTissueOut->data, out.value.data(), count * sizeof(float));

    // SRS-ALERT-004 (QA-B-168, #130): DL processing was applied -- Info,
    // "AI-processed".
    //  ... (위치가 계약이라는 것과 극성 근거를 주석으로)
    xpe_alert_push("AI-processed: bone suppression applied (SRS-ALERT-004)",
                   XPE_ALERT_INFO);
    return XPE_OK;
```

**위치가 계약입니다.** 이 함수에는 성공 출구가 하나뿐이고, 그 앞의 모든 조기
반환(형식 불일치, 모델 없음, 모델 손상, `Run` 실패 — **스텁 빌드의 모든 호출**)은
큐를 건드리지 않은 채 끝납니다.

**발신 주체·심각도의 근거**(`QA-B-167` 실측과 리더 `a3330d9`):

- **모듈이 냅니다** — `xpe_alert_push` 제품 호출 20건 전부 `modules/`, `clients` 0
- **Info 입니다** — SRS 표의 실패 조건은 전부 Warning/Error. `SDD:874` 의 실패
  귀속은 리더가 `SRS-SAFE-008` 로 정정했습니다
- **선례** — `xpe_calib_generate_gain.cpp:208,436` 의 Info 알림

새 시험 파일: `modules/ai/tests/test_alert_ai_processed.cpp` (11건),
`modules/ai/CMakeLists.txt` 에 등록.

---

## 3. 양방향 단언

### 양성 — 적용됐으면 난다 (풀 빌드 전용)

| 시험 | 단언 |
|---|---|
| `DlAppliedRaisesTheInfoAlert` | 큐에 1건, 심각도 `XPE_ALERT_INFO` |
| `OneCallRaisesExactlyOneAlert` | 호출 1회 → **정확히 1건** (타일·행 단위로 밀면 64칸 큐가 넘쳐 실제 경고를 밀어냅니다 — `SRS-ALERT-007`) |
| `TwoCallsRaiseTwoAlerts` | 호출 2회 → 2건 (한 번만 내면 뒤 영상이 태깅 없이 나갑니다) |

### 음성 — 적용 안 됐으면 안 난다 (두 구성 공통)

| 시험 | 반환 | 알림 |
|---|---|---|
| `UnsupportedFormatRaisesNothing` | `UNSUPPORTED_FORMAT` | **0** |
| `MissingModelRaisesNothing` | ≠ `XPE_OK` | **0** |
| `BrokenModelRaisesNothing` | ≠ `XPE_OK` | **0** |
| `MismatchedDimensionsRaiseNothing` | `INVALID_INPUT` | **0** |
| `WithoutInitRaisesNothing` | `NOT_INITIALIZED` | **0** |

**이 다섯이 코드를 제약하는 쪽입니다.** 함수 입구에서 무조건 내는 구현은 양성
셋을 전부 통과하고 이 다섯을 전부 실패합니다(§5 에서 실측).

### 3.0 부재 단언에는 대조군을 붙였습니다

`TheProbeItselfCanSeeAQueuedInfoAlert` — 직접 Info 를 하나 밀어 넣고 **읽기
경로가 그것을 1건으로 세고 심각도까지 맞히는지** 단언합니다. 이것 없이는
위 `== 0` 다섯 줄이 **읽기가 망가진 경우에도 그냥 통과**합니다.

### 3.1 카드가 든 음성 경로 셋은 **이 함수에 없습니다**

카드 §2 가 `enabled=0` pass-through, non-chest skip, 저신뢰 반환을 들었습니다.
**셋 다 오늘 `xpe_bone_suppress` 에 없습니다**:

```
ai.cpp:590   (void)configJsonOrNull;     <- 설정을 통째로 무시
```

`enable` 플래그도, 신체부위 게이트도, 신뢰도 게이트도 없습니다
(`confidence_threshold` 는 `xpe_ai_init` 이 상태에 저장하지만
`xpe_bone_suppress` 는 읽지 않습니다). **없는 코드에 단언하면 그 단언이 무엇을
막는지 말할 수 없으므로**, 실제로 있는 조기 반환 **전부**를 덮었습니다.

> 이 셋이 생기면 그때 음성 단언을 늘려야 합니다 — 그 시점에 이 문단이 무엇이
> 빠졌는지 알려 줍니다.

---

## 4. 두 구성이 **달라야** 합니다

```
스텁:  ===STUB_BUILD=0===  ===STUB_ALERT=0===
       11건 중 7 통과 / 4 건너뜀 (양성 3 + EXPECT_ONNX 게이트 1)
풀:    ===FULL_BUILD=0===  ===FULL_ALERT=0===   (XPE_AI_EXPECT_ONNX=1)
       11건 중 10 통과 / 1 건너뜀 (스텁 전용 계약 1)
```

카드 §4 가 지목한 함정입니다. `AiIpcBridgeTest` 는 두 구성에서 **10/10 동일**해서
실경로를 단언하지 못합니다(`QA-B-167`). 여기서는 **어느 쪽에서 무엇이 건너뛰는지가
서로 반대**입니다:

| | 스텁 | 풀 |
|---|---|---|
| `DlAppliedRaisesTheInfoAlert` | 건너뜀 | **통과** |
| `DlNotAppliedInAStubBuildRaisesNothing` | **통과** | 건너뜀 |

**건너뜀이 조용히 지나가지 않게** 했습니다 —
`DlAppliedRaisesTheAlert_ProvableWhenTheCallerSaysSo` 가 `XPE_AI_EXPECT_ONNX=1`
일 때 스텁이면 **빨강**을 냅니다(`QA-B-160` 이 세운 형태). `#205` 는 아무것도
돌지 않는데 초록으로 읽힌 일이었습니다.

---

## 5. 반증 — 둘 다 터집니다

풀 빌드에서만 양쪽이 살아 있으므로 거기서 쟀습니다. **주입이 실제로 들어갔는지
`grep` 으로 먼저 확인했습니다**(`QA-B-164`: 빈 주입이 0 을 낸 적 있음).

### A. 알림을 **지우면** — 양성이 빨개집니다

```
주입 확인: grep -c "AI-processed: bone suppression" ai.cpp  ->  0
===FX_BUILD=0===  ===FX_RUN=1===

[  FAILED  ] 3 tests
  DlAppliedRaisesTheInfoAlert / OneCallRaisesExactlyOneAlert / TwoCallsRaiseTwoAlerts
```

### B. 함수 **입구로 올리면** — 음성 다섯이 전부 빨개집니다

카드가 경고한 *"항상 내는 구현"* 을 직접 만들어 봤습니다.

```
주입 확인: grep -c "FALSIFY-B" ai.cpp  ->  1
===FX_BUILD=0===  ===FX_RUN=1===

[  FAILED  ] 8 tests
  UnsupportedFormatRaisesNothing        <- 음성 5 전부
  MissingModelRaisesNothing
  BrokenModelRaisesNothing
  MismatchedDimensionsRaiseNothing
  WithoutInitRaisesNothing
  DlAppliedRaisesTheInfoAlert           <- 양성 3 도 (1건 기대에 2건이 나서)
  OneCallRaisesExactlyOneAlert
  TwoCallsRaiseTwoAlerts
```

> **B 가 이 카드의 답입니다.** 양성만 있었다면 이 구현은 **8건 중 3건만** 빨강을
> 냈고, 그 3건은 "중복"이라는 다른 이유로 났을 것입니다 — *"적용 안 됐는데
> 태깅했다"* 는 **어떤 시험도 말하지 못했습니다.**

원복 후 `grep` 재확인: `FALSIFY-B` **0건**, 정상 push **1건**.

---

## 6. 검증

```
스텁 알림:   ===STUB_BUILD=0===  ===STUB_ALERT=0===   7통과/4건너뜀
풀 알림:     ===FULL_BUILD=0===  ===FULL_ALERT=0===   10통과/1건너뜀
풀 ai 전체:  ===FULL_AI_ALL=0===   190건 중 187 통과 / 3 건너뜀(스텁 전용 계약)
ci-post:     ===POST_BUILD=0===  ===POST_CTEST=0===
             100% tests passed, 0 tests failed out of 705
반증 A:      ===FX_RUN=1===  3건 빨강 (주입 확인 후)
반증 B:      ===FX_RUN=1===  8건 빨강 (주입 확인 후)
```

종료 코드는 전부 파이프 없이 받았습니다. 스크립트: `_run.bat`, `_falsify.bat`,
`_verify.bat`. 원본 백업: `_ai.cpp.orig`.

**`ci-post` 가 705/705 입니다** — `QA-B-154` 이래 보고해 온 선재 빨강
(`DuplicateExportTest`)은 이 프리셋에 없습니다(그 건은 전체 ctest 756 구성의 것).

건드린 것: `modules/ai/src/ai.cpp`(+23), `modules/ai/CMakeLists.txt`(+4),
`modules/ai/tests/test_alert_ai_processed.cpp`(신규). **워커·프로토콜·in-process
경로·문서는 손대지 않았습니다**(카드 §5).

## 7. 미검증 · 잔여 위험

- **GUI 가 이 알림을 실제로 표시하는지 안 봤습니다.** `clients`/`gui` 는 제
  소유가 아니고, 제 쪽은 **큐에 넣는 것까지**입니다. `SRS-ALERT-004` 가 요구하는
  *"`"AI-processed"` label 표시"* 의 표시 절반은 **아직 없습니다**
- **`xpe_bone_suppress` 하나에만 넣었습니다.** `xpe_dl_denoise`·
  `xpe_stitch_images`·`xpe_bodypart_recognize` 도 DL 처리인데, 그것들은 오늘
  실경로가 없습니다(`QA-B-167`). **같은 알림이 필요한지 판단하지 않았습니다**
- **`XpeImageMetadata` 플래그를 건드리지 않았습니다.** `SDD:868` 은 `SRS-SAFE-008`
  로 *"Tag output as AI-processed"* 를 따로 요구합니다 — 알림과 메타데이터
  태깅은 **다른 것**이고 후자는 이 카드 밖입니다
- **문구를 제가 골랐습니다**(`"AI-processed: bone suppression applied
  (SRS-ALERT-004)"`). SRS 는 label 표시만 요구하고 문구를 정하지 않습니다.
  `clients/` 에 이 문자열을 앵커로 쓰는 시험이 생기면 **레인 간 계약**이 됩니다
- **큐 포화 상호작용을 재지 않았습니다.** Info 는 `SRS-ALERT-007` 의 폐기
  우선순위 첫 대상이라, 영상 64장을 연속 처리하면 이 알림이 서로를 밀어냅니다 —
  **설계상 그렇습니다만 측정하지 않았습니다**
- 워커 쪽 셋(`QA-B-169`·`170`·`171`)은 손대지 않았습니다

---

Refs #130
