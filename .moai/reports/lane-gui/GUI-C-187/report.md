# GUI-C-187 — 파일 대화상자가 작업 디렉터리를 바꾸는가 (관측 카드)

레인: gui · 이슈: `#225` 행 10, `#130` · 출처: GUI-C-186 보고서 미검증 (2) · 코드 변경 없음(관측 카드). 푸시 없음.

## 결론

**바뀌지 않음.** 진짜 앱의 `Load Raw Image` 대화상자(`Microsoft.Win32.OpenFileDialog`, `RestoreDirectory` 미설정)를 서로 다른 세 폴더에서 확정했고, **세 번 모두** 프로세스의 `Environment.CurrentDirectory` 는 대화상자를 열기 전·폴더를 옮겨 다닌 뒤·확정 뒤가 같았다. 따라서 이 환경에서는 대화상자가 기본 모델 디렉터리(`data/models`)를 영상 폴더 아래로 풀리게 만들지 **않는다.** 결함이 아니므로 고칠 방향 두 가지는 적지 않는다(카드 3번 경로). 단 **한계가 둘 있다**(§3): 확정을 "Open 버튼"이 아니라 파일 항목의 열기(더블클릭에 해당)로 했고, 관측은 이 앱의 실행 파일이 아니라 같은 코드를 담은 임시 도구 프로세스에서 했다.

## 1. 방법

- 저장소 밖 임시 도구(`harness_Program.cs.txt`, `harness_csproj.txt`)가 **진짜 앱의 `App`·`MainWindow`·뷰모델**을 같은 프로세스에서 띄우고, 앱 자신의 `LoadImageCommand`(= 툴바의 Load Raw Image 버튼이 묶인 명령)를 실행해 앱이 직접 대화상자를 연다. 도구는 그 앞뒤로 프로세스의 현재 디렉터리를 읽는다.
- 대화상자는 **UI Automation 패턴만으로** 다뤘다. 키보드·마우스 입력은 어떤 형태로도 만들지 않았다(리더 지시: 전역 입력은 리더 터미널로 간다). 단계:
  1. 탐색 트리(`네임스페이스 트리 컨트롤`)에서 `ExpandCollapsePattern` 으로 `내 PC → (D:) → 시험 폴더` 를 펼치고, `SelectionItemPattern.Select()` 로 폴더 A/B/C 를 고른다(대화상자가 그 폴더로 이동). **이 이동 뒤에도** 현재 디렉터리를 읽었다.
  2. 파일 목록의 `image` 항목에 `InvokePattern.Invoke()`(열기 = 더블클릭에 해당) → 대화상자가 확정되고 닫힌다.
  3. 닫힌 뒤 2.5초를 두고(앱이 선택한 영상을 읽는 시간) 다시 읽는다.
- 시험 폴더는 `D:\xpe-c187-<무작위>\{A,B,C}`, 각각 번들 합성 영상(1024×1024)의 사본 `image.raw`. 끝나고 지웠고 남은 것 없음을 확인했다(내 이전 실패 실행이 `Temp` 에 남긴 `xpe-c187-*` 5개도 지움).
- 앱이 파일을 실제로 받았다는 증거: 상태줄이 `Load failed: Raw file is too small. Expected at least 18874368 bytes, got 2097152.` 이다. 설정의 기본 크기(3072²×2 바이트)와 시험 영상(1024²×2 = 2097152 바이트)이 달라 읽기는 실패했지만, **대화상자가 파일 경로를 돌려줬고 앱이 그 파일을 열었다**는 뜻이다. 현재 디렉터리와는 무관하다.

## 2. 관측 출력 (원문, `observation.txt` 에서 발췌 — 시각은 시험 실행 시각)

```
process id 37592; launched with current directory = C:\Users\drake\AppData\Local\Temp\claude\D--workspace-github-xpe-gui\a336fd5a-dd76-4e0c-b047-c2a7ee584ec1\scratchpad\c187\bin\Debug\net8.0-windows
dialog is Microsoft.Win32.OpenFileDialog; RestoreDirectory is not set by the app (MainWindowViewModel.LoadImage)
test folders: D:\xpe-c187-e00c7337\{A,B,C} each holding image.raw (a copy of the bundled 1024x1024 synthetic fixture)
round 1: BEFORE            Environment.CurrentDirectory = C:\Users\drake\...\scratchpad\c187\bin\Debug\net8.0-windows
round 1: dialog found: name='Load Raw Image' class='#32770'
round 1: selected the tree item 'A' (SelectionItemPattern)
round 1: after navigating the dialog to D:\xpe-c187-e00c7337\A (nothing confirmed yet): Environment.CurrentDirectory = C:\Users\drake\...\scratchpad\c187\bin\Debug\net8.0-windows
round 1: invoking the list item 'image' (InvokePattern: what a double-click does)
round 1: dialog closed=True; AFTER             Environment.CurrentDirectory = C:\Users\drake\...\scratchpad\c187\bin\Debug\net8.0-windows
round 1: changed = False; equals the opened folder = False
round 1: status bar: Load failed: Raw file is too small. Expected at least 18874368 bytes, got 2097152.
round 2: ... selected the tree item 'B' ... dialog closed=True; AFTER = (같은 경로) ... changed = False; equals the opened folder = False
round 3: ... selected the tree item 'C' ... dialog closed=True; AFTER = (같은 경로) ... changed = False; equals the opened folder = False
test folders deleted
```

(경로의 `...` 는 보고서에서만 줄인 것이다. 원문 전체와 각 라운드의 BEFORE/AFTER 값은 `observation.txt` 에 그대로 있고, 세 라운드 모두 BEFORE·이동 뒤·AFTER 가 글자 그대로 같다.)

## 3. 미검증 · 한계

1. **확정을 "Open 버튼"으로 하지 못했다.** 이 대화상자의 `열기(O)` 컨트롤은 UI Automation 에서 클래스 `Button` 인 `Pane` 이고 **어떤 패턴도 노출하지 않는다**(Invoke 없음, `LegacyIAccessible` 도 FlaUI 로 확인해 없음, 같은 줄의 `취소` 버튼은 Invoke 를 노출). 그래서 같은 대화상자의 다른 확정 경로(목록 항목 열기)를 썼다. 두 경로가 대화상자 내부에서 같은 "확정" 처리로 이어지는지, **Open 버튼·파일 이름 입력 후 Enter 로 확정할 때도 현재 디렉터리가 안 바뀌는지는 관측하지 못했다.**
2. **프로세스는 앱의 실행 파일이 아니다.** 같은 `App`/`MainWindow`/뷰모델/대화상자 코드가 임시 도구 프로세스에서 돌았다. 현재 디렉터리는 프로세스 단위 속성이라 같은 결과일 가능성이 높지만, `ImageProcTest.exe` 자체를 띄워 본 것은 아니다.
3. **한 환경만**: 한국어 Windows 11, 한 대화상자 형태(Vista 이후 항목 대화상자 — 트리 `FolderBandModuleInner` 로 확인). 다른 Windows 버전, `RestoreDirectory = true`, 네트워크(UNC) 폴더, 대화상자의 즐겨찾기·최근 항목 경로는 보지 않았다.
4. **그 앞 질문의 나머지는 이 카드로 풀리지 않는다**: 바로 가기의 "시작 위치"·`dotnet run` 의 작업 디렉터리는 여전히 미검증이다(GUI-C-186 §1).
5. 영상 읽기는 크기 불일치로 실패했다(위). 영상 읽기가 성공하는 경로에서 현재 디렉터리가 바뀌는지는 이 관측이 보지 못한다(읽기 자체가 디렉터리를 바꾸는 코드는 찾지 못했다 — 검색: gui 소스에서 `SetCurrentDirectory` 0건. `Environment.CurrentDirectory` 는 GUI-C-186b 의 기준 캡처가 **읽기만** 한다).

## 4. 이 결과가 의미하는 것

- GUI-C-186 이 걱정한 경로(대화상자가 작업 디렉터리를 영상 폴더로 옮겨 기본 모델 디렉터리가 영상 폴더 아래로 풀림)는 **이 환경의 이 확정 경로에서는 일어나지 않았다.**
- 그래도 GUI-C-186b 의 "시작 때 기준 디렉터리를 한 번 정함"은 그대로 필요하다: 이 관측은 한 환경·한 경로의 결과이고, 기준 고정은 작업 디렉터리가 어떤 이유로든 움직여도 같은 설정이 같은 경로를 뜻하게 하는 보험이다. 이 카드는 그 구현을 바꾸지 않는다.

## 증거 파일

`observation.txt`(원문) · `harness_Program.cs.txt` · `harness_csproj.txt`(FlaUI 참조는 Open 컨트롤 확인에 쓰였고 현재 `Program.cs` 는 쓰지 않음) · `text_lint.txt`

🗿 MoAI
