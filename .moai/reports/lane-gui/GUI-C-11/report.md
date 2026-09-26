# GUI-C-11 — P1AReady 보정 체인 테스트를 캘리브레이션 상태 모델에 맞춤

- 카드: GUI-C-11 · Refs #117 #98 · 커밋 `bda0301` (미푸시)
- 워크트리: `D:/workspace-github/xpe-gui` / `dev/gui` / HEAD `7275916` (main 병합, `.claude` 변경 0건)
- **결과: 실패 0 / 통과 97 / 건너뜀 0 / 전체 97** — CI 실물 바이너리로 실측

---

## 0. 이번에 바뀐 것 — 검증을 추론에서 관측으로 되돌렸다

GUI-C-08·C-10 에서 두 번 "로컬 바이너리가 구본이라 검증 불가" 로 끝냈다. 이번엔
**CI 아티팩트를 직접 내려받아** 최신 바이너리로 검증했다.

```bash
gh run download 34446484770 -n xpe-ci-common-binaries     -D <tmp>
gh run download 34446484770 -n xpe-ci-preprocess-binaries -D <tmp>
gh run download 34446484770 -n xpe-ci-post-binaries       -D <tmp>
cp <tmp>/*/*.dll build/ci-common/bin/
```

`gh` 는 `C:/Program Files/GitHub CLI/gh` 에 있고 인증돼 있다. 이 레시피가 세 카드에 걸친
"구 바이너리" 한계를 없앤다 — 네이티브를 빌드하지 않고도 CI 와 같은 것을 실행한다.

**부수 확인 1건**: 같은 실행에서 **GUI-C-10 의 짧은 `dataSize` 2건이 통과**했다.
그때 "구 바이너리 탓" 이라고 적은 진단이 가설에서 **관측**이 됐다.

## 1. 착수 전 표 — 세 케이스가 무엇을 기대하고 무엇을 받는가

최신 바이너리 기준(`step0-fresh-baseline.log`, 실패 5 / 통과 91 / 전체 96):

| 케이스 | 기대(수정 전) | 실제 | 계약상 옳은 것 |
|---|---|---|---|
| `CorrectionChain_WithoutCalibration_PreservesSyntheticInput` | `OK` + 입력 보존 | **-16** | `CALIB_NOT_LOADED` |
| `CorrectionChain_RunTwice_DeterministicRmseIsZero` | `OK` | **-16** | 캘리브 로드 후 `OK` |
| `CorrectionChain_Output_HasNoNanOrInf` | `OK` | **-16** | 캘리브 로드 후 `OK` |
| `DataSizeContractTests.OffsetCorrect_ExactDataSize_PassesSizeGate` | OK/NOT_INITIALIZED | **-16** | `-16` 도 통과로 허용 |
| `DataSizeContractTests.OffsetCorrect_ZeroDataSize_PassesSizeGate` | OK/NOT_INITIALIZED | **-16** | 〃 |

**5건 전부 카드 항목 2·3 에 정확히 대응한다.** 설명되지 않는 실패는 없었다.
"캘리브레이션 없이 입력 보존" 은 계약에 없는 동작이므로 **테스트를 계약에 맞췄다**
(구현을 테스트에 맞추지 않았다).

## 2. 바꾼 것

### 2-1. `WithoutCalibration_*` → 계약 관측 케이스

`CorrectionChain_WithoutCalibration_ReturnsCalibNotLoaded` 로 바꾸고, **세 진입점
각각**(`xpe_offset_correct` / `xpe_gain_correct` / `xpe_defect_correct`)이 `-16` 을
내는지 단언한다. 앞에 `shutdown()` 을 둬 같은 프로세스의 다른 테스트가 로드해 둔
캘리브레이션을 확실히 떨군다(전역 저장소는 DLL 인스턴스당 하나다).

### 2-2. 나머지 둘 → 캘리브레이션을 로드한 뒤 실행

**픽스처 파일을 쓰지 않았다.** `tests/test_data/calibration_cases/` 는 README 만 있고
`.raw` 는 git-ignore 된 로컬 미디어다(README 에 "must be copied from local source media").
CI 에는 없으므로 거기 의존하면 **CI 에서 조용히 스킵**된다 — 이 레인이 GUI-C-05·06 에서
싸운 바로 그 형태다.

대신 자급식 경로를 썼다: 합성 프레임 → `xpe_calib_generate_offset` / `xpe_calib_generate_gain`
→ 임시 디렉터리에 `.xcal` 저장 → `xpe_calib_load_offset` / `xpe_calib_load_gain` → 전역
저장소 적재 → `offset_correct` → `gain_correct`(FLOAT32 출력). 각 네이티브 호출의 반환코드를
전부 단언해서, 깨지면 어느 단계가 깨졌는지가 이름으로 나온다.

`defect_correct` 는 이 두 테스트의 체인에서 제외했다 — 결함 맵 생성 경로가 클라이언트
쪽에 없다. `WithoutCalibration_*` 이 `defect_correct` 의 `-16` 은 덮는다.

### 2-3. `AssertPassedSizeGate` — `CALIB_NOT_LOADED` 허용

크기 검사가 캘리브레이션 검사보다 앞서므로(`offset_correct.cpp:305` vs `:325`) `-16` 은
"크기 게이트가 거절하지 않았다"의 정상 결과다. `NOT_INITIALIZED` 는 init 자체가 안 먹은
경우를 위해 남겼다. 여전히 `INVALID_INPUT` 만 거절로 본다.

### 2-4. 새 오류 코드 매핑

`XpeErrorCode` 에 `CALIB_NOT_LOADED = -16` 추가 + `ErrorString_ForAllDefinedCodes` 에
`InlineData` 1행 추가(회귀). 네이티브 `xpe_error_string` 은 이미 `-16` 을
"Calibration data not loaded" 로 매핑하고 있다(`xpe_common.cpp`).

## 3. Lane A 가 짚은 4곳 — 3곳 반영, 1곳 반영 안 함

| # | 위치 | 처리 |
|---|---|---|
| 1 | `DataSizeContractTests.cs:76-77` | 반영 (§2-3) |
| 2 | `LeakEnduranceTests.cs:54` | **반영 안 함 — 근거 아래** |
| 3 | `ErrorCodeMappingTests.cs:31` | 반영 (§2-4) |
| 4 | `PInvoke/XpeCommonNative.cs:33` | 반영 (§2-4) |

**2번을 바꾸지 않은 이유**: 그 단언은 `xpe_init`(xpe_common)의 반환값을 본다.
`CALIB_NOT_LOADED` 를 반환하는 곳을 전수 확인하면 **preprocess 보정 3함수뿐**이다.

```
grep -rn "CALIB_NOT_LOADED" modules/common/src/ modules/preprocess/src/
  modules/common/src/xpe_common.cpp        -> return "Calibration data not loaded";   (문자열 매핑)
  modules/preprocess/src/defect_correct.cpp -> return XPE_ERR_CALIB_NOT_LOADED;
  modules/preprocess/src/gain_correct.cpp   -> return XPE_ERR_CALIB_NOT_LOADED;
  modules/preprocess/src/offset_correct.cpp -> return XPE_ERR_CALIB_NOT_LOADED;
```

`xpe_init` 에는 반환 경로가 없다. 여기에 `-16` 을 허용 목록에 넣으면 근거 없이 단언을
넓히는 것이고, 실제로 그런 값이 오면 그건 **잡아야 할 결함**이다. Lane A 의 "같은 패턴"
지적은 형태는 맞지만 이 지점에는 해당하지 않는다.

## 4. 재실측

```
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug

수정 전(최신 바이너리):  실패 5 / 통과 91 / 건너뜀 0 / 전체 96   step0-fresh-baseline.log
수정 후:                실패 0 / 통과 97 / 건너뜀 0 / 전체 97   step1-tests.log
```

97 = 96 + `xpe_error_string` 회귀 1행. **건너뜀 0** 이므로 캘리브레이션 생성·로드 경로가
실제로 실행됐다(스킵으로 빠져나간 것이 아니다).

## 5. 미검증 (Gaps)

- **CI 실행 결과는 아직 없다.** 위는 CI 아티팩트를 로컬에서 돌린 것이고, CI 러너 환경
  (임시 디렉터리 권한, 병렬성)에서 캘리브레이션 생성이 같게 동작하는지는 leader 가 확인한다
- `xpe_calib_generate_*` 가 만든 맵의 **수치적 타당성은 검증하지 않았다.** 이 테스트는
  결정성과 유한성만 본다 — 보정 결과가 물리적으로 옳은지는 네이티브 단위테스트 소관이다
- **C# enum 에 `-11`~`-15` 가 없다**(`SAFETY_VIOLATION`, `INTERNAL`, `DICOM_INVALID`,
  `DICOM_CONFORMANCE`, `NOT_IMPLEMENTED`). 따라서 `ErrorString_ForAllDefinedCodes` 는
  이름과 달리 전수가 아니다. 카드 범위가 `-16` 뿐이라 그것만 넣고 주석으로 표시했다 —
  **별도 판정 사안**
- `defect_correct` 는 캘리브레이션 로드 후 경로를 타지 않았다(§2-2)
- Release 구성 미빌드, WPF 앱 미실행

## 6. 잔여 위험

- 전역 캘리브레이션 저장소는 **DLL 인스턴스당 하나**라 같은 프로세스의 테스트끼리 상태를
  공유한다. `WithoutCalibration_*` 이 앞에 `shutdown()` 을 두는 이유이고, xUnit 병렬 실행이
  켜지면 이 클래스는 서로 간섭할 수 있다. 현재 `xunit.runner.json` 은
  `parallelizeTestCollections: false` 라 안전하다 — 그 설정을 바꾸면 이 테스트가 먼저 깨진다
- 임시 `.xcal` 은 테스트마다 새로 만들고 `finally` 에서 지운다. 삭제 실패는 무시한다
  (임시 디렉터리이므로 누적돼도 해가 없다)

## 부록 — 사용한 명령

```bash
git merge origin/main                                   # → 7275916
export PATH="/c/Program Files/GitHub CLI:$PATH"
gh run list --branch main --limit 5
gh run download 34446484770 -n <artifact> -D <tmp>      # common / preprocess / post
cp <tmp>/*/*.dll build/ci-common/bin/
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
grep -rn "CALIB_NOT_LOADED" modules/common/src/ modules/preprocess/src/
```
