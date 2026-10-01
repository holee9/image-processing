# GUI-C-184 — #225 행 10 AI 메뉴 1차 (bone suppression 만)

레인: gui · 이슈: `#225`, `#130` · 시작 전 `origin/main`(`37a980a3`, QA-B-171C 의 `85dc2f4f` 포함)을 `dev/gui` 에 병합했다. 푸시 없음(푸시는 리더).

## 결론

행 10 이 **AI bone suppression 하나**를 실행하는 메뉴로 켜졌다. GUI-C-178 에서 채택한 설계대로이고, 가장 중요한 규칙 — **실패한 호출의 출력은 입력과 같으므로, 성패는 반환 코드로만 가른다** — 을 시험으로 고정했다. **네이티브 호출 자체(`xpe_ai.dll` 로드·버퍼·`xpe_bone_suppress`)는 이 트리에서 한 번도 실행되지 않았다**(컴파일만) — §5 의 1번.

| 반환 코드 | 단계 상태 | 화면에 나오는 것 |
|---|---|---|
| `0` 이고 화소가 달라짐 | `Applied` | 체인 상태줄에 `AI-processed: bone suppression` |
| `0` 이고 화소 같음 | `AppliedNoChange` | 라벨 **없음** |
| `-3` 등 시도했으나 실패 | `RequestedNotApplied` | `AI bone suppression NOT applied (code -3): the original image is shown. …`, 화면은 입력 영상 |
| `-1`·`-6`·`-7` 시도 전 거절 | `RequestedNotApplied` | `AI bone suppression not attempted (code -7: …)` — "실패"와 따로 |
| `xpe_ai_init` 실패 / DLL 없음 | `RequestedNotApplied` | `not started: …` |
| Mock 백엔드 | `RequestedNotApplied` | `AI bone suppression requires the native backend (xpe_ai.dll).` |

## 1. 설계 반영 (리더 지시 대조)

- **새 StageId** `ai_bone_suppress`, 체인 순서 preprocess → gsvg → AI → 표시 파이프라인. 설정 `AiBoneSuppressionInChain`(기본 꺼짐)과 `AiModelDirectory`(기본 `data/models`, 빈 값은 기본으로 — 모듈이 NULL 경로를 거절하므로).
- **interop**: `XpeAiNative`(`xpe_ai_init` / `xpe_ai_shutdown` / `xpe_bone_suppress`), 설정 JSON `{"use_worker": true}` 는 객체를 직렬화해 만든다(키 철자가 틀리면 모듈이 조용히 무시해 **모델이 호스트 프로세스에서 돌게 되므로** — REQ-AI-003).
- **ushort ↔ float32**: 입력 ÷65535, 출력 ×65535 로 되돌리며 [0,1] 로 자름. **가정이다**(§5-2).
- **rc == 0 → Applied, rc != 0 → RequestedNotApplied.** 실패 호출의 출력 버퍼는 `rc == 0` 일 때만 읽는다. 체인 러너의 `SequenceEqual` 경로를 타지 않도록 비-0 은 `Ran=false`, 화소 없음으로 돌려준다. **이를 단언하는 시험**: 같은 화소를 입력으로 되돌려 주는 `-3`·`-4`·`-9` 는 `RequestedNotApplied` 이고 화면은 원본(`DisplaysRaw`), 같은 화소의 `rc=0` 은 `AppliedNoChange`(대조군).
- **`AI-processed` 라벨은 상태 `Applied` 에서만**: `AiBoneSuppressionStage.LabelFor(chain)` 이 상태에서 파생한다. 알림 문구나 메시지에서 읽지 않는다. 성공 메시지 자체에도 라벨이 없다(`AppliedNoChange` 일 때 그 메시지가 화면에 나오므로).
- **`-7` 은 시도 안 함으로 따로**: 지시는 `-7` 뿐이었으나 `ai.cpp` 를 읽어 보니 `-1`(입력/크기 거절)·`-6`(초기화 안 됨)도 워커 경로에 들어가기 **전**의 검증이라 같은 부류로 묶었다(**내 확장** — 틀렸으면 `Classify` 한 줄). 지금 main 에서는 너무 큰 영상이 `-7` 이 아니라 `-1` 이다(검증기가 거른다) — 설계 보고서의 "-7 = 너무 큼"은 그 시점 헤더 기준이었다.
- **init/shutdown 은 한 곳·한 잠금**(Codex #17): `GuiAiSession` 의 `WithLock` 아래에서만 호출한다. 다음 카드의 상태 폴링·복구 버튼은 같은 `WithLock` 을 쓰면 된다. 세션은 **실행마다 시작·종료하지 않는다**(`xpe_ai_init` 이 멱등이고 워커의 연속 실패 횟수가 세션 소속이라 매번 재시작하면 그 횟수를 가린다). 종료는 백엔드 `Shutdown()` 에서, 이 프로세스가 시작했을 때만(끝내려고 DLL 을 새로 읽지 않는다).
- **툴팁을 실제 범위로**, 헤더를 `Run _AI Bone Suppression` 으로. AutomationId `RunFullPipelineMenuItem` 은 유지(시험·세 축 개수가 키로 씀). 명령은 영상이 없으면 안내만 한다.
- **스위치가 보이도록**: Parameters 탭에 `AI bone suppression` 체크박스(`AiBoneSuppressionInChainCheckBox`)를 추가했다. 메뉴는 켜기만 하므로, 이것이 없으면 한 번 켠 단계를 UI 로 끌 방법이 없었다.
- 세 축 개수 **5 → 4**(이름 목록 · 트리 순회 · XAML 파싱), A11 이 4=4 로 통과.

## 2. 시험

| 시험 | 단언 |
|---|---|
| `AiBoneSuppressionStageTests` 22건 | 위 규칙 전부(실패 코드 3종×(입력 반환 → RequestedNotApplied), rc=0 과 라벨, 라벨이 다른 상태·다른 단계에 안 붙음, 성공 메시지에 라벨 없음, 반환 코드 분류, 시도 전 거절 문구 vs 실패 문구, rc=0 이지만 NaN 출력은 결과 아님, 초기화 실패 문구, 65536개 전부의 척도 왕복, 범위 밖 자름, 계획 순서·기본 꺼짐, 설정 복사·빈 모델 경로, **init/shutdown 호출이 소스에서 정확히 한 곳(GuiAiRunner.cs)이며 앞에 `WithLock(` 이 있음**) |
| E2E **C-08** | 모듈이 성공할 수 없는 빌드에서 메뉴를 호출하면 `ai_bone_suppress=RequestedNotApplied`, 화면의 그려진 화소 해시가 이전과 같음, `AI-processed` 없음, 사유 문구. Native 잡이 추론 빌드를 올리면 일부러 빨강(성공 경로는 모델이 있는 별도 시험 필요) |
| 기존 시험 갱신 3건 | `Plan_FollowsTheSettings`(단계 3개, AI 는 꺼짐), `Collection_FindsTheKnownBindings`(40→41, 새 체크박스 이름을 명시), 러너 `gui/ImageProcTest.E2E` 의 "Full pipeline menu must be disabled" 단언 → "활성이고 명령이 있고 헤더가 AI Bone Suppression" |

로컬: `Functional` **269 통과 · 건너뜀 1(기존) · 실패 0**. `AutomationReportBackendTests` 16 통과 · 건너뜀 1(A03 기존) · 실패 0(A06 이 러너를 실행해 위 단언 변경을 확인). `ProcessingChainScenarios`(Mock + 번들 합성 1024² 영상) 5 통과 · 건너뜀 4(네이티브 전용): **C-08 통과** — `chain: …, ai_bone_suppress=RequestedNotApplied … — ai_bone_suppress: AI bone suppression requires the native backend (xpe_ai.dll).`, 해시 `493639b21667e13b` 이전과 동일.

## 3. 반증 (실제 소스, 7팔)

| 팔 | 망가뜨린 것 | 빨강이 된 시험 |
|---|---|---|
| failure-code-minus3-read-as-success | `-3` 을 성공으로 분류 | **입력을 되돌려 주는 실패가 AppliedNoChange 로 읽히는 시험** 외 2 |
| label-also-given-to-AppliedNoChange | 라벨을 AppliedNoChange 에도 | 라벨 시험 |
| success-message-carries-the-label | 성공 메시지에 라벨 | 메시지 시험 |
| minus7-read-as-a-failed-attempt | -7 을 실패로 | 분류 + 시도 전 거절 문구 |
| ai-stage-ignores-its-setting | 설정과 무관하게 켜짐 | 계획 시험 |
| shutdown-called-outside-the-one-place | 한 곳 밖에서 `xpe_ai_shutdown` 호출 추가 | 호출 위치 시험 |
| row-10-menu-disabled-again | XAML 에 `IsEnabled="False"` 복원 | **A11**(세 축 불일치) |

모든 팔에서 빌드 성공, 소스 바이트 동일 복구, 복구 후 22/22 (`falsification_arms.txt`).

## 4. 범위 밖으로 둔 것 (지시대로)

"이번 세션 비활성"의 영속 표시와 복구 버튼(post 의 상태 조회 API 대기), 입력 스케일 계약(post 확인 대기). 실패 3번째 이후엔 호출마다 `-3` 과 "NOT applied" 문구가 나오지만 **"3회 실패로 이번 세션 비활성"이라는 사실은 화면이 모른다**(모듈이 알림을 더 올리지 않는다). 복구는 앱 재시작뿐이다.

## 5. 미검증 · 한계

1. **네이티브 호출은 한 번도 실행되지 않았다.** `GuiAiRunner`(P/Invoke, `xpe_alloc_image` 버퍼 복사, `xpe_bone_suppress` 호출, `xpe_free_image`)와 `GuiAiSession` 은 컴파일만 됐고 로컬에 `xpe_ai.dll` 을 빌드·로드하지 않았다(네이티브 빌드 실행은 CI 몫). C-08 은 Mock 에서만 관측했다 — Native 의 `-3`/DLL 없음 경로는 **CI 에서 처음 관측된다**. `ci.yml` 은 `xpe_ai.dll` 을 Native E2E 에 올리지 않는 것으로 읽혔다(329행 주석이 워커 exe 를 언급할 뿐 스테이징 줄은 못 찾음 — 확인한 범위는 `xpe_ai` 이름 검색 한 번). 그렇다면 CI 에서는 `-3` 이 아니라 **"DLL 을 찾지 못함"** 이 관측된다. C-08 은 둘 다 통과하도록 썼다.
2. **입력 스케일은 가정이다**: ÷65535 / ×65535, [0,1]. 헤더는 float32 만 요구하고 SDD 는 "normalize to [0,1]" 이라 적되 누가 하는지 말하지 않는다. 주석(`AiBoneSuppressionStage` 머리말)에 가정으로 적었다. 실제 모델에서 이 척도가 맞는지는 모른다. 스텁 빌드에서는 추론이 없어 이 척도의 성공 경로가 **한 번도 관측되지 않는다**(시험은 값 변환만 확인).
3. **모델 경로 기본값 `data/models`** 가 실제로 존재하는지·`bone_suppress.onnx` 가 들어갈 자리인지 확인하지 않았다. 스텁 빌드는 경로를 열지 않는다(헤더).
4. **모듈이 올리는 알림**(성공 Info, 실패 Warning)이 `InvokeNative` 의 알림 배출로 화면 알림 목록에 오는 것은 관측하지 않았다 — 기존 배출 경로를 그대로 쓴다는 읽기뿐.
5. **흉부 한정을 적용하지 않았다.** SDD 는 PA/AP 흉부에만 적용하고 그 밖은 입력을 돌려주라 하는데(GUI-C-178 §4-9, 리더 결정 대기), 이 단계는 **어느 부위에서든** 실행을 시도한다.
6. 라벨은 체인 상태줄 텍스트에만 나온다. 전용 UI 요소는 없고 화면에서 읽히는 모양은 관측하지 않았다(C-08 은 실패 경로라 라벨이 **없음**만 확인).
7. `AiBoneSuppressionInChain` 은 설정 파일에 저장되는 값이다(`PreprocessInChain` 과 같음). 한 번 켜면 다음 실행에도 켜져 있고 실패하는 빌드에서는 렌더마다 경고가 쌓일 수 있다 — 이 동작은 관측하지 않았다.
8. 호출 위치 시험은 **소스 텍스트**를 읽는다. 리플렉션이나 다른 어셈블리의 호출은 보지 못한다(시험 주석에 적음).

## 증거 파일

`falsification_arms.txt` · `local_runs.txt` · `text_lint.txt`


---

## 추가 지시(리더) — C-08 의 Native 갈래를 좁힌다

리더가 아티팩트를 직접 받아 `xpe-ci-post-binaries` 에 `xpe_ai.dll`·`xpe_ai_worker.exe` 가 있고 그대로 `build/e2e-native-dlls` 로 스테이징됨(ci.yml 772)을 확인했다. 따라서 위 §5-1 의 "ci.yml 에서 스테이징 줄을 못 찾았다"는 **틀린 추정이었고(검색 범위가 이름 한 번이었다)**, Native 에서는 DLL 이 로드되고 스텁이라 `-3` 이 관측될 것이다. 그러면 C-08 이 "DLL 없음"도 통과시키던 갈래는 DLL 스테이징을 빠뜨린 잡을 **실패 경로로 통과**시킬 수 있다.

- C-08 Native: 사유에 **모듈이 돌려준 반환 코드**가 있어야 한다 — `AI bone suppression (NOT applied|not attempted) (code N`. `was not found`(DLL 없음)와 `not started`(init 거절)는 받지 않는다. Mock 은 그대로(`requires the native backend`).
- 그 문구를 시험으로 고정: `TheMessagesOfAModuleAnswer_MatchTheNativeCasePattern` — 반환 코드 6종(-3 -4 -9 -1 -6 -7)의 메시지는 패턴에 맞고, init 거절과 DLL 없음 문구는 맞지 않는다(패턴은 C-08 에 복사돼 있고, DLL 없음 문구는 이 프로젝트가 링크할 수 없는 `GuiAiRunner.cs` 에서 복사했다고 시험 주석에 적음).
- 반증: 모듈 응답 메시지의 `(code N)` 를 `[code N]` 으로 바꾸면 위 시험과 `NotAttempted_IsSaidApartFromFailed` 가 빨강, 복구 확인.
- 로컬: `AiBoneSuppressionStageTests` 23/23, C-08 은 Mock 에서 통과(`requires the native backend`). **Native 갈래는 이 트리에서 실행하지 못했다** — CI 의 `gui-e2e-native` 가 처음 실행한다. 정말 `-3` 이 나오는지(스텁이 `init` 을 통과시키는지, 워커 경로가 스텁에서 어떻게 답하는지)는 그때 관측된다.

🗿 MoAI
