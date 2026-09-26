# GUI-C-44 — W-02 가 CI 에서 늘 건너뛰었다: 생성기 탐색 경로 (#141)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-44 · Refs #141 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui` · 병합 `cbae11f`
- 커밋 1건: `9f45bf0` — 미푸시
- **결과: Native E2E 0/16/0/16 · Mock 0/15/1/16 · 통합 0/180/1/181 · slnx 0/0**

---

## 1. 원인 — 한 레인의 디렉터리만 보고 "스테이징 안 됐다" 고 말했다

leader 가 trx 를 내려받아 확인한 것(run 34538498373, main `9352ac3`):
`W02_RunPreprocessing_MatchesTheBackendItRunsOn` 이 `NotExecuted` 이고 사유는

> `xpe_calib_fixture_gen.exe was not found under build/ci-common/bin`

**CI 는 `build/e2e-native-dlls/` 로 스테이징한다.** 픽스처는 `build/ci-common/bin` 만 봤다 —
이 레인이 로컬에서 쓰는 경로다. 그래서 CI 에서 W-02 는 **한 번도 실행된 적이 없다.**

세 층이 겹쳐 이걸 오래 가렸다:

| 층 | 무엇을 놓쳤나 |
|---|---|
| 픽스처 | 한 레인의 경로만 보고 "없음" 을 "스테이징 안 됨" 으로 단정 |
| 사유 문구 | "build/ci-common/bin 에 없다" → **"엉뚱한 곳을 봤다" 와 구분되지 않는다** |
| CI 가드 | `Counters.notExecuted` 만 읽어 런타임 skip 을 못 셌다(leader 자기 정정, `e65f300` 으로 수정) |

**Skip 설계 자체는 고치지 않았다.** C-37 의 판단 — 측정 안 된 것을 통과처럼 세지 않는다 —
은 그대로 옳다. 틀린 것은 **어디를 봤는지 말하지 않은 것**이다.

## 2. 수정 (`9f45bf0`)

```diff
-        var generator = ResolveRepositoryFile(Path.Combine("build", "ci-common", "bin", "xpe_calib_fixture_gen.exe"));
+        var generator = ResolveGenerator(out var searched);
         if (generator is null)
         {
-            note = "xpe_calib_fixture_gen.exe was not found under build/ci-common/bin — stage the …";
+            note = "xpe_calib_fixture_gen.exe was not found, so the preprocess success path is NOT " +
+                   $"measured. Looked in: {string.Join(" ; ", searched)}. …";
```

`ResolveGenerator` 의 순서:

1. **`XPE_NATIVE_DIR`** — CI 가 주는 변수이고, **그 실행이 실제로 쓰는 디렉터리**다.
2. `build/ci-common/bin` 을 테스트 출력에서 상위로 거슬러 탐색 — 로컬 경로를 깨뜨리지 않는다.

폴백을 없애지 않은 이유: 변수를 안 주고 도는 로컬 실행이 계속 동작해야 한다.

## 3. 양쪽을 관측했다 (카드 2항)

CI 배치를 로컬에서 재현했다 — 생성기를 `XPE_NATIVE_DIR` **에만** 두고
`build/ci-common/bin` 에서는 치웠다.

### (a) 찾은 경우 — 실행된다

```
XPE_E2E_BACKEND=Native XPE_NATIVE_DIR=/tmp/ci-sim  dotnet test … --filter W02
통과!  - 실패: 0, 통과: 1, 건너뜀: 0, 전체: 1 (4 s)
```

**수정 전이라면 이 배치에서 Skip 이었다.** 이것이 CI 가 매번 있던 상태다.

중간에 한 번 더 걸렸다: 처음에는 `xpe_*.dll` 과 생성기만 복사했더니

```
xpe_calib_fixture_gen.exe exit=-1073741515, files=[]
```

`0xC0000135`(STATUS_DLL_NOT_FOUND) — `fmt.dll`·`spdlog.dll` 등 의존 DLL 이 없어서다.
**경로 수정은 이미 작동했고**(생성기를 찾아 실행까지 갔다), 실패 지점이 한 칸 뒤로 옮겨진 것이다.
전체 세트를 넣자 통과했다. 이 구분이 없었다면 "고쳐도 안 된다" 로 오판할 자리다.

### (b) 못 찾은 경우 — Skip 하고 **어디를 봤는지 말한다**

```
건너뜀! - 실패: 0, 통과: 0, 건너뜀: 1, 전체: 1

xpe_calib_fixture_gen.exe was not found, so the preprocess success path is NOT measured.
Looked in: C:/Users/…/Temp/ci-nogen\xpe_calib_fixture_gen.exe ;
  …\clients\ImageProcTest.E2ETests\bin\Debug\net8.0-windows\build\ci-common\bin\… ;
  …\clients\ImageProcTest.E2ETests\bin\Debug\build\ci-common\bin\… ;
  …\clients\ImageProcTest.E2ETests\bin\build\ci-common\bin\… ;
  …\clients\ImageProcTest.E2ETests\build\ci-common\bin\… ;
  …\clients\build\ci-common\bin\… ;
  D:\workspace-github\xpe-gui\build\ci-common\bin\… ;
  D:\workspace-github\build\ci-common\bin\… ;
  D:\build\ci-common\bin\xpe_calib_fixture_gen.exe.
Stage the xpe-ci-preprocess-binaries artifact into one of these, or point XPE_NATIVE_DIR at a
directory holding the generator.
```

**9개 경로가 전부 남는다.** 이 문구가 처음부터 있었다면 leader 가 trx 를 열었을 때 원인이
그 자리에서 보였다.

## 4. 반증 (빌드 결과 포함)

위 (b)가 그대로 반증이다 — `XPE_NATIVE_DIR` 를 생성기 없는 디렉터리로 두면 W-02 만 Skip 된다.

```
=== 빌드:
    경고 0개
    오류 0개                       ← 빌드는 정상
건너뜀! - 실패: 0, 통과: 0, 건너뜀: 1, 전체: 1
```

**중간에 하나 배웠다.** 첫 반증 시도는 **완전히 빈 디렉터리**를 줬는데, 그러자 W-02 가 Skip 이
아니라 **실패**했다 — `Run Preprocessing is disabled on the native backend.` 네이티브 DLL 자체가
없어 백엔드가 죽었고, 생성기 경로와 무관한 다른 실패였다. DLL 은 두고 생성기만 뺀 배치로
좁혀서 다시 재고서야 의도한 반증이 됐다. **"실패했으니 반증 성공" 으로 읽었다면 엉뚱한 것을
증명한 셈이다.**

## 5. 실측 (verbatim)

```
XPE_E2E_BACKEND=Native XPE_NATIVE_DIR=<staging> dotnet test …E2ETests…    (앱 폴더 비움)
통과!  - 실패: 0, 통과: 16, 건너뜀: 0, 전체: 16 (1 m 4 s)

dotnet test clients/ImageProcTest.E2ETests/… -c Debug                     (Mock)
통과!  - 실패: 0, 통과: 15, 건너뜀: 1, 전체: 16 (10 s)

dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
통과!  - 실패: 0, 통과: 180, 건너뜀: 1, 전체: 181

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개 / 오류 0개
```

Mock 의 건너뜀 1은 Native 전용 출처 가드(C-41)다 — W-02 가 아니다.

Baseline 귀속: E2E 16건은 C-43 커밋(`1a5c479`) 직후와 같다. 이 카드는 시나리오를 더하지
않고 **한 시나리오가 실제로 돌게** 만들었다.

## 6. 미검증 (Gaps)

- **CI 에서 실제로 도는 것을 보지 못했다.** 로컬에서 CI 배치를 재현했을 뿐이다 —
  `build/e2e-native-dlls` 라는 실제 경로가 아니라 임시 디렉터리를 `XPE_NATIVE_DIR` 로 줬다.
  다음 main CI 가 첫 확인이다.
- **CI 의 `build/e2e-native-dlls` 에 생성기가 실제로 들어 있는지 확인하지 못했다.** CI 가
  `xpe-ci-preprocess-binaries` 를 그 디렉터리에 푸는지는 ci.yml(leader 소유)의 문제다 —
  없으면 **경로를 고쳐도 여전히 Skip 이고, 이제는 사유가 그것을 말한다.**
- **Mock 실행 1회가 1건 실패했다가 재실행에서 재현되지 않았다.** 그 실행은 앱 출력 폴더로
  DLL 을 되돌리는 명령과 같은 턴이었다(측정 중 탐색 대상이 바뀜) — 그럴듯한 설명이지만
  **확인하지 못했다.** 실패한 테스트 이름을 잡아 두지 못한 것이 이 관측의 한계다.
- 생성기가 있어도 **의존 DLL 이 빠지면 실행이 실패**한다(§3a). 그 경우 Skip 사유가
  `exit=-1073741515` 로 남지만, 그 숫자가 무엇인지는 사유가 설명하지 않는다.

## 7. 잔여 위험 (Residual risk)

- **탐색 순서가 `XPE_NATIVE_DIR` 우선이다.** 그 변수가 DLL 만 있고 생성기가 없는 디렉터리를
  가리키면, 폴백이 로컬 `build/ci-common/bin` 의 **다른 버전** 생성기를 집을 수 있다 —
  네이티브 DLL 과 생성기가 서로 다른 run 에서 온 조합이 조용히 가능하다. 출처 기록(C-41)은
  DLL 만 다루고 생성기의 출처는 대조하지 않는다.
- **사유 문구가 길다**(경로 9개). 읽히지 않을 위험이 있지만, 짧게 줄이면 이 카드가 고친
  바로 그 모호함으로 돌아간다.
- W-02 가 CI 에서 처음 실제로 도는 순간, **지금까지 한 번도 CI 에서 검증되지 않은 경로**가
  처음 실행된다 — 거기서 새 실패가 나올 수 있고, 그것은 회귀가 아니라 첫 측정이다.

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
# (a) 찾은 경우 — CI 배치 재현
cp build/ci-common/bin/*.dll build/ci-common/bin/xpe_calib_fixture_gen.exe /tmp/ci-sim/
mv build/ci-common/bin/xpe_calib_fixture_gen.exe /tmp/gen.bak
XPE_E2E_BACKEND=Native XPE_NATIVE_DIR=/tmp/ci-sim dotnet test …E2ETests… --filter W02

# (b) 못 찾은 경우 = 반증
cp /tmp/ci-sim/*.dll /tmp/ci-nogen/      # DLL 만, 생성기 없음
XPE_E2E_BACKEND=Native XPE_NATIVE_DIR=/tmp/ci-nogen dotnet test … --filter W02 \
    --logger "trx;LogFileName=w02c.trx"
grep -o '<Message>[^<]*</Message>' /tmp/trx/w02c.trx
```
