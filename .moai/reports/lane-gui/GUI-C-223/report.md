# GUI-C-223 — main 빨강: 시그니처 대조 시험이 dicom 헤더 함수 수 10 을 고정 (#249)

기준: `1097265f` 위의 단독 커밋(219·219b·222 와 섞지 않음). 바뀐 파일은 `clients/ImageProcTest.IntegrationTests/Functional/NativeModuleSignatureParityTests.cs` 하나.

## 1. 숫자 고정이 이 시험의 목적에 맞는가 — 아니다

단언 바로 위의 주석이 목적을 적고 있다: "a parser that silently found nothing would otherwise make every comparison below pass". 지키려는 것은 **파서가 헤더를 읽었다**는 성질이지 "선언 목록이 바뀌지 않았다"가 아니다. 숫자는 그 성질의 대용품이었고, 모듈 소유자가 선언을 하나 추가할 때마다(이번 `xpe_dicom_version`) 레인 경계 밖의 시험이 깨졌다. 게다가 파서 출력 옆에 손으로 적은 숫자는 마지막에 숫자를 베낀 사람만큼만 독립적이다.

바꾼 것: 헤더별로 **독립 도출**과 대조한다. 독립 도출 = 헤더 텍스트에서 주석·전처리기 줄을 지우고, 내보내기 표지(`XPE_API`) 뒤 첫 `(` 앞의 식별자를 정규식으로 뽑은 이름 집합(엔진의 주석 제거·선언 패턴을 쓰지 않는다). 파서가 읽은 이름 집합과 **같아야** 하고, 다르면 메시지가 "읽지 못한 함수 이름"과 "텍스트에 없는데 읽힌 이름"을 말한다. 대조군: 독립 도출 자체가 비면 "둘 다 비었다"가 일치로 보이므로 preprocess·dicom·ai 헤더는 0 보다 커야 하고 common 은 비어 있으면 안 된다. 다른 모듈의 고정 숫자(48·10·9·6·8·11·`8+5+3`)도 같은 모양이라 같은 방식으로 바꿨다.

의도적으로 하지 않는 것: 선언이 **추가**되었다는 사실 자체에서 빨개지는 것. 그것이 이번 사고(다른 레인의 정당한 추가가 main 을 빨갛게 함)다. 추가된 함수는 센서스(`Census_OfTheBindings_IsNotEmpty`)의 "unbound: …" 출력에 이름으로 나타나고, 바인딩이 있는데 헤더와 다르면 `EveryStaticExtern…`/`EveryDelegate…` 가 빨개진다. 카드의 반증 3("가짜 선언을 넣으면 이름으로 말하며 빨강")은 이 판단 때문에 그대로 하지 않았다 — 아래 3번에 결과.

## 2. `xpe_dicom_version` 에 C# 바인딩이 없어도 통과하는 것이 맞는가 — 맞다

대조는 C# 쪽(정적 extern·델리게이트)에서 출발해 헤더에서 그 이름을 찾는다. 헤더에만 있고 바인딩이 없는 함수는 위반이 아니라 센서스의 "unbound" 목록에 오른다. main 헤더로 센서스를 돌린 실제 출력: `7/11 header functions have a C# binding … unbound: xpe_dicom_cfind_mwl, xpe_dicom_cstore, xpe_dicom_version, xpe_dicom_write_j2k` — 이미 3개가 바인딩 없이 있었고 `xpe_dicom_version` 이 네 번째다.

## 3. 반증 (main `e8db144c` 의 헤더로 만든 임시 워크트리)

| 단계 | 결과 |
|---|---|
| 옛 시험 + main 헤더(dicom 11) | 실패 1, `Expected: 10  Actual: 11` — main 의 빨강 재현 |
| 새 시험 + main 헤더 | 통과 4/4 |
| 새 시험 + dev/gui 헤더(dicom 10) | 통과 4/4 |
| 헤더에 가짜 선언 `xpe_dicom_fake_added` 추가 | 통과(설계상 빨개지지 않음, 위 1번), 센서스가 이름으로 말할 대상이 됨 |
| 파서가 못 읽는 선언 `xpe_dicom_fake_unreadable(void) { … }` 추가 | 실패 — `the text declares 12 exported function(s), the parser read 11. Declared but NOT read: [xpe_dicom_fake_unreadable]` |
| preprocess 헤더의 `XPE_API` 표지를 전부 지움(파서도 텍스트 스캔도 0) | 실패 — `no function was read at all`(대조군), 뒤 두 시험도 빨강 |

## 미검증

- CI 에서의 실행 결과는 아직 없다. 모듈 헤더 20개를 CRLF 로 바꾼 임시 워크트리에서는 통과 4/4 를 확인했다(CI 가 만든 체크아웃 자체는 아님).
- 이 시험이 내보내기 표지 방식(`XPE_API`)의 헤더만 읽는다는 전제는 그대로다(기존 파서와 같은 전제).
