[2026-10-21 18:34 KST] [xpe-pre]
# QA-A-276 — 2G·신규 PRE 동일조건 비교의 PRE 쪽 출력 1건과 조건 기록 (Refs #278)

범위는 신규 PRE 쪽 절반이다. 2G 쪽 출력은 없고, 제조사 DLL은 로딩·실행하지 않았다. 1G·Hn_Viewer_NET은 다루지 않았다.
이 문서는 "품질 개선"을 주장하지 않는다. 짝(2G 출력)도 독립 검증도 없다.

## 1. 10/18 품질 미합격 PRE RAW 2건 (기존 증거 인용, 새로 돌리지 않음)

출처: `.moai/reports/lane-pre/CODEX-TEMP-2026-10-18/v5-final-raw-two-cases/` 의 `execution-evidence.json`, `Dark_15/report.json`, `Bright_29/report.json` (같은 폴더의 `final-v5-raw-handoff.md` 와 대조).

| 항목 | Dark_15 | Bright_29 |
|---|---|---|
| 입력 경로 | `image-processing/tests/test_data/cyan_test/calfiles/Dark_15.raw` | `.../calfiles/Bright_29.raw` |
| 입력 SHA256 | `b2620ef9a0dfad5d04c88acb4317a76626ca723f326e6e30b8cc48e94af6dd37` | `912a096c07b08241fb55673b0585a49f354d42a0db4aa2999e059cb16fae9a9a` |
| 자료형·크기 | UINT16LE, 3072×3072 | 같음 |
| 입력 종류 선언 | `--input-kind dark` (호출자 선언, 취득 인증 아님) | `--input-kind flat` (같음) |
| 맵 | `xpe-data/bpm-eval-260/clean_set_v2/` 의 offset/gain/defect.xcal (아래 3.2 표와 같은 SHA) | 같음 |
| 설정 | `--gain-semantics normalized_gain`, 읽기·온도·비선형·비닝·고스트 우회(기본 runner 설정) | 같음 |
| 바이너리 | `xpe_pre_e2e.exe` `9645a8e2…e925d`, `xpe_preprocess.dll` `d404d407…7643d`, `xpe_common.dll` `56bb73af…41fa4` | 같음 |
| 실행 시 HEAD | `75c0043195e5afacf7d6b424bee32c063b9e99a0` + 미커밋 (`git_sha` 표기 `75c00431+uncommitted-followup-v5`) | 같음 |
| 종료 코드 | 0, `run_succeeded=true`, shipped parity true | 0, 같음 |
| 결과 | offset 품질 pass, 포화 비율(clamp) 23.95 % → `review_required`, 암전류 편향 2.3395, 부호 있는 편향 1.6494, DSNU 2.5367 (모두 화소값 단위) | 최종 평탄도 잔차 0.6428 % (1단계 ≤1 % pass, 출하 ≤0.5 % fail), 이득 단계 잔차 0.6428 % fail, 줄무늬 2.2076 (≤3 pass), FPN 감소 20.95 dB |
| 전체 판정 | phase1=false, release=false, CES null, COMPLETE 미측정 | 같음 |

두 건 모두 report.json 의 `degraded_evidence=true`, "COMPLETE 미측정"이다. 이 표의 수치는 기존 report.json 값을 옮긴 것이다.

## 2. 입력 1건

`image-processing/tests/test_data/cyan_test/Bright_Chest phantom_75kv_320ma_25.6(SID_110)_00.raw` 를 골랐다. 읽기만 했다.
- SHA256 `62f2979250d5e0a503bf59fff5775f907d750e81226815275a6c2f291e57f4be` — 출처 목록(`PRE-2G-COMPARE-20261020/qa-input-provenance.md`)의 값과 같고, 실행 전후 같았다(아래 5절).
- 파일 길이 18,874,368 바이트 = 3072 × 3072 × 2 (UINT16LE 로 읽는 것과 길이가 맞다. 길이는 형식의 유일한 증거가 아니다).
- 보정 상태(오프셋 보정 여부): **미확인**. 추정으로 채우지 않았다. `conditions.json` 에 `unknown / unverified` 로 적었다.
- 같은 폴더 `calfiles/` 는 맵 생성 프레임 출처로만 썼다(3.2).

## 3. 기존 PRE 경로로 만든 출력

### 3.1 경로와 한 가지 이탈 (리더 확인 요청)

기존 PRE 경로는 `xpe_pre_e2e.exe` 이다. 이 도구는 `xpe_preprocess_pipeline_out` 을 호출하지만 **출력 배열을 파일로 쓰지 않고** report.json 에 요약(최소·최대·평균)만 남긴다(`xpe_pre_e2e --help` 에 출력 파일 옵션 없음, 소스 `xpe_pre_e2e.cpp` 의 `output_summary`). 그래서 "출력 RAW 1건" 계약 필드를 채울 기존 도구가 없었다.
이탈: 같은 호출을 그대로 하고 배열만 쓰는 36줄짜리 일회용 도구(`dump_pre.cpp`, 이 폴더에 사본, 제품 트리·`modules/` 에는 넣지 않음)를 만들어 기존 DLL에 링크해 실행했다. 알고리즘·설정·DLL은 바꾸지 않았다. 도구 SHA256: 소스 `78cc9e20…a04e`, 실행 파일 `0d064191…c135`(스크래치 폴더, 증거 밖).
같은 입력을 `xpe_pre_e2e.exe` 로도 1회 돌려 대조했다(`--input-kind image`, 같은 맵): 보고된 최소·최대·평균이 일치했고 `matches_shipped_pipeline=true`.

| | 일회용 도구 출력 배열 | `xpe_pre_e2e` report.json `output_summary` |
|---|---|---|
| 최소 | 4258.3544921875 | 4258.3544921875 |
| 최대 | 77956.15625 | 77956.15625 |
| 평균 | 25668.254828610923 | 25668.254828610923 |

이 도구 방식이 허용되지 않으면 출력은 폐기하면 된다(증거 밖 개인 폴더). 그 경우 막힌 필드는 계약의 `output.path`/`output.sha256` 이고, 기존 도구에 출력 쓰기가 없다는 점이 사유다.

### 3.2 조건 (계약 형식 전문은 `conditions.json`)

- 입력: 위 2절.
- 맵 (모두 `xpe-data/bpm-eval-260/clean_set_v2/`):

| 맵 | 크기(바이트) | SHA256 | 만든 프레임 수 |
|---|---:|---|---|
| offset.xcal (다크) | 37,748,888 | `5569bacf66fb25f10b9137ebf3f466e9ce8d94857c90dc4a7340df362c290c39` | 다크 7장(`Dark_08`~`Dark_14`, `Dark_07` 은 첫 프레임이라 제외) |
| gain.xcal (이득) | 37,748,888 | `a7ddadbd5b69a350c44c638fd8887096ca4cb448eda322e5d8976575c7520d22` | 밝은 영상 6장(`Bright_23`~`Bright_28`) + 위 다크 평균 |
| defect.xcal (BPM) | 214,270 | `4579e06bcf5be09828935eb48c71bd78d1f51afc9fc97f83e678158dfa2f105c` | 밝은 영상 18장(BPM 용) + 위 다크 |

  프레임 수는 같은 폴더 `manifest.json`(SHA256 `5f2a286b…e177`)의 `frames` 와 `inputs` 에서 옮겼다. 이 맵들은 `cyan_test/calfiles` 프레임에서 만들어졌고, 이번 입력(Chest phantom)의 취득과 같은 세션이라는 증거는 없다(3.5).
- PRE: 소스 HEAD 는 `75c0043195e5afacf7d6b424bee32c063b9e99a0` 로 적었다 — 이 DLL을 마지막으로 실행한 10/18 증거가 기록한 값이다. 그 바이너리는 이 HEAD 에 미커밋 변경을 더해 지었고(DLL 수정 시각 2026-10-19 02:00), 그 변경은 이후 `e404b70e0cb3fced2a80b49edb5197f4f345c74b` 로 커밋됐다. **DLL 과 어느 커밋이 같은 소스인지는 확인하지 못했다**(재빌드하지 않음). 현재 dev/preprocess HEAD 는 `b140f7f2238c7485308ea7ad4baf0c1538de5099`.
  DLL: `xpe_preprocess.dll` `d404d4071bd8c9b0d799cb5e98c67512022e3f5bd0e6d0cae192c865afe7643d`, `xpe_common.dll` `56bb73afc0af27a50471cc3148462aaa333de94ff1636c8a3952a67427641fa4` (실행 전후 같음).
- 설정 JSON 전문(`xpe_preprocess_pipeline_out` 에 넘긴 문자열): `{"bypassReadout":true,"bypassTemp":true,"bypassNonlinearity":true,"bypassBinning":true,"bypassGhost":true}` — SHA256 `945ced65cd3b5e809b0d120d5def42d16e294607c5179e73236559f638f08a6f`.
- 적용 확인: `pipeline_out` 이 돌려준 `meta.flags=0x200c` = 오프셋(0x2000) | 결함(0x4) | 이득(0x8) 보정 완료 비트. 우회 비트(0x4000)는 없음. 고스트·읽기·온도·비선형·비닝은 설정으로 우회(적용 안 함).
- 출력: `xpe-pre/build/qa276_out/chest00_pre_out.f32` (git 이 무시하는 `build/` 아래 개인 폴더, 증거 폴더 밖). SHA256 `c2210d3dc5ee329bab6203435b4395eb52e1a7dad61d489530625801c2e9821a`, 자료형 **FLOAT32 little-endian**, 3072×3072, 한 줄 12,288 바이트, 파일 길이 37,748,736 바이트. 유한값만(비유한 0).

### 3.3 관측한 사실 두 가지 (짝을 맞출 때 걸릴 수 있는 곳)

1. 출력이 FLOAT32 이다. 계약은 두 출력의 자료형이 같아야 한다(`DTYPE_MISMATCH`). 2G 쪽이 UINT16 이면 변환 없이는 짝이 안 맞는다. 변환은 하지 않았다.
2. 출력 최대 77,956 이고 화소의 0.479 % 가 65,535 를 넘는다(입력 최대는 65,535, 포화 화소 존재). UINT16 으로 자르면 값이 잘린다. 입력 쪽 포화는 이 문서가 원인을 판단하지 않는다.

### 3.4 배치 확인

출력이 입력과 같은 방향으로 놓였는지 1/8 격자 표본의 상관계수로 봤다: 그대로 0.9935, 상하 뒤집기 0.9566, 좌우 뒤집기 0.4118, 전치 0.1765. 그대로가 가장 높고, 출력은 버퍼를 변환 없이 썼다. 상하 뒤집기와의 차이는 작으므로(팬텀이 상하 대칭에 가까움) 이 확인은 "그대로"를 지지하지만 증명은 아니다.

### 3.5 계약 검사 결과 (확인한 것)

`tools/calibration/comparison_manifest.py` (main `47d932e3`, 4판)의 `validate_pair` 를 `conditions.json` 에 실행했다. 기준 폴더는 `D:/workspace-github` (경로가 기준 폴더 밖이면 `PATH_OUTSIDE_BASE`).
- 있는 그대로(2G 쪽 없음): 사유 `CORRECTION_STATE_UNVERIFIED, MODE_MISSING, ENGINE_OUT_OF_SCOPE, UNSUPPORTED_FIELD, GEOMETRY_MISMATCH, DTYPE_MISMATCH, STAGE_MISMATCH, UNITS_MISMATCH, STAGE_NOT_ATTRIBUTABLE, MAP_IDENTITY_DIFFERS, INVALID_ROI`. 입력·출력·맵 4개의 파일 SHA 는 모두 맞았다(`SHA_MISMATCH` 없음). 앞의 두 개를 뺀 나머지는 A 쪽이 비어서 생긴다.
- 대조(메모리 안에서만, 저장 안 함): A 쪽을 B 와 같게 복제하면 남는 사유는 `CORRECTION_STATE_UNVERIFIED, MODE_MISSING` 둘뿐. 시험 전용 값으로 이 둘을 채우면 사유가 없어진다. 즉 B 블록 자체는 계약 형식을 만족하고, 지금 막는 것은 입력의 보정 상태·모드 미확인과 2G 쪽 부재이다.
- 이 검사는 형식·SHA·크기 확인이다. 품질이나 동등성 판단이 아니다.

## 4. 남은 미확인 항목

1. 입력의 보정 상태(오프셋 보정 여부) — `unknown`. 취득 모드(`mode`)도 `unknown`.
2. 이 입력(Chest phantom)과 맵이 같은 세션·같은 검출기 모드에서 취득됐는지.
3. 맵(`clean_set_v2`)이 이 입력에 맞는 보정 조건인지. 이득 맵은 평균 화소값 약 20,300 인 밝은 영상 6장으로 만들어졌고, 이번 입력의 평균은 약 35,948 이다(관측값만 적음, 적합성 판단 아님).
4. DLL 과 `75c00431`/`e404b70e`/`b140f7f2` 중 어느 소스가 같은지.
5. 2G 쪽 출력의 자료형·보정 상태·단계(engine-direct 여부). PRE 쪽 `engine-direct` 는 "`pipeline_out` 출력을 후처리 없이 썼다"는 의미로 적었고, 2G 쪽과 같은 단계인지는 모른다.
6. 계약의 `mode`, `comparison_kind`(비워 둠) — 2G 출력이 생길 때 정한다.
7. 일회용 도구 사용 승인 여부(3.1).
8. 10/18 표(1절)의 수치는 기존 report.json 값이며 이번에 다시 재현하지 않았다.

## 5. 보존·정리 확인 (이번 실행에서 관측)

- 입력 RAW SHA256 실행 후 `62f29792…57f4be` 그대로. 맵 3개와 DLL 2개 SHA 도 위 값과 같다.
- 실행 프로세스: 일회용 도구 1회(`2026-10-21T09:32:20Z`~`09:32:21Z`, 종료 코드 0), `xpe_pre_e2e.exe` 1회(종료 코드 0), 동시 실행 없음. 종료 후 `tasklist` 에서 `dump_pre`/`xpe_*` 프로세스 0개.
- 저장소에 쓴 것은 이 증거 폴더뿐이다. 기존 산출물·dirty 파일·원데이터 수정 없음, push 없음.
- 개인 출력 폴더 `xpe-pre/build/qa276_out/` 에는 출력 배열, 실행 로그, 대조용 `e2e_check/` 가 있다(전부 증거 밖, git 무시).

## 6. 이 폴더의 파일

- `report.md`: 이 문서.
- `conditions.json`: 비교 계약 4판 형식의 PRE 쪽 조건 기록(`engines.A` 는 일부러 비움).
- `dump_pre.cpp`: 3.1의 일회용 도구 소스.

주의: `.gitignore` 가 `.moai/reports/**` 중 `lane-*/**/*.md` 만 풀어 두므로 `conditions.json`·`dump_pre.cpp` 는 git 이 무시한다(디스크에는 있음). 커밋 대상은 `report.md` 뿐이다.
