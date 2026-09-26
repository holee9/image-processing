# GUI-C-129 — 클램프 알림(#194)이 화면에 온다 (#198)

## 1. 결과 — **화면에 왔다**

```
clamp line: [17:14:10.312] ALERT WARN NATIVE_ALERT: 869755 pixel(s) fell outside the gain
polynomial's fitted dose range [8000.0, 20000.0] and were evaluated at the range edge;
values beyond the calibrated levels are not extrapolated (issue #194)

preprocess line: [17:14:10.313] Chain preprocess: Applied — Preprocess: offset ->
nonlinearity -> gain -> defect on 1024x1024 (Abdomen).
```

`modules/preprocess` 가 민 알림이 네이티브 큐 → `DrainNativeAlerts` → `RaiseAlert` → 화면 로그까지 왔다. `#198` 이 열린 이유였던 알림 중 하나가 **실제로 보인다.**

## 2. (a) 범위 밖 화소를 만든 방법과 이유

두 층에서 다르게 만들었다.

**통합시험** — 프레임을 직접 짰다. 화소의 절반을 `65535` 로 두고 나머지를 1000 근처에 두었다. 생성기가 적합한 범위는 `[8000, 20000]` 이었고, **4096 화소 전부가** 위로 벗어났다. 포화 화소·직접 조사 영역이 `#194` 가 말하는 바로 그 경우다.

**E2E** — 프레임을 손대지 않았다. GUI 가 쓰는 합성 1024×1024 는 값이 0~63426 이라(C-118 에서 측정) 범위 밖 화소가 이미 충분하다. **869,755 화소**가 벗어났다.

**"그냥 돌리면 조용할 수 있다" 는 지적은 옳았고, 이 프레임에서는 조용하지 않았다.** 조용한 경우를 위해 E2E 에는 건너뜀 경로를 두었다 — 알림이 없고 로그가 "범위 안" 을 말하면 **단언하지 않고 건너뛴다**.

## 3. (b) 단언 — 양성·음성·대조군

| 층 | 단언 | 결과 |
|---|---|---|
| 통합 | 다항식 게인 + 범위 밖 프레임 → 알림 1건, **건수 포함**(`^\d+ pixel\(s\) fell outside`) | 통과 |
| 통합 | **대조군**: 같은 프레임 + **스칼라** `gain.xcal` → 클램프 알림 **없음** | 통과 |
| E2E | 화면 로그에 그 줄이 오고 `ALERT`·`NATIVE_ALERT`·건수를 달고 있다 | 통과 |

**대조군이 pre 의 `ScalarGainRaisesNoPolyAlert` 와 같은 축이다** — 없으면 "알림이 떴다" 가 **무엇에나 뜨는 알림**과 구별되지 않는다.

**E2E 의 준비**: 생성기는 다항식을 `gain_poly.xcal` 로 쓰고 GUI 는 `gain.xcal` 을 이름으로 적재하므로, 픽스처가 전자를 후자 위에 복사한다. **제품 코드는 손대지 않았다** — GUI 는 `gain.xcal` 에 든 것을 적재할 뿐이고, 이번에는 그것이 다항식이다.

## 4. 반증 — 관측을 앞에

`RealXpeBackend.DrainNativeAlerts` 의 `_alerts.AddRange(drained)` 를 지워 배선을 끊었다.

```
log lines: 24
preprocess line: Chain preprocess: Applied — Preprocess: offset -> nonlinearity -> gain -> defect ...
→ The gain stage ran with a polynomial calibration and no clamp alert reached the log
  (#198/#194). Lines carrying ALERT: 1.
```

**전처리는 똑같이 돌았고 알림만 사라졌다** — 기록에 그 대비가 남는다.

## 5. 도중에 잡은 것 — **한 프로세스에 알림 큐가 둘이었다**

새 통합시험이 **단독으로는 통과하고 전체 실행에서는 빨강**이었다(3회 연속 재현, `Alerts seen: 0`).

원인은 순서도 병렬도 아니었다(이 프로젝트는 `parallelizeTestCollections: false`). **모듈 인스턴스가 갈렸다** — 게인 단계는 이미 적재된 `xpe_preprocess` 가 바인딩한 `xpe_common` 의 큐에 밀어 넣는데, 내 헬퍼가 **전체 경로로 다시 적재**해 다른 인스턴스의 큐를 읽고 있었다. 앞선 시험이 다른 경로의 `xpe_common` 을 먼저 적재하면 둘이 갈린다.

**이름으로 적재하도록 바꿨다**(`NativeLibrary.Load("xpe_common.dll", assembly, null)`) — 프로세스에 이미 있는 모듈을 돌려주므로, **쓰인 큐를 읽는다.** 3회 연속 259 통과로 확인했다. 이유를 시험 주석에 적었다.

**이것이 첫 단독 통과를 믿었으면 놓쳤을 것**이다. 단독 초록은 "이 시험이 맞다" 가 아니라 "이 조건에서 맞다" 였다.

## 6. 빌드·시험

```
BUILD_EXIT=0
IntegrationTests   : 통과 259, 건너뜀 1, 실패 0   (3회 연속)
Mock  E2E 전체     : 통과 124, 건너뜀 24, 실패 0 (6m 23s)
Native (클램프 E2E): 통과 1, 실패 0
```

건너뜀이 23 → 24 인 것은 이번에 더한 네이티브 전용 1건이다 — Mock 백엔드에는 네이티브 게인 단계가 없어 **재는 것이 없으므로 건너뛴다**(실패가 아니라).

스테이징: `build/ci-a18ab2e` 를 받아 `ci-common/bin` 을 갱신했다. **이전 생성기는 `--gain-poly` 를 몰랐고**(`exit 2: unknown argument`), 그 상태에서 시험이 **정확한 사유로 건너뛰는 것**을 먼저 확인한 뒤 갱신했다.

## 7. 미검증 (Gaps)

- **`#196` 은 이 카드에 넣지 않았다**(지시대로). 비선형 알림은 여전히 화면에서 확인되지 않았다.
- **범위 안 프레임에서 조용한 것**은 E2E 에서 재지 않았다 — 건너뜀 경로만 두었고 그 경로를 실행하지는 않았다. 통합시험의 대조군은 "스칼라 파일" 축이지 "범위 안" 축이 아니다.
- E2E 는 **합성 1024×1024** 한 프레임으로만 봤다. 손목 프레임(3072²)에서는 재지 않았다.
- 생성기의 `--poly-degree`·`--poly-levels` 는 기본값만 썼다. 범위를 좁혀 더 적은 화소가 걸리는 경우는 보지 않았다.
- `xpe_common` 인스턴스가 갈리는 문제는 **내 시험에서만** 고쳤다. 다른 시험이 같은 방식으로 큐를 읽는다면 같은 함정에 걸린다 — 확인하지 않았다.

## 8. 잔여 위험

- E2E 가 생성기를 1024×1024 로 한 번 돌린다(GUI-C-48 측정: 53 s). 이 시나리오만 쓰는 전용 픽스처라 다른 시험에는 영향이 없지만, 네이티브 실행 시간이 그만큼 늘어난다.
- 클램프 건수가 **869,755/1,048,576** 이다 — 프레임의 83% 가 적합 범위 밖이라는 뜻이고, 이것은 합성 프레임의 성질이지 실제 임상 프레임의 성질이 아니다. **알림이 뜨는 것**을 보인 것이지 **얼마나 자주 뜨는지**를 보인 것이 아니다.

🗿 MoAI
