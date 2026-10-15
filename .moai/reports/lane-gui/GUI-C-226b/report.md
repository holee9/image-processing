# GUI-C-226b — Codex #122 보류 1건: 거절 문구의 수명 (#249)

커밋 둘: 앱 `GUI-C-226b`(a)(`MainWindow.xaml.cs` 하나), 시험·증거 `GUI-C-226b`(b).

## 원인

226a 는 거절 문구를 필드(`processingRefusal`)에 두고 `ShowNativePreviewText` 만 앞에 붙이게 했다. 그런데 미리보기 글을 쓰는 다른 경로 열두 곳(단계 선택 변경, 우회 미리보기, 실행 결과 셋, 표시·내보내기 결과, 실패 처리)이 텍스트 상자에 직접 대입했다. 그래서 (1) 단계를 바꾸면 문구가 사라지고, 그다음 준비 상태 새로고침이 아직 남은 필드로 **옛 거절을 다시 그렸고**, (2) 재검증이 성공해 명령이 실행돼도 필드만 지워지고 글은 다시 그려지지 않아 거절 문구가 **남았다**. 이름으로 다시 찾은 직접 대입은 12곳(`NativePreviewText.Text =` 검색: 단일 행 8 + 여러 행 4)이었고, 226a 의 도우미 `ShowNativePreviewText` 를 부르는 곳이 3곳이었다.

## 변경 (앱)

글을 쓰는 곳을 **함수 하나**로 모았다.
- `SetNativePreviewText(text)`: 평소 메시지를 `nativePreviewMessage` 에 두고 다시 그린다.
- `SetProcessingNotice(notice)`: 거절 안내를 정하고 **바로** 다시 그린다.
- `RenderNativePreviewText()`: 텍스트 상자에 대입하는 **유일한** 곳. 안내가 있으면 `"Native preview: " + 안내 + " " + 평소 메시지`.
- 직접 대입 12곳과 도우미 호출 3곳은 전부 `SetNativePreviewText(...)` 로 바꿨다.

**안내의 수명(정한 것과 이유)**: 안내는 "마지막으로 거절된 명령"에 대한 말이다. 그래서 (a) 다른 쓰기(단계 변경, 새로고침)가 일어나도 **남아 있고** 옛 문구로 되살아나지도 않는다. (b) 재검증이 성공하면 "다시 검사 중, 기다려라"가 거짓이 되므로 **"다시 검사했고 지금 준비됐다, 명령을 다시 눌러라"** 로 바뀐다(준비 상태 새로고침에서 한 번). (c) 다음 명령이 확인을 통과하면 **즉시 지워지고 다시 그려진다**(그 명령이 미리보기 글을 쓰지 않아도). 거절이 다시 나면 처음 문구로 돌아간다.
Codex 가 둘 중 어느 쪽이 맞는지 정하라고 해서: "거절이 유효한 동안은 계속 보임"을 골랐다. 사용자가 누른 버튼이 실행되지 않았다는 사실은 백그라운드 재검증이 끝나도 사라지지 않기 때문이다.

## 시험

- **소스 스캔** `TheNativePreviewText_HasExactlyOneWriter_AndNothingElseNamesTheTextBox`(통합): 앱의 모든 `.cs` 에서 `NativePreviewText.Text =`/`+=` 가 정확히 1건이고 `RenderNativePreviewText()` 안에 있으며, `MainWindow` 밖의 어떤 파일도 그 이름을 쓰지 않는다. 새 직접 대입이 생기면 빨강.
- **E2E R06**(UIA 패턴만): 사설 네이티브 폴더, 통과 판정 뒤 DLL 교체, `RunSelectedAlgorithmButton` Invoke → 거절. 그다음 ① 재검증이 성공하면 안내가 "준비됐다"로 바뀌고 옛 "기다려라"가 없다, ② 단계 체크박스를 Toggle 하면 안내가 남고(그 옆에 새 메시지 "stage selection changed") "기다려라"가 되살아나지 않으며, `Refresh Modules` 뒤에도 그렇다, ③ 명령을 다시 누르면 안내가 즉시 사라진다(그 명령은 "select a calibration SWU row first" 만 쓰고 미리보기 글은 안 쓴다). R05 는 그대로 통과한다.

## 반증 (`falsification_*.txt`)

| 결함 | 결과 |
|---|---|
| 직접 대입 하나를 되살림 | 소스 스캔 빨강 |
| 안내를 지우되 다시 그리지 않음 | R06 ③ 빨강(명령이 돈 뒤에도 안내가 남음 — Codex 의 두 번째 증상) |
| 재검증 성공 시 안내를 바꾸지 않음 | R06 ① 빨강(시간 초과) |

## 실행 증거

- 통합 시험 전체: 862 통과 / 0 실패 / 1 건너뜀(파일 링크 권한) — `integration_full_suite.txt`
- 레거시 E2E 전체: 16 통과 — `legacy_e2e_current_tree.txt`

## 미검증과 잔여 위험

- R06 에서 거절 직후 처음 읽은 글은 이미 "준비됐다" 상태였다(재검증이 빨라서). "기다려라" 문구가 **보이는 구간**은 이 시험이 단언하지 않는다(R05 가 핵심 문장의 존재만 단언). 그 구간을 단언하려면 재검증을 일부러 늦추는 이음매가 필요한데 만들지 않았다.
- 안내가 있는 상태에서 다른 명령(Apply Native Preview, Run Chain)이 확인을 통과해도 같은 `ConfirmProcessingContentAsync` 를 지나므로 같은 방식으로 지워진다고 읽었지만 화면에서 누르지는 못했다(이미지·교정 폴더가 필요).
- 안내가 길어져 한 줄이 넘는 텍스트 블록에서의 보이는 모양은 확인하지 않았다(`TextWrapping=Wrap`).
