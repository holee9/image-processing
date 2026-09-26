# GUI-C-10 — 부족 `dataSize` 호출 → 네이티브 INVALID_INPUT 회귀 케이스

- 카드: GUI-C-10 · Refs #123 · 커밋 `5c2e6bf` (미푸시)
- 워크트리: `D:/workspace-github/xpe-gui` / `dev/gui` / HEAD `0247b51` (main 병합 완료, `.claude` 변경 0건)
- **상태: 구현 완료. 단, 핵심 2건은 이 레인에서 검증 불가 — §4·§5**

---

## 1. 무엇을 만들었나

`Functional/DataSizeContractTests.cs` — api-spec "XpeImageBuffer.dataSize on input"(#123)
세 절을 클라이언트 쪽에서 관측하는 6건. 축은 GUI-C-09 조사 표에서 클라이언트가 실제로
호출하는 것 중 골랐다.

| 축 | 진입점 | 형식 | 케이스 |
|---|---|---|---|
| preprocess | `xpe_offset_correct` | UINT16 | 정확 / 짧음 / 0 |
| enhance_basic | `xpe_log_transform` | FLOAT32 | 정확 / 짧음 / 0 |

`PInvoke/XpeEnhanceBasicNative.cs` — 테스트 프로젝트에 enhance_basic P/Invoke 표면이
없어 신설했다. `XpePreprocessNative` 와 같은 형태(정적 `DllImport` 없음, 부재 시 스킵).

설계 두 가지를 기록한다.

- **뒷받침 할당은 항상 전량이고 선언된 `dataSize` 만 짧다.** 가드를 지나친 호출은 자기
  할당 안을 읽으므로, 실패가 메모리 손상이 아니라 반환코드로 드러난다
- **반환코드만 단언한다.** 픽셀 값 동작은 다른 곳 소관이고, 현재 실패 중인 #117 계열과
  얽히지 않게 하려는 것이다

## 2. 기대치 정정 1건 — `offset_correct` 의 "통과" 절

처음에 "정확한 `dataSize` → `OK`" 로 썼는데 실측이 `NOT_INITIALIZED` 였다.
**내 기대치가 틀렸다**: 캘리브레이션이 로드되지 않으면 `xpe_offset_correct` 는 다음
게이트에서 멈춘다(`offset_correct.cpp:315` `if (!g_calib.offset_map) return NOT_INITIALIZED`).

네이티브에서 **크기 검사(`:305`)가 캘리브레이션 검사(`:315`)보다 앞선다.** 따라서
`OK` 든 `NOT_INITIALIZED` 든 "크기 게이트가 거절하지 않았다"는 동일한 관측이고,
`INVALID_INPUT` 만이 거절이다. 그래서 두 값을 허용하는 `AssertPassedSizeGate` 로 바꿨다
— "`INVALID_INPUT` 이 아니다" 같은 약한 단언(어떤 실패든 통과)은 피했다.

## 3. 실측

```
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
실패! - 실패: 8, 통과: 88, 건너뜀: 0, 전체: 96
```

| 구분 | 결과 |
|---|---|
| 전체 케이스 | 90 → **96** (신규 6) |
| 신규 6건 | **4 통과 / 2 실패** |
| 기존 실패 | **6건, 변동 없음** (8 − 신규 2) |

통과한 4건: `OffsetCorrect_ExactDataSize_PassesSizeGate`, `OffsetCorrect_ZeroDataSize_PassesSizeGate`,
`LogTransform_ExactDataSize_IsAccepted`, `LogTransform_ZeroDataSize_IsAccepted`.
실패한 2건: `OffsetCorrect_ShortDataSize_ReturnsInvalidInput`, `LogTransform_ShortDataSize_ReturnsInvalidInput`.

로그: `step1-tests.log`(정정 전), `step2-tests.log`(정정 후)

## 4. 실패 2건의 원인 — 로컬 바이너리가 가드보다 오래됐다

추론이 아니라 두 갈래로 확인했다.

**행동 증거.** 짧은 `dataSize` 로 `xpe_offset_correct` 를 부르면 소스상 `INVALID_INPUT`
이 먼저 나와야 하는데(§2 의 순서), 실측은 `NOT_INITIALIZED` 였다. 크기 검사 자체가
바이너리에 없다는 뜻이다. `xpe_log_transform` 은 `OK` 를 돌려줬다 — 역시 거절이 없다.

**시각 증거.**

| 항목 | 시각 |
|---|---|
| `enhance_basic` 가드 커밋 `2dc9cb6` | 2026-09-10 07:27 |
| `preprocess` 가드 커밋 `258b045` | 2026-09-10 08:28 |
| 로컬 `xpe_preprocess.dll` 최신본 | **2026-08-18 17:29** |
| 로컬 `xpe_enhance_basic.dll` 최신본 | **2026-08-18 17:18** |

`build/` 전체를 훑어 가드 커밋 이후(`-newermt "2026-09-10 08:00"`) 빌드된 두 DLL 은
**0건**이다. 이 레인은 네이티브 빌드가 금지이므로 **여기서는 검증할 수 없다.**

**반증 조건**: 최신 main 을 빌드한 바이너리로 실행했을 때도 이 2건이 실패한다면 위 진단이
틀린 것이고, 그때는 테스트가 아니라 가드 쪽을 봐야 한다.

## 5. CI 도 enhance_basic 3건은 판정하지 못한다 — 보고 사항

`ci.yml` 의 `dotnet-tests` 잡은 아티팩트를 **2개만** 내려받는다(`:182-191`):
`xpe-ci-common-binaries`, `xpe-ci-preprocess-binaries`. **`xpe_enhance_basic.dll` 은
스테이징되지 않는다.**

결과적으로 CI 에서는:

| 케이스 | CI 예상 |
|---|---|
| `OffsetCorrect_*` 3건 | 실행됨 — 최신 preprocess 바이너리로 판정 가능 |
| `LogTransform_*` 3건 | **스킵** — DLL 부재 |

즉 **enhance_basic 의 짧은 `dataSize` 케이스는 로컬에서도(구 바이너리) CI 에서도(DLL 부재)
관측되지 않는다.** 해소하려면 `ci.yml` 이 `ci-post` 아티팩트도 같은 폴더로 내려받아야
하는데, `ci.yml` 은 main 소유이고 GUI-C-08 에서 "수정 금지" 로 지정됐다 — **leader 판정
사안**이다. 테스트 자체는 DLL 이 스테이징되는 순간 그대로 작동한다.

## 6. 미검증 (Gaps)

- **짧은 `dataSize` 2건이 실제로 `INVALID_INPUT` 을 받는지 관측하지 못했다.** §4 의 이유다.
  "가드가 작동한다"는 주장은 하지 않는다 — 네이티브 단위테스트(`test_datasize_guard.cpp`)가
  그것을 증명하고 있고, 이 카드는 그 계약이 **클라이언트 마샬링 경로에서도** 성립하는지를
  묻는 것이다. 그 답은 CI 가 낸다
- 카드가 적은 "현재 CI 3 실패 = #117" 과 로컬 기존 실패 6건은 수가 다르다. 로컬 6건은
  2026-08-18 바이너리 기준이라 CI 와 직접 비교할 수 없다(GUI-C-08 §6 와 같은 한계).
  **이 카드가 기존 실패 수를 바꾸지 않았다는 것만 관측했다**(6 → 6)
- `xpe_offset_correct` 외의 preprocess 2함수, `xpe_log_transform` 외의 enhance_basic
  6진입점은 클라이언트 쪽에서 다루지 않았다. 네이티브 단위테스트가 진입점별로 덮는다
- Release 구성 미빌드, WPF 앱 미실행

## 7. 잔여 위험

- **로컬에서 실패하는 테스트 2건을 커밋했다.** 근거는 §4 이고 반증 조건도 적었지만,
  진단이 틀렸다면 CI 가 2건 더 빨개진다. leader 가 main 병합 전에 보류를 택할 수 있도록
  이 사실을 보고 첫머리에 둔다
- `AssertPassedSizeGate` 가 `NOT_INITIALIZED` 를 허용하므로, 언젠가 캘리브레이션이
  자동 로드되면 이 두 케이스는 `OK` 를 받아도 계속 통과한다 — 의도한 설계지만, 크기 게이트
  이후의 동작 변화는 이 테스트가 잡지 못한다

## 부록 — 사용한 명령

```bash
git merge origin/main                              # → 0247b51
cp "…/build/ci-post/bin/xpe_enhance_basic.dll" build/ci-common/bin/
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
find "…/build" -name "xpe_preprocess.dll" -o -name "xpe_enhance_basic.dll" -printf "%TY-%Tm-%Td …"
git log -1 --format="%h %ad" --date=iso -- modules/preprocess/src/offset_correct.cpp
```
