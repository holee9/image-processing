# GUI-C-88 보고 — 새 DLL 에서 W-21·W-22·W-23·W-25 의 #171 반증 (#177, #171)

코드 변경 없음, 커밋 없음. 이 카드의 기준("빨강이 안 나오는 시험만 고친다")에 해당하는 시험이 없었다.

## 1. 새 DLL

- **준비 방법**: `Stage-NativeArtifacts.ps1 -RunId 35178852195 -Destination build/ci-776292b/bin` (`STAGE_EXIT=0`). 결과는 `stage-776292b.txt` 에 있다.
- **출처 기록**: `provenance-776292b.json` 의 `runId=35178852195`, `headSha=776292bf49ee…` 다. main `776292b` 의 CI 산출물이며 a4253be 가 포함돼 있다.
- **PowerShell 에서 `gh` 가 안 잡힌 문제**: `gh` 가 `C:\Users\drake\bin\gh`(실행할 수 없는 파일)로 해석됐다. 그래서 `C:\Program Files\GitHub CLI` 를 PATH 앞에 넣고 실행했다.
- **옛 DLL 과 분리**: 새 DLL 은 `build/ci-776292b/bin` 에 따로 두었다. 옛 `build/ci-common/bin` (83ffa7a) 은 그대로다.

## 2. 기준 실행 (새 DLL, Native, 수정 없음) — `base-native`

- **결과**: 통과 6 / 실패 0. 대상은 W-07, W-21, W-22, W-23, W-25, NativeProvenance 이다.
- **W-07 확인**: C-87 에서 로컬로 볼 수 없던 "새 DLL 에서 W-07 초록" 을 여기서 확인했다.
- **부위 변경 단계의 실측 값**:

| 시험 | 부위 변경 단계 기록 | 이 단계가 구별하는가 |
|---|---|---|
| W-21 | `preset: input=32768 hud=32768` | **아니다** — 앱 기본값도 32768 이라 HUD 가 렌더를 따르지 않아도 같은 값이다 |
| W-25 | `preset: main hud=32768 detached hud=32768` | **아니다** — W-21 과 같은 이유 |
| W-22 | `preset: v10 -> v11 indicator=''` | 그렇다 — 값이 같아도 파이프라인이 다시 돌아 처리 영상 버전이 오른다 |
| W-23 | `preset: v2 -> v3` (호출 2) | 그렇다 — W-22 와 같다 |

## 3. #171 반증 — 새 DLL Native / Mock

반증마다 한 곳만 바꿔 빌드했다(모두 `BUILD_EXIT=0`, 경고 0 / 오류 0). 대상 시험 4개를 돌렸다.

| 반증 (끈 통제) | 바꾼 곳 | Native 실패 | Mock 실패 | Native 실패 메시지 |
|---|---|---|---|---|
| ② HUD 를 Settings 에 바인딩 | `ViewportShell.xaml` HUD 중심값 | **W-21** | W-21 | `After typing C=12345 without applying, the HUD reads C=12345. The image was rendered at C=32768 …` |
| ① 설정 변경 시 오래됨 갱신 제거 | `OnSettingsPropertyChanged` 의 `RefreshParametersStale()` | **W-22, W-25** | W-22, W-25 | W-22: `… the stale indicator reads '(absent)' …` / W-25: `Control failed: the main window shows no stale indicator …` |
| ③ 파이프라인 실패 시 오래됨 표시 제거 | catch 의 `PreviewStaleReason = StalePipelineFailed` | **W-23** | W-23 | `… the stale indicator reads '(absent)' … (#171 ③)` |
| 분리 뷰어 배너 제거 | `grid.Children.Add(staleBanner)` | **W-25** | W-25 | `… the detached viewer showing the same image reads '(absent)' (#171 ①)` |
| 분리 뷰어 HUD 제거 | `grid.Children.Add(hud)` | **W-25** | W-25 | `After the preset the detached viewer named C=(no HUD) … (#171 ②)` |

- **판정**: 네 시험 모두 새 DLL Native 에서 해당 통제를 끄면 빨강이다. 부위 변경 단계가 값으로 구별하지 못하는 W-21·W-25 도 **이후 단계(편집 뒤·적용 뒤의 값)** 에서 빨강을 낸다.
- **① 반증이 W-25 도 빨갛게 만드는 이유**: W-25 는 먼저 "메인 창 표시가 뜬다" 를 대조로 확인한다. 분리 뷰어 배너도 같은 상태 원천을 쓴다(C-80 에서 기록). Mock 에서도 같은 결과가 나왔다.
- **복원**: 반증 뒤 원본으로 되돌렸다. `FALSIFY` 표식은 0건이고, 복원 빌드도 `BUILD_EXIT=0` 이다. `git status` 에는 추적 파일 변경이 없다.

## 4. 판단

- **고칠 시험 없음**: 카드 3항("빨강이 안 나오는 시험은 고친다")에 해당하는 시험이 없다. 시험 코드는 바꾸지 않았다.
- **사실로 남기는 것 — W-21·W-25 의 부위 변경 단계는 Native 에서 더는 아무것도 구별하지 않는다.**
  - W-21 의 이 대조가 원래 걸러 내려던 것은 "렌더를 따르지 않는 HUD" 다. Native 에서 그 역할은 이제 적용 단계(`hud=12345` 로 바뀌는지)만 한다.
  - 이 단계를 명시 값(예: 12345 → 적용 → 부위 변경으로 32768 복귀)으로 바꿀지는 lead 판단에 맡긴다. 카드 범위를 넘는다.

## 5. 미검증 / 잔여 위험

- **반증 실행 횟수**: 각 반증은 1회씩만 실행했다.
- **HUD 고정 반증**: 한 번 바인딩된 뒤 갱신되지 않는 HUD(OneTime) 로는 반증하지 않았다. 이 형태는 부위 변경 단계가 원래 잡으려던 경우지만, W-21 의 적용 단계가 대신 잡을 것이라는 판단은 판독이다.
- **CI 결과**: main `776292b` 의 `gui-e2e-native` 는 이 측정 시점에 진행 중이었다.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| 새 DLL 에서 #171 반증 확인 | GUI-C-88 (이 카드) |
| W-21·W-25 부위 변경 대조를 명시 값으로 바꿀지 | lead 판단 |
| 백엔드 전환 = 새 실행 묶음 | GUI-C-89 (다음) |
