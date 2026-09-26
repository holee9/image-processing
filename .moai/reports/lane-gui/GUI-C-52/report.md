# GUI-C-52 — 노트의 trx 도달 확인, 정적 이음매 제거 (#136)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-52 · Refs #136 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건: `5241b7e` — 미푸시
- **노트 위치: `UnitTestResult` → `Output` → `StdOut`** (leader 가 CI 아티팩트에서 찾을 자리)
- **이음매: (a) 정적 제거** — 인스턴스 상태로 옮겨 누수가 구조적으로 불가능
- **결과: Native 0/29/0/29(벽시계 70 s) · Mock 0/28/1/29 · 통합 0/180/1/181 · slnx 0/0**

---

## 1. 노트는 trx 의 어디에 들어가나 — 실측

재획득이 일어난 실행의 trx 를 열었다.

```xml
<UnitTestResult … outcome="Passed" …>
  <Output>
    <StdOut>reacquire note: 'The launched window did not support AutomationId
            (framework=Wpf); re-located it by process id 10504.'</StdOut>
  </Output>
</UnitTestResult>
```

**필드: `UnitTestResult` / `Output` / `StdOut`.** `ErrorInfo` 가 아니다 — 그쪽은 실패 시
메시지·스택이 들어가는 자리이고(C-49 에서 실패 케이스 사유를 읽은 곳), 노트는 통과한 실행에도
남아야 하므로 `StdOut` 이 맞다.

### 시나리오 `Measure` 헬퍼의 출력도 같은 자리인가 — 따로 확인했다

위 확인은 `WindowReacquireTests` 가 쓴 것이다. 카드가 묻는 것은 **시나리오**의 노트이므로,
같은 헬퍼가 쓰는 다른 문자열로 경로를 확인했다 — C-50 의 Native 실행 trx:

```
$ grep -o "<StdOut>[^<]*elapsed[^<]*</StdOut>" build/e2e-c50-native/run-1.trx | head -2
<StdOut>W-11 Overlay elapsed 376 ms (Native)</StdOut>
<StdOut>W-02 elapsed 3733 ms</StdOut>
(같은 파일에 elapsed 20줄)
```

`Measure` 헬퍼의 출력이 `StdOut` 에 들어간다. 노트는 **같은 헬퍼에서 같은 호출로** 쓰므로 같은
경로다. **구조로 미룬 것이 아니라 두 절반을 각각 쟀다** — 헬퍼 출력이 StdOut 에 간다는 것과,
노트 문자열이 StdOut 에 간다는 것.

## 2. 정적 이음매 — (a) 제거를 골랐다

| 선택지 | 판단 |
|---|---|
| **(a) 정적을 없앤다** | **채택.** 이음매를 생성자 인자로 옮겨 값이 그 픽스처 인스턴스와 함께 살고 죽는다. **누수가 드물어지는 게 아니라 불가능해진다** |
| (b) 유지 + 시작 시점 단언 | 버림. 누수를 **그 자리에서 드러낼** 뿐 여전히 존재하게 두고, 병렬 실행을 켜는 사람이 읽어야 할 제약을 남긴다. 없앨 수 있는 것을 지키게 만드는 쪽은 나중에 비용을 낸다 |

```csharp
internal ApplicationFixture(int simulateUnreadableChecks, bool skipLeftoverSweep)
```

`CanReadAutomationId` 도 정적에서 인스턴스 메서드로 바뀌었고, 카운터는 `_simulateUnreadableChecks`
필드다. 테스트의 `try/finally` 되돌림은 **사라졌다** — 되돌릴 전역 상태가 없다.

**카드가 지키라고 한 것은 그대로 두었다**: `SkipLeftoverSweep` 의 존재 이유(C-36 경합), 전용
컬렉션 분리, 실패 시 원래 요소 유지, 상한 2초.

**부수 효과**: 병렬 실행 제약이 사라졌다. C-51 이 "병렬 실행이 켜지면 안전하지 않다" 고 적었던
것은 정적 공유 때문이었고, 인스턴스 상태에는 그 문제가 없다. (다른 이유로 직렬화가 필요한 것은
그대로다 — 같은 exe 두 인스턴스의 경합, C-36.)

## 3. 반증 (빌드 결과 포함) — 첫 집계가 틀렸다

노트를 `output.WriteLine`(ITestOutputHelper)에서 `Console.WriteLine` 으로 약화했다.

**첫 판정: "반증 통과"** — `grep -c 'reacquire note'` 가 **1** 을 반환해 여전히 trx 에 있는 것으로
보였다. 그대로 적었다면 "경로는 아무래도 상관없다" 는 결론이 나왔을 것이다.

**요소 단위로 다시 셌다:**

| 배치 | 전체 | `UnitTestResult` 안 | 실행 전역 |
|---|---|---|---|
| 정상 (`ITestOutputHelper`) | 1 | **1** | 0 |
| 반증 (`Console`) | 1 | **0** | **1** |

```
=== 빌드: 경고 0개 / 오류 0개
```

**차이가 있다.** 콘솔로 보내면 실행 전역 `StdOut`(xUnit 어댑터 배너가 들어가는 그 블록)에만
남고, **어느 테스트에도 붙지 않는다.** 즉 "노트가 어느 실행에서 나왔는지" 는 남지만
"**어느 테스트에서** 나왔는지" 는 사라진다 — CI 에서 아티팩트를 읽을 때 필요한 것이 후자다.

**내 첫 집계가 거칠어서 못 볼 뻔했다.** "trx 에 있다" 와 "테스트 결과에 붙어 있다" 는 다른
주장이고, 전자로 후자를 확인할 수 없다.

## 4. 실측 (verbatim)

```
dotnet test …E2ETests… --filter "…WindowReacquireTests" --logger "trx;LogFileName=c52.trx"
통과!  - 실패: 0, 통과: 2, 건너뜀: 0, 전체: 2

XPE_E2E_BACKEND=Native XPE_NATIVE_DIR=<staging> dotnet test …E2ETests… --no-build
통과!  - 실패: 0, 통과: 29, 건너뜀: 0, 전체: 29 (기간 1 m 8 s / 벽시계 70 s)

dotnet test clients/ImageProcTest.E2ETests/… --no-build                        (Mock)
통과!  - 실패: 0, 통과: 28, 건너뜀: 1, 전체: 29 (16 s)

dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
통과!  - 실패: 0, 통과: 180, 건너뜀: 1, 전체: 181

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개 / 오류 0개
```

Baseline 귀속: E2E 29건 · 통합 181건 모두 C-51 과 동일 — 이 카드는 테스트를 더하지 않고
이음매의 **형태**만 바꿨다. Native 벽시계 70 s 는 C-48~51 의 69~72 s 범위 안이다.

## 5. 미검증 (Gaps)

- **실제 결함 조건에서 노트가 채워지는 것은 여전히 못 봤다.** 카드 지시대로 재현을 시도하지
  않았다 — **밟은 것은 모사된 방아쇠**이고, C-51 의 그 표현을 그대로 유지한다.
- **시나리오가 실제로 노트를 낸 trx 를 보지 못했다.** §1 은 두 절반(헬퍼 출력이 StdOut 에
  간다 / 노트가 StdOut 에 간다)을 각각 쟀을 뿐, **시나리오가 노트를 낸 한 줄**을 본 것은
  아니다. 그 줄은 실제 결함이 나야 생긴다.
- **CI 아티팩트에서 직접 확인하지 않았다.** 로컬 trx 의 구조를 쟀고, CI 가 같은 형식을 쓴다는
  것은 leader 의 확인(C-50)에 의존한다.
- **`ErrorInfo` 에는 무엇이 들어가는지 이번에 다시 재지 않았다** — C-49 에서 실패 사유를 읽은
  자리라는 기억에 의존한다.

## 6. 잔여 위험 (Residual risk)

- **노트는 통과한 실행의 `StdOut` 에만 있다.** CI 에서 아티팩트를 열어 `StdOut` 을 읽지 않으면
  보이지 않는다 — 실패와 달리 요약에 뜨지 않는다.
- **`internal` 생성자는 테스트 어셈블리 안에서만 보인다.** 다른 어셈블리가 이 픽스처를 쓰게 되면
  이음매를 쓸 수 없고, 그때는 설계를 다시 봐야 한다.
- **직렬화 필요성은 그대로다** — 이음매 때문이 아니라 같은 exe 두 인스턴스의 경합(C-36) 때문이다.
  이 카드가 없앤 것은 **이음매에서 오는** 병렬 제약뿐이다.
- 왜 가끔 Win32 공급자를 받는지는 **미결**(별건, 카드 범위 밖).

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.E2ETests/… --no-build \
  --filter "FullyQualifiedName~WindowReacquireTests" \
  --logger "trx;LogFileName=c52.trx" --results-directory build/e2e-c52
grep -o "<StdOut>[^<]*elapsed[^<]*</StdOut>" build/e2e-c50-native/run-1.trx | head -2
# 반증: output.WriteLine -> Console.WriteLine 후 UnitTestResult 단위로 재집계
python - <<'PY'
import re; t=open(path).read()
results=re.findall(r'<UnitTestResult.*?</UnitTestResult>', t, re.S)
print(sum(1 for r in results if 'reacquire note' in r), t.count('reacquire note'))
PY
```
