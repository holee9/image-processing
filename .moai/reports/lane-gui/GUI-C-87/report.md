# GUI-C-87 보고 — W-07 Native 기대값을 DN 프리셋에 맞춤 (#177)

커밋(dev/gui, 미푸시): ad2bfec (dev/postprocess 병합), 2593143 (W-07)

## 0. 먼저 적는 제약 — 수정 전 Native W-07 이 빨강인지는 로컬에서 볼 수 없다

- **로컬 DLL 의 출처**: 로컬 Native DLL(`build/ci-common/bin`)은 `provenance.json` 기준 `83ffa7a` 에서 빌드됐다(CI run 34535620953). a4253be 보다 이전 커밋이다.
- **새 프리셋 DLL 이 없다**: `gh run list --branch dev/postprocess` 에 a4253be 로 돈 CI 실행이 없다. 네이티브 빌드는 이 레인이 할 수 없다(#98).
- **결과**: 로컬 DLL 은 옛 HU 프리셋(Lung −600/1600)을 돌려준다. 병합 뒤 수정 전 W-07 을 Native 로 돌려도 **통과**하므로 판독을 이 방법으로는 확인할 수 없다.
- **대신 보인 것**: 수정한 W-07 이 옛 DLL 에서는 **빨강**이다(아래 표). 즉 이 시험은 프리셋 값에 반응한다.
- **CI 몫**: 새 DLL 에서 초록인지는 lead 의 병합 뒤 CI(`gui-e2e-native`)가 확인한다.

## 1. 병합

- 명령: `git merge --no-ff dev/postprocess` → `MERGE_EXIT=0`, 병합 커밋 `ad2bfec`. a4253be 가 포함됐다(`merge-base --is-ancestor` 로 확인).
- **dev/postprocess 의 구성**: `origin/main` 에 a4253be 한 커밋을 더한 것이다(`rev-list --count origin/main..dev/postprocess` = 1).
- **병합 결과의 차이**: `origin/main` 대비 `modules/` 차이는 a4253be 의 display 파일 5개뿐이다.
- **삭제 파일**: 병합 출력에 보인 `modules/preprocess/src/mode_selector.cpp` 와 `simd_dispatch.cpp` 는 `origin/main` 에도 없는 파일이다. 이번 병합이 새로 지운 것이 아니라 main 의 상태를 따라온 것이다.

## 2. W-07 변경

- **기대값**: Native Lung 기대값을 `C=32768`, `W=65535` 로 바꾸고, 주석을 "detector DN, provisional full range (#177, #151)" 로 고쳤다. Mock 분기(`C=25000`)는 그대로 두었다.
- **적용 여부 구별**:
  - 문제: 새 Native 프리셋은 앱 기본 창과 값이 같다. 그래서 프리셋이 적용되지 않아도 상태줄이 기대 문자열을 이미 담고 있을 수 있다.
  - 대책: 시험이 먼저 창을 `C=12345/W=4321` 로 바꿔 적용하고, 상태줄에서 그 값을 확인한다.
  - 판정: 그다음 Lung 을 골라 상태줄이 프리셋 값으로 **되돌아오는지** 본다.
  - 시작 상태 보정: 이미 Lung 이 선택돼 있으면 Bone 을 먼저 고른다.
- **도우미**: `WorkbenchObservation.TypeWidth` 를 추가했다.

## 3. 결과

| 실행 | 적용 전 상태줄 | 적용 후 상태줄 | 결과 |
|---|---|---|---|
| Mock (`w07-mock.txt`) | `VOI(Linear, C=12345, W=4321)` | `VOI(Linear, C=25000, W=50000)` | 통과 |
| Native, 옛 DLL (`w07-native-olddll`) | `C=12345, W=4321` | `C=-600, W=1600` | **실패** (`Not found: "C=32768"`) — DLL 이 a4253be 이전 |
| 반증 Mock — 뷰모델 프리셋 대입 3줄 끔 (`falsify-mock.txt`) | — | `C=12345, W=4321` 그대로 | 실패 |
| 반증 Native (`falsify-native`) | — | `C=12345, W=4321` 그대로 | 실패 |

전체 실행 (C-86 의 미커밋 시험 `EvidenceBundleScenarios` 는 필터로 제외):
- Mock E2E `full-mock.txt`: 실패 0 / 통과 84 / 건너뜀 1.
- Native E2E, 옛 DLL `full-native-olddll`: 통과 84, 실패는 **W-07 하나뿐**. trx 의 Failed 2건 중 1건은 실행 전체 요약이다.
- 통합 `full-int.txt`: 실패 0 / 통과 209 / 건너뜀 1.
- 빌드: `BUILD_EXIT=0` (경고 0 / 오류 0). 병합 뒤 빌드(`build-c87.txt`), 반증 빌드, 복원 뒤 빌드 모두 같다.

## 4. "부위를 바꾸면 창이 바뀐다" 를 전제로 한 다른 시험 (목록만)

- **검색 범위**: `clients/ImageProcTest.E2ETests`, `clients/ImageProcTest.IntegrationTests` 의 `*.cs`, 패턴 `SelectBodyPart|BodyPartSelector|.Select("Lung"|"Bone"`. 추가로 `tools/*.ps1` 에서 `BodyPart|CreateVoiPreset` 를 찾았고 결과는 0건이다.

| 시험 | 부위 변경을 쓰는 방식 | 새 Native 프리셋에서 |
|---|---|---|
| W-21 (`ViewportTruthScenarios.cs:75`) | 대조군: 프리셋 변경 뒤 HUD 가 입력칸 중심값을 따라가는지 | 변경 전후 모두 32768 이라 **"HUD 가 렌더를 따라간다" 대조가 헛돌 수 있음** |
| W-22 (`:125`) | 대조군: 프리셋 변경이 처리 영상 버전을 올리는지 | 파이프라인이 다시 돌면 버전이 오르므로 값 동일과 무관할 가능성 — 미측정 |
| W-25 (`:198`) | 분리 뷰어 HUD 가 렌더 값과 같은지 | W-21 과 같은 형태로 헛돌 수 있음 |
| W-23 (`FailedRenderScenarios.cs:41`) | 결함 주입 호출 2 를 일으키는 수단 + 버전 증가 대조 | W-22 와 같음 — 미측정 |

이 판단은 판독에 근거한다. 새 DLL 이 없어서 실행으로 확인하지 못했다.

## 5. 미검증 / 잔여 위험

- **Native 초록**: 새 Native DLL 에서 W-07 이 통과하는지는 보지 못했다(0절).
- **W-07 의 공유 상태 변경**: W-07 은 같은 공유 앱에서 창을 12345/4321 로 옮긴다. 마지막에 Lung 프리셋으로 되돌리지만, 프리셋 적용이 실패하면 같은 컬렉션의 뒤 시험이 바뀐 창에서 시작한다.
- **GUI-C-86**: 작업이 시작돼 있다. 시험 파일이 커밋되지 않은 채 남아 있고, 수정 전 빨강 측정은 끝났다. 이 카드의 커밋에는 넣지 않았다.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| W-07 Native 기대값 | GUI-C-87 (이 카드) |
| W-21/W-25 대조가 새 프리셋에서 헛돌 가능성 | 신규 카드 후보 (lead) |
| 증거 묶음 RunId | GUI-C-86 (재개 예정) |
