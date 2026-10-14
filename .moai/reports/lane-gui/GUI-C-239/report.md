[2026-10-14 15:35 KST] [xpe-gui] GUI-C-239 보고 — 실데이터 워크스루를 CI 에서 돌릴 준비 (#262 마무리, Refs #251)

푸시하지 않았다. 기준 `031d5079`. 변경: 시험 한 파일 `clients/ImageProcTest.E2ETests/Scenarios/Operator/QuickStartWalkthroughScenarios.cs` (W3·W7·W10, 도우미 2개). 제품 코드 변경 없음. 가용 메모리 4632 MB 로 시작해 5608 MB 로 끝났고 내 쪽 남은 앱/testhost 0 (`memory_before.txt`, `memory_after.txt`).

## 1. 다섯 변수만으로 못 도는 시험의 처분

`XPE_C231_NATIVE_DIR` 이 설정되면 수집되는 `[EnvGatedFact]` 16건을 읽어 각각이 요구하는 변수를 뽑았다. 다섯 변수(NATIVE_DIR, CALIB_DIR, RAW, RAW_SIZE, BASELINE_F32)로 충분한 것은 14건이고, **못 도는 것은 2건**이었다. 둘 다 "변수 없음"을 `Assert.False` 로 **실패** 처리하고 있었다(조용하지는 않지만 CI 에서는 공급 불가능한 실패).

| 시험 | 필요했던 것 | 처분 |
|---|---|---|
| W3 실패 메시지 | `XPE_C231_SETS`(폴더 `ok1024`·`expired1024`·`empty`·`nodll`), `XPE_C231_FIXTURE_RAW` | **저장소·스테이징 폴더 안 입력으로 공급**. 원본 raw 는 `gui/ImageProcTest/fixtures/gui-s0/raw`(저장소 안). 맵 세트는 시험이 **스테이징된 `xpe_calib_fixture_gen.exe`**(DLL 폴더에 들어 있음)로 1024² 합성 두 벌(유효, 만료 `--expiry-ms 946684800000`)을 임시 폴더에 만들고, `empty`·`nodll` 은 빈 폴더. 변수가 있으면 그것이 우선(수작업 세트용). 생성기가 스테이징 폴더에 없으면 "스테이징 결함" 문장으로 실패. |
| W10 오래된 DLL | `XPE_C232B_OLD_NATIVE_DIR`(`xpe_preprocess_pipeline_out` 이전 빌드의 DLL 폴더) 외 둘 | **이유를 적은 건너뜀**. 옛 빌드 DLL 은 저장소에도 CI 산출물에도 없다(CI 는 현재 모듈만 빌드). `[SkippableFact]` 로 바꾸고 이유 끝에 `XPE-SKIP-ALLOWED:265`(리더 지시로 #262 에서 변경). 변수가 있으면(사람이 옛 DLL 을 가리키면) 그대로 돈다. 맵 세트·원본 raw 는 W3 와 같은 도우미로 공급. |

W0 은 게이트 대상이 아니고(`[Fact]`, 데이터 불필요), 나머지 14건 중 W1·W2·W4·W5·W6·W8·W11·W12·W13a·W13b·W14·W15 는 다섯 변수 중 일부로 돈다(W7·W9 는 BASELINE_F32 까지). W16 은 `[SkippableFact]` 로 변수 점검.

주의: W10 의 토큰은 처음 #262 로 달았다가 리더 지시로 새 이슈 #265 로 바꿨다(#262 는 실데이터 CI 가 붙으면 허용 목록에서 빠진다). 다른 번호를 원하면 이유 문장 끝 토큰만 바꾸면 된다. 허용 목록(`ci.yml`)에 그 번호가 있어야 건너뜀 게이트를 통과한다.

## 2. 생성 세트로 W14·W15·W16 (+전체)

- **세트 생성**: pre 의 `xpe-pre/build/ci-preprocess/bin/xpe_calib_fixture_gen.exe --out <dir> --caldata <CalData_6> --expiry-ms 4102444800000`, 1.9 초(`generated_set_manifest.txt`). CalData_6 원본은 리더 체크아웃 `tests/test_data/CalData_6`. 생성 파일은 `calib-real-v2` 와 **바이트가 다르다**(sha256 목록, 헤더 차이): offset `dcc76…` vs `0f888…`, gain `46a39…` vs `00405…`, defect `f00b1…` vs `ff9df…`.
- **전체 클래스 실행**(`QuickStartWalkthroughScenarios` 18건, 생성 세트 + 리더의 최신 CI DLL 폴더 `build/e2e-native-dlls`, 변수 다섯 개만): **통과 15, 실패 3** (`full_class_generated_set.txt`). W14·W15·W16 은 통과. 실패 3건: W3·W10(위, 변수 없음) 과 **W7**.
- **W7 실패는 헤더 차이가 아니었다**: 열기 직후 "Save float enabled" 가 True 인데 시험은 False 를 단언했다. 기본 체인이 열자마자 전처리하므로(GUI-C-234b) 열기 직후 저장 가능한 것이 맞다. **`calib-real-v2` 로도 똑같이 실패**해서(`w7_with_calib_real_v2.txt`) 낡은 단언으로 판단했고 True 로 고쳤다(이유 주석 포함).
- **수정 뒤**: W3 통과(2분 2초, 이 중 맵 세트 생성 75 초), W7 통과(22 초), W10 건너뜀(이유 문장과 `XPE-SKIP-ALLOWED:262` 확인) (`w3_w7_w10_generated.txt`). 그래서 다섯 변수만으로 클래스 전체가 통과 17 + 건너뜀 1 로 정리된다 — 단, 이 최종 상태를 **한 번에 전체로는 돌리지 않았다**(수정한 3건은 위처럼 따로, 나머지 15건은 수정 전 전체 실행의 결과; 수정은 W3·W7·W10 과 도우미에만 닿는다).
- **헤더 차이가 앱·시험에 준 영향: 관측되지 않았다.** W14(처리 영상 sha256 이 1단계 기준과 일치)·W7(바이트 동일)이 생성 세트로 통과했다. 알림 `XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT: 44053 of 9437184 (0.467%)` 은 생성 세트에서 나오는데, 같은 문장이 `calib-real-v2`(GUI-C-232 워크스루)에도 나온다 — 헤더 차이가 만든 것이 아니다. 화면의 "FIXTURE SET" 은 고정 문구(`StudyQueue.xaml`)다.

## 3. CI 러너 시간 (로컬 실측, 상한 추정이 아님)

- 수정 전 전체 실행: **8분 50초**(18건 중 W3·W10 은 즉시 실패라 0). 가장 긴 것은 W9 4분 10초, 나머지는 9~31 초 (`full_class_time.txt`).
- 더해지는 것: W3 **2분 2초**(맵 세트 생성 75 초 포함), W7 수정 후 +12 초 정도. 그래서 클래스 전체 **약 11분** (로컬, 이 기계, 관측기 없이). 현재 Native 잡이 약 25분이고 한계가 45분이므로 **약 36분 전후**가 되는 계산이다. CI 러너가 이 기계보다 느리면(과거 기록에는 반대도 있었다) 여유가 줄어든다 — 이 계산은 두 기계의 속도 비를 모르는 추정이다.
- 줄일 수 있는 곳(제안, 하지 않았다): W9(4분 10초, 쓰기 실패 재현)와 W3 의 맵 세트 생성(75 초)이 큰 몫이다. 생성기는 `--width/--height` 로 더 작게(예: 512) 만들 수 있으면 줄지만 W3 는 1024 크기를 전제하므로 바꾸지 않았다.

## 한계
- 위 "최종 상태" 전체를 한 번에 돌리지 않았다(위).
- DLL 은 리더의 최신 CI 산출물(`build/e2e-native-dlls`)이다. CI 러너에서 실행한 것은 아니다.
- 만료 세트의 만료 시각(2000-01-01)은 내가 정했다: 이전 `expired1024` 가 어떻게 만들어졌는지는 확인하지 못했다. W3 의 만료 케이스는 출력만 하고 단언하지 않는다(기존 동작 그대로).
- 시간 추정은 로컬 한 번의 실측이다.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| 1 못 도는 시험 목록과 처분 | GUI-C-239 (W3 공급, W10 건너뜀) |
| 2 생성 세트로 W14·W15·W16 확인, 헤더 차이 보고 | GUI-C-239 (+W7 낡은 단언 정정) |
| 3 CI 러너 시간 추정 | GUI-C-239 |
