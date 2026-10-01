# GUI-C-181 — #225 행 21 Help → Troubleshooting (A 방식)

레인: gui · 이슈: `#225` · 구현 커밋 `f95127e0`(푸시 없음, 푸시는 리더). 시작 전 `origin/main`(`ef98cd67`, `c1e6f71b` 포함)을 `dev/gui` 에 병합했다(fast-forward).

## 결론

행 21 을 켰다. **DocFX 가 생성한 페이지를 열고, 없으면 "생성 안 됨 + 생성 방법"을 말한다.** 어느 상태에서도 빈 창이나 무반응이 없다. 비활성 개수는 **6→5**이고 세 축(이름 목록 · 트리 순회 · XAML)이 5=5 로 일치한다. 이 트리에서 **브라우저를 실제로 띄우는 경로는 관측하지 않았다**(자동화에서는 열기를 억제하는 행 14·20 과 같은 구조) — §4.

| 상태 | 상태줄 | 열기 |
|---|---|---|
| 페이지 없음, 출력 폴더도 없음 | `Troubleshooting page has not been generated. Generate it: install DocFX with 'dotnet tool install -g docfx' and then run 'docfx docs/help/docfx/docfx.json' (see .github/workflows/docs-generate.yml).` | 아무것도 열지 않음 |
| 출력 폴더만 있음 | `Troubleshooting page is incomplete: docs/help/generated/docfx exists but has no content/troubleshooting.html. <같은 안내>` | 아무것도 열지 않음 |
| 페이지 있음 | `Troubleshooting page opened — generated <시각>: <경로>` | 브라우저로 열기(자동화에서는 억제 + 기록) |
| 체크아웃이 아님 | `Troubleshooting page needs the repository; this build is not running from a checkout.` | 아무것도 열지 않음 |

## 1. 변경

- `TroubleshootingPageService`(새, 행 20 의 `ApiReferenceService` 와 같은 모양): 경로 `docs/help/generated/docfx/content/troubleshooting.html`.
- `MainWindowViewModel`: `OpenTroubleshootingCommand`, 자동화 억제 플래그, 마지막 경로. 행 20 의 방식 그대로(복제이며 공용화하지 않았다 — 문구와 상태 속성이 달라 한 메서드로 묶으면 행 20 의 문구를 건드려야 했다).
- `MainWindow.xaml`: `IsEnabled` 제거 · 명령 바인딩 · 헤더에서 `(Phase 1b)` 삭제 · 툴팁을 실제 동작으로. 이름 목록(`MainWindow.xaml.cs`)에서 뺐다.
- 자동화: 메뉴를 눌러 상태·경로·억제 여부를 보고서에 싣는다.

**경로는 추측하지 않았다.** `docs/help/docfx/docfx.json` 의 `build.output`(`../generated/docfx`)과 `content` 항목의 `dest`(`content`)에서 계산되며, **main 의 docs-generate 실행(`36855777520`)이 만든 실제 산출물**에서 `content/troubleshooting.html` 이 있는 것을 확인했다(26196 B, `<h1>Troubleshooting Guide</h1>`, `Message` 열 머리글, `Stop: no render is in flight.` 포함, 자리표시 문구 없음, 표 행 56개 — `local_runs.txt`).

## 2. 시험

| 시험 | 단언 |
|---|---|
| `TroubleshootingPageServiceTests` 5건 | 세 상태(임시 디렉터리). 경로 상수 = `docfx.json` 에서 계산한 경로. 생성 안내 = `docs-generate.yml` 이 실제로 돌리는 두 명령 |
| E2E **A16** | 페이지 **없음** 상태(CI 가 보는 상태): 경로 null · 억제 false · `has not been generated`(또는 출력 폴더가 있으면 `is incomplete`) · 두 명령이 상태줄에 있음. 있음/없음은 이 시험이 디스크를 직접 읽어 판정 |
| E2E **A17** | 페이지 **있음** 상태: 체크아웃에 없으면 스텁을 gitignore 된 `docs/help/generated` 에 만들고 끝나면 지운다(만든 폴더만, 비었을 때만). 경로 일치 · 억제 true · `Troubleshooting page opened` · `launch suppressed` |
| A11 | 세 축 개수 일치(5=5) |

로컬: `Functional` 219 통과 · 건너뜀 1(`ErrorCodeMapping NOT_IMPLEMENTED`, 무관) · 실패 0. `AutomationReportBackendTests` 16 통과 · 건너뜀 1(`A03`, 기존) · 실패 0 — **A16·A17 은 건너뛰지 않고 실행**됐다. 이전 보고(GUI-C-180)의 `TroubleshootingDocQuoteTests` 는 이제 문서가 있어 이 트리에서 초록이다.

## 3. 반증 (실제 소스, 4팔)

| 팔 | 망가뜨린 것 | 결과 |
|---|---|---|
| wrong-page-path | 경로 상수를 `trouble.html` 로 | `ThePagePath_IsWhereDocfxWritesTheSourceMarkdown` 와 `Generated_ReturnsThePage…` 빨강 |
| hint-drops-the-run-command | 안내에서 `docfx docs/help/docfx/docfx.json` 제거 | `TheGenerationHint_IsTheTwoCommandsTheDocsWorkflowRuns` 빨강 |
| menu-disabled-again | XAML 에 `IsEnabled="False"` 복원 | **A11 빨강**(세 축 불일치) |
| says-not-generated-although-it-exists | 페이지가 있는데 없다고 답하게 | **A17 빨강** |

네 팔 모두 빌드는 성공했고 소스는 바이트 동일하게 복구했다. 스테이징한 페이지가 남지 않았음도 확인했다(`docs/help/generated` 없음). (`falsification_arms.txt`)

## 4. 미검증 · 한계

1. **브라우저를 실제로 띄우는 경로는 관측하지 않았다.** 자동화에서는 열기를 억제하고, 이 외의 경로(`Process.Start` 와 그 예외 → `Troubleshooting page could not be opened: <메시지>`)는 행 20 의 코드와 같은 모양이지만 실행되지 않았다.
2. **스텁 페이지와 실제 DocFX 페이지는 다르다.** A17 이 쓰는 것은 한 줄짜리 스텁이다. 실제 생성 페이지가 존재함은 CI 산출물에서 봤지만, **그 파일을 이 앱이 연 적은 없다**(이 트리에 docfx 가 없고 전역 도구 설치도 하지 않았다). 브라우저에서 DocFX 의 `modern` 템플릿이 `file://` 로 제대로 그려지는지(검색·스크립트)는 확인하지 않았다.
3. **A16 은 생성된 페이지가 이미 있는 체크아웃에서는 건너뛴다**(그 상태를 만들 수 없다). CI 에는 생성물이 없어 거기서 실행된다는 것은 **예상**이다 — CI 는 아직 돌려 보지 않았다.
4. **운영자 설치본**: 이 메뉴는 체크아웃 + docfx 생성을 전제한다(리더 결정). 설치본에 페이지를 넣는 것은 별도 카드다.
5. Mock 백엔드 한 가지로만 돌렸다(이 메뉴는 백엔드와 무관하다).

## 5. 문서(리더 소유)에 필요한 후속 — 제안

방금 추가한 문구가 `docs/help/content/troubleshooting.md` 에 없다. 인용 시험이 지키려면 행이 있어야 한다. 제안(원문 인용 그대로, 반영은 리더 몫):
- 기존 첫 행 설명 "Shown by Self-check, GUI E2E, Benchmark runner and API reference" 에 **Troubleshooting** 을 추가한다(인용 `<command> needs the repository; this build is not running from a checkout.` 의 와일드카드가 이미 `Troubleshooting page` 도 받는다).
- 새 행: `Troubleshooting page has not been generated.` · `Troubleshooting page is incomplete: <directory> exists but has no content/troubleshooting.html.` — 의미: DocFX 출력(또는 페이지)이 없다 — 조치: `Generate it: install DocFX with 'dotnet tool install -g docfx' and then run 'docfx docs/help/docfx/docfx.json' (see .github/workflows/docs-generate.yml).` 
- `Troubleshooting page could not be opened: <message>` 를 기존 "could not be opened" 행에 추가한다.

## 증거 파일

`falsification_arms.txt` · `local_runs.txt` · `text_lint.txt`

🗿 MoAI
