# GUI-C-40 — `CalibGenerateOffsetDelegate` 6인자 반영 (C-39 보류분 해제, #138)

- 카드: GUI-C-40 · Refs #138 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 병합: `git merge origin/main` → **83ffa7a** (fast-forward, dc6c855→83ffa7a 35커밋)
- 커밋 1건: `05b2b1a` — 미푸시
- **결과: C-39 가드 RED→GREEN · 통합 0/180/1/181 · slnx 0/0**

---

## 1. 전제 재실측 — 이번에는 성립한다

merge 직후 헤더를 다시 쟀다.

```
$ grep -n -A7 "XPE_API XpeErrorCode xpe_calib_generate_offset(" modules/preprocess/include/xpe/preprocess_api.h
257:XPE_API XpeErrorCode xpe_calib_generate_offset(const XpeImageBuffer* dark_frames,
258-                                               int32_t num_frames,
259-                                               float integration_time_ms,
260-                                               float temperature_c,
261-                                               const char* output_path,
262-                                               const char* config_json_or_null);

$ grep -c 'config_json_or_null' modules/preprocess/include/xpe/preprocess_api.h
3
```

C-39 시점의 실측은 같은 명령으로 5인자 · `grep -c` 0 이었다. **같은 명령, 다른 결과** — 전제가
실제로 바뀐 것이고, C-39 의 보류는 그때 볼 수 있는 상태에 대해 옳았다.

헤더가 정의하는 여섯 번째 인자의 의미(그대로 인용):

> `@param config_json_or_null` Generation parameters as JSON, or NULL for the defaults. **NULL
> behaves exactly as this function did before the parameter existed** (QA-A-39, #138), so an existing
> caller that passes NULL sees no change.

`null` 을 넘기는 근거가 이 문장이다 — 기존 측정 동작을 유지한다는 것을 **헤더가 보증**한다.
`"mean"` 을 문자열로 박아 넣지 않은 이유이기도 하다: 기본값 정의가 바뀌면 헤더 쪽이 바뀌어야지,
호출부가 그 시점의 기본값을 복사해 둘 자리가 아니다.

## 2. 가드 RED → GREEN — 이 카드의 핵심

델리게이트를 **건드리기 전에** 먼저 돌렸다.

### RED (변경 전, merge 직후)

```
$ dotnet test … --filter "FullyQualifiedName~PreprocessDelegateArityGuardTests"

CalibGenerateOffsetDelegate declares 5 parameter(s) but xpe_calib_generate_offset takes 6
  in modules/preprocess/include/xpe/preprocess_api.h. A delegate is bound by name, so this
  mismatch raises no compile or link error — the callee reads a slot the caller never wrote.

실패!  - 실패: 1, 통과: 5, 건너뜀: 0, 전체: 6, 기간: 28 ms
```

**빌드는 통과한 상태다.** 컴파일러도 링커도 아무 말이 없었고, 유일하게 말한 것이 이 가드다.
C-39 가 인공 반증(파라미터를 일부러 제거)으로만 보여 줬던 동작이, 이번에는 **실제 드리프트**에서
작동했다.

### GREEN (변경 후)

```
통과!  - 실패: 0, 통과: 6, 건너뜀: 0, 전체: 6, 기간: 18 ms
```

## 3. diff

```diff
 public delegate XpeCommonNative.XpeErrorCode CalibGenerateOffsetDelegate(
     [In] XpeCommonNative.XpeImageBuffer[] darkFrames,
     int numFrames,
     float integrationTimeMs,
     float temperatureC,
-    [MarshalAs(UnmanagedType.LPStr)] string outputPath);
+    [MarshalAs(UnmanagedType.LPStr)] string outputPath,
+    [MarshalAs(UnmanagedType.LPStr)] string? configJsonOrNull);
```

```diff
-    generateOffset(new[] { darkFrame }, 1, 100.0f, 25.0f, offsetPath));
+    // null config (#138): the defaults, which the header defines as the pre-parameter
+    // behaviour — this smoke test measures the chain, not the generation method.
+    generateOffset(new[] { darkFrame }, 1, 100.0f, 25.0f, offsetPath, null));
```

2파일 11+/3−.

## 4. 실측 (verbatim)

```
git merge origin/main
Fast-forward … HEAD = 83ffa7a

dotnet test … --filter "…PreprocessDelegateArityGuardTests"     (변경 전)
실패!  - 실패: 1, 통과: 5, 건너뜀: 0, 전체: 6

dotnet test … --filter "…PreprocessDelegateArityGuardTests"     (변경 후)
통과!  - 실패: 0, 통과: 6, 건너뜀: 0, 전체: 6

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개 / 오류 0개

dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
통과!  - 실패: 0, 통과: 180, 건너뜀: 1, 전체: 181
```

Baseline 귀속: 통합 **181**(0/180/1/181)은 `8996415` 시점과 **동일한 수치**다. merge 로 35커밋이
들어왔지만 gui 쪽 테스트 수는 변하지 않았고, 이 커밋은 시그니처만 바꿨다.

## 5. 관측 — 이 방향의 불일치는 런타임이 잡지 못했다

보고할 값이 하나 더 나왔다. `PreprocessCorrectionChainSmokeTests` 3건이 **건너뜀 0으로 통과**했다.
그런데 스테이징된 DLL 은 A-39 이전 아티팩트다:

```
md5  2ddc8be92d37c1ce0f722eda1a810559  build/ci-common/bin/xpe_preprocess.dll
```

이 값은 **C-37 보고서에 기록된 것과 동일**하다(run 34528935791, A-39 이전). 즉 지금
**6인자 C# 이 5인자 DLL 을 호출했고, 아무 일도 일어나지 않았다.**

cdecl 은 호출자가 스택을 정리하므로 **호출자가 인자를 더 밀어 넣는 방향은 무해**하다 — 피호출자가
읽지 않을 뿐이다. C-39 반증에서 테스트 호스트가 중단됐던 것은 **반대 방향**이었다(델리게이트가
더 적게 밀어 넣어 피호출자가 쓰이지 않은 슬롯을 읽음).

정리하면 이 결함 유형은:

| 방향 | 빌드 | 런타임 |
|---|---|---|
| 델리게이트 < 헤더 (C-39 반증) | 통과 | 비결정적 — 테스트 호스트 중단(관측) |
| 델리게이트 > 헤더 (지금, 구 DLL 상대) | 통과 | **아무 신호 없음**(관측) |

**어느 방향도 빌드가 잡지 못하고, 한 방향은 런타임도 잡지 못한다.** 이 가드가 소스 대조여야 하는
이유가 여기서 실측으로 확인됐다.

## 6. 미검증 (Gaps)

- **6인자 DLL 로는 한 번도 실행하지 않았다.** 스테이징 아티팩트가 A-39 이전 것이고, 네이티브
  빌드는 이 레인 금지 조항(#98)이다. 따라서 `configJsonOrNull` 이 **실제로 전달되어 기본값
  경로를 타는지는 미검증**이다 — 지금 통과한 것은 그 인자를 무시하는 구 DLL 상대다.
- **DLL 이 5인자라는 것을 역어셈블로 확인하지 않았다.** 근거는 md5 가 A-39 이전 아티팩트와
  일치한다는 것뿐이다(§5). 그 귀속이 틀렸다면 §5 의 결론도 다시 봐야 한다.
- **`null` 이외의 config 값은 넘겨 보지 않았다.** `"method"` / `"sigma"` 등 헤더가 적은 키와
  `XPE_ERR_CONFIG_INVALID` 경로는 이 레인이 만지지 않았다(모듈 테스트 소관, A-39).
- **가드는 여전히 개수만 본다.** 여섯 번째 인자의 타입·마셜링(`LPStr` ↔ `const char*`)이 맞는지는
  대조하지 않는다 — C-39 §6 에 적은 한계 그대로다.
- CI 반영 결과 미확인.

## 7. 잔여 위험 (Residual risk)

- **6인자 DLL 이 스테이징되는 첫 실행이 진짜 검증**이다. 마셜링이 틀렸다면 그때 드러난다.
  지금 통과는 "인자를 무시하는 상대와 통과" 이므로 그 이상을 뜻하지 않는다.
- **구 DLL 과 새 DLL 이 섞인 상태가 조용하다**(§5). CI 아티팩트를 갈아 끼울 때 `cp -f` 를
  쓰지 않으면(C-31 실측) 구 DLL 이 남고, 이 방향의 불일치는 신호를 내지 않는다.
- 앞으로 인자가 또 늘면 가드가 다시 RED 를 낸다 — 의도된 동작이고, 이번이 그 예행이 아니라
  실제 1회차였다.

## 부록 — 사용한 명령

```bash
git fetch origin main && git merge origin/main            # → 83ffa7a
grep -n -A7 "XPE_API XpeErrorCode xpe_calib_generate_offset(" modules/preprocess/include/xpe/preprocess_api.h
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.IntegrationTests/… --filter "…PreprocessDelegateArityGuardTests"   # RED
# (델리게이트 + 호출부 변경)
dotnet test clients/ImageProcTest.IntegrationTests/… --filter "…PreprocessDelegateArityGuardTests"   # GREEN
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
md5sum build/ci-common/bin/xpe_preprocess.dll
```
