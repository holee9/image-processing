# GUI-C-232 보고 — 3단계: 사용자가 1단계를 앱에서 막힘 없이 따라 하게 고친다 (Refs #251)

## 핵심 증거 (D·E)

실데이터 세트 `xpe-data/calib-real-v2` + 원본 `wrist_lat_3072x3072.raw` 로, 문서(빠른 시작)대로 UIA 만으로 앱을 따라 했습니다(전경 키·마우스 없음). 앱이 저장한 float 파일:

```
saved     : 37748736 bytes sha256 d152e8ebba2cba142a95ba7ba981354a393fa18551b46572d8b1c3f36e8fdfd4
reference : 37748736 bytes sha256 d152e8ebba2cba142a95ba7ba981354a393fa18551b46572d8b1c3f36e8fdfd4   (xpe-data/m1-baseline-v2/..._f32le.raw)
identical : True; first differing byte: -1
```

이 결과는 **`xpe_preprocess_pipeline_out` 경로**(E, 앱의 기본 경로)에서 나왔습니다. 로그: `Chain preprocess: Applied — Preprocess: xpe_preprocess_pipeline_out (offset -> gain -> defect, input kept) on 3072x3072 (Abdomen).` 상태줄이 크기와 sha256 을 그대로 말합니다. 증거: `walkthrough_w7_save_identical_pipeline_out.txt`. (16비트 PNG 도 저장됨: 16,912,866 바이트, 표시용.) 따라서 E 의 "바꾼 뒤에도 바이트 동일" 은 **성립**합니다 — 차이가 없다는 뜻은 아니고, 이 입력·이 맵에서 같았다는 관측입니다.

## 바뀐 것

| 항목 | 내용 | 시험 |
|---|---|---|
| A | `CanRunPreprocessing` 변경 알림 추가(백엔드 교체 시, `MainWindowViewModel`) | W8: Mock 에서 꺼짐 → `Backend Mode > Native` 직후 켜짐 |
| B | `RawImageLoader.DecideSize`: 길이가 가로×세로×2 면 그대로, 아니면 **정사각이면 길이로 크기를 정해 열고 요약·상태·WARN 알림에 적음**, 그 밖엔 거절(`Raw file is too small…` / `Raw file has N bytes, more than…`) | W8: 3072 파일+1024 설정 → `RAW 3072x3072 … [size from the file length: …]`; 1000×500, 2000×1100 → 거절 문구 관측 |
| C | 하단 `…; preprocess native bridge pending)` 삭제 → `…; the preprocess result is the chain line)`. 단계 결과는 `chain:` 줄 | W2, W7 |
| D | File 메뉴 `Save Corrected Image (float32 .raw)...`, `(16-bit PNG)...` (`CorrectedImage.cs`, VM 명령). 실행 성공 후에만 켜지고 새 영상을 열면 꺼짐. 기본 이름 `<원본>_corrected_f32le.raw` | W7 |
| E | `GuiPreprocessRunner`: `xpe_preprocess_pipeline_out`(설정 `bypassTemp/Nonlinearity/Binning`, ghost 없음 — pre 의 `cfgFinal` 과 같음)이 기본. **DLL 에 수출 함수가 없을 때만** 옛 단계별 호출로 돌아가고 요약에 이유를 적음 | 아래 |
| 6 | `quick-start.html` 갱신, `quick-start.ko.html` 신규(색인·영문 페이지에서 연결), `scope.html`·`index.html` 정정 | 아래 |

## 시험과 증거 (이 폴더)

- `walkthrough_w2_script_route_real_set`(스크립트 경로, 실데이터), `w3_failures`(맵 없음/만료/크기 불일치 `xpe_preprocess_pipeline_out failed (-1)`/DLL 없음), `w4`(전환→저장→재시작), `w5`, `w6`(패널), `w7`(저장·해시), `w8`(A·B), `doc_menu_paths_check`(문서의 굵은 메뉴 경로 11개가 실제 앱에 있음). 모두 통과.
- 문서 인용 대조(`doc_vs_evidence_quick-start.txt` 38개, `…ko.txt` 36개): **missing 0**. 첫 대조에서 비어 있던 것: B 의 `more than` 문구(관측하려고 W8 에 2000×1100 파일 추가)와 도구 이름·서술문 몇 개.
- 옛 DLL 대조: `walkthrough_fallback_old_dll_no_pipeline_out.txt` — 2026-10-03 DLL(수출 없음)에서 `Applied` + 로그 `(this xpe_preprocess.dll has no xpe_preprocess_pipeline_out: the older stage-by-stage path ran; update the DLLs)`. 같은 옛 DLL 은 실데이터 gain 맵을 `-17` 로 거절(`…w7_old_dll_real_set_rejected_minus17.txt`, 시험이 빨강인 것이 기록입니다) — 실데이터 세트는 새 DLL 이 필요합니다.
- 통합 927 통과/0 실패(`BackendLifecycleTests` 표에 새 읽기 전용 멤버 `CurrentCorrected` 추가, `TroubleshootingDocQuoteTests` 가 인용하는 `Raw file is too small…` 문구는 짧은 파일 경우에 남김). SelfCheck 통과. E2E 메뉴 회귀 31개 중 2 실패였는데 둘 다 제품 결함이 아니었습니다: `D01` 은 제가 같은 때 SelfCheck 와 빌드를 돌려 생긴 간섭(단독 재실행 통과), `A06` 은 `ImageProcTest.E2E` 러너 복사본이 소스보다 오래됨(재빌드 후 통과).

## 앱이 보여 주는 것 = 1단계 기준 인가

- **같다**: 이 세트·이 원본에서 저장한 float 파일은 기준 영상 v2 와 바이트까지 같음(위).
- **화면 그림은 다르다(의도)**: 화면은 float 을 최댓값 기준 16비트로 늘려 그린 것(`ScaleToUInt16`)이라 값이 다르게 보임. 비교는 저장한 float 파일로 하라고 문서에 적음.
- **앱이 하지 않는 단계**: pipeline_out 에서 온도·비선형·binning 은 bypass, ghost 는 핸들 없음. 노출(mAs 2.0, SID 1000)은 고정, kVp·화소 간격은 설정값. 231 의 차이 목록 중 "단계별 호출" 항목은 해소, 나머지는 문서·scope 에 유지.

## 미검증 (Gaps)

- **화면 그림 자체**(평평함·결함 화소)는 판정하지 않았습니다(저장 파일의 해시만 기준 영상과 대조).
- 최신 main DLL 인지 증명하지 못했습니다: 쓴 DLL 은 `xpe-data/app-run` 에서 복사한 2026-10-07 빌드(provenance 없음 → 스크립트 검사 `UNPROVEN`)이고, 그 DLL 이 pipeline_out 을 수출함은 확인했습니다. 리더가 CI 산출물로 같은 해시가 나오는지 다시 보면 확정됩니다.
- 저장 대화상자는 UIA 로 입력한 파일 이름을 무시했습니다(편집칸·호스트 콤보 모두 읽어 보면 입력값인데도 제안된 이름·폴더로 저장). 그래서 W7 은 이름을 입력하지 않고 앱이 제안한 위치의 파일을 읽은 뒤 지웁니다. 실제 사용자가 이름을 입력하는 흐름은 UIA 로 관측하지 못했습니다.
- 한국어 Windows 한 대, CI 미확인. 새 시험 W1~W8 은 `XPE_C231_NATIVE_DIR` 가 있을 때만 발견됩니다(CI 건너뜀 게이트 회피). W0 만 CI 에서도 돕니다.
- `docs/help/content/troubleshooting.md` 는 리더 소유라 건드리지 않았습니다: B 로 "크기가 틀려도 열린다" 는 설명이 달라졌을 수 있으니 확인이 필요합니다.
- 한국어 문서는 `humanize` 패스를 거치지 않았습니다(번역투 점검은 직접 읽어서만 했습니다).

## 리더 결정 요청

1. `dev/gui` 에 `origin/main`(7410bc95)을 병합했습니다(승인받은 대로, 충돌 없음, 푸시 안 함). main 이 이미 dev/gui 의 231 을 담고 있어 빠르게 감기는(fast-forward) 병합이었고 병합 커밋은 생기지 않았습니다. 푸시 대상은 이번 커밋 하나입니다.
2. 제품 코드 변경 포함이라 Codex 검토 대상: `GuiPreprocessRunner`(pipeline_out 경로·폴백), `RawImageLoader.DecideSize`(정사각 추론이 "사용자가 지정한 크기와 다르게" 여는 동작 — 허용하는 것이 맞는지), 저장 명령.
