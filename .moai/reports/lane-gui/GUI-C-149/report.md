# GUI-C-149 (#208) — 제 차단 전제가 틀렸다. 클래스를 개명하고, `P02`~`P09` 를 전수했다

상태: **자기 정정 완료 · 클래스 개명 · 전수 표 완성 · 개명 2건 추가 · 반증 완료.**

전수 결과: **8건 중 2건 불일치.** 그중 하나(`P09`)는 **자기 단언이 자기 이름을 반박**하고 있었다.

---

## 1. 먼저 자기 정정 — `C-148` §1.2 의 차단 전제는 틀렸다

`C-148` 에서 이렇게 적었다:

> 클래스명은 33건/14파일이고 **그중 하나가 `#200` 의 `P10PreviewTileSignature.txt`** 다 — 카드 §4 가 막는 자리라 개명하면 완결이 안 된다.

**리더가 반박했고, 제가 다시 세어 확인했다. 리더가 맞다.**

| 인용처 | 성격 | 개명에 걸리나 |
|---|---|---|
| `GsvgLargeFrameScenarios.cs:45`(선언)·`:888`(`typeof`) | **소스 자신** | 같이 바뀐다 |
| `Fixtures/P10PreviewTileSignature.txt:9` | `# HOW TO UPDATE: read PreviewTileSignatureTolerance in …` — **산문 안내 한 줄** | 한 줄 고치면 끝 |
| `build/**` 96건 | 미추적 로컬 실행 로그 | 기록. 안 고침 |
| `.moai/reports/**` 14건 | 과거 보고서 | 기록. 안 고침 |

**무엇이 틀렸나**: 저는 *"`#200` 산출물이 클래스명을 인용한다 → 개명하려면 그 파일을 고쳐야 한다 → 카드가 막는다"* 로 갔다.
세 번째 화살표가 틀렸다. 카드가 막은 것은 **`#200` 의 주소**, 즉 **파일명과 기록된 서명값 1024개**다.
클래스명은 그 파일 **안의 설명글**이었다. 둘은 다르다.

**참조가 있다는 것과 그것이 주소라는 것은 다르다.** 저는 인용의 **존재**만 세고 **무엇을 주소로 쓰는지**는 열어 보지 않았다.
`C-148` 안에서 `P01` 에 대해서는 정확히 그 구분을 했으면서(결합 인용 vs 기록), 클래스명에서는 하지 않았다.

실측으로 확인한 것: 개명 후 `P10PreviewTileSignature.txt` 의 diff 는 **안내 문장 한 줄뿐**이고, 값 라인은 **1024개 그대로**다(§4.3).

## 2. 클래스 개명

`GsvgLargeFrameScenarios` → **`GsvgWristSliceScenarios`** (파일명 동반).

`LargeFrame` 이 대형 프레임을 약속하는데 적재는 1024² 슬라이스다 — `C-147`(픽스처)·`C-148`(`P01`)과 **같은 허위 보증**이고 마지막 자리였다.
클래스 doc 에 `C-147` 방식의 긍정형 사유를 남겼다: 옛 이름, 왜 바꿨는지, **그리고 왜 `#200` 이 영향받지 않는지**(주소는 파일명과 값이다).

`P10PreviewTileSignature.txt:9` 의 안내 문장도 새 파일명을 가리키게 고쳤다. **파일명·서명값은 건드리지 않았다.**

## 3. `P02`~`P09` 전수 — 두 열 표 (카드 §3, 이 카드의 핵심)

| # | 이름이 약속하는 것 | 단언이 실제로 하는 것 | 일치? |
|---|---|---|---|
| `P02_RepeatedRender_RunsTheStageAgain` | 반복 렌더에서 단계가 다시 돈다 | 2·3회차 `StageMs > 1.0` — 캐시가 있으면 0 으로 보고될 것 | **일치** |
| `P03_AssumedDefaults_ChangeTheImageByThisMuch` | **"이만큼" — 크기(magnitude)** | 두 해시 중 하나라도 기준과 다르다 = **도달 여부만**. `Δmean` 은 `WriteLine` 보고. 본문 주석도 *"No threshold is asserted"* 라고 적어 둠 | **불일치 — 이름이 과장** |
| `P04_TheDrawnHud_NamesTheChain` | 그려진 HUD 가 체인을 적는다 | HUD 문자열에 `chain:`·`gsvg=`·`times:` 와 상태 첫 절이 들어 있다 | **일치** |
| `P05_TheExportedReport_CarriesTheChainAndTheVignetteFlag` | 내보낸 보고서가 체인과 vignette 플래그를 싣는다 | `gsvgMode=="VirtualGrid"`, `status=="Applied"`, `reason` 에 `vignette=0`, `elapsedMs>0` | **일치** |
| `P06_WhatTheModuleDid_PerMode` | 모드별로 모듈이 무엇을 했는지 | 모드별 `reason` 이 공백이 아니다 | **일치**(약함 — 이름이 상한·내용을 약속하지 않으므로 과장은 아니다. §6 에 기록) |
| `P07_TheTimeToADrawnFrame_SplitsIntoPhases` | 시간이 단계들로 쪼개진다 | `work=`·`preview=` 존재 + `phases ≤ work + 1.0`(부분 합이 전체를 넘지 않음) | **일치** |
| `P08_TheAppsOwnStageTime_StaysWithinItsMeasuredSpread` | 측정된 산포 안에 머문다 | `median < profile.GateMs` — **이 파일에서 유일한 상한 단언**. 게이트가 측정 기준선의 배수이고, doc 이 검출 하한 1.6배와 주입 실측 4건까지 적어 둠 | **일치**(모범) |
| `P09_PyramidLevels_ChangeTheDrawnPixels` | **피라미드 레벨이 그려진 화소를 바꾼다** | `Assert.Equal(offHash, unityHash)` — **레벨만으로는 화소가 바이트 단위로 같다.** 바뀌는 것은 **gain** 을 1.0 에서 옮겼을 때(`Assert.NotEqual(offHash, gainedHash)`) | **불일치 — 자기 단언이 이름을 반박** |

**불일치 2 / 8.** 이름에 크기·해상도를 담은 시험은 `P02`~`P09` 에 **없다**(`P01` 이 유일했고 `C-148` 에서 고쳤다).
`Cost`·`Budget`·`Under`·`Within` 류 단어는 `P08` 의 `StaysWithin` 하나이고, 그것은 **실제로 상한을 단언한다.**

### 3.1 개명 2건 — 각각 인용 전수를 거쳤다

카드 지시대로 한 번에 바꾸지 않고 건별로 셌다.

| 옛 이름 | 결합 인용 | 기록성 인용 | 판단 | 새 이름 |
|---|---|---|---|---|
| `P03_AssumedDefaults_ChangeTheImageByThisMuch` | **1건**(선언) | `GUI-C-138/measured-p10.txt:71` | 개명 | **`P03_AssumedDefaults_ReachTheDrawnPixels`** |
| `P09_PyramidLevels_ChangeTheDrawnPixels` | **1건**(선언) | `GUI-C-138/measured-p10.txt:77` | 개명 | **`P09_PyramidGain_ChangesTheDrawnPixels_LevelsAloneDoNot`** |

`P09` 의 새 이름은 **두 단언을 다 담는다** — gain 이 바꾼다는 것과, 레벨만으로는 안 바뀐다는 것.
그 사실은 본문 주석에 이미 정확히 적혀 있었다(*"Levels ALONE cannot change the pixels, and this case says so rather than hiding it"*). **이름만 반대를 말하고 있었다.**

`P03`·`P09` doc 에 옛 이름과 개명 사유를 남겼다.

## 4. 반증 (카드 §5)

### 4.1 빌드 · 실행

`dotnet build` **오류 0개**.

```
Native, GsvgWristSliceScenarios 전체: 통과 13, 실패 1, 전체 14
  실패 = P10_ThePreviewChange_KeptTheDrawnPixels  ← 리더가 CI 로 로컬 전용 판정한 건. 손대지 않음
```

### 4.2 개명한 셋을 **이름으로 관측**

```
통과 …GsvgWristSliceScenarios.P03_AssumedDefaults_ReachTheDrawnPixels [17 s]
통과 …GsvgWristSliceScenarios.P09_PyramidGain_ChangesTheDrawnPixels_LevelsAloneDoNot [19 s]
통과 …GsvgWristSliceScenarios.P11_ThePreviewRender_StillCarriesTheRecordedTileSignature [13 s]
```

새 **클래스명**으로 전부 실행된다는 것이 같은 줄에서 보인다.

### 4.3 `#200` 이 상하지 않았다 — 두 방향으로 확인

**첫째, diff**: `P10PreviewTileSignature.txt` 의 변경은 **안내 문장 한 줄뿐**이다.
값 라인 수 **1024**(`HEAD` 도 1024). 중간에 BOM 을 넣었다가 즉시 제거했다 — `#` 앞에 BOM 이 붙으면 파서가 첫 줄을 다르게 읽을 수 있어 그대로 둘 수 없었다.

**둘째, 그 파일을 읽는 시험이 통과**: `P11` 이 초록이고 출력이
`P11 tiles=1024 mean|dtile|=0.001321 max|dtile|=0.043945` — **`GUI-C-138` 이 기록한 값과 같다.**
파일이 상했다면 여기서 먼저 터진다. **양성 대조군**이다.

### 4.4 잔여 인용

옛 식별자 셋(`GsvgLargeFrameScenarios`·옛 `P03`·옛 `P09`)이 코드·설정에 남은 자리는 **새 doc 의 역사 기술 3곳**뿐이다.
결합 인용 **0건**. 과거 보고서·`build/` 로그는 **의도적으로 남겼다** — 쓰인 시점의 기록이다.

## 5. 바꾼 것

| 자리 | 변경 |
|---|---|
| `Scenarios/Workflows/GsvgLargeFrameScenarios.cs` → `GsvgWristSliceScenarios.cs` | 클래스 개명 + 개명 사유 doc |
| 같은 파일 `P03` | `…ChangeTheImageByThisMuch` → `…ReachTheDrawnPixels` + 사유 doc |
| 같은 파일 `P09` | `…PyramidLevels_ChangeTheDrawnPixels` → `…PyramidGain_ChangesTheDrawnPixels_LevelsAloneDoNot` + 사유 doc |
| `Fixtures/P10PreviewTileSignature.txt:9` | 안내 문장의 파일명만. **값·파일명 무변경** |

**단언 본문·문턱·픽스처 크기·`P-10`·`P-11`** — 손대지 않았다.

## 6. 남기는 관측

- **`P06_WhatTheModuleDid_PerMode`** 는 이름이 과장하지 않으므로 불일치로 세지 않았다. 다만 단언이 *"`reason` 이 공백이 아니다"* 뿐이라 **내용은 보지 않는다** — 모듈이 엉뚱한 사유를 적어도 통과한다. 이름을 고칠 일은 아니고 **단언을 키울지는 별건**이다. 이 카드 범위가 아니라 보고만 한다
- `P10`·`P11` 은 카드 §4 대로 손대지 않았다

## 7. 미검증 / 잔여 위험

- **CI 에서 안 돌았다** — 전부 로컬. **로컬 실측, CI 상한 추정**
- **Mock 미실행.** 이 컬렉션은 Native 전용이고 시험을 추가하지 않아 전제 단정이 새로 생기지 않았다
- `P-10` 로컬 전용 판정은 **리더의 CI 관측을 인용**한 것이다. 다만 이번에 독립 단서가 하나 생겼다 —
  서명 파일 머리말이 *"Recorded from the **post-efd14c1 (#156)** xpe_display.dll"* 이라고 적는데, 제 스테이징은 그보다 이전 산출물일 수 있다.
  **바이너리 계보를 실제로 대조하지는 않았다**
- `P02`~`P09` 판정은 **단언문과 doc 을 읽어** 내렸다. `MeasureRender`·`Field`·`Mean` 같은 보조 함수가 이름과 다른 것을 읽는지까지는 추적하지 않았다
- `P01` 은 `C-148` 에서 이미 고쳐 이번 표에서 제외했다

---

Refs #208
