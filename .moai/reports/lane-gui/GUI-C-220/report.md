# GUI-C-220 — main 빨강 2건 (줄바꿈 의존 시험, C11 설명 없는 미실행)

Refs #249. 기준 SHA: `21c9f612` (dev/gui). 219 작업 중인 변경은 이 커밋에 넣지 않았다 (명시적 pathspec 2개).

## 1. ModuleSignatureParityControlTests — CRLF 체크아웃에서 실패

### 원인
시험 소스(`Header`, `Cs`)는 원시 문자열 리터럴이라 이 파일이 체크아웃된 줄바꿈을 그대로 가진다. CI 는 `.cs` 를 CRLF 로 받는다(`.gitattributes` 에 규칙 없음). 변형 지점을 `"static class N\n"` 처럼 LF 로 적은 시험 3곳(`Control_ACommentThatMentionsADllImport_IsNotABinding` 외)이 CRLF 소스에서 못 찾았다. 다른 변형 시험은 이미 호출 쪽에서 `Cs.Replace("\r\n", "\n")` 를 했으나 `Mutate` 자체는 정규화하지 않아 일부 호출만 빠져 있었다.

### 수정
- `Mutate` 가 소스를 LF 로 정규화한 뒤 변형 지점을 찾는다 (호출마다 정규화하던 것을 한 곳으로).
- 제품 쪽(시그니처 대조 엔진)이 CRLF 입력을 제대로 읽는지는 별도 시험 `TheEngine_ReadsCrlfSourcesLikeLfOnes` 로 직접 묻는다: 깨끗한 쌍은 CRLF 에서도 0건, 깨진 쌍은 LF 와 CRLF 가 같은 문제 목록을 낸다(엔진이 CRLF 파일에서 아무것도 못 읽으면 두 번째에서 갈린다).

### 반증 (CI 와 같은 CRLF 체크아웃)
`git clone --config core.autocrlf=true` 로 CI 와 같은 체크아웃을 만들어(`.cs` 전부 CRLF 확인: `file` 이 "with CRLF line terminators") 같은 시험을 돌렸다.

| 단계 | 명령 | 결과 |
|---|---|---|
| 수정 전 (HEAD 의 시험 파일) | `dotnet test --filter ModuleSignatureParity` | 실패 1, 통과 38 — `Not found: "static class N\n"` (CI 와 같은 메시지) |
| 수정 후 | 같은 명령 | 통과 40, 실패 0 |
| 수정 후, LF 워크트리 | 같은 명령 | 통과 40, 실패 0 |

### "같은 묶음의 다른 시험" — 전수 확인
CRLF 클론에서 통합 시험 전체를 돌렸다: 통과 808, 실패 1, 건너뜀 19. 실패 1건은 CRLF 와 무관한 **GUI-C-219 변경이 만든 회귀**였다: `NoAppCode_RunsTheOracleInTheAppsProcess_ExceptTheWorkerEntry` 가 준비도 프로브 소스에서 `XpePreprocessOracleProcess.Run(` 를 찾는데, 219 에서 프로브가 공유 판정 보관소(`PreprocessOracleVerdicts`)를 부르도록 바뀌었다. 219 의 일부로 고쳤다(프로브는 보관소를 부르고 보관소의 실행기가 자식 프로세스 호스트인지 단정). 이 커밋에는 넣지 않았다.
CRLF 만으로 갈리는 시험은 위 1건뿐이다(클론 전체에서 줄바꿈 때문에 실패한 것은 이 시험 하나였고 수정 후 사라짐).

한계: 클론은 로컬 빌드 DLL(`clients/ImageProcTest.IntegrationTests/bin/Debug/net8.0`)을 복사해 돌렸다. CI 가 만든 DLL 로 돌린 것이 아니다. 건너뜀 19건은 이 로컬 구성에서 전제가 안 서는 시험이며 CRLF 와 무관하게 LF 워크트리에서도 건너뛴다(LF 워크트리 전체와 건수를 대조하지는 않았다 — 미검증).

## 2. E2E C11 — CI 에서 설명 없이 실행되지 않음

### 로컬과 CI 의 차이 (축 목록)
| 축 | 로컬 | CI | 판정 |
|---|---|---|---|
| `xpe_ai` 모듈 버전 | 일부 구성은 오래된 DLL, `$TEMP/c215_native` 는 현재 트리에서 빌드 | 현재 트리에서 빌드 | **결정적** (아래) |
| 백엔드 | Native | Native | 같음 |
| 관리자 권한 | 아니오 | 예 | 원인 아님: 관리자 아닌 로컬에서도 같은 건너뜀이 재현됨 |
| C10 과의 순서 | 전체 실행 | 전체 실행 | 원인 아님: C11 만 단독으로 돌려도 같은 건너뜀(19 s) |

### 실측
현재 트리에서 빌드한 `xpe_ai` 로 C11 만 단독 실행(`XPE_NATIVE_DIR=$TEMP/c215_native`, `XPE_E2E_BACKEND=Native`): 건너뜀, 메시지 `The module did not count the refused calls, so no worker-off mark can be made here (state 'worker=Active; failures=0; ceiling=3').` — CI 로그의 메시지와 같다. 네 번 호출 모두 `failures=0`.
모듈은 모델 폴더의 파일이 모델이 아닐 때의 거부를 "모델을 쓸 수 없음"(-4)으로 보고하고 세지 않는다(QA-B-195). 세는 것은 워커/전송 결함뿐이고(`modules/ai/src/ai.cpp` 의 "WHAT COUNTS" 주석), 이 앱이 텍스트 파일로 워커를 결함 상태로 만들 방법은 없다. 그래서 이 모듈에서는 C11 의 전제(꺼진 워커 표시)가 원리적으로 성립하지 않는다.

### 처분: (b) 허용 건너뜀
단정을 느슨하게 하지 않았다. 전제가 없으면 건너뛰는 기존 동작을 유지하고 이유 문장 끝에 CI 건너뜀 게이트가 읽는 토큰 `XPE-SKIP-ALLOWED:249` 를 붙였다. 앱의 거부를 세는 모듈에서는 시험이 다시 실행되어 정리(RestoreAiSession)를 검증한다.
검증: 위 명령에 `--logger trx` 를 붙여 trx 를 읽었다 — `outcome="NotExecuted"`, 메시지에 `XPE-SKIP-ALLOWED:249` 포함(CI 게이트가 읽는 형태).

### 리더 몫: ci.yml 허용 목록 행 (`$allowed` 해시테이블)
```
'249' = 'GUI-C-220 C11: the AI module reports a refusal on a not-a-model directory as model-unavailable (-4) and does not count it, so no worker-off mark can be made (QA-B-195)'
```
키 249 는 이슈 번호다(#249). 같은 키를 다른 건너뜀이 쓰면 함께 면제되므로, 별도 이슈를 만들 수 있으면 그 번호로 바꾸는 편이 좋다.

### 관찰 (이번 카드 범위 밖, 수정하지 않음)
`xpe_ai.dll` 이 없는 Native 구성(로컬 스테이징 디렉터리)에서는 C11 이 건너뛰지 않고 실패했다(메뉴 항목이 비활성이라 `requireEnabled` 단정). C10 도 같은 호출을 쓴다. CI 에는 `xpe_ai` 가 있어 영향이 없으나, 그 구성에서 건너뛰는 것이 맞는지는 별도 판단이다.

## 미검증
- CI 가 만든 DLL 로 C11 을 직접 돌리지 않았다(로컬에서 현재 트리 빌드로 같은 메시지를 재현).
- 이 수정 뒤의 CI 실행 결과는 아직 없다.
