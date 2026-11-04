[2026-10-23 17:09 KST] [xpe-gui] 결과 — 두 세트 × 실제 DPI 96/144의 네 UIA 흐름을 실행했고 GUI 저장물과 CLI 출력은 네 조건 모두 비트 단위로 같았다. 단, 최종 PNG 56장의 주 창 본문은 흰색이어서 화면 구성·한글 가독성은 입증하지 못했다. 관찰 사실을 인계하며 합격 판정은 하지 않는다.

## 최종 증거와 실행 범위

- 최종 실행: [run-20261023-165511](run-20261023-165511/). 각 조건의 명령·종료 코드·크기·해시는 [flow-summary.md](flow-summary.md)에 있다.
- **스크린샷 56장 각각의 표**: [screenshot-review-v2.md](run-20261023-165511/screenshot-review-v2.md). 실제 DPI, PNG 크기, UIA 화면 밖 컨트롤 수, □/U+FFFD/?/인코딩 의심 표식/… 이름 수, 읽은 FontSize와 9pt 미만을 기록했다. 실제 렌더를 읽지 못한 항목은 미확인으로 남겼다.
- 스크린샷·UIA·비교·로그 **125개 파일의 SHA-256와 바이트 수**: [sha256.tsv](run-20261023-165511/sha256.tsv). 재계산한 불일치 0개. 구조화 결과: [evidence-summary-v2.json](run-20261023-165511/evidence-summary-v2.json).
- RAW는 기존 비교 batch의 s0_gf/s1_um300 첫 프레임(3072×2560)을 각각 새 실행 폴더에 복사해 GUI로 열었다. 같은 batch에서 변환된 기존 XCal을 읽었다. 원본 52개를 manifest SHA-256로 최종 재대조했으며 불일치 0개였다.

| 세트 | 배율 / GetDpiForWindow | 도구 / 앱 종료 | GUI·CLI 비교 | 파일당 바이트 | 앱 잔여 |
|---|---|---|---|---:|---:|
| s0_gf | 100% / 96 | 0 / 0 | 같음 | 31,457,280 | 0 |
| s1_um300 | 100% / 96 | 0 / 0 | 같음 | 31,457,280 | 0 |
| s0_gf | 150% / 144 | 0 / 0 | 같음 | 31,457,280 | 0 |
| s1_um300 | 150% / 144 | 0 / 0 | 같음 | 31,457,280 | 0 |

앱 시작 → 캘리브레이션 3폴더 Browse → RAW 열기 → Run Preprocessing → Metrics → float32 저장 → File/Exit 순서다. 각 단계의 창 전체 PNG와 UIA 요소 덤프를 남겼다. 종료 후에는 창이 없으므로 `08-exit.uia.json=[]`과 프로세스 종료 기록을 남겼고, 종료 직전 창은 `07-saved` PNG다. Windows 배율은 UIA로 변경했으며 finally에서 원래 **100%(권장)**으로 복원했다(`RESTORE_SCALE_EXIT=0`, `display-restored-100.json`). 이미지를 확대해 150% 조건으로 대신하지 않았다.

## 화면·글자 발견 항목 — 판정 전 관찰

| 항목 | 실제 관찰 | 확인 범위 / 남은 사항 |
|---|---|---|
| 주 창 본문이 흰색 | 최종 PrintWindow PNG 56개 모두 제목 표시줄을 제외한 본문에서 RGB 각 채널 245 초과 화소 비율이 99%를 넘었다. `06-metrics` 표본은 100%였다. | 레이아웃·겹침·잘림·빈 영역·버튼 가독성을 이 PNG로 정상 입증할 수 없다. PrintWindow 성공은 내용 렌더의 입증이 아니다. |
| 실제 화면 캡처 범위 | 최종 실행은 다른 프로세스가 전경에 있어 screen 캡처를 생략하고 앱 창만 PrintWindow로 캡처했다. 이전 탐색 실행 `run-20261023-162704/s0_gf-uia3-163842/`의 screen PNG에서도 주 창 본문 흰색과 정상적으로 그려진 Win32 대화상자를 직접 보았다. | 최종 144 DPI 화면의 실제 표시와 PrintWindow의 차이는 별도 확인이 필요하다. 타 앱 화면을 대신 캡처하거나 전역 입력으로 전경을 빼앗지 않았다. |
| 9pt 미만 | UIA TextPattern의 `LaneBVoiWidthInput`, `VerdictNotesInput`은 **8.25pt**였다(두 DPI 모두). | 다른 많은 TextBlock의 FontSize는 UIA 미지원이었다. 이를 9pt 이상으로 간주하지 않았다. 각 PNG별 읽기 분모/분자를 표에 기록했다. |
| Metrics 자리표시자 | PRE Applied와 저장 가능 상태 이후에도 Metrics에는 `METRIC`, `Metrics populated after pipeline run.` 문구가 남아 있었다. | 지표 표가 실제 수치로 채워졌다는 증거는 얻지 못했다. 원인/수정은 이 관측 카드에서 확정하지 않는다. |
| 선택적 후단 DLL 부재 | `ChainStatusText`: `preprocess=Applied, enhance_basic=RequestedNotApplied`. pinned 폴더에 `xpe_enhance_basic.dll`이 없다는 이유가 표시되며 노출지수도 미측정으로 기록됐다. | 이 카드는 GUI+PRE+Display를 빌드했다. 이 사실을 PRE 실패나 노출지수 측정 성공으로 바꾸지 않았다. Metrics 자리표시자의 원인으로 단정하지 않는다. |
| 한글/말줄임 | UIA 이름에서 □/U+FFFD 및 지정 인코딩 의심 표식은 최종 PNG 대응 덤프에서 0개, … 이름은 0개였다. ? 이름은 최대 2개이며 Help의 정상 `?` 표식도 포함된다. | 흰 본문 때문에 실제 한글 글리프 깨짐과 렌더 중 잘린 문구는 미확인이다. UIA 문자열 정상과 실제 가독성을 동일시하지 않는다. |
| 화면 밖 컨트롤 | 각 덤프의 `IsOffscreen` 버튼/메뉴 수를 이미지별 표에 기록했다. | 숨겨진 메뉴 항목도 포함하므로 실제 잘림 확정 수로 해석하지 않는다. |

## 실패한 시도와 보완 사항

이전 시도는 `run-20261023-162704/`에 보존했다. 기본 UIA2에서는 폴더 입력의 Value 패턴이 노출되지 않아 UIA3로 바꿨다. 폴더 경로를 슬래시로 전달한 시도에서는 `폴더 이름이 올바르지 않습니다.`가 표시되어 Windows 경로 구분자로 바꿨다. 초기 저장 입력 시도에서는 `002.raw`에 대한 덮어쓰기 확인이 떴지만 **확인하지 않고 앱을 종료**했다. 이후 새 폴더의 RAW 복사본을 열고 앱 제안 파일명으로 저장했다. 원본 52개의 최종 해시 불일치가 0개임을 별도로 확인했다.

150%에서는 메뉴 팝업을 파일 대화상자로 잘못 잡은 도구 시도가 있었고, 대화상자 제목과 실제 HWND 소멸을 확인하도록 보완했다. 최종 네 조건은 이 보완 이후의 실행이다. 도구 내부 코드 검토에서 바이트 불일치와 비동기 Invoke 오류가 종료 코드에 반영되지 않을 수 있는 점을 찾아, 불일치 종료 1/Invoke 오류 전파/실제 DPI 확인을 추가했다. 최종 네 조건 재실행과 동일 크기의 다른 세트 출력으로 만든 음성 대조(`equal=false`, 종료 1)를 확인했다. 이 내부 도구 검토는 독립 QA나 정식 Reviewer 판정을 대체하지 않는다.

## 검증·커밋·인계

- 금지 입력 검색: `git grep --no-index -n -e Mouse\. -e Keyboard\. -e SendInput -- clients/ImageProcTest.GuiObservation/Program.cs clients/ImageProcTest.GuiObservation/PreCli.cs .moai/reports/lane-gui/GUI-C-255/observe.ps1 .moai/reports/lane-gui/GUI-C-255/dpi150.ps1 .moai/reports/lane-gui/GUI-C-255/final-run.ps1`: 종료 **1**, 출력 없음 = 검색 범위에서 일치 **0건**.
- `git diff --check`, `git diff --cached --check`: 종료 0. 관측 도구 빌드: 종료 0, 경고 0/오류 0. 제품 GUI 빌드는 기존 경고 22개/오류 0개였으며 빌드 증거에 구분했다.
- `verify.ps1`: 종료 0, `OWNED_GUI_REMAINING=0`, 최종 확인 `2026-10-23T17:07:08.4286119+09:00`. 이 세션이 띄운 네 최종 앱 PID를 다시 조회했다.
- 0단계 커밋 **9f480960** — 빌드 기록. main 병합 **aca11dfc**.
- 1단계 커밋 **71464bbc** — UIA 관측 도구·CLI 비교·실행 기록. 제품 동작 변경 없음.
- 2단계는 이 보고서·스크린샷별 표·해시 인덱스 커밋이다. 최종 SHA는 [handoff.md](handoff.md)에 후속 기록한다. 큰 RAW/PNG 원본은 로컬 증거 폴더에 보존하고 해시/관찰표를 커밋한다.
- 기존 미커밋 변경을 보존했다. 2G DLL 실행, `.dcm`/`.par` 열기, 장비 연결, 타 워크트리 쓰기, push 없음. GUI-C-254c patch는 재적용하지 않았다.

리더에게 남기는 사항은 **흰 본문의 원인과 실제 표시 확인, Metrics 수치 표, 글꼴/가독성**이다. 이번 기록은 합격 선언이 아니며 QA 독립 재실행과 Reviewer 검토 후 리더 판정을 요청한다. 작성 직전 `date.exe -Iseconds`: `2026-10-23T17:09:13+09:00`.

## 이전 진행 기록

## 0단계 빌드 기록

- 실행: `.moai/reports/lane-gui/GUI-C-255/build.ps1` (Windows PowerShell, 작업 프로세스 환경 복원 후). 실행 디렉터리 `run-20261023-162704/`.
- 로컬 표준 스크립트 `tools/ci/Invoke-LocalVsCommonBuild.ps1 -BuildDir build/gui-c255-native`는 MSVC 환경을 정상 구성했다. 최초 네이티브 빌드는 vcpkg applocal 후처리에서 `powershell.exe`를 PATH에서 찾지 못해 종료 1이었다.
- 원문: `'powershell.exe'은(는) 내부 또는 외부 명령, 실행할 수 있는 프로그램, 또는 배치 파일이 아닙니다.` (UTF-16 원문 로그 `native-retry.log`).
- 작업 프로세스 PATH에 Windows PowerShell 디렉터리를 추가하고 `native-retry.ps1`에서 VS 환경을 구성한 뒤 `cmake --build build/gui-c255-native --target xpe_preprocess xpe_display --parallel 4`: 종료 **0**. 로그 `native-retry-2.log`.
- `dotnet build gui/ImageProcTest/ImageProcTest.csproj -c RelWithDebInfo -p:Platform=x64`: 종료 **0**, 경고 22개/오류 0개. 앱 경로 `gui/ImageProcTest/bin/x64/RelWithDebInfo/net8.0-windows/ImageProcTest.exe`.
- 카드에 적힌 입력 폴더는 image-processing 안에 없었다. 리더 메시지 `20261022-1707-qa.md` 등에 기록된 실제 경로 `D:/workspace-github/xpe-2g-offline/batch-20261022-1230/in/`를 확인했다. 원본 RAW와 동일 batch로 변환된 `pre-w3072-20261022-1240/<set>/xcal`을 읽기 전용으로 사용한다. 2G 실행/.par/.dcm 열기 없음.

## 최초 수신

[2026-10-23 15:17 KST] [xpe-gui] 수신 — 세션명 xpe-gui, 현재 Codex thread ID 01a15dc2-bad7-7fd0-8716-0b632a2f048f, 내부 에이전트 /root. 리더의 GUI-C-255 카드를 수신했으며 0단계 main 병합부터 착수한다. 회신 채널은 협업 규약에 따라 이 파일이다.

## 세션 통신 및 착수 기록

[2026-10-23 15:19 KST] [xpe-gui] 리더 보고: 현재 세션명은 **xpe-gui**이며, 통신에 사용할 현재 thread ID는 **01a15dc2-bad7-7fd0-8716-0b632a2f048f**다. `list_threads`에서 active 상태와 제목을 확인했다. 과거 명부의 `01a15c15-70d5-7952-8c53-adfafe1175f2`는 현재 활성 세션 주소가 아니다.

- 리더에게 보내는 채널: `lane-sessions.md` §3 및 `collab-roster-20261021.md` §2에 지정된 자기 워크트리의 이 보고서. 현재 도구 목록에는 Claude `SendMessage`가 없으므로 별도 Codex 세션을 리더라고 추정하여 보내지 않았다. 규약상 리더가 이 경로를 직접 읽는다.
- 받은 작업: GUI-C-255 (Refs #278), 실제 GUI PRE 흐름 2세트, 단계별 전체 창 스크린샷과 UIA 덤프, GUI 저장물/CLI 비트 비교, DPI 100%/150% 화면과 글자 관찰. 전역 입력 및 push 금지.
- 0단계 실행: `git merge main`, 종료 코드 **0**, `Merge made by the 'ort' strategy.` 충돌 없음. 병합 커밋 **aca11dfc**. 사용자 지시가 명시한 main 병합만 수행했다.
- 병합 뒤 기존 삭제/수정 5개 상태는 그대로 남아 있다. checkout/reset/기존 파일 삭제 없음.
- 빌드는 아직 실행하지 않았다. 따라서 **빌드 완료/결과 단계가 아니다**. 다음 작업은 RelWithDebInfo GUI 및 PRE DLL 빌드다. GUI-C-254c patch는 재적용하지 않았다.
