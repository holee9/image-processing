# GUI-C-39 — 델리게이트 인자 수 대조 가드 (#138)

- 카드: GUI-C-39 · Refs #138 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건: `188e499` — 미푸시
- **결과: 통합 0/177/1/178 · slnx 0/0 · 반증에서 가드만 실패(1/6)**
- **델리게이트는 5인자 그대로 두었다** — 근거는 §1

---

## 1. 카드 전제가 제 트리·원격과 맞지 않는다

카드는 "Lane A 가 `xpe_calib_generate_offset` 을 6인자로 바꿨다" 를 전제로 델리게이트에
`configJsonOrNull` 추가를 지시했다. **바꾸기 전에 전제를 실측했다.**

```
$ git rev-parse --short origin/main
dc6c855

$ git show origin/main:modules/preprocess/include/xpe/preprocess_api.h | sed -n '237,241p'
XPE_API XpeErrorCode xpe_calib_generate_offset(const XpeImageBuffer* dark_frames,
                                               int32_t num_frames,
                                               float integration_time_ms,
                                               float temperature_c,
                                               const char* output_path);

$ git show origin/main:modules/preprocess/include/xpe/preprocess_api.h | grep -c 'config_json_or_null'
0
```

이 워크트리의 헤더도 동일한 5인자다. 6인자 형태는 **제가 볼 수 있는 어디에도 없다** —
리더의 미푸시 로컬 main 에만 있다(그렇게 통보받았고, 그 자체는 확인할 수 없다).

**그래서 델리게이트를 바꾸지 않았다.** 지금 6인자로 바꾸면:

- 볼 수 있는 유일한 헤더(5인자)와 어긋난다 — 이 카드가 없애려는 바로 그 불일치를 새로 만든다
- 카드가 함께 지시한 소스 대조 가드가 **즉시 실패**한다
- 로컬에서 검증할 수단이 없다 (네이티브 빌드는 이 레인 금지 조항, #98)

반대로 가드만 넣으면 오늘 `5 == 5` 로 통과하고, **6인자 헤더가 병합되는 순간 실패하며
양쪽 개수를 이름으로 지목한다.** 카드가 원한 보호는 그쪽이 온전히 제공한다.

## 2. 델리게이트 ↔ 헤더 인자 수 대조표 (실측)

`XpePreprocessNative.cs` 의 델리게이트 6종 전부. 인자 0개인 것도 포함해 빠짐없이 적는다.

| 델리게이트 | 인자 수 | 네이티브 함수 | 헤더 인자 수 | 판정 |
|---|---|---|---|---|
| `InitDelegate` | 1 | `xpe_preprocess_init(const char* config)` | 1 | 일치 |
| `ShutdownDelegate` | 0 | `xpe_preprocess_shutdown(void)` | 0 | 일치(가드 대상 아님) |
| `VersionDelegate` | 0 | `xpe_preprocess_version(void)` | 0 | 일치(가드 대상 아님) |
| `CorrectionDelegate` | 3 | `xpe_offset_correct` / `xpe_gain_correct` / `xpe_defect_correct` | 3 | 일치 |
| `CalibLoadDelegate` | 1 | `xpe_calib_load_offset` / `_gain` / `_defect_map` | 1 | 일치 |
| `CalibGenerateOffsetDelegate` | 5 | `xpe_calib_generate_offset` | **5** | 일치 (카드 전제는 6) |
| `CalibGenerateGainDelegate` | 5 | `xpe_calib_generate_gain` | 5 | 일치 |

세 그룹은 시그니처가 같아 델리게이트 하나를 공유한다(`CalibLoadDelegate` 3개 export,
`CorrectionDelegate` 3개 스테이지). 가드는 그룹당 대표 1개와 대조한다.

`xpe_calib_save`(QA-A-29 가 3인자로 바꾼 것)는 `RequiredExports` 에는 있으나
**이 파일에 델리게이트가 없다** — 호출하지 않으므로 대조 대상이 아니다.

## 3. 가드 — `PreprocessDelegateArityGuardTests`

두 단언이다.

| 단언 | 무엇을 막나 |
|---|---|
| `DelegateArity_MatchesTheHeader` (Theory, 5쌍) | 헤더가 인자를 늘렸는데 델리게이트가 그대로인 상태 |
| `EveryArgumentTakingDelegate_HasARow` | 새 델리게이트가 대조표에 없어 **무방비로 남는 것** |

두 번째가 없으면 가드 자체에 구멍이 생긴다 — 대조표에 적힌 것만 지켜지고, 새로 추가된
델리게이트는 아무도 보지 않는다.

### 가드가 첫 실행에서 자기 결함을 잡았다

초판은 인자 목록을 `\(([^)]*)\)` 로 떼어냈다. **파라미터 안의 `[MarshalAs(UnmanagedType.LPStr)]`
괄호에서 잘린다.** 실제 출력:

```
CalibGenerateGainDelegate declares 4 parameter(s) but xpe_calib_generate_gain takes 5
These delegates take arguments but have no arity row: InitDelegate
```

`CalibGenerateGainDelegate` 는 5인자인데 4로 셌다. 커버리지 단언은 `InitDelegate` 누락을
잡았다. 괄호 균형 스캐너(`ReadBalancedArguments`) + 깊이 0 쉼표 세기로 교체했고,
`CalibGenerateOffsetDelegate` 가 초판에서 **우연히** 맞았던 것(잘린 조각이 마침 5개)도
이때 드러났다.

## 4. 반증 — 실측 결과가 예상보다 나빴다

`CalibGenerateOffsetDelegate` 에서 `float temperatureC` 를 지우고, 호출부
(`PreprocessCorrectionChainSmokeTests.cs:231`)의 `25.0f` 도 함께 지웠다. **컴파일은 통과한다** —
C# 양쪽이 서로 맞고 헤더만 어긋난, 실제 드리프트와 같은 형태다.

가드만 돌린 결과:

```
CalibGenerateOffsetDelegate declares 4 parameter(s) but xpe_calib_generate_offset takes 5
  in modules/preprocess/include/xpe/preprocess_api.h. …
실패!  - 실패: 1, 통과: 5, 전체: 6, 기간: 25 ms
```

가드 1건만 실패한다. 원복 후 6/6 재확인.

**그런데 같은 상태로 통합 스위트 전체를 돌리자 테스트 호스트가 중단됐다.**

```
테스트 실행이 중단되었습니다.
```

DLL 이 스테이징돼 있어 스모크 테스트가 어긋난 델리게이트를 실제로 호출했고, 단언 실패 없이
프로세스가 죽었다. 첫 시도에서 `통과! 152/153` 으로 보였던 것은 **크래시로 잘린 부분 집계**였다
(정상 전체는 178). 이걸 "통과" 로 읽었다면 반증이 실패한 것으로 잘못 판정했을 자리다.

이 관측 때문에 가드 주석·메시지의 "아무 오류 없이 돈다" 표현을 **실측대로** 고쳤다:
컴파일·링크 오류가 없다는 것까지가 확인된 사실이고, 실행 결과는 진단 불가(중단)이다.

## 5. 실측 (verbatim)

```
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
통과!  - 실패: 0, 통과: 177, 건너뜀: 1, 전체: 178

dotnet test … --filter "FullyQualifiedName~PreprocessDelegateArityGuardTests"
통과!  - 실패: 0, 통과: 6, 건너뜀: 0, 전체: 6

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개 / 오류 0개
```

Baseline 귀속: 직전 커밋 `c0cb7b2` 시점 통합은 **172**(0/171/1/172). 이번 178 = 172 + 가드 6.
증가분은 새 가드 6건뿐이고 기존 171 통과·1 스킵은 그대로다.

## 6. 미검증 (Gaps)

- **6인자 헤더에 맞춘 델리게이트를 만들지도 돌리지도 않았다.** 볼 수 있는 헤더가 5인자이기
  때문이며, 이것이 이 카드에서 완료하지 못한 유일한 항목이다.
- **`configJsonOrNull` 의 마셜링 형태를 확인하지 않았다.** 6인자 헤더가 올라오면 인자 수뿐
  아니라 타입·`[MarshalAs]` 도 봐야 한다 — 가드는 **개수만** 본다.
- **타입 대조는 하지 않는다.** `int32_t` ↔ `int`, `const char*` ↔ `LPStr` 같은 불일치는
  이 가드가 잡지 못한다. 개수가 맞으면서 타입이 틀린 드리프트는 여전히 조용하다.
- **`xpe_calib_save`** 는 델리게이트가 없어 대조 대상에서 빠졌다. 나중에 호출하게 되면
  커버리지 단언이 대조표 추가를 강제한다.
- 헤더를 **텍스트로** 읽는다. 매크로 뒤에 숨은 선언이나 조건부 컴파일 분기는 못 본다.
- CI 반영 결과 미확인.

## 7. 잔여 위험 (Residual risk)

- **가드가 통과해도 ABI 가 맞다는 뜻은 아니다** — 개수만 같으면 통과한다(§6 타입 항목).
  "있다 ≠ 작동한다" 의 이 카드판이다.
- **정규식이 소스 형식에 의존한다.** `public delegate` 선언 형식이나 `XPE_API` 매크로
  표기가 바뀌면 `Assert.Fail`(찾지 못함)로 떨어진다 — 조용히 건너뛰지는 않도록 했다.
- **헤더 경로가 이 레인 소유 밖이다**(`modules/**` 는 Lane A/B). 파일이 이동하면 가드가
  경로 실패로 떨어진다. 스킵이 아니라 실패라서 눈에 띈다.
- 6인자 헤더가 병합되는 순간 이 가드는 **의도대로 빨간불이 된다.** 그때 할 일은
  델리게이트 1줄 추가 + 호출부 `null` 1개이며, 가드가 그 자리를 정확히 지목한다.

## 부록 — 사용한 명령

```bash
git fetch origin main
git show origin/main:modules/preprocess/include/xpe/preprocess_api.h | sed -n '237,241p'
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug --filter "…ArityGuardTests"
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
```
