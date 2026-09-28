# GUI-C-147 (#208) — 픽스처를 `Wrist1024SliceApplicationFixture` 로 개명하고 공백을 그 자리에 적었다

상태: **개명 완료 · 참조 전수 0건 남음 · 반증 완료.** 크기는 바꾸지 않았다.

그리고 **이 카드와 무관한 빨강 1건을 만났다** — `P-10`. 팔로 갈라 **제 변경 소행이 아님을 확정**했다(§4). 손대지 않았다.

---

## 1. 새 이름 — 두 반쪽을 다 담는다

| | 이전 | 이후 |
|---|---|---|
| 클래스 | `LargeFrameApplicationFixture` | **`Wrist1024SliceApplicationFixture`** |
| 컬렉션 클래스 | `LargeFrameApplicationCollection` | **`Wrist1024SliceApplicationCollection`** |
| 컬렉션 id 문자열 | `"gui-large-frame-application"` | **`"gui-wrist-1024-slice-application"`** |
| 파일 | `Fixtures/LargeFrameApplicationFixture.cs` | **`Fixtures/Wrist1024SliceApplicationFixture.cs`** |
| 덤프 경로 | `xpe-gui-e2e-large-frame-render.bgra` | `xpe-gui-e2e-wrist-1024-slice-render.bgra` |

`LargeFrame` 은 **무엇이 큰지를 말하지 않아서** 3072 를 싣는다고 읽혔다. 새 이름은 **파일이 손목 3072² 이고 적재가 1024²** 라는 두 사실을 함께 담는다. `Wrist` 가 픽셀 내용을, `1024Slice` 가 실제 적재 크기를 말한다.

## 2. 머리말 — 없는 것을 보증하던 문장을 지우고, 있는 것을 적었다

**지운 문장** (`:7-8`):

> *the size REQ-GSVG-019 states (1.0 s) and the size the post lane measured the module at*

이 픽스처는 그 크기를 싣지 않는다. 대신 네 단락을 넣었다:

| 단락 | 내용 |
|---|---|
| 무엇인가 | `wrist_lat_3072x3072.raw` 18 MB 파일의 **첫 1024×1024** 를 싣는다. 옛 이름과 옛 문장이 무엇을 약속했는지도 남겼다 |
| **왜 있는가(긍정형)** | 크기가 아니라 **화소 내용**. 같은 1024² 라도 기본 합성 프레임과 다르고, `GUI-C-117` 이 단계를 끊어도 통과하던 단언을 발견해 여기로 옮긴 것이 그 근거다 |
| `REQ-GSVG-019` | **이 픽스처에 기대지 않는다.** 모듈 요구이고 post 의 `QA-B-102/103`(실제 3072², 713–757 ms)로 충족된다. 이 컬렉션에 `1.0 s` 와 비교하는 자리는 없다 |
| **공백 (카드 §2)** | **3072 프레임의 GUI 종단 간 시간은 측정된 적이 없다.** 모듈 수치는 있고 GUI 통과 수치는 없다. 요구 위반은 아니지만 *"E2E 에서도 확인했다"* 로 읽을 근거가 없다 |

`:15-21` 의 **Measured caveat 는 그대로 보존**했다(카드 §1 지시). 지금은 그 위 단락들이 맥락을 주고, caveat 가 수치와 경위를 준다.

## 3. 참조 전수 (카드 §5 의 핵심 산출)

`#180` 형태 — 하나 틀리면 셋 — 를 피하려 **코드·설정 전 범위**를 훑었다.

**바꾼 곳 4파일 10개소**:

| 파일 | 개소 |
|---|---|
| `Fixtures/Wrist1024SliceApplicationFixture.cs` | 클래스·생성자·컬렉션·id 문자열·덤프 경로 |
| `Scenarios/Workflows/GsvgLargeFrameScenarios.cs` | `[Collection(...)]`, 주 생성자 매개변수, `RenderDumpPath` 참조 |
| `Scenarios/Workflows/LaneDenoiseScenarios.cs` | `[Collection(...)]`, 주 생성자 |
| `Scenarios/Workflows/NativeAlertScenarios.cs` | `[Collection(...)]`, 주 생성자 |

**전수 결과** (`*.cs`·`*.csproj`·`*.json`·`*.yml`, `bin/obj/build` 제외):

| 검색 | 결과 |
|---|---|
| `LargeFrameApplicationFixture` / `...Collection` / `gui-large-frame-application` / `large-frame-render` | **1건** — 그리고 그것은 **새 파일 머리말의 역사 기술**이다(*"previously called `LargeFrameApplicationFixture`"*). 결합하는 참조 **0건** |
| **대조군**: 새 이름이 실제로 박혔는가 | `Wrist1024SliceApplication` **10건** — 검색이 이 식별자를 읽고 있다는 관측 |
| 디스크에 옛 파일이 남았는가 | 없음(`git mv`) |
| CI 워크플로가 이 이름으로 거르는가 | `.github/workflows/*.yml` 에서 이 컬렉션명/클래스명으로 거는 필터 **없음** |

**보고서 안의 옛 이름은 고치지 않았다** — `.moai/reports/lane-gui/GUI-C-10x/…` 등은 그때의 관측 기록이라 소급 수정 대상이 아니다.

## 4. 반증 (카드 §4) — 그리고 **제 것이 아닌 빨강 1건**

### 4.1 빌드

`dotnet build` **오류 0개**(경고 1건은 `ProcessingChainScenarios.cs:322` 의 기존 xUnit2031, 이 카드와 무관).

### 4.2 컬렉션 실행 — 16 통과 / **1 실패**

```
Native: 실패 1, 통과 16, 건너뜀 0, 전체 17 (3 m 45 s)
  실패: GsvgLargeFrameScenarios.P10_ThePreviewChange_KeptTheDrawnPixels
        P10 hash=36fc547e253b07f1 mean=85.205  (recorded f2a5640e9bd1a7fc)
Mock:   실패 0, 통과 0, 건너뜀 17, 전체 17
```

### 4.3 귀속 — 팔로 갈랐다. **제 변경 소행이 아니다**

"이름만 바꿨으니 화소가 움직일 리 없다" 는 추론이지 관측이 아니다. 그래서 **워킹 트리를 HEAD 로 되돌려**(옛 이름) 다시 빌드하고 같은 두 시험을 돌렸다.

| 팔 | `P-10` | 해시 | `P-11` |
|---|---|---|---|
| **개명 전**(HEAD, 옛 이름) | **실패** | `36fc547e253b07f1` | **통과** |
| **개명 후**(제 변경) | **실패** | `36fc547e253b07f1` | **통과** |

**두 팔이 같은 해시로 같은 결과다.** 이 빨강은 제 변경 이전부터 있었다. 되돌린 파일은 저장본에서 복원했고 `git status` 로 확인했다.

### 4.4 그래서 `P-10` 을 어떻게 했나 — **아무것도 안 했다**

`P-10` 의 실패 메시지 자체가 절차를 적어 두었다: *"P-11 이 초록이면 그 변화는 정당한 보정에서 유도된 허용범위 안이고, 그 근거를 인용한 뒤 이 해시를 다시 기록하는 것이 맞다."* **`P-11` 은 두 팔 모두 초록**이므로 그 갈래에 해당한다.

그래도 **재기록하지 않았다.** 이유 둘:

1. **카드 §3 이 `#200` 의 시험을 손대지 말라고 한다.** `P-10`·`P-11` 이 바로 그것이다
2. **원인을 아직 인용하지 못했다.** 그 절차는 *"화소를 움직인 변경을 찾아 인용한 뒤"* 재기록하라고 한다. 후보는 있으나(실패 메시지가 예시로 드는 `#156` VOI 배치 보정 계열, 최근 `modules/display` 의 `efd14c1`) **확인하지 않았고, 확인 없이 적지 않는다**

**리더 판단 사항으로 올린다** — §6 참조.

## 5. 바꾼 것

| 파일 | 변경 |
|---|---|
| `Fixtures/LargeFrameApplicationFixture.cs` → `Fixtures/Wrist1024SliceApplicationFixture.cs` | 개명 + 머리말 재작성(caveat 보존) + 컬렉션 id·덤프 경로 |
| `Scenarios/Workflows/{GsvgLargeFrame,LaneDenoise,NativeAlert}Scenarios.cs` | 형식명 참조만. **단언·문턱·크기 인자 무변경** |

`--automation-width/height` · 새 3072 픽스처 · `#200` 시험 — **전부 손대지 않았다**(카드 §3).

## 6. 리더 판단이 필요한 것

- **`P-10` 빨강**: 제 변경 이전부터 있다(§4.3 의 두 팔). `P-11` 초록이라 허용범위 안이지만, **원인을 인용하기 전에는 재기록하지 않는다** 는 그 시험 자신의 지시를 따랐다. 원인 지목과 재기록 지시가 필요하다
- **같은 결함이 옆집에도 있다**: `GsvgLargeFrameScenarios` 의 머리말이 *"Measurements on the 3072×3072 frame"* 이라 적고, 시험 이름 하나가 `P01_RenderCost_OnA3072Frame` 이다. **이 픽스처와 같은 약속**이다. 카드가 픽스처 이름·머리말로 범위를 못 박았고 시험 개명은 `#180` 형태의 인용 파급을 낳으므로 **건드리지 않고 보고**한다

## 7. 미검증 / 잔여 위험

- **CI 에서 안 돌았다.** 위 수치는 전부 로컬이다 — **로컬 실측, CI 상한 추정**으로 읽어야 한다
- **`P-10` 빨강이 CI 에서도 나는지 모른다.** 제 `build/ci-common/bin` 의 DLL 은 `2026-09-26` 자 로컬 산출물이고, 기록된 해시가 어느 바이너리에서 나왔는지 대조하지 않았다. **로컬 전용 현상일 가능성이 남아 있다** — 판단 근거는 CI 로그여야 한다
- **Mock 에서는 17건 전부 건너뜀**이라 개명이 Mock 경로에서 실제로 실행된 관측은 없다(구조상 이 컬렉션은 Native 전용)
- 전체 E2E 스위트를 돌리지 않았다 — 이 세 시나리오 클래스만 돌렸다. 다른 클래스가 이 컬렉션 id 를 문자열로 참조할 가능성은 전수 검색으로만 배제했고 실행으로 배제하지 않았다
- 보고서 문서 안의 옛 이름(`GUI-C-102`~`138`)은 의도적으로 남겼다 — 소급 수정 안 함

---

Refs #208
