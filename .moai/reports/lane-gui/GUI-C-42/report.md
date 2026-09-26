# GUI-C-42 — 출처 기록의 신뢰 경계를 드러낸다 (#98)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head **`83ffa7ae8f85998974a826eed9eedbf1562dd33e`** ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105` · 로컬 HEAD `97e7a78` → 경고 경로 통과.

- 카드: GUI-C-42 · Refs #98 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui` · 병합 `d675625`
- 커밋 2건: `b430d64`(신뢰 경계 + CI 호환 가드) · `97e7a78`(실패 경로 수정) — 미푸시
- **결과: Native E2E 0/13/0/13 · Mock E2E 0/12/1/13 · 통합 0/180/1/181 · slnx 0/0**

---

## 1. CI 실물 대조 — 두 생산자는 만난 적이 없었다

C-41 은 형식을 **메시지로 전달받아** 레인 쪽 산출물로만 검증했다. 이번에 실측한 결과:

- CI 가 `provenance.json` 을 쓴 최신 실행은 **run 34536951123**(head `e0e76cac`)이다.
- 그런데 `git merge-base --is-ancestor 4deddfa e0e76cac` → **false**. 그 기록을 읽는 가드는
  **그 run 의 트리에 아직 없었다.** 그 run 의 Native E2E 는 `Passed: 8, Skipped: 1, Total: 9` —
  가드 없는 9건이다.

**즉 CI 가 쓴 기록을 지금까지 아무도 읽은 적이 없다.** 프로즈로 합의하고 한쪽에서만 검증한 형식은
합의가 아니라 가정이다.

### 대조표 (CI 실물 ↔ 레인 스크립트 산출물)

| 필드 | CI (`source="ci"`, run 34536951123) | 레인 (`source="lane"`, run 34535620953) | 판독기 요구 |
|---|---|---|---|
| `source` | `"ci"` | `"lane"` | 읽되 강제 안 함 |
| `runId` | `"34536951123"` (문자열) | `"34535620953"` (문자열) | **필수** |
| `headSha` | `e0e76cac…` (40자) | `83ffa7ae…` (40자) | **필수** |
| `files[].name` | `xpe_common.dll` 등 5건 | 24건 | **필수(≥1)** |
| `files[].md5` | 소문자 hex | 소문자 hex | **필수** |
| `files[].length` | 정수 | 정수 | 읽음 |
| `headBranch` / `workflow` / `runCreated` / `stagedAtUtc` | **없음** | 있음 | 무시 |
| 대상 범위 | `xpe_*.dll` 만 (5) | `*.dll` + `*.exe` 전부 (24) | — |
| 인코딩 | pwsh 7 `utf8`(BOM 없음) | PS 5.1 `utf8`(BOM 있음) | 양쪽 파싱됨 |

**필수 4필드가 일치한다.** 레인 쪽은 상위집합이고, 판독기는 CI 가 쓰지 않는 필드에 의존하지
않는다. **레인을 고칠 것은 없었다**(카드 1항의 "다르면 레인 쪽을 맞춘다" 조건 미발동).

차이 둘은 기록해 둔다: CI 는 `xpe_*.dll` 만 담고(테스트 exe·gtest 등 제외), BOM 유무가 다르다.
둘 다 `File.ReadAllText` 가 흡수하므로 판독에 영향이 없다 — **이번 실행에서 실제로 읽어 확인했다.**

### 일회성 대조가 아니라 영구 가드로 고정했다

CI 가 찍은 파일을 로그에서 **그대로** 떠 와 `Fixtures/Samples/ci-provenance.json` 으로 커밋하고,
`CiProvenanceCompatibilityTests` 가 판독기로 그것을 읽는다. 네트워크를 쓰지 않는다 — **CI 에 닿지
않을 때 "의견 없음" 을 내는 테스트는 정확히 그때 쓸모가 없다.** ci.yml 이 형식을 바꾸면 이 테스트가
실패하고 샘플을 다시 뜨게 된다.

```
통과!  - 실패: 0, 통과: 3, 건너뜀: 0, 전체: 3
```

## 2. 실패 경로 — 가장 나쁜 상태를 재현하고 고쳤다 (`97e7a78`)

| 시나리오 | 수정 전 | 수정 후 |
|---|---|---|
| 없는 run id (`99999999999`) | `gh run view` 단계에서 throw, **exit=1, 목적지 무변경** | 동일 |
| 복사 중 실패 | **낡은 기록 잔존** | 기록 없음, exit=1 |

**없는 run id**: 헤드 SHA 를 못 얻으면 다운로드조차 하지 않는다. 실측 — `exit=1`,
`provenance.json` 의 `runId` 는 `34535620953` 그대로, `xpe_preprocess.dll` md5 `9f81d2f0…` 그대로.

**복사 중 실패**(카드가 지목한 최악): 목적지에 `xpe_common.dll` **이름의 디렉터리**를 만들어
복사를 실패시켰다. 수정 전:

```
exit=1
남은 provenance runId: "34535620953"        ← 낡은 기록
부분 스테이징: fmt.dll gmock.dll … test_xpe_common.exe (8개) + xpe_common.dll/
```

새 DLL 8개가 깔린 채 **이전 run 을 가리키는 기록이 그대로 남아** 그 파일들을 자기 것이라 주장한다.
가드는 통과한다. 이것이 "반쯤 스테이징된 디렉터리 + 낡은 기록" 이다.

수정: **첫 복사 전에 기존 기록을 지운다.** 같은 조건 재실행 →

```
exit=1
ls: cannot access '/tmp/dest42/provenance.json': No such file or directory
```

기록이 없으므로 가드가 **거부**한다. 조용히 잘못 귀속하는 것보다 낫다.

## 3. 신뢰 경계를 출력이 말한다 (`b430d64`)

가드 출력 **첫 줄**에 인쇄한다:

```
note: this record is written by the staging procedure (ci.yml or Stage-NativeArtifacts.ps1),
not independently verified — there is no signature, so it attests what was staged, not that
the binaries are genuine.
native provenance: source=ci run=34536951123 head=e0e76cac583a7b3afdc2792092b19bed13eb43b6
  …
```

주석이 아니라 출력인 이유: **주석은 파일을 열어야 읽히고, 이 출력을 읽는 사람은 Native 통과의 값을
정하는 중이다.**

## 4. 반증 — 가드 공백이 드러났다 (빌드 결과 포함)

**약화 방식**(지우지 않음, 보강된 규약): `TrustBoundary` 상수를 `""` 로 바꿨다.

**1차 결과: 아무것도 실패하지 않았다.** 신뢰 경계 문장을 통째로 없애도 스위트 전체가 녹색이었다.
즉 그 문장은 **요구사항이 아니라 장식**이었고, 아무도 강제하지 않는 주의문은 다음 편집이 잡음으로
지운다 — 그러면 Native 통과가 조용히 실제보다 강하게 읽힌다.

단언을 추가한 뒤 같은 약화를 재실행:

```
=== 빌드:
    경고 0개
    오류 0개                                    ← 빌드는 정상이다
=== 테스트:
  실패 …CiProvenanceCompatibilityTests.EveryRenderedProvenance_LeadsWithTheTrustBoundary [20 ms]
   The rendered provenance must lead with the trust boundary, not a blank line.
실패!  - 실패: 1, 통과: 11, 건너뜀: 1, 전체: 13
```

**빌드가 0/0 으로 성공한 상태에서 가드 1건만 실패한다** — 컴파일이 깨져 낡은 바이너리로 통과가
위장될 여지가 없다(보강된 규약이 요구한 확인). 원복 후 13/13 재확인.

## 5. 실측 (verbatim)

```
git merge origin/main                                              # → d675625
gh run view 34536951123 --job=<gui-e2e-native> --log                # CI 실물 provenance.json 확보
git merge-base --is-ancestor 4deddfa e0e76cac…                     # → false (가드 미포함)

powershell -File …/Stage-NativeArtifacts.ps1 -RunId 99999999999 -Destination build/ci-common/bin
exit=1 · runId "34535620953" 유지 · xpe_preprocess.dll 9f81d2f0… 유지

powershell -File …/Stage-NativeArtifacts.ps1 -RunId 34536951123 -Destination /tmp/dest42 …
(수정 전) exit=1 · 낡은 runId 잔존 + 8파일 부분 스테이징
(수정 후) exit=1 · provenance.json 없음

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개 / 오류 0개

XPE_E2E_BACKEND=Native XPE_NATIVE_DIR=<staging> dotnet test …E2ETests…   (앱 폴더 비움)
통과!  - 실패: 0, 통과: 13, 건너뜀: 0, 전체: 13 (59 s)

dotnet test clients/ImageProcTest.E2ETests/… -c Debug                    (Mock)
통과!  - 실패: 0, 통과: 12, 건너뜀: 1, 전체: 13 (8 s)

dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
통과!  - 실패: 0, 통과: 180, 건너뜀: 1, 전체: 181
```

Baseline 귀속: E2E 는 C-41 시점 **10건** → **13건**(CI 호환 2 + 신뢰 경계 1). Mock 의 건너뜀 1은
Native 전용 출처 가드다. 통합 181 은 `05b2b1a` 이후 불변 — 이 카드는 통합을 건드리지 않았다.

## 6. 미검증 (Gaps)

- **CI 에서 이 가드들이 실제로 도는 것을 보지 못했다.** 샘플은 CI 로그에서 떠 왔지만, 판독기가
  CI **환경에서** CI 가 쓴 파일을 읽는 실행은 아직 없다(가드가 그 run 에 없었다). 다음 main CI 가
  첫 실행이다.
- **샘플은 한 run 의 것이다.** ci.yml 이 조건부로 다른 모양을 쓰는 경로가 있다면(예: 아티팩트
  없음) 그 경로는 샘플에 없다.
- **md5 이상은 여전히 검증하지 않는다.** 서명이 없고, 손으로 쓴 `provenance.json` 은 그대로
  통과한다 — 이제 그 사실이 출력에 있을 뿐이다. 이것은 **의도된 경계**이지 결함이 아니다.
- **`headSha` 가 그 바이너리를 만든 커밋인지 확인하지 않는다.** `gh run view` 값을 옮겨 적는다.
- **실패 경로 2개만 시험했다.** `gh` 미인증, 아티팩트 없는 run, 디스크 가득참, 다운로드 중 중단은
  시험하지 않았다. 특히 **미인증은 카드가 지목했으나 재현하지 못했다** — 이 세션의 토큰을
  제거하는 것이 다른 작업에 영향을 주므로 하지 않았다.
- **기록에 없는 여분 파일은 여전히 검사 대상이 아니다**(C-41 §6 그대로).

## 7. 잔여 위험 (Residual risk)

- **첫 복사 전 기록 삭제는 성공 경로도 잠깐 무기록 상태로 만든다.** 그 창에서 다른 프로세스가
  Native 스위트를 돌리면 실패한다 — 조용한 오귀속보다 낫다는 판단이고, 로컬 단일 사용자
  환경에서는 관측되지 않았다.
- **신뢰 경계가 매 실행 출력된다.** 반복되면 읽히지 않게 되는 종류의 문장이다. 다만 지워지면
  이제 테스트가 실패하므로, 사라지는 방식으로는 무력화되지 않는다.
- **샘플 픽스처는 사람이 로그에서 떠 온 것이다.** CI 실물과 바이트가 같다고 주장하려면 그
  추출 과정을 믿어야 한다 — `{`…`}` 구간을 통째로 잘라 JSON 파서로 검증했지만, 자동화된
  경로는 아니다.
- 앱 출력 폴더의 네이티브 DLL 조건은 이 카드도 고치지 않았다(C-38 §4, C-41 §6).

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/GitHub CLI:$PATH"
gh run view 34536951123 --json jobs --jq '.jobs[] | select(.name=="gui-e2e-native") | .databaseId'
gh run view 34536951123 --job=<id> --log > /tmp/ci-native.log

export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.E2ETests/… --filter "…CiProvenanceCompatibilityTests" \
       --logger "console;verbosity=detailed"
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
XPE_E2E_BACKEND=Native XPE_NATIVE_DIR="$(pwd)/build/ci-common/bin" dotnet test …E2ETests…
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
```
