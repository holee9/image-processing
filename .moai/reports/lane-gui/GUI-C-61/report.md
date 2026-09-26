# GUI-C-61 — 1건이 실패하는 이유를 좁힌다 (#149)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-61 · Refs #149 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- **커밋 없음** — 조사 카드이고 제품 변경이 없습니다. 증거는 이 보고서입니다.
- **C-60 에서 제가 "결정적" 이라고 적은 것이 틀렸습니다**(§1).
- **원인 미확정.** 위치·양상은 특정했고 후보 둘을 추가로 배제했습니다.
- **결과: Native 3회 0 실패 · Mock 0/57/2/59 · 통합 0/193/1/194 · slnx 0경고 0오류**

---

## 1. 먼저 정정 — "결정적" 이 아닙니다

C-60 에서 저는 `Escape` 를 빼면 **3/3 결정적으로 1건 실패**한다고 적었습니다. 같은 조건에서
**6회** 돌려 봤습니다.

| 실행 | 결과 | 벽시계 |
|---|---|---|
| 1 | 통과 (18/18) | 14 s |
| 2 | 실패 1건 — `ProcessedOnly` | 19 s |
| 3 | 실패 1건 — `Difference` | 19 s |
| 4 | 실패 1건 — `ProcessedOnly` | 19 s |
| 5 | 통과 (18/18) | 14 s |
| 6 | 실패 1건 — `Difference` | 19 s |

**6회 중 4회 실패, 2회 통과.** C-60 의 3/3 은 **우연히 일치한 표본**이었고, 세 번으로 "결정적" 을
말한 것이 틀렸습니다. (실패한 실행이 5 s 더 걸리는 것은 실패 케이스의 `WaitFor` 만료
시간입니다 — 숫자가 서로를 지지합니다.)

**여전히 맞는 것**: 실패할 때는 **정확히 1건**이고, 어느 케이스인지는 고정이 아닙니다.

## 2. 위치 — trx 의 시작시각으로 실행 순서를 뽑았습니다

**실행 순서는 6회 모두 동일**했습니다(W-11 4건 → W-12 4건 → W-14 4건 → W-13 6건).

| 실행 | 실패 위치 | 그 위치의 케이스 |
|---|---|---|
| 2 · 4 | **13번** | W-13 의 **첫 번째** (`ProcessedOnly`) |
| 3 · 6 | **15번** | W-13 의 **세 번째** (`Difference`) |
| 1 · 5 | — | — |

**위치는 고정이 아닙니다.** 13번은 "키보드 시나리오 직후의 첫 메뉴 케이스" 이지만 15번은 메뉴
케이스 두 건이 성공한 뒤입니다. **"첫 번째" 가설도 "마지막" 가설도 성립하지 않습니다.**

## 3. 양상 — 모드가 안 바뀐 것이 아니라 **항목이 트리에 없습니다**

실패 메시지를 읽었습니다. 이것이 이 카드에서 가장 크게 범위를 줄입니다.

```
'CompareProcessedOnlyMenuItem' was not found after opening View → Compare Mode.
'CompareDifferenceMenuItem'   was not found after opening View → Compare Mode.
```

**"눌렀는데 모드가 안 바뀌었다" 가 아니라 "누를 것을 못 찾았다" 입니다.** 클릭 경로의 문제가
아니라 **UIA 트리에 항목이 나타나지 않는** 문제입니다.

프로브로 그 순간을 한 번 잡았습니다(24 사이클 중 1회):

```
FINDPROBE 03 CompareOverlayMenuItem: window=MISS desktop=MISS ViewMenu=Expanded Compare=absent
```

- `ViewMenu=Expanded` — **View 메뉴는 열려 있습니다.**
- `CompareModeMenuItem=absent` — 그런데 방금 클릭한 **하위 메뉴 부모 자신이 트리에서
  사라졌습니다.**
- `desktop=MISS` — 데스크톱 전체를 뒤져도 없습니다.

## 4. 배제 목록 (C-56 방식)

| 가설 | 판정 | 근거 |
|---|---|---|
| 키 제스처가 메뉴를 열어 둔다 | 배제(C-60) | F5~F8 뒤 전 메뉴 `Collapsed`, 포커스 창 |
| 항목 `Invoke()` 가 메뉴를 안 닫는다 | 배제(C-60) | invoke 뒤 `Collapsed`, 3회 반복 정상 |
| 사전 전환 버튼이 직후 메뉴를 방해한다 | 배제(C-60) | 버튼 포함 3회 반복 정상 |
| **팝업이 창 트리 밖(데스크톱)에 있다** | **배제** | 실패 시 **데스크톱 검색도 MISS**(§3) |
| **실패 위치가 고정이다**(첫/마지막) | **배제** | 13번과 15번 양쪽에서 발생(§2) |
| **특정 항목의 문제다** | 배제(C-60 재확인) | `ProcessedOnly`·`Difference`·`SourceOnly`·`Overlay` 넷에서 관측 |
| 5 s 대기가 짧다 | **미검증** | 대기 만료 시점에도 부모가 `absent` 였으므로 "곧 나타났을" 정황은 없으나, 더 긴 대기를 시도하지는 않았습니다 |

**원인은 확정하지 못했습니다.** 남은 방향은 "클릭이 하위 메뉴를 여는 대신 부모 팝업을
닫아 버리는 순간이 있다" 이지만, **그것을 바꿔서 증상이 사라지는 것을 보이지 못했으므로
원인이라고 적지 않습니다.**

**재현 난이도의 차이도 기록해 둡니다**: 스위트는 6회 중 4회(메뉴 케이스 36건 중 4건 ≈ 11 %),
격리 프로브는 72 사이클 중 1건(≈ 1.4 %). **같은 조작인데 스위트에서 훨씬 자주 납니다** —
무엇이 다른지는 모릅니다.

## 5. 할 일 2 — 하나는 봤고, 하나는 못 봤습니다

### (a) 메뉴 체크 — **실행으로 확인했습니다**

임시 프로브로 앱을 띄워 `Toggle` 패턴을 읽었습니다.

```
READOUT after 'Difference' (mode=DifferenceHeatmap):
    Swipe=Off Split=Off Overlay=Off Difference=On SourceOnly=Off ProcessedOnly=Off
READOUT after 'Swipe' (mode=SwipeVertical):
    Swipe=On  Split=Off Overlay=Off Difference=Off SourceOnly=Off ProcessedOnly=Off
```

**정확히 하나가 켜지고, 그것이 현재 모드입니다.** C-58 이 넣은 뒤 한 번도 실행으로 보지 않았던
것이 이제 관측됐습니다.

**덤 — C-60 의 게이트가 파일 경로에서 실제로 동작합니다.** 이 프로브는 배포된
`appsettings.json` 의 `comparisonMode` 를 `"NotAMode"` 로 바꿔 놓고 띄웠는데:

```
READOUT mode at launch: SwipeVertical
```

C-59 에서 같은 조작이 `NotAMode` 를 그대로 보고하던 자리입니다.

### (b) 기동 로그 — **못 봤습니다. 이유가 있습니다**

```
READOUT LogListBox found without doing anything: False
READOUT ShowLogsPanelMenuItem found: True
READOUT LogListBox after showing the panel: False
READOUT tabs present:            ← TabItem 0개
READOUT LogListBox was never reachable.
```

`LogListBox` 는 `Views/AnalysisPanel.xaml:352` 에 있는데 **View 메뉴로 패널을 켜도 UIA 트리에
나타나지 않고, `TabItem` 도 하나도 없습니다.** C-34 가 기록한 "탭이 선택될 때까지 자식이 트리에
없다" 와 같은 계열로 보이지만 **탭 자체가 안 보이므로 그 회피도 쓸 수 없습니다.**

**그리고 파일로 우회하는 길도 막혀 있습니다** — 로그를 파일로 내보내는 유일한 경로는 자동화
보고서인데, **자동화 모드는 배포된 `appsettings.json` 을 읽지 않습니다**
(`MainWindow.xaml.cs:95-117`, 격리된 설정을 씁니다). 즉 **"잘못된 설정을 읽는 조건" 과 "로그를
파일로 얻는 조건" 이 서로 배타적**입니다.

**C-46 의 사유(배포 설정 파일을 고치는 테스트는 커밋하지 않는다)는 여전히 유효하고**, 이번에도
임시 프로브로만 하고 지웠습니다. 다만 **이 항목을 막은 것은 그 사유가 아니라 도달 불가**입니다.

## 6. 미검증 (Gaps)

- **원인 미확정**(§4). 배제 목록이 결과입니다.
- **더 긴 대기를 시도하지 않았습니다.** 5 s 만료 시점에 부모가 `absent` 였다는 관측은 있지만,
  30 s 를 기다리면 나타나는지는 모릅니다.
- **스위트와 프로브의 발생률 차이(11 % vs 1.4 %)의 원인을 모릅니다.**
- **실패 순간의 팝업 내부를 못 봤습니다.** 그 목록을 찍는 프로브를 추가한 뒤에는 48 사이클 동안
  한 번도 재현되지 않았습니다 — **관측을 넣으니 사라진 것**이고, 그 자체가 타이밍 성격을
  시사하지만 **증거는 아닙니다.**
- **기동 로그가 실제로 출력되는지는 여전히 미관측**(§5b). 코드 경로만 확인했습니다.
- **`Escape` 회피가 필요한지 재확인하지 않았습니다** — 유지하라는 지시대로 두었고, 없을 때
  실패한다는 것만 재확인했습니다.

## 7. 잔여 위험 (Residual risk)

- **`Escape` 없이는 메뉴 시나리오가 약 11 % 실행에서 1건 실패합니다.** 회피가 빠지면 CI 가
  간헐적으로 붉어집니다.
- **회피가 가리는 것은 없다는 C-60 의 결론은 유지됩니다** — 이번 관측도 메뉴가 열린 채 남는
  상태를 보여 주지 않았습니다. 다만 **트리에서 항목이 사라지는 순간**이 실제 사용자에게 어떻게
  보일지는 모릅니다(UIA 관점의 관측이고, 화면 픽셀은 보지 않았습니다).
- **`LogListBox` 가 UIA 로 도달 불가**라는 것은 이 카드 밖의 문제입니다 — 로그를 단언하는 어떤
  시나리오도 지금은 쓸 수 없습니다.

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.E2ETests/… --no-build \
  --filter 'FullyQualifiedName~ComparisonEntryPointScenarios|FullyQualifiedName~ComparisonModeScenarios' \
  --logger "trx;LogFileName=set18-N.trx" --results-directory build/e2e-c61
# trx 의 startTime 으로 실행 순서와 실패 위치를 뽑음
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
powershell -File clients/ImageProcTest.E2ETests/Staging/Repeat-E2E.ps1 \
  -Times 3 -Backend Native -NativeDir build/ci-common/bin -ResultsDirectory build/e2e-c61final
```
