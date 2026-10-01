# GUI-C-171 — E2E `S07` 이 "View 메뉴가 열리지 않음" 으로 실패하는 원인

레인: gui · 이슈: `#225` · 진단 카드. 커밋은 푸시하지 않았다.

## 결론 (카드 §4 형식)

> **최소 재현 조합**: 가르지 못했다. 앞 시험을 하나씩 빼도 12/12 실패였고(§3), 짝·단독이 통과하던 것은 이번 카드에서 같은 조건으로 다시 재지 못했다(§6).
> **원인**: **기타 — 데스크톱 상태.** C-170 의 S07 재작성도, 특정 앞 시험의 종료 처리도 아니다. 시험이 보낸 **마우스 클릭과 ESC 키가 앱이 아니라 그 순간 앞에 있던 창으로 갔다.** 그 창은 같은 데스크톱의 `xpe-leader` 터미널(WindowsTerminal)이었다.
> **고칠 방향**: 메뉴를 클릭·키 입력 없이 UI Automation 패턴으로 열고 닫는다. 실제 입력이 시험 대상인 시험은 한 파일(`GlobalInput`)로 모으고 기본 실행에서 건너뛴다. **이 카드에서 시험 쪽을 수정했다**(§5).

## 1. C-170 의 S07 재작성이 원인인가 — 아니다

같은 트리, 같은 4개 조합(`A11·A13·A02·S07`)에서 S07 파일만 바꿔 교대 5회씩(작업 트리의 `S07` + 진단 vs `f7ca055` 의 옛 `S07`). 옛 `S07` 은 "항목이 있고 비활성" 을 보는 시험이며 첫 동작은 같은 View 메뉴 열기다. 옛 시험의 "항목 null" 은 항목이 XAML 에 있으므로 "메뉴가 안 열림" 과 같은 뜻이다.

| # | 팔 | 가용 GB | 결과 | 실패 종류 | 실패 시점의 창 |
|---|---|---|---|---|---|
| 1 | NEW-S07 | 7.5 | 통과 | - |  |
| 2 | OLD-S07 | 10.3 | 실패 (3/5) | 열리지 않음 (옛 S07: 항목 null) |  |
| 3 | NEW-S07 | 10.3 | 실패 (4/5) | 열리지 않음 | 전경=WindowsTerminal '✳ xpe-leader'; 클릭 아래=WindowsTerminal '✳ xpe-leader' @(354,414) |
| 4 | OLD-S07 | 8.8 | 실패 (3/5) | 열리지 않음 (옛 S07: 항목 null) |  |
| 5 | NEW-S07 | 10.4 | 실패 (4/5) | 열리지 않음 | 전경=WindowsTerminal '✳ xpe-leader'; 클릭 아래=WindowsTerminal '✳ xpe-leader' @(406,466) |
| 6 | OLD-S07 | 10.4 | 실패 (3/5) | 열리지 않음 (옛 S07: 항목 null) |  |
| 7 | NEW-S07 | 10.5 | 실패 (4/5) | 열리지 않음 | 전경=WindowsTerminal '✳ xpe-leader'; 클릭 아래=WindowsTerminal '✳ xpe-leader' @(354,414) |
| 8 | OLD-S07 | 10.4 | 실패 (3/5) | 열리지 않음 (옛 S07: 항목 null) |  |
| 9 | NEW-S07 | 7.9 | 실패 (4/5) | 열리지 않음 | 전경=WindowsTerminal '✳ xpe-leader'; 클릭 아래=WindowsTerminal '✳ xpe-leader' @(276,336) |
| 10 | OLD-S07 | 9.5 | 실패 (3/5) | 열리지 않음 (옛 S07: 항목 null) |  |

- **옛 S07 5/5 실패**(두 케이스 모두 "항목 null"), 현재 S07 1/5 통과. 같은 첫 동작이 재작성 전에도 실패한다.
- 현재 S07 의 실패 4건은 **전부** 전경 창이 `WindowsTerminal '✳ xpe-leader'` 이고, View 메뉴를 클릭하려던 좌표 아래의 창도 그 터미널이었다. 앱 메인 창은 정상이었다(최소화 아님, 화면 안, `keyboardFocus=True`).

## 2. 첫 클릭이 어디로 갔는가 — 앱을 덮은 다른 창

S07 의 **실패 경로에서만**(앵커가 2초 안에 안 보일 때) 전경 창, 앱 메인 창 상태, View 메뉴 클릭 좌표 아래의 창을 읽어 단언 메시지에 넣었다(통과 경로의 타이밍은 그대로, 읽기만 하고 입력은 보내지 않는다). 위 표의 마지막 열이 그 기록이다.

앱 창은 실행마다 다른 위치에 뜬다(클릭 좌표가 (172,232)~(432,492) 로 달랐다). 그런데 어디에 뜨든 그 아래에는 같은 터미널 창이 있었다 — 이 데스크톱에서는 터미널이 앱이 뜨는 영역을 덮고 있다.

## 3. 앞 시험이 남긴 상태인가 — 아니다 (시험을 하나씩 빼기)

현재 S07 + 진단, 4개 조합에서 한 시험씩 빼고 3회전(라운드 로빈).

| # | 팔 | 가용 GB | 결과 | 실패 종류 | 실패 시점의 창 |
|---|---|---|---|---|---|
| 1 | full4 | 7.9 | 실패 (4/5) | 열리지 않음 | 전경=WindowsTerminal '✳ xpe-leader'; 클릭 아래=WindowsTerminal '✳ xpe-leader' @(172,232) |
| 2 | no-A11 | 9.3 | 실패 (3/4) | 열리지 않음 | 전경=WindowsTerminal '✳ xpe-leader'; 클릭 아래=WindowsTerminal '✳ xpe-leader' @(224,284) |
| 3 | no-A13 | 10.5 | 실패 (3/4) | 열리지 않음 | 전경=WindowsTerminal '✳ xpe-leader'; 클릭 아래=WindowsTerminal '✳ xpe-leader' @(354,414) |
| 4 | no-A02 | 10.6 | 실패 (3/4) | 열리지 않음 | 전경=WindowsTerminal '✳ xpe-leader'; 클릭 아래=WindowsTerminal '✳ xpe-leader' @(380,440) |
| 5 | full4 | 9.5 | 실패 (4/5) | 열리지 않음 | 전경=WindowsTerminal '✳ xpe-leader'; 클릭 아래=WindowsTerminal '✳ xpe-leader' @(250,310) |
| 6 | no-A11 | 10.7 | 실패 (3/4) | 열리지 않음 | 전경=WindowsTerminal '✳ xpe-leader'; 클릭 아래=WindowsTerminal '✳ xpe-leader' @(406,466) |
| 7 | no-A13 | 10.8 | 실패 (3/4) | 열리지 않음 | 전경=WindowsTerminal '✳ xpe-leader'; 클릭 아래=WindowsTerminal '✳ xpe-leader' @(432,492) |
| 8 | no-A02 | 10.8 | 실패 (3/4) | 열리지 않음 | 전경=WindowsTerminal '✳ xpe-leader'; 클릭 아래=WindowsTerminal '✳ xpe-leader' @(224,284) |
| 9 | full4 | 10.8 | 실패 (4/5) | 열리지 않음 | 전경=WindowsTerminal '✳ xpe-leader'; 클릭 아래=WindowsTerminal '✳ xpe-leader' @(198,258) |
| 10 | no-A11 | 10.8 | 실패 (3/4) | 열리지 않음 | 전경=WindowsTerminal '✳ xpe-leader'; 클릭 아래=WindowsTerminal '✳ xpe-leader' @(354,414) |
| 11 | no-A13 | 10.8 | 실패 (3/4) | 열리지 않음 | 전경=WindowsTerminal '✳ xpe-leader'; 클릭 아래=WindowsTerminal '✳ xpe-leader' @(380,440) |
| 12 | no-A02 | 10.8 | 실패 (3/4) | 열리지 않음 | 전경=WindowsTerminal '✳ xpe-leader'; 클릭 아래=WindowsTerminal '✳ xpe-leader' @(380,440) |

**12/12 실패, 전부 같은 모양**(전경=터미널, 클릭 아래=터미널). 어느 앞 시험을 빼도 사라지지 않으므로 특정 앞 시험의 종료 처리가 범인이라는 가설은 지지되지 않는다. 범인은 시험이 아니라 데스크톱의 창 겹침이다.

## 4. 부작용 — 시험이 터미널에 입력을 보냈다

`OpenViewMenu`/`CloseMenu` 는 클릭 외에 `Keyboard.Press(ESC)` 를 보낸다. FlaUI 의 키보드·마우스 입력은 앱이 아니라 **전경 창**으로 간다. 실패하는 실행에서는 전경이 `xpe-leader` 터미널이었으므로, 그 실행들의 ESC 와 클릭이 리더 터미널에 전달됐다. 리더가 확인해 준 대로 그 세션에는 이유 없는 "Request interrupted" 가 여러 번 있었고 질문 하나가 "거절"로 처리됐다. **인과를 내가 직접 재현해 입증한 것은 아니다**(리더의 관찰과 내 진단이 일치할 뿐이다). 이 카드 이전의 S07 반복 실행(C-170/170b 의 A/B 실험 포함)도 같은 위험을 안고 있었다.

## 5. 수정 (시험 쪽 — 별도 커밋)

**입력 없이 메뉴를 다룬다.** `Fixtures/UiaMenu.cs`: 메뉴를 UIA ExpandCollapse 패턴(`Expand`/`Collapse`)으로 열고 닫는다. 이 호출은 요소에 주소가 붙어 있어 전경이 누구인지, 앱이 화면 어디에 있는지, 무엇이 덮고 있는지에 상관없고 다른 창에 아무것도 보내지 않는다. 재시도는 없다.

**TAB 커밋도 입력 없이.** `Fixtures/UiaInput.cs`: 텍스트 상자에 값을 쓴 뒤 TAB 으로 포커스를 옮기던 것을, UIA 로 다른 텍스트 상자에 포커스를 주는 것으로 바꿨다(바인딩은 포커스를 잃을 때 값을 받는다).

**실제 입력이 시험 대상인 것은 한 파일로 격리.** `Fixtures/GlobalInput.cs` 가 `Keyboard`/`Mouse`/`.Click` 을 부르는 **유일한 파일**이다. 모든 메서드가 먼저 `Require` 를 호출해, `XPE_E2E_ALLOW_GLOBAL_INPUT=1` 이 없으면 **무엇이 전송되기 전에** 시험을 건너뛴다. 대상: W14(키 제스처), W15(마우스 휠), W16(F5/F8), V02~V07 중 6건(휠·드래그), 복사 버튼(비활성 버튼에 대한 마우스 클릭). UIA `Invoke` 는 비활성 버튼에서 예외가 나므로 "마우스가 흡수되는가" 를 대신 볼 수 없다.

**W27 은 키 입력 대신 메뉴로 준비하게 바꿨다.** 키로 준비한 이유가 "메뉴로 준비하면 W14 키 입력 뒤에 항목을 못 찾는다(GUI-C-96, #163 증상)" 였는데, 그 원인이 위와 같은 것이었다.

기존 주석이 같은 현상을 이미 세 번 기록하고 있었다: `GUI-C-58/61`(다른 시험 뒤 메뉴 실패, 원인 불명), `GUI-C-80/81`(덮은 창이 클릭을 가로챔), `GUI-C-82`(첫 클릭이 메뉴를 못 엶). 모두 재시도·포그라운드 우회로 가려졌고, 이번에 재시도 루프 4곳(`ApplyDisplayPipeline`·`OpenDetached`·`InvokeFileMenuItem`·`ResetView`)을 결정적인 한 번의 `Expand` 로 바꿨다.

## 6. 입력 API 전수 (변경 전 → 후)

- **변경 전**: `input_scan_before.txt` — 67줄, 15개 파일. 메뉴 `Click` 10 · ESC 10 · TAB 5 · 키 제스처 3 · 마우스 휠/드래그 · 비활성 버튼 클릭 1.
- **변경 후**: `input_scan_after.txt` — **E2E 프로젝트의 직접 입력 호출 0건**(`GlobalInput.cs` 제외). `clients`+`gui` 전체 스캔의 1건은 제품 코드 `MainWindow.xaml.cs:79` 의 `Keyboard.Modifiers`(Ctrl 눌림 **읽기**)로 입력 생성이 아니다.
- `GlobalInput.cs` 안의 호출 7줄과, 그것을 거치는 호출 16줄(각 래퍼가 전송 전에 안에서 `Require` 를 먼저 거친다)이 같은 파일에 목록으로 있다.
- **입력이 아닌 포커스 API 는 남아 있다**: `SetForeground()` 8줄(8개 파일), `.Focus()` 7줄. 창을 앞으로 가져오거나 앱 안에서 포커스를 옮길 뿐 다른 창에 입력을 보내지 않는다. 화면 캡처로 픽셀을 읽는 시험이 창이 보여야 해서 쓰는 것이라 남겼다. 다만 성공하면 사용자가 타이핑하던 창에서 포커스를 빼앗는다.

## 7. 검증

**입력 없는 빌드, 같은 4개 조합 5회** (실행 전에 복사본에서 같은 grep 을 돌려 직접 호출이 0건임을 확인해야만 시작하도록 했다):

| # | 팔 | 가용 GB | 결과 | 실패 종류 | 실패 시점의 창 |
|---|---|---|---|---|---|
| 1 | FIXED-full4 | 8.0 | 통과 | - |  |
| 2 | FIXED-full4 | 9.1 | 통과 | - |  |
| 3 | FIXED-full4 | 8.1 | 통과 | - |  |
| 4 | FIXED-full4 | 9.0 | 통과 | - |  |
| 5 | FIXED-full4 | 10.7 | 통과 | - |  |

**전체 E2E**(`--logger trx`, 입력 API 0건 뒤에만 실행, 종료 코드 0): **실행 133 · 총계 177 · 건너뜀 44 · 실패 0.**
- 건너뜀 44 = **입력 게이트 13**(이전에는 전부 통과하던 시험) + 기존의 네이티브 전용 등 **31**. 이전 전체 실행은 통과 145 · 건너뜀 32 였고, 통과 12 감소는 게이트 13 에서 FlaUI 재획득 건이 건너뜀→통과로 바뀐 1건을 뺀 값과 같다(시험 이름별 비교로 확인).
- **변환된 코드가 실제로 실행되어 통과한 것**: `PanelToggle` 10 · `ComparisonEntryPoint` 12(W13 메뉴 6 + W27 메뉴 준비 6) · `DetachViewer` 1 · `ViewStateRender` V01 · `TypeCenter`/`TypeWidth` 를 쓰는 W07·W21·W22·W25(TAB 대체 검증, W22 는 커밋이 안 되면 실패하는 시험) · `OpenDetached` 를 쓰는 W25.

## 8. 미검증 · 한계 — 중요한 것부터

1. **수정 전 빌드와 교대로 비교하지 못했다.** 리더 지시로 입력을 보내는 실행을 모두 중단했으므로 수정 빌드(5/5 통과)를 수정 전 빌드와 같은 시간대에 나란히 재지 않았다. 근거는 "같은 데스크톱에서 이 세션 내내 입력 있는 빌드가 한 번도 안 통과했다(A/B 10 + 조합 12 + 앞 실행들)" 와 "입력 없는 빌드가 5/5" 이다. **그 사이에 창 배치가 바뀌었을 가능성은 배제하지 못했다.**
2. **네이티브 전용이라 실행되지 않은 변환**: `GsvgWristSliceScenarios`(13건 전부 건너뜀)의 `InvokeFileMenuItem`·`SetNumber`, `NativeAlertScenarios`(2건)의 `SettleMenuBar`, `ProcessingChainScenarios` 의 `SetNumber`/`SetText` 를 쓰는 C02·C04·C06·C07. 컴파일은 되지만 **돌려 보지 못했다.**
3. **게이트된 13건은 기본 실행에서 더 이상 돌지 않는다.** 실제 키·휠·드래그·비활성 버튼 클릭 시험의 기본 커버리지가 줄었다. 아무도 쓰지 않는 데스크톱에서 `XPE_E2E_ALLOW_GLOBAL_INPUT=1` 로 돌려야 하며, **나는 돌리지 않았다.**
4. **짝·단독 시험이 통과하던 것**(C-170 보고서 §8)은 같은 조건으로 다시 재지 못했다. 위 원인 설명(창 겹침)과 모순되지 않지만 그 통과들이 그 시점의 창 배치 때문이었다는 것은 **추정**이다.
5. **전경 권한이 막혀서라는 세부 메커니즘**(왜 `SetForeground` 가 앱을 앞에 못 세웠는가)은 가르지 않았다. 이 카드의 결론은 "입력이 다른 창으로 간다" 까지이고, 그 이유까지는 아니다.
6. S07 의 **실패 경로 진단 코드**(전경·창 상태 읽기)는 남겼다. 읽기만 하며 통과 경로에 영향이 없다.

## 증거 파일

`table_one_final.md` · `table_combos.md` · `table_fixed.md` · `run_<번호>_<팔>.txt`(회차별 원문) · `input_scan_before.txt` · `input_scan_after.txt` · `full_e2e.txt` · `trx/c171_full_e2e.trx` · `console_*.txt`

🗿 MoAI
