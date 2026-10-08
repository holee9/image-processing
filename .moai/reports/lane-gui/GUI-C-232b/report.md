# GUI-C-232b 보고 — Codex #165 보류 6건 (Refs #251)

커밋 7개(`ff619b09`, `00f65a14`, `21c58565`, `9e21e7eb`, `7f5f62b1`, `fbc68753`, `a39fe4d3`, `61bd3ee9` + 이 보고서와 문서 커밋). 모두 dev/gui, 푸시 전. 핵심 증거(앱 저장 float = 기준 영상 v2 바이트 동일)는 다시 확인했습니다.

## 바이트 동일 재확인 (카드 7번)

`walkthrough_w7_save_identical.txt` — 같은 세트(`xpe-data/calib-real-v2`)·같은 원본(`wrist_lat_3072x3072.raw`, 이번에는 임시 폴더의 사본), 문서대로 UIA 만으로(전경 입력 없음):

```
DLL: ...\c231_native_fresh\xpe_preprocess.dll sha256 57b64b7cc9a02fce7f7eb4240266fde724a554689d9f60af09ae3b4540f23a7f written 2026-10-07 16:20:08; provenance.json absent
LOG  preprocess: Applied — Preprocess: xpe_preprocess_pipeline_out (offset -> gain -> defect, input kept) on 3072x3072 (Abdomen).
saved     : 37748736 bytes sha256 d152e8ebba2cba142a95ba7ba981354a393fa18551b46572d8b1c3f36e8fdfd4
reference : 37748736 bytes sha256 d152e8ebba2cba142a95ba7ba981354a393fa18551b46572d8b1c3f36e8fdfd4
identical : True; first differing byte: -1
```

**DLL 출처**: `xpe-data/app-run` 의 2026-10-07 16:20 빌드를 복사한 것이며 provenance 가 없어 "main 최신 빌드" 임을 증명하지 못했습니다(스크립트 검사로는 UNPROVEN). 리더의 CI 산출물로 같은 해시가 나오는지 보면 확정됩니다.

## 항목별

| # | 내용 | 코드/시험 | 관측 |
|---|---|---|---|
| 1 | 수출 없는 옛 DLL 은 **실패 처리**(단계별 폴백 경로 차단) | `GuiPreprocessRunner.Run` (`ff619b09`) | W10: 2026-10-03 DLL(sha256 `a9be9c8c…`)에서 `preprocess=RequestedNotApplied` + `Preprocessing not run: this xpe_preprocess.dll has no xpe_preprocess_pipeline_out (the DLL is too old). Update the native DLLs and start the app again.`, 저장 항목 비활성 (`walkthrough_w10_…txt`) |
| 2 | 저장 이름의 원본 경로는 **로드 성공 뒤에만** 확정 | `MainWindowViewModel` (`00f65a14`) | 소스 순서만 확인. **UIA 시험(A 열기·보정 → 잘못된 B 열기 실패 → 저장 이름)은 쓰지 못했습니다** — 미검증 |
| 3 | 저장 후보를 **그 실행의 ChainResult 에 묶음**(전역 "마지막 실행" 슬롯 삭제), VM 이 폐기·취소 검사 뒤 프레임 확정 직전에 그 결과의 후보만 확정 | `RealXpeBackend`, `CorrectedImage.cs`, `MainWindowViewModel` (`21c58565`) | 소스 순서 고정 시험 2건(`CorrectedImageCommitTests`). **두 Apply 를 겹쳐 실제로 늦게 끝나게 하는 동작 시험은 쓰지 못했습니다** — 미검증 |
| 4 | 크기 추론 삭제, 길이가 다르면 열지 않음(필요 크기·실제 바이트·설정법), 보정 맵 헤더 크기와 다르면 열지 않음 | `RawSizeRules` (`9e21e7eb`, `a39fe4d3`) | W8: 3072 파일+1024 설정, 1000×500, 2000×1100 모두 거절 / W3: 1024 맵+3072 영상 거절(맵 경로·두 크기 표시). CI 시험 10건(외부 데이터 없음) |
| 5 | 저장은 같은 폴더 임시 파일에 쓴 뒤 교체 / W7 은 임시 폴더의 사본에서만 저장·삭제 | `AtomicFile` (`7f5f62b1`), W7 (`fbc68753`) | CI 시험 4건. W9: 잠긴 대상 → `Save failed: The process cannot access the file…` + 이전 파일 그대로 + 폴더에 임시 파일 없음 |
| 6 | 저장 대화상자는 성공한 원본 폴더에서 시작 / 이름·덮어쓰기·쓰기 실패 관측 / W7 로그에 pipeline_out 로그·DLL 해시 / CI 에서 도는 시험 | `InitialDirectory` (`fbc68753`), W9 | 아래 |
| 7 | 빠른 시작(영·한) 갱신, 문서 인용 대조 | `help/quick-start*.html` | 대조 42/40개 missing 0, 메뉴 경로 W0 통과 |

## 6번 관측 상세 (W9, `walkthrough_w9_typed_name_overwrite_failure.txt`)

- **입력한 이름은 UIA 로 확인하지 못했습니다(한계 기록).** 저장 대화상자의 이름 칸에 `WM_SETTEXT` 로 이름을 넣어도(값 패턴으로 넣었던 232 와 같은 결과) 파일은 제안 이름(`wrist_lat_3072x3072_corrected_f32le.raw`)으로 저장되었습니다. 원인은 찾지 못했습니다. 따라서 사용자가 이름을 입력하는 흐름은 **관측하지 못했습니다.**
- 저장 위치: 제안 폴더가 열린 원본의 폴더(임시 사본 폴더)임을 확인 — `InitialDirectory` 가 효과.
- 같은 이름 재저장: 파일이 통째로 교체되었고(쓰기 시각 변경, 내용 = 기준 영상) 폴더에 다른 파일이 늘지 않았습니다. **덮어쓰기 확인창은 시험이 관측하지 못했습니다** (확인창에 "예"를 누르는 코드는 있으나 출력에 그 줄이 없음).
- 잠긴 대상: 위 5번.

## 하지 않은 것·결정 요청

- **`RunStages`(단계별 경로) 삭제를 보류**했습니다. `Run` 에서는 닿지 않게 했습니다(1번). 지우면 소스 고정 시험 5건이 깨집니다: `BaselineReviewFixTests`(2건: 계수 도우미 호출), `NonlinearityLutCallSiteTests`, `NonlinearityStageWiringTests`(비선형 단계 호출을 요구), `TroubleshootingDocQuoteTests`(리더 소유 `docs/help/content/troubleshooting.md` 가 인용). 이 시험들은 "앱이 비선형 단계를 부른다"는 요구를 고정하고 있어 pipeline_out(비선형 건너뜀, pre 의 `cfgFinal` 과 같음) 결정과 충돌합니다. 요구 문서와 troubleshooting 페이지를 어떻게 할지 리더의 결정이 필요합니다.
- 2번·3번의 **동작 시험**(UIA / 겹친 Apply)을 쓰지 못했습니다. 소스 순서 고정만 있습니다.
- 화면 그림 자체, 한국어 Windows 한 대, CI 미확인. W1~W10 은 환경 변수가 있을 때만 발견되고, CI 에서 도는 것은 W0 + `AtomicFileTests` + `RawSizeRulesTests` + `CorrectedImageCommitTests` 입니다.
- `docs/help/content/troubleshooting.md` 는 리더 소유라 건드리지 않았습니다.
- 통합 943 통과/0 실패/1 건너뜀.

## 리더 검토 포인트

1. 맵 크기 교차 확인은 `<폴더>/offset.xcal|gain.xcal|defect.xcal` 헤더(바이트 16·20)만 읽습니다. 맵 이름 규칙이 다른 폴더는 검사하지 않습니다(그때는 단계가 거절).
2. 후보를 묶는 `ConditionalWeakTable` 은 결과 객체가 사라지면 후보도 사라집니다.
