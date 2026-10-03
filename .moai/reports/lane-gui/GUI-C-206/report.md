# GUI-C-206 — GUI-C-204 발견 셋 처리 (#225)

증거(이 폴더): `falsification_arms.txt`(팔 5개), `stamp_with_and_without_git.txt`(git 있을 때·없을 때 빌드 표식).
시험: IntegrationTests 650 통과 / 0 실패 / 1 건너뜀(전체), E2E A01·Z01 은 Mock·Native 모두 통과(아래).

## 1. About 에 앱 빌드 식별자

- **빌드 표식**(`ImageProcTest.csproj`): 타깃 `XpeStampGitRevision` 이 `git rev-parse --short=10 HEAD` 를 `SourceRevisionId` 로 넣어 정보 버전이 `1.0.0+<sha>` 가 되고, 빌드 구성은 어셈블리 메타데이터 `BuildConfiguration` 으로 들어간다. `IgnoreExitCode`·`ContinueOnError` 라 git 이 없거나 저장소가 아니면 **`unknown`** 이고 **빌드는 실패하지 않는다**.
  측정(`stamp_with_and_without_git.txt`): git 있을 때 `d700e74be0`, PATH 에서 git 을 뺐을 때 `unknown`, 종료 코드 0.
- **읽기**: `Services/BuildIdentity.cs`(`Current`, `From`, `Describe`) — About 이 "Build: <sha> (<config>)" 줄을 `ImageProcTest GUI-S0` 아래에 보인다. 값이 비면 `unknown`, 빈 문자열이나 지어낸 값이 아니다.
- **시험이 기대값을 같은 경로로 계산하지 않는다**(`MenuBehaviorScenarios.A01`, `AssertBuildLine`): 대화상자의 sha 가 ① 실행 파일에 찍힌 제품 버전(`FileVersionInfo`)의 `+sha` 와 같고 ② 구성이 실행 파일이 놓인 폴더(`bin/<Debug|Release>/…`)와 같고 ③ git 을 쓸 수 있으면 `unknown` 이 아니고 10자리 16진이며 실재하는 커밋이고 HEAD 의 조상이며 ④ CI 가 `GITHUB_SHA` 를 주면 그 커밋의 접두와 같다.
  HEAD 와의 동일성이 아니라 **조상**인 이유: 빌드는 체크아웃보다 오래될 수 있다(빌드 뒤에 한 커밋). CI 는 ④ 가 정확히 같음을 요구한다.
- 단위 시험 `BuildIdentityAndZoomLimitTests`(통합): 스탬프 읽기·없을 때 unknown·csproj 가 표식과 git 부재 내성을 갖는지.
- 이 기계 측정: `A-01 build line: sha=d700e74be0 config=Debug; exe ProductVersion='1.0.0+d700e74be0'`(Mock·Native 모두).

## 2. 줌 상한 — blame 먼저

`git blame`: 상한 16.0 은 **네 곳**(Zoom In 명령 `MainWindowViewModel.ZoomIn`, 설정 속성 `AppSettings.ComparisonZoomScale`, 뷰포트 `ZoomScale` 설정자, 뷰포트 마우스 휠)이고 **전부 같은 커밋 `977df225b`**("대용량 비교 뷰어 및 wrist fixture 구현")에서 한꺼번에 들어왔다. 둘 이상이 필요한 이유를 적은 곳이 없다(커밋 메시지·주석 모두). 값이 설정 파일·바인딩으로도 들어오므로 **설정자의 clamp 자체는 유효한 방어**지만, 숫자를 네 번 쓸 이유는 없었다 — 의도된 이중이 아니라 복제로 판단했다.

고침: `Models/ComparisonModes.cs` 의 `ComparisonZoomLimits.Max = 16.0` 한 곳을 원천으로 네 곳이 읽는다(그 파일은 gui·통합·E2E 시험 프로젝트가 이미 링크하므로 추가 링크 없음). 각 지점의 clamp 는 남겼다(경로가 다르다). **하한은 합치지 않았다**: 명령은 0.05, 휠은 0.01 로 **이미 다르다**(동작 변경이라 범위 밖) — 상수 주석에 적었다.

## 3. Current Workflow Help — 고치지 않음 (리더 결정)

## 4. 반증 (`falsification_arms.txt`, 전부 바이트 동일 복원, 마지막 재빌드 성공)

| 팔 | E2E | 통합 |
|---|---|---|
| Z-a 상수 자체를 160 으로 | Z01 **빨강** | 천장 시험 **빨강** |
| Z-b Zoom In 이 자기 리터럴 160 으로 되돌아감 | Z01 **초록**(다른 복사본이 막음) | 단일 원천 시험 **빨강** |
| Z-c 설정자가 자기 리터럴 160 으로 되돌아감 | Z01 **초록** | 단일 원천 시험 **빨강** |
| A-a About 가 Build 줄을 뺌 | A01 **빨강** | csproj/About 연결 시험 **빨강** |
| A-b 표식이 깨짐(없는 리비전을 물어 unknown) | A01 **빨강** | csproj 시험 **빨강** |

**한쪽만 바꾸면 터지게**는 E2E 로는 안 된다: 네 곳이 같은 상한이면 한 곳이 되돌아가도 나머지가 막아 **행동은 그대로**다(Z-b·Z-c 초록) — 이것이 204 에서 발견한 바로 그 은폐다. 그래서 복제를 직접 겨누는 **원천 단일성 시험**(네 지점이 `ComparisonZoomLimits.Max` 를 읽고 줌 줄에 숫자 리터럴이 없음)을 두었고 Z-b·Z-c 에서 그것이 빨강이다. 이 시험은 소스 텍스트를 읽는다(이름이 아니라 그 줄의 식을 단언).

## 5. 리더가 덧붙인 한 줄

C10 이 호출마다 `AiStatusSummary` 와 진단을 출력한다(`C10 after call 1/2: summary=…`) — 다음 CI 로그에서 "러너는 호출 두 번에 3, 로컬은 2" 를 호출 수와 호출당 계수로 가를 수 있다.

## 미검증

1. CI 에서 `git` 이 빌드 시점에 쓰이는지 — 체크아웃이 얕은(shallow) 경우도 `rev-parse` 는 되지만, 빌드 환경에 git 이 없으면 `unknown` 이 되어 A01 의 ③ 이 건너뛰는 것이 아니라 **④ 와 ① 만** 검사한다(git 불가는 출력에 남김). Release 빌드·배포 번들에서는 확인하지 않았다.
2. About 의 값은 **빌드 시점**의 커밋이다. 작업 트리가 더러울 때(커밋 안 된 변경)를 표시하지는 않는다.
3. 줌 상한 합치기는 동작을 바꾸지 않는다(같은 값)는 것을 E2E Z01 과 통합 650 건으로 확인했으나, 설정 파일에 범위 밖 값이 든 경우의 로드 시험(`CorruptSettingsFileTests` 가 일부 다룸)을 따로 늘리지는 않았다.
4. CI 에서 새 시험(A01 의 git 호출 포함)이 도는지는 푸시 뒤에야 안다.
