# GUI-C-261 A/B 비교

A: b9be6ae3b7f74413ccb6284d13a3220f2a8cde24 / D:/workspace-github/xpe-gui-ab
B: e61be66303aa37ba482da5d85332eaf6148cced2 / D:/workspace-github/xpe-gui

순서 A1→B1→A2→B2. 각 회차 총4/0P/4F/0SKIP.

| 시험 | 트리·회차 | 결과 | 실패 메시지 첫 줄 |
|---|---|---|---|
| A06_GuiE2E_RunsFromTheApp_AndTheVerdictIsReported | A1 | Failed | The GUI E2E runner RAN and FAILED when launched from the app: 'GUI E2E FAILED (exit -532462766): Unhandled exception. System.InvalidOperationException: WPF E2E failed.'. |
| EB02_TwoRunSets_KeepSeparateEvidence | A1 | Failed | No backend.json in D:\workspace-github\xpe-gui-ab\gui\ImageProcTest\bin\Debug\net8.0-windows\evidence\20261113-140826-8bc0c2. |
| IB01_InitializeBackend_StartsANewRunSet | A1 | Failed | An evidence folder is missing. |
| IB02_InitializeAfterTheDllsAppear_RecordsMockThenNative | A1 | Failed | No backend.json in D:\workspace-github\xpe-gui-ab\gui\ImageProcTest\bin\Debug\net8.0-windows\evidence\20261113-140819-de1334. |
| A06_GuiE2E_RunsFromTheApp_AndTheVerdictIsReported | B1 | Failed | The GUI E2E runner RAN and FAILED when launched from the app: 'GUI E2E FAILED (exit -532462766): Unhandled exception. System.InvalidOperationException: WPF E2E failed.'. |
| EB02_TwoRunSets_KeepSeparateEvidence | B1 | Failed | No backend.json in D:\workspace-github\xpe-gui\gui\ImageProcTest\bin\Debug\net8.0-windows\evidence\20261113-141038-b413b0. |
| IB01_InitializeBackend_StartsANewRunSet | B1 | Failed | An evidence folder is missing. |
| IB02_InitializeAfterTheDllsAppear_RecordsMockThenNative | B1 | Failed | No backend.json in D:\workspace-github\xpe-gui\gui\ImageProcTest\bin\Debug\net8.0-windows\evidence\20261113-141031-1ccd28. |
| A06_GuiE2E_RunsFromTheApp_AndTheVerdictIsReported | A2 | Failed | The GUI E2E runner RAN and FAILED when launched from the app: 'GUI E2E FAILED (exit -532462766): Unhandled exception. System.InvalidOperationException: WPF E2E failed.'. |
| EB02_TwoRunSets_KeepSeparateEvidence | A2 | Failed | No backend.json in D:\workspace-github\xpe-gui-ab\gui\ImageProcTest\bin\Debug\net8.0-windows\evidence\20261113-141139-859e1a. |
| IB01_InitializeBackend_StartsANewRunSet | A2 | Failed | An evidence folder is missing. |
| IB02_InitializeAfterTheDllsAppear_RecordsMockThenNative | A2 | Failed | No backend.json in D:\workspace-github\xpe-gui-ab\gui\ImageProcTest\bin\Debug\net8.0-windows\evidence\20261113-141133-f82edb. |
| A06_GuiE2E_RunsFromTheApp_AndTheVerdictIsReported | B2 | Failed | The GUI E2E runner RAN and FAILED when launched from the app: 'GUI E2E FAILED (exit -532462766): Unhandled exception. System.InvalidOperationException: WPF E2E failed.'. |
| EB02_TwoRunSets_KeepSeparateEvidence | B2 | Failed | No backend.json in D:\workspace-github\xpe-gui\gui\ImageProcTest\bin\Debug\net8.0-windows\evidence\20261113-141315-7f4dca. |
| IB01_InitializeBackend_StartsANewRunSet | B2 | Failed | An evidence folder is missing. |
| IB02_InitializeAfterTheDllsAppear_RecordsMockThenNative | B2 | Failed | No backend.json in D:\workspace-github\xpe-gui\gui\ImageProcTest\bin\Debug\net8.0-windows\evidence\20261113-141308-4b6edf. |

## 실행 전 evidence 상태

이름을 정렬한 양 끝값이다. snapshots도 폴더 수와 이름 정렬에 포함한다. 폴더를 삭제·초기화하지 않았다.

| 회차 | 존재 | 폴더 수 | 처음 이름 | 마지막 이름 |
|---|---|---:|---|---|
| A1 | False | 0 | None | None |
| B1 | True | 1302 | 20260930-120004-c851ea | snapshots |
| A2 | False | 0 | None | None |
| B2 | True | 1302 | 20260930-120004-c851ea | snapshots |

## 해석과 한계

부모에서도 네 실패가 같은 유형으로 두 번씩 관측됐다. F261-1 및 리더 판정6에 따라, 개정이 실패의 필수 조건은 아님으로 한정한다. 개정의 추가 영향·전체 순서 효과는 미판정이다. 실패 4개는 기존 로컬 실패, 원인 미확정으로 남긴다. 두 트리 경로·기존 evidence 상태는 다르며 Native 환경은 ab-preflight.json의 동일한 절대경로 값이다.
