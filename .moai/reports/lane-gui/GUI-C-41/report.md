# GUI-C-41 — 스테이징 네이티브 아티팩트의 출처를 증거로 남긴다 (#98)

> **이번 실행이 쓴 네이티브**: run **34535620953** (XPE CI Pipeline, main) ·
> head **`83ffa7ae8f85998974a826eed9eedbf1562dd33e`** ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105` ·
> `xpe_common.dll` `2b6fc4e42f8d61289cf4a5be06e44970` ·
> `xpe_display.dll` `55b1763a56405d2db8310d7f424e5242` ·
> `xpe_calib_fixture_gen.exe` `8b9a86e158203dc7edd3630fc9324216` (총 24개).
> 로컬 HEAD `9d256b5` ≠ 위 head SHA → **경고 경로 통과**(실패 아님).

- 카드: GUI-C-41 · Refs #98 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 2건: `9d256b5`(스테이징 스크립트) · `4deddfa`(가드) — 미푸시
- **결과: Native E2E 0/10/0/10 · Mock E2E 0/9/1/10 · 통합 0/180/1/181 · slnx 0/0**

---

## 1. C-40 의 구멍이 이번 실행에서 닫혔다

C-40 은 `xpe_preprocess.dll` md5 `2ddc8be9…`(A-39 **이전** 아티팩트)로 Native 9/9 를 냈다.
이번 스테이징은 `9f81d2f0…`(run 34535620953, head `83ffa7a` = 6인자 병합 커밋)이다.

**즉 C-40 이 "미검증" 으로 남긴 것 — 6인자 DLL 로 실제 실행 — 이 이번에 이루어졌다.**
`CalibGenerateOffsetDelegate` 의 여섯 번째 인자와 `null` 전달이 진짜 6인자 바이너리를 상대로
통과했다(스모크 3건 포함 통합 181건, Native E2E 10건).

다만 이것은 **이번 실행에 한한 사실**이고, 다음 실행이 낡은 아티팩트를 쓰면 다시 조용해진다.
그 재발을 막는 것이 §3 의 가드다.

## 2. 커밋 1 — 스테이징 스크립트 (`9d256b5`)

`clients/ImageProcTest.E2ETests/Staging/Stage-NativeArtifacts.ps1`.
`tools/` 는 lead 소유이므로 이 레인 소유 경계(`clients/**`) 안에 두었다.

형식은 **CI 가 쓰는 것과 동일**하다(leader 통보, ci.yml `21c1e92`) — `provenance.json`,
`source` / `runId` / `headSha` / `files[{name, md5, length}]`, md5 소문자 hex.
`source` 만 다르다: CI 는 `"ci"`, 이 스크립트는 `"lane"`. **읽는 쪽은 하나면 된다.**

실제 산출(발췌):

```json
{
    "source":  "lane",
    "runId":  "34535620953",
    "headSha":  "83ffa7ae8f85998974a826eed9eedbf1562dd33e",
    "headBranch":  "main",
    "workflow":  "XPE CI Pipeline",
    "runCreated":  "2026-09-10T22:05:08Z",
    "stagedAtUtc":  "2026-09-10T22:14:40.5805958Z",
    "files":  [ { "name": "fmt.dll", "md5": "fe69d93c…", "length": 332800 }, … ]
}
```

의도적으로 넣은 것 둘:

- **`Copy-Item -Force`.** C-31 에서 건너뛰는 복사가 낡은 DLL 을 남긴 것을 실측했다.
- **md5 는 모든 복사가 끝난 뒤 목적지 파일마다 한 번씩.** 첫 판은 복사 직후에 해시했는데,
  같은 파일명이 여러 아티팩트에 들어 있어(`xpe_common.dll` 2건) **나중 복사가 이기고 앞선 기록의
  md5 는 디스크의 어떤 것과도 맞지 않게** 됐다. 첫 실행 산출물에서 그게 보여 고쳤다 —
  **틀린 출처 기록은 없는 것보다 나쁘다.**

## 3. 커밋 2 — 가드 (`4deddfa`)

`NativeProvenanceTests.NativeRun_NamesTheBinariesItExercises` + `Fixtures/NativeProvenance.cs`.

| 상태 | 동작 |
|---|---|
| `provenance.json` 없음 / 깨짐 / `runId`·`headSha` 없음 | **실패** |
| 있음 | run id · head SHA · 파일별 md5 를 **테스트 출력에 인쇄** |
| 기록 md5 ≠ 디스크 md5 | 해당 줄에 `(ON DISK … — the file changed after it was staged)` |
| head SHA ≠ 현재 커밋 | **경고 후 통과** |
| `XPE_E2E_BACKEND` ≠ Native | Skip |
| `XPE_NATIVE_DIR` 미고정 | 실패 (#129 — 고정하지 않으면 로더가 고르고, 어떤 기록도 그걸 설명하지 못한다) |

**앱을 띄우지 않는다.** 대상은 GUI 가 아니라 디렉터리이고, `ApplicationFixture` 안에서 던지면
출처 판정 하나 때문에 시나리오 9건이 함께 죽어 원인이 묻힌다.

**SHA 불일치를 실패로 만들지 않은 이유**: #98 상 최신 아티팩트가 항상 있을 수 없다. 실패시키면
레인이 배우는 것은 "파일을 지우면 통과한다" 뿐이다. 판단은 읽는 쪽 몫이고, 이 가드는 판단을
**가능하게** 만드는 데까지다.

### 실제 출력 (verbatim, `--logger console;verbosity=detailed`)

```
native provenance: source=lane run=34535620953 head=83ffa7ae8f85998974a826eed9eedbf1562dd33e
  directory: D:/workspace-github/xpe-gui/build/ci-common/bin
  fe69d93cf5a77487c99030f70751bc78  fmt.dll
  …
  2b6fc4e42f8d61289cf4a5be06e44970  xpe_common.dll
  55b1763a56405d2db8310d7f424e5242  xpe_display.dll
  9f81d2f07fb7f7e23e90c86332467105  xpe_preprocess.dll
  8b9a86e158203dc7edd3630fc9324216  xpe_calib_fixture_gen.exe
WARNING: these binaries were built at 83ffa7ae8f85998974a826eed9eedbf1562dd33e, not at the commit
under test (9d256b5ba3a0c6e34d593044719d9a0cad7c1c8e). Native results describe THAT code, not
necessarily this working tree (#98 — this lane does not build native binaries).
```

경고 경로가 **실제로 탔고**(로컬 커밋이 미푸시라 SHA 가 다르다) 통과는 유지됐다.

## 4. 반증

`provenance.json` 만 지웠다:

```
실패 …NativeProvenanceTests.NativeRun_NamesTheBinariesItExercises [1 ms]
  D:/…/build/ci-common/bin\provenance.json is missing. A Native run has to name the binaries it
  exercised: stage them with clients/ImageProcTest.E2ETests/Staging/Stage-NativeArtifacts.ps1
  -RunId <id> -Destination <dir>, which writes this file.

실패!  - 실패: 1, 통과: 9, 건너뜀: 0, 전체: 10
```

10건 중 **이 가드 1건만** 실패한다. 원복 후 10/10 재확인.

첫 반증 시도는 **2건**이 실패했다 — `S05_RuntimePanel_ShowsBackendVersion` 이 함께 떨어졌는데,
앱 출력 폴더에 네이티브 DLL 3개가 남아 있어 탐색 순서상 그쪽이 먼저 잡힌 것이다(C-38 §4 에
기록한 조건, #129 정책대로). 조건을 맞춘 뒤 다시 재서 1건으로 확정했다. **2건을 그대로 적었다면
"가드만 실패" 라는 주장이 실측과 어긋났을 자리다.**

## 5. 실측 (verbatim)

```
powershell -File clients/ImageProcTest.E2ETests/Staging/Stage-NativeArtifacts.ps1 \
           -RunId 34535620953 -Destination build/ci-common/bin
Staged 24 file(s) …  run 34535620953 (XPE CI Pipeline, main)
                     head 83ffa7ae8f85998974a826eed9eedbf1562dd33e

XPE_E2E_BACKEND=Native XPE_NATIVE_DIR=<staging> dotnet test …E2ETests…    (앱 폴더 비움)
통과!  - 실패: 0, 통과: 10, 건너뜀: 0, 전체: 10 (1 m 3 s)

dotnet test clients/ImageProcTest.E2ETests/… -c Debug                     (Mock)
통과!  - 실패: 0, 통과: 9, 건너뜀: 1, 전체: 10 (8 s)

dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
통과!  - 실패: 0, 통과: 180, 건너뜀: 1, 전체: 181

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개 / 오류 0개
```

Baseline 귀속: E2E 는 C-40 시점 **9건** → 이번 **10건**(가드 1건 추가). Mock 의 건너뜀 1은 그
가드가 Native 전용이라 스킵된 것이다. 통합 181(0/180/1/181)은 `05b2b1a` 시점과 동일 —
이 카드는 통합 테스트를 건드리지 않았다.

## 6. 미검증 (Gaps)

- **CI 가 쓴 `provenance.json` 을 읽어 보지 못했다.** 형식은 leader 통보(ci.yml `21c1e92`)에
  맞췄고 이 레인이 만든 `source="lane"` 파일로만 검증했다. `source="ci"` 실제 산출물과의
  일치는 CI 실행에서 확인된다.
- **md5 이상의 검증은 하지 않는다.** 파일이 그 run 에서 나왔다는 것은 **기록을 믿는 것**이지
  서명으로 확인한 것이 아니다. 손으로 쓴 `provenance.json` 도 가드를 통과한다.
- **`headSha` 가 그 아티팩트를 실제로 만든 커밋인지 확인하지 않는다.** `gh run view` 가 준 값을
  그대로 적는다.
- **스크립트를 실패 경로로 돌려 보지 않았다** — 존재하지 않는 run id, 아티팩트 없는 run,
  `gh` 미인증 상태는 시험하지 않았다(성공 경로 1회만 실행).
- **디렉터리에 기록되지 않은 파일이 있어도 가드는 말하지 않는다.** 기록된 파일이 디스크와
  다른 것은 잡지만, **기록에 없는 여분의 DLL** 은 검사 대상이 아니다.
- 앱 출력 폴더의 네이티브 DLL 은 여전히 로컬 Native 측정을 좌우한다(§4). 이 카드는 그것을
  고치지 않았다 — 출처 기록의 대상은 `XPE_NATIVE_DIR` 이다.

## 7. 잔여 위험 (Residual risk)

- **경고는 무시할 수 있다.** 설계상 그렇게 만들었다(#98). 경고가 계속 뜨는 상태가 정상으로
  굳으면 C-40 과 같은 오독이 다시 생길 수 있다 — 다만 이번에는 보고서 첫 줄에 SHA 가 있으므로
  오독이 **기록에 남는다**.
- **`source="lane"` 은 사람이 만든 기록이다.** CI 가 쓴 것보다 신뢰도가 낮다는 사실을 필드가
  드러내지만, 가드는 둘을 동등하게 통과시킨다.
- **가드가 Native 전용이다.** Mock 실행은 스킵되므로, Mock 만 도는 CI 잡은 이 검사를 전혀 받지
  않는다 — 의도한 범위이지만, "스킵 = 검사됨" 으로 읽히지 않도록 이 문장을 남긴다.
- 앞으로 아티팩트 이름이 바뀌면 스크립트의 기본 `-Artifacts` 목록이 조용히 0건을 받는다 —
  그 경우 스크립트가 `throw` 하도록 해 두었다(`No .dll or .exe was found`).

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/GitHub CLI:$PATH"
gh run list -L 8 --json databaseId,headSha,workflowName,conclusion
powershell -NoProfile -ExecutionPolicy Bypass \
  -File clients/ImageProcTest.E2ETests/Staging/Stage-NativeArtifacts.ps1 \
  -RunId 34535620953 -Destination build/ci-common/bin

export PATH="/c/Program Files/dotnet:$PATH"
XPE_E2E_BACKEND=Native XPE_NATIVE_DIR="$(pwd)/build/ci-common/bin" \
  dotnet test clients/ImageProcTest.E2ETests/… --logger "console;verbosity=detailed"
dotnet test clients/ImageProcTest.E2ETests/… -c Debug          # Mock
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
```
