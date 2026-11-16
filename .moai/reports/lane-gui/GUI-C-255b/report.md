[2026-10-23 17:22 KST] [xpe-gui] 수신 — GUI-C-255b: PrintWindow 0x2 단일 캡처 확인 → 필요하면 WPF 캡처 훅 → 비백색 본문 >5% 단언 → 두 세트 × DPI 96/144 재실행과 이미지 직접 관찰. 합격 판정·push 없음.
[2026-10-23 17:52 KST] [xpe-gui] 결과 — PrintWindow 0x2의 흰 캡처 1건을 실패로 재현했다. WPF 캡처 훅으로 전환한 최종 네 흐름에서 132장 모두 본문 비백색 >5%를 확인했고, 이미지에서 겹침·문구 잘림·9pt 미만 글자를 관찰했다. 자체 합격 판정은 하지 않는다.

## 증거와 캡처 변경

- 새 폴더: `run-20261023-172433/`. 이전 GUI-C-255 결과를 덮어쓰지 않았다.
- 단일 가설 확인, 실패 원문, WPF 방법·대조군·빌드: [capture-method.md](capture-method.md).
- 최종 캡처와 비교 결과: [final-evidence-summary.json](run-20261023-172433/final-evidence-summary.json).
- RAW·PNG·UIA·글꼴 메타데이터·품질·요청/응답·비교·로그 **480개 파일**: [sha256.tsv](run-20261023-172433/sha256.tsv). 재검산 불일치 0개.
- WPF client 56장 + OS 프레임을 포함한 window 56장 + 별도 네이티브 대화상자 20장 = **132장**. 각 client/window 짝은 같은 본문을 공유하며 관찰표에서 함께 연결했다. 캡처 단언 76개 실패 0, 독립 픽셀 재계산 132장 중 5% 이하 0. 최소 비백색 비율 12.3522%.
- 흰 캡처 대조는 별도로 **1건 실패**(flags=2, 비백색 0%, 종료 1)로 보존했다. 기존 코드도 flags=2여서 “플래그 누락” 가설은 해당하지 않았다. PrintWindow 반환과 WPF 본문 회수의 불일치까지만 확인했으며, GPU/컴포지터 원인을 단정하지 않았다.

## 최종 네 조건의 실제 실행

실행기 `run-flows.ps1`은 `dotnet <관측 도구 DLL> D:/workspace-github/xpe-gui <run> <set> <dpi> hook`을 한 번에 하나씩 실행했다. Windows 배율 설정은 UIA로 변경했고 종료 finally에서 100%(권장)으로 복원했다(`RESTORE_SCALE_EXIT=0`, `flows-173546/display-restored-100.json`). 캡처 과정에서 전경 전환/포커스 변경을 호출하지 않았다.

| 세트 | 실제 DPI | 최종 하위 폴더 | 도구 / 앱 종료 | GUI·CLI 결과 | 캡처 / 앱 잔여 |
|---|---:|---|---|---|---|
| s0_gf | 96 | s0_gf-uia3-173555 | 0 / 0 | 바이트 같음 | 33장 / 0 |
| s1_um300 | 96 | s1_um300-uia3-173711 | 0 / 0 | 바이트 같음 | 33장 / 0 |
| s0_gf | 144 | s0_gf-uia3-173838 | 0 / 0 | 바이트 같음 | 33장 / 0 |
| s1_um300 | 144 | s1_um300-uia3-173956 | 0 / 0 | 바이트 같음 | 33장 / 0 |

흐름은 앱 시작 → Offset/Gain/Defect 폴더 선택 → RAW 열기 → Run Preprocessing → Metrics → float32 저장 → File/Exit다. 각 출력은 31,457,280바이트다. s0_gf GUI/CLI SHA-256은 `F1724B40D8691F8F1B6F5B13C81B53AD1EA36E52B0CDE86D275A9C4E9190E1D5`, s1_um300은 `76BD1D1CFFFB4783AB8DF821E817E18353C110B3506D28DC26F2F0B1A8487AB6`이며 두 DPI에서 같다.

## 스크린샷별 이미지 직접 관찰표

각 표는 실제 이미지에서 본 결과를 기록한다. 정확한 WPF 글꼴 pt는 이미지에서 보이는 라벨에 대응하는 현재 시각 트리 FontSize로 보완했고, UIA 값만으로 화면 정상 여부를 추정하지 않았다. 네이티브 대화상자는 별도 이미지를 직접 보았다. 소유 창의 WPF 캡처에는 네이티브 모달이 포함되지 않음을 구분했다.

| 조건 | 이미지별 표 | 직접 본 창/대화상자 | 같은 본문의 client 짝 |
|---|---|---:|---:|
| s0_gf / 96 | [visual-s0-96.md](visual-s0-96.md) | 19 | 14 |
| s1_um300 / 96 | [visual-s1-96.md](visual-s1-96.md) | 19 | 14 |
| s0_gf / 144 | [visual-s0-144.md](visual-s0-144.md) | 19 | 14 |
| s1_um300 / 144 | [visual-s1-144.md](visual-s1-144.md) | 19 | 14 |

| 발견 항목 | 이미지에서 직접 본 사실 |
|---|---|
| 겹침 | Calibration 패널을 열면 좁아진 뷰어의 STUDY/WINDOW 카드가 상단 비교 모드 툴바를 가린다. 로드 후 긴 STUDY 메타데이터는 영상 일부도 덮는다. 두 DPI에서 관찰됐다. |
| 잘림 | 로드 후 하단 두 영상의 `ProcessedOnly / zoom fit / ...` 상태 띠가 우측에서 잘린다. 창 하단의 긴 chain 상태와 저장 경로도 끝까지 보이지 않는다. `…`로 줄인 것과 경계에서 그냥 잘린 것을 구분해 기록했다. |
| 빈 영역 | 로드 전 중앙은 RAW 없음 안내와 함께 비어 있다. 로드 후에도 Metrics 영역은 실제 수치 대신 `Metrics populated after pipeline run.` 자리표시자가 남아 있다. |
| 한글 | 네이티브 폴더/열기/저장 대화상자의 관찰한 `폴더 선택`, `취소`, `파일 이름` 등은 글리프가 읽혔다. 긴 OneDrive/드라이브 이름은 사이드바 폭 때문에 잘린다. 작은 WPF 한글은 표별로 읽힌 범위와 미해결 범위를 구분했다. 전 화면 한글 정상으로 일반화하지 않았다. |
| 글꼴 <9pt | 이미지에서 작고 흐린 보조 라벨을 확인했다. 대응 WPF 메타데이터에서 LANE/VOI/메타데이터 라벨 등에 **6.75pt**, `미적용` 및 Metrics 자리표시자 등에 **7.5pt**, 일부 탭/보조 텍스트에 **8.25pt**가 기록됐다. 두 DPI 모두 pt 값은 같다. |
| 비활성 텍스트 대비 | 상단 비활성 `Grid suppression` 콤보의 밝은 텍스트가 밝은 배경과 구분하기 어렵게 보인다. 비활성 기능의 의도나 최종 적합성은 판정하지 않았다. |

## 확인과 인계 범위

- `--capture-controls`: WHITE=0, DARK=1, TRANSPARENT=0, 종료 0. 투명 검은 픽셀로 5%를 통과하지 않도록 흰 배경 합성을 적용했다.
- `git grep --no-index -n -e Mouse\. -e Keyboard\. -e SendInput -e SetForegroundWindow -- clients/ImageProcTest.GuiObservation/Program.cs clients/ImageProcTest.CaptureHook/StartupHook.cs .moai/reports/lane-gui/GUI-C-255b/run-flows.ps1`: 종료 1, 출력 없음 = 지정 범위 일치 0건.
- `verify.ps1`: 종료 0, `OWNED_GUI_REMAINING=0`, `2026-10-23T17:52:04.9545185+09:00`.
- 원본 batch의 사용 세트 RAW/보정 자료 52개를 manifest SHA-256로 다시 대조했고 불일치 0개였다.
- 캡처 코드·방법 기록 로컬 커밋: **28a2b5e7**. 관찰표·480개 인덱스·보고서 커밋 SHA는 후속 `handoff.md`에 기록한다. 커밋은 명시적 pathspec만 사용했다.
- 캡처 훅과 관측 도구 빌드는 각각 종료 0, 경고/오류 0. WPF와 Win32 client 픽셀 크기를 검사하고 확대/축소 없이 합성했다. 실제 창 DPI 96/144도 매 단계 검사했다.
- 시험용 캡처 코드와 보고서만 수정했다. 제품 처리/레이아웃 변경, 제조사 DLL 실행, `.par`/`.dcm` 열기, 전역 입력, 타 워크트리 쓰기, push 없음. 기존 미커밋 변경은 보존했다.

새 PNG는 **WPF 시각 트리를 오프스크린 렌더한 결과**다. 데스크톱 컴포지터가 실제 화면에 정상 표시했다는 증거로 바꾸지 않는다. 캡처와 이미지 관찰 사실을 리더에게 인계하며, UI 품질·화질·제품 합격 판정은 하지 않는다. 로컬 커밋과 마지막 검증은 후속 handoff.md에 기록한다.
