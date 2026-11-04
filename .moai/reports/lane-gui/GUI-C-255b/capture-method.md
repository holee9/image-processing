[2026-10-23 17:52 KST] [xpe-gui] GUI-C-255b 캡처 방법과 대조군 (Refs #278)

## 먼저 1장으로 확인한 가설

기존 GUI-C-255 도구도 `PrintWindow(hwnd, hdc, 2)`였다. 새 단일 실행 명령은 `dotnet <관측 도구 DLL> D:/workspace-github/xpe-gui <run 폴더> s0_gf 96 probe`다. `run-20261023-172433/s0_gf-uia3-172458/01-launch-6752832.print.png` 한 장을 만들었다.

- API 반환: `PRINTWINDOW(flags=0x2)=True`.
- 본문 비백색 비율: **0**. `01-launch-6752832.quality.json`: flags=2, accepted=false.
- 도구 종료 **1**, 원문 `Capture failed: body nonwhite ratio must exceed 5%`.
- 앱 PID 6384 종료, 잔여 0. 이 실패는 보존했고, 흰 캡처를 성공으로 세지 않았다.

따라서 이 사례는 0x2 누락으로 설명되지 않는다. 확인한 현상은 이 환경에서 PrintWindow 0x2가 WPF 본문을 흰색으로 반환한다는 것이다. GPU/데스크톱 컴포지터의 더 깊은 원인은 이 시험으로 확정하지 않는다.

## 전환한 방법

시험용 `clients/ImageProcTest.CaptureHook`을 관측 도구가 시작한 자기 자식 앱에만 `DOTNET_STARTUP_HOOKS`로 로드했다. 제품 소스나 정상 실행 경로는 바꾸지 않았다.

1. 앱 Dispatcher에서 현재 Window.Content를 VisualBrush/RenderTargetBitmap으로 읽는다. 실제 WPF DPI와 픽셀 크기를 사용한다. 투명 픽셀은 흰 배경 위에 합성한다.
2. Win32 client 크기와 WPF bitmap 크기를 대조한 뒤, **크기 변경 없이** OS 창 프레임 위에 배치한다. 전체 창 `.window.png`와 원본 본문 `.wpf-client.png`를 함께 보존한다.
3. 네이티브 폴더/파일 대화상자는 자기 HWND를 별도로 PrintWindow 0x2로 캡처해 `.dialog.png`로 남긴다. WPF 소유 창 이미지를 네이티브 대화상자 이미지로 간주하지 않는다.
4. 각 client/window/dialog 본문에서 비백색 화소 비율 **>5%**를 요구한다. 실패하면 종료 1이다. `--capture-controls`는 `WHITE=0 DARK=1 TRANSPARENT=0 threshold=0.05`, 종료 0이었다. 완전 흰색/투명 대조군은 통과하지 않는다.

첫 WPF 단일 probe의 본문 비백색 비율은 **0.9986833438214935**, 종료 0이었다. `s0_gf-uia3-172806/01-launch-48824592.window.png`에서 실제 메뉴·패널·글자가 보이는 것을 직접 확인했다.

이후 네이티브 대화상자까지 추가한 최종 네 실행에서 **132장 전부**를 Pillow로 다시 계산했다. 5% 이하 0장, 최소 비백색 비율 **0.12352185772268091**. 품질 기록 76개에서도 실패 0개다. 이는 흰 캡처 검출의 기술적 단언이며 제품 합격 판정이 아니다. 단색 어두운 그림도 이 수치만 통과할 수 있으므로 별도로 이미지 직접 관찰표를 작성했다.

WPF 캡처는 시각 트리를 오프스크린 렌더한 결과다. 실제 데스크톱 컴포지터 표시가 정상임을 입증하지 않는다. 캡처를 위해 전경 창 전환/포커스 변경 API나 전역 입력을 사용하지 않았다. 배율은 기존 Windows Settings UIA 방식으로 96/144 조건을 만들고 원래 100%로 복원했다.

## 빌드와 바이너리

- `dotnet build clients/ImageProcTest.CaptureHook/ImageProcTest.CaptureHook.csproj -c RelWithDebInfo --verbosity quiet`: 종료 0, 경고/오류 0.
- `dotnet build clients/ImageProcTest.GuiObservation/ImageProcTest.GuiObservation.csproj -c RelWithDebInfo --verbosity quiet`: 종료 0, 경고/오류 0.
- CaptureHook DLL SHA-256: `f224add7cf32bd90cd8bb2cc050937e3c44b3ef56c003c2a6d6a8f4cb07b5f36`.
- 관측 도구 DLL SHA-256: `20717fa632748dc9ff0aa9546e0c2443fe7bccbca57d30d8ddc334a662e63d29`.
- 내부 도구 코드 검토에서 per-HWND 방법 표기를 보완했고, 별도 네이티브 캡처/alpha 처리/출력 경로 제한/원자적 요청·응답/실제 DPI 크기 대조에 중요한 잔여 결함을 보고받지 않았다. 정식 QA/Reviewer 판정을 대신하지 않는다.
