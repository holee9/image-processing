# GUI-C-180 — Troubleshooting 영어 최종본과 인용 문구 일치 시험

레인: gui · 이슈: `#225` · 푸시 없음(로컬 커밋). `docs/` 는 건드리지 않았다. 행 21 도 켜지 않았다.

## 결론

1. **영어 최종본**: `troubleshooting.md`(이 폴더). 열은 **Message / What it means / What to do**, 섹션 **A·B·C·Appendix**, 41행. 백틱 인용 **102개**가 전부 소스 원문이다. 조치가 없는 행은 모두 같은 문장 *"No action is defined; the message itself is the diagnosis."* 로 적었다. 줄 번호 열은 없다(리더 결정 c). 조치가 로그 패널에만 있는 행(`Build gui/<project> first.`)은 그렇다고 문서가 말한다(결정 d).
2. **시험**: `TroubleshootingDocQuoteTests`(`clients/ImageProcTest.IntegrationTests/Functional/`). 문서의 **백틱 안 문구**는 소스에 있는 원문이어야 하고 `<자리표시>` 는 와일드카드다. **문서가 자리표시면 건너뛰지 않고 실패한다.**
3. ⚠ **리더가 문서를 반영하기 전에는 이 시험이 빨강이다**(자리표시 페이지 상태에서 확인함). 시험 커밋 `a4e0e7d0` 은 보고서 커밋과 **따로** 있다. `dev/gui` 에 푸시하면 CI 가 빨개지므로, 시험 커밋은 **문서를 반영한 커밋과 같은 푸시에** 올리거나 문서 반영 뒤에 올리는 편이 맞다. 최종본을 임시로 두면 **초록**(7/7)이다 — **리더 반영 후 초록**.

| 커밋 | 내용 |
|---|---|
| `a4e0e7d0` | 시험 (문서 반영 전에는 빨강이 정상) |
| (이 보고서 커밋) | 영어 최종본 + 증거 + 보고서 |

## 1. 영어 최종본의 규칙

- **백틱은 소스 원문 인용에만 쓴다.** 경로·식별자·값은 백틱 없이 쓴다. 그래야 "백틱이면 검증된다"가 성립한다(그렇지 않은 짧은 조각은 시험이 실패시킨다).
- 인용은 원문 그대로이고, 값이 달라지는 부분만 `<message>`·`<path>`·`<code>` 같은 자리표시다. 여러 줄·여러 조각으로 쓰인 소스 문장은 이어 붙인 한 문장으로 인용했다.
- 한 메시지가 여러 파일에서 조립되는 경우(예: 오래된 러너 빌드)는 각 조각을 따로 인용하고 조각 사이는 평문으로 이었다.
- 심각도(WARN/ERROR)·경고 코드(`LOAD_FAILED` 등)는 인용이 아니라 설명이라 백틱 없이 "What it means" 에 적었다.

## 2. 시험이 하는 일

1. 문서에서 백틱 span 을 모두 뽑는다. **0개면 실패**("still the placeholder").
2. 여섯 글자 미만이거나 자리표시뿐인 span 은 "검증할 수 없는 인용" 으로 **실패**시킨다.
3. `gui/`, `clients/ImageProcTest/`, `modules/` 의 `.cs`·`.cpp`·`.c`·`.h` **233개**를 읽는다. 시험·빌드 산출물·third_party 는 뺀다(시험이 문서를 인용해 스스로 통과하는 일을 막기 위해). 인접한 문자열 리터럴(C++ `"a" "b"`, C# `"a" +` 줄바꿈 `$"b"`)을 이어 붙이고 `\"` 를 풀어 비교하며, **주석(블록·한 줄 전체 `//`)을 지운 뒤** 찾는다 — 주석에만 남은 문구는 못 찾은 것이다.
4. 못 찾은 인용을 전부 한 번에 보고한다.

시험 6+1개: 페이지 시험 1개와 매처 대조 6개(문구 한 단어가 바뀌면 못 찾음 · 자리표시가 보간 구멍/printf 변환을 대신하되 줄을 넘지 않음 · 여러 조각 리터럴 이어 붙이기 · 이스케이프된 따옴표 · 주석에만 있는 문구는 못 찾음 · 짧은 span 거부).

**범위가 지시보다 하나 넓다**: 지시는 `gui/`·`modules/` 였는데 `clients/ImageProcTest/`(옛 앱)를 포함했다. 알림 큐 오버플로 문구(`1 alert was dropped …`)가 그곳의 `AlertDisplayFormatter` 에 있기 때문이다(앱이 `gui` 쪽에서 그 파일을 링크해 쓴다 — `ImageProcTest.csproj`).

## 3. 검증

| 상태 | 결과 |
|---|---|
| 자리표시 페이지(현재 커밋된 것) | 페이지 시험 **실패**: *"…quotes no message, so nothing on it is verified — it is still the placeholder…"*, 매처 대조 6개 통과 |
| 최종본을 임시로 둠 | **7/7 통과** |
| 최종본에서 한 문구 변경(`discarded` → `thrown away`) | **실패**, 그 인용을 지목: `` `Render stopped; the result was thrown away after <ms> ms of work.` `` |
| **반대 방향**: 소스 문구 변경(`Stop: no render is in flight.` → `Stop: nothing is rendering.`), 최종본 유지 | **실패**, 그 인용을 지목: `` `Stop: no render is in flight.` `` |

`docs/help/content/troubleshooting.md` 는 매번 커밋된 바이트로 복구됐고(해시 비교), 소스 변경 반증 뒤 GUI 를 다시 빌드해 러너 신선도 가드를 맞췄다. (`test_states.txt`, `arm_source_change.txt`)

**시험이 처음 놓칠 뻔한 것 둘(고쳤다)**
- **주석만 남은 문구**: 처음엔 주석과 코드를 구별하지 못했다. 파이썬 거울로 102개를 재 보니 현재는 0개였지만, 시험이 구별하지 못하면 나중에 조용히 통과하므로 주석 제거를 시험에 넣고 대조를 추가했다(`comments_check.txt`).
- **쓰이지 않는 파일**: `Display pipeline failed: <message>` 가 **어디서도 참조되지 않는 `PipelineOrchestrator.cs`**(C-152 §3)에서 먼저 매칭됐다. 그대로면 실제 코드(`MainWindowViewModel`)의 문구가 바뀌어도 이 스텁이 인용을 살려 둔다. 그 파일을 검색에서 뺐다(주석에 이유와 되돌리는 조건을 적음).

## 4. 미검증 · 한계

1. **시험이 검증하는 것은 백틱 인용뿐이다.** "What it means" 와 "What to do" 의 영어 서술은 앞선 GUI-C-179 의 코드 근거(원인·조치)를 영어로 옮긴 것이며 **시험도 소스 대조도 없다.** 백틱 없이 적은 경로·값·코드 이름(예: build/ci-post, docs/project/sprint-plan.md, `evidence/<run id>` 위치)이 맞는지는 이번에 다시 대조하지 않았다.
2. **와일드카드가 느슨하다**: `<…>` 는 같은 줄에서 최대 200자까지 무엇이든 받는다. 인용의 문구가 자리표시 안쪽으로 옮겨 가는 변경은 못 잡는다.
3. **"문구가 소스에 있다" 와 "그 상황에서 그 문구가 뜬다" 는 다르다.** 시험은 앞의 것만 단언한다. 같은 문구가 다른 곳에도 있으면 먼저 찾은 곳에서 만족한다(첫 일치 파일은 `quotes_found.txt` 에 기록하되 시험은 기록하지 않는다).
4. **문자열 리터럴이 상수·변수로 조립되는 문장은 전체를 한 번에 인용할 수 없다.** 예: `API reference has not been generated.` 와 그 뒤의 생성 방법은 소스에서 상수가 따로라 문서도 조각으로 인용했다. `@"…"` 축자 문자열과 C# 11 원시 문자열은 처리하지 않는다. 줄 끝 `//` 주석은 지우지 않는다(URL 의 `//` 와 구별하려면 파서가 필요하다).
5. **도움말 번들의 마크다운 렌더러를 보지 않았다.** 표 안 줄바꿈에 `<br>` 를 썼고 백틱 안의 `<…>` 를 그대로 보여 주는지는 렌더러에 달려 있다 — 반영 전에 확인이 필요하다(`HelpBundleService` 가 어떻게 그리는지 읽지 않았다).
6. **영어 문장은 원어민 검토를 받지 않았다.** 리더 검토 대상이다.
7. 시험은 `Functional` 범주라 CI 의 `dotnet-tests` 잡에서 돈다. 이 시험을 CI 에서 돌려 본 적은 없다(로컬 7개, 상태 3종).

## 증거 파일

`troubleshooting.md`(영어 최종본, `docs/help/content/troubleshooting.md` 로 옮길 내용) · `quotes_found.txt`(백틱 span 별 첫 일치 소스 파일) · `test_states.txt` · `arm_source_change.txt` · `comments_check.txt` · `text_lint.txt`

🗿 MoAI
