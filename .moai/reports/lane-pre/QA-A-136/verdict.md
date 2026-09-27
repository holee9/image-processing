# QA-A-136 (#198) — 다항식 픽스처를 만들 수 있게 했습니다. 다만 **적재만으로는 알림이 뜨지 않습니다**

Lane A (pre), `dev/preprocess`, 커밋 `0cf1f3d`. `origin/main 7ebbb21` 기준.

## 주장

| # | 주장 | 판정 |
|---|---|---|
| 1 | `--gain-poly` 로 만든 파일이 실제로 `XCAL_TYPE_GAIN_POLY` 다 | **참** — 형식 바이트로 확인 |
| 2 | 적재가 성공한다 | **참** |
| 3 | **적재만으로 알림이 뜬다** | **거짓** — 카드 전제와 다릅니다. 아래 |
| 4 | 적재 + `xpe_gain_correct` 1회로 다항식 알림이 밀린다 | **참** |
| 5 | 대조군: 기본(스칼라) 경로는 `XCAL_TYPE_GAIN`, 그 알림 없음 | **참** |
| 6 | 기존 스칼라 출력이 바뀌지 않는다 | **참** — SHA-256 3건 동일 |

## 3번 — 전제 정정

카드가 *"적재만으로 알림이 뜨므로 영상 처리는 필요 없습니다"* 라고 적었습니다. **지금 도구가 만드는 파일에는 맞지 않습니다.**

`xpe_calib_load_gain.cpp:156` 의 다항식 적재 알림은 **선량 범위가 없는 파일에만** 뜹니다. 그런데 QA-A-123 이후 생성기가 `dose_min`/`dose_max` 를 기록하므로, 갓 만든 파일을 적재하면 **큐가 조용합니다.**

**실제로 밀리는 다항식 알림은 `gain_correct.cpp:374-382` 의 범위 밖 클램프 건수**입니다:

> `"%zu pixel(s) fell outside the gain polynomial's fitted dose range [%.1f, %.1f] and were evaluated at the range edge ... (issue #194)"`

이것은 **파이프라인이 필요 없습니다** — 적재 후 `xpe_gain_correct()` **한 번**이면 뜹니다. gui 는 그 한 줄만 더하면 됩니다.

두 사실을 각각 시험으로 고정했습니다 — `LoadAloneIsSilent`(조용함), `PolyAlertIsPushed`(뜸). 다음 사람이 뜰 수 없는 알림을 쫓지 않도록.

## 증거

### 빌드·시험

```
BUILD_EXIT=0
100% tests passed, 0 tests failed out of 742
CTEST_EXIT=0
```
명령: `cmake --build build\ci-preprocess --config RelWithDebInfo` (타깃 지정 없음) + `ctest --test-dir build\ci-preprocess -C RelWithDebInfo`.

신규 6건:
```
647 FixtureGenGainPolyTest.TypeByteIsGainPoly ..................... Passed
648 FixtureGenGainPolyTest.DefaultRunStaysScalarAndWritesNoPolyFile  Passed
649 FixtureGenGainPolyTest.GeneratedPolyFileLoads ................. Passed
650 FixtureGenGainPolyTest.LoadAloneIsSilent ..................... Passed
651 FixtureGenGainPolyTest.PolyAlertIsPushed ..................... Passed
652 FixtureGenGainPolyTest.ScalarGainRaisesNoPolyAlert ........... Passed
```

### 형식 바이트

`XCalFileHeader.type` 은 오프셋 8 (`xcal_format.h:154` static_assert). 시험이 파일 앞 12바이트를 읽어 그 자리를 `XCAL_TYPE_GAIN_POLY`(3) 와 비교합니다 — **파일 이름이 아니라 바이트**입니다. 대조군은 같은 방식으로 `gain.xcal` 에서 `XCAL_TYPE_GAIN`(1) 을 읽습니다.

### 생성기 실행 (`--width 64 --height 64 --gain-poly`)

```
offset.xcal    4abb7555a6d83fec6f76baed992e30795c9c65d4d80e58acf81e1014d583efd0
gain.xcal      1134214e792b76ce7cbcfbd19f62657b9129b027edeacd969e48991539ccd57a
defect.xcal    ed042434394f944d29842c557a64ae2a4404531d6c5f37d708e34f07cf760ef0  (1272 defect pixels)
gain_poly.xcal a66bc853fd2cdfaa1236cf5fbedd45bcaef40bed0fe379111fac3971526bd459  (degree<=2, 4 dose levels)
```
`gain_poly.xcal` 49,623 바이트. 중간 파일 8건은 정리됩니다. **위 세 SHA 는 `--gain-poly` 없이 돌린 실행과 동일합니다** — 기존 경로가 바이트 단위로 안 바뀐 근거입니다.

## 반증

다항식 생성 호출을 런타임 거짓 조건(`opt.polyLevels < 0 &&`)으로 막고 첫 준위 게인 파일을 `gain_poly.xcal` 로 옮겨 **스칼라 파일이 그 자리에 오게** 했습니다. **`BUILD_EXIT=0` 확인 후**:

```
1/6 TypeByteIsGainPoly ..................... ***Failed
2/6 DefaultRunStaysScalarAndWritesNoPolyFile   Passed
3/6 GeneratedPolyFileLoads ................. Passed
4/6 LoadAloneIsSilent ...................... Passed
5/6 PolyAlertIsPushed ...................... ***Failed
      Value of: alertContains("gain polynomial's fitted dose range")
      Actual: false
6/6 ScalarGainRaisesNoPolyAlert ............ Passed
CTEST_EXIT=8
```

**형식 바이트 단언과 알림 단언이 둘 다 빨강, 대조군 둘은 초록.** 단언이 형식에 반응한다는 뜻입니다. 패치는 원복했고 이후 전체 ctest 가 다시 742/742 입니다.

**첫 반증 시도는 무효였습니다** — `if (false && ...)` 가 `/WX` 의 도달 불가 경고에 걸려 `BUILD_EXIT=1` 이 났고, 낡은 바이너리가 5건 통과를 찍었습니다. 빌드 결과를 읽지 않았으면 "반증이 안 터진다" 로 읽힐 뻔했습니다.

## baseline 귀속

- 빌드·ctest: 위 명령을 이 트리·이 커밋에서 직접 실행한 출력입니다.
- 형식 바이트·SHA·파일 크기: 이번 실행에서 측정했습니다.
- `#198` 본문의 *"저장소의 `.xcal` 0건"*·*"호출부는 모듈 시험뿐"* 은 **리더 실측이며 재현하지 않았습니다.**

## 미검증

- **gui 쪽 경로를 보지 않았습니다.** 알림이 큐에 들어가는 것까지만 쟀고, 화면까지 오는지는 Lane C 소유입니다.
- **선량 범위가 없는 다항식 파일을 만드는 선택지는 넣지 않았습니다.** 카드가 요구하지 않았고, gui 가 `load_gain` 의 적재 알림 자체를 띄워야 한다면 그 선택지가 따로 필요합니다 — **판단은 리더 몫입니다.**
- `--poly-degree` 3·4 와 `--poly-levels` 5 이상 조합을 돌리지 않았습니다. 기본값(2 / 4)만 측정했습니다.
- **신규 시험 6건을 한 프로세스에서 연속 실행하면 `ScalarGainRaisesNoPolyAlert` 가 실패합니다.** ctest 는 시험마다 프로세스를 나누므로 게이트는 초록이지만, 앞선 시험이 적재한 다항식 교정이 프로세스에 남는 것으로 보입니다. **원인을 확정하지 않았습니다.**
- 픽스처 파일은 커밋하지 않았습니다(카드 지시).

## 잔여 위험

- **위 프로세스 내 상태 잔류**가 다른 시험 묶음에서도 같은 형태로 나타날 수 있습니다. 오늘 `feedback_teardown_poisoned_the_next_measurement` 와 같은 계열입니다 — ctest 분리 실행이 가려 주고 있을 뿐입니다.
- **카드 전제가 틀렸다는 것이 이 보고의 핵심**인데, gui 가 이미 "적재만 하면 뜬다" 로 작업을 잡아 뒀다면 그쪽도 한 줄 바뀝니다. `#198` 코멘트에 같은 내용을 적었습니다.
