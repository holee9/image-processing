# QA-A-229 M3 설계 — 세션 일치(FUNC-011)와 isValid 역참조 (D1·D5, #245)

구현하지 않았다. 결정이 필요한 갈림길과 각 선택지의 비용만 적는다. 근거는 모두 이 트리에서 읽은 코드·문서이고, 읽지 않은 것은 §6 에 적었다.

## 1. 먼저 알아낸 것: "세션 일치"에는 비교 상대가 정의돼 있지 않다

- 헤더 약속은 두 줄이다: `xpe_calib_load_offset` 의 "AC-CAL-001: Validate SHA-256, check session matching, verify expiry"(`preprocess_api.h:128`)와 "XPE_ERR_CONFIG_INVALID if session mismatch"(`:135`). **무엇과 무엇이 맞는지** 쓰여 있지 않다.
- SRS-CALIB-FUNC-011(`SRS-CALIB-001_Software_Requirements_Specification.md:128`)은 다른 것을 요구한다: `xpe_calib_session_create()` 가 UUID v4 세션을 만들고, 세션이 타임스탬프·온도·kVp/mAs·빈닝·교정 파일 버전을 담고, `xpe_ghost_reset()` 까지 프레임 사이에 유지. 이 함수는 공개 헤더에 없다(grep 0). 즉 요구가 말하는 "세션"과 헤더가 말하는 "세션 일치"는 같은 기능이 아니다.
- 파일 쪽 사실: `XCalFileHeader::session_id[64]`(`xcal_format.h:150`, 오프셋 40)가 있고, 로더가 읽어 `g_calib.offset_session_id` / `gain_session_id` 에 보관(`xpe_calib_load_offset.cpp:59-80`, `xpe_calib_load_gain.cpp:202-245`), `xpe_calib_save` 가 다시 써 넣는다(`xpe_calib_save.cpp:59`). **어디서도 비교하지 않는다** — 읽어서 들고 있다 돌려줄 뿐이다. `calibration_cache.cpp:485` 는 캐시 적중 시 "SHA-256 과 세션 검사는 반복하지 않는다"고 쓰는데, 반복할 세션 검사가 원본에 없다.
- 검사가 들어갈 자리는 이미 정해져 있다: `xcal_reader.cpp` 는 SHA-256(`:290`) 다음에 만료(`:307`)를 본다. 헤더 문장 순서(SHA → 세션 → 만료)대로라면 세션 검사는 그 둘 사이다.

## 2. 세션 ID 가 오는 곳과 값 분포

- 생성기가 쓰는 값은 리터럴 `"generated"`(`xpe_calib_generate_offset.cpp:148`, `xpe_calib_generate_gain.cpp:393·835`). 생성기로 만든 교정 파일은 전부 이 값이다.
- 시험 픽스처는 빈 값(`test_req_p1a_066.cpp:112` memset 0)이거나 값을 무시한다(`test_xpe_preprocess_calibration.cpp:34` 인자 미사용). `"test-session-123"`·`"different-session"` 은 M2b 에서 삭제한 레거시 파일에만 있었다.
- 호출자: `clients`·`gui` 에 세션을 로더에 넘기는 코드는 문자열 기준 0건이다(`session` grep 에 걸린 것은 AI 세션·시험 세션 문구뿐). 유일한 언급은 `AlgorithmValidationCatalogService.cs:31` 의 "adapter-pending … Add session-id and isolation evidence" 한 줄(어댑터 미구현이라고 스스로 적음). 다른 레인 모듈(`modules/*/include`)의 호출도 0건.
- 문서가 말하는 위험: SHA-CALIB-001 HAZ-CALIB-007 — 다중 검출기가 교정 저장소를 공유할 때 다른 검출기의 오프셋·이득이 적용되어 체계 오차가 생긴다(`SHA-CALIB-001_Software_Hazard_Analysis.md:150-153`).

## 3. 설계 선택지 (D1)

| | 비교 | 막는 것 | 필요한 것 | 못 막는 것 |
|---|---|---|---|---|
| **S1 파일 간 일치** | 오프셋 파일의 session_id == 이득 파일의 session_id (한쪽이 빈 값·`generated` 면 비교 안 함) | 서로 다른 세션에서 만든 짝의 혼용 | 새 API 없음. 로더 두 곳과 캐시 적중 경로 | 둘 다 같은 "틀린 검출기"에서 온 경우 — HAZ-007 의 핵심 시나리오 |
| **S2 기대 세션 지정** | 파일 session_id == 호출자가 지정한 기대 세션 | HAZ-007 시나리오 | `xpe_calib_set_session(const char*)` 같은 새 공개 함수(또는 init JSON 키), 호출자(GUI)가 값을 넘겨야 함, ABI 추가 | 호출자가 지정하지 않으면 무효(빈 기대값 = 검사 안 함) |
| **S3 SRS 전체** | `xpe_calib_session_create` UUID 세션을 핸들·교정 파일에 묶음 | FUNC-011 전부 | 세션 객체(온도·kVp·mAs·빈닝 포함), ghost 핸들과 결합, 캐시 키 변경 | — 범위가 가장 크고, 온도·kVp 입력은 현재 쓰이지 않는다(M2·M2b) |

권고(결정은 리더): S1 을 먼저 구현하고 S2 를 같은 틀(빈 값 = 미지정)로 얹을 수 있게 비교 함수를 하나로 둔다. 이유는 S1 이 호출자 변경 없이 들어가면서 S2 의 비교기를 그대로 재사용한다는 점이다. 다만 S1 만으로는 HAZ-007 이 닫히지 않는다는 한계를 문서에 적어야 한다. S3 는 SRS 개정 이슈다.

## 4. 공통 의미론 (어느 선택지든)

- **빈 값·옛 파일**: 거부하면 기존 파일이 전부 깨진다 — 생성기가 쓰는 `"generated"` 가 모든 생성 산출물에 있고 시험 픽스처는 빈 값이다. 그래서 규칙은 "양쪽이 비어 있지 않고 서로 다를 때만 불일치". 빈 값과 `"generated"` 를 같은 "미지정" 으로 취급할지(후자를 예약어로 선언할지)는 결정이 필요하다.
- **실패 시 상태**: `XPE_ERR_CONFIG_INVALID`(헤더가 이미 약속한 코드). 로더는 스테이징 후 설치하는 구조라(`xpe_calib_stage_*` → `xpe_calib_commit_*_locked`) 설치 전에 판정하면 전역 상태가 그대로다. 이미 적재된 다른 맵도 건드리지 않는다. 적재 시점의 거부이므로 "출력 불변·이력 미커밋" 원칙은 해당 없고, S3 에서 핸들이 세션을 갖는 경우에만 `xpe_ghost_correct` 가 기존 되돌림 평면(REQ-P1A-032)을 쓰는 경로가 생긴다.
- **초기화 전 적재**: 로더는 초기화 전에도 적재된다(M2b 에서 3종 모두 시험으로 고정). 세션 검사는 이 성질을 바꾸면 안 된다 — 기대 세션(S2)이 아직 지정되지 않은 상태에서 적재가 거부되면 안 된다는 뜻이다.
- **캐시 적중**: 현재 적중은 세션 검사를 건너뛴다(§1). S1 은 비교에 필요한 id 가 이미 엔트리(`calibration_cache.cpp:132` `sessionId[64]`)에 있어 적중에서도 같은 판정을 재현할 수 있다. S2 는 기대 세션이 바뀌면 적중이 우회되므로 판정을 엔트리에 저장하지 말고 적중마다 비교해야 한다. 빠른 경로가 느린 경로와 같은 판정을 내는지는 나쁜 입력으로 단언해야 한다(QA-A-193→195 의 교훈).
- **시험 계획**: 일치 시 OK, 불일치 시 `CONFIG_INVALID` + `g_calib` 지문 불변, 한쪽 빈 값·`generated` 통과, 캐시 적중이 미스와 같은 판정, 오류 우선순위(SHA → 세션 → 만료) 입력 하나씩. 반증은 비교 제거·순서 뒤바꿈·적중 경로 우회.
- 구현 전까지 헤더 `:128`·`:135` 는 약속을 거짓으로 둔다. M2 와 같은 처리("미구현 — FUNC-011, #245")를 구현 전에 먼저 넣을지는 결정이 필요하다.

## 5. isValid 역참조 (D5)

- 사실: `GhostCorrectorHandle::isValid` 는 `gh->magic == kMagic` 으로 판정한다(`xpe_preprocess_internal.h:89-92`). 포인터를 역참조하므로 `xpe_ghost_destroy` 가 `magic = 0` 을 쓰고 `delete`(`ghost_correct.cpp:395-398`)한 뒤에 같은 포인터가 들어오면 **해제된 메모리를 읽는다**. 재파괴(`xpe_ghost_destroy` 두 번)도 같은 읽기에서 시작한다. 호출 지점은 4곳(`ghost_correct.cpp:128·299·382·395`).
- 계약: 헤더는 이미 "파괴 후에는 다른 어떤 함수에도 넘기지 말 것"(`preprocess_api.h:804`)이라고 쓴다. 그러니 이것은 계약 위반 시 진단의 문제이고, 정상 경로의 결함이 아니다. 시험 `Boundary.GhostUseAfterDestroyReturnsError` 는 이름과 달리 `xpe_ghost_reset(nullptr)` 만 호출한다(`test_boundary.cpp:99-105`, 주석이 스스로 "cannot safely call … with a destroyed handle" 라고 적음) — 이름이 약속하는 동작은 시험된 적이 없다.

| 선택지 | 방법 | 막는 것 | 비용·위험 |
|---|---|---|---|
| **H0 계약만** | 구조 유지, 시험 이름을 실제 동작으로 정정, 헤더에 "파괴 후 사용은 미정의" 명시 | — | 비용 0, 진단 없음. `isValid` 의 magic 검사가 해제 메모리를 읽는다는 사실은 남음 |
| **H1 핸들 등록표** | 전역 `std::unordered_set<const void*>`(+뮤텍스). create 가 넣고 destroy 가 뺀다. `isValid` 는 집합 조회(역참조 없음) | 파괴 후 사용·재파괴가 안전하게 `INVALID_INPUT` 로 | 호출당 전역 잠금(핸들 자체 뮤텍스는 이미 호출 전체를 잡는다). 주소 재사용(ABA)은 못 막음: 파괴한 주소에 새 핸들이 생기면 낡은 포인터가 유효로 읽힘. 파괴와 사용의 동시성은 여전히 호출자 몫 |
| **H2 토큰 핸들** | 핸들을 포인터가 아닌 (슬롯, 세대) 토큰으로(타입은 여전히 `void*`, ABI 불변). 표 조회 시 세대 비교 | H1 + ABA | 슬롯 표·세대 카운터, 포인터 가정 코드(시험의 `static_cast<GhostCorrectorHandle*>` 사용처) 수정, 동시 파괴까지 막으려면 조회 때 참조 소유(shared_ptr)가 필요해 "호출 중 파괴 지연" 의미론이 추가됨 |
| **H3 ASan 시험** | 코드 변경 없이 ASan 빌드 CI 잡에서 계약 위반 시험을 돌려 사용-후-해제를 기록 | 아무것도 막지 않음, 증거만 | CI 잡 추가(워크플로 소유는 리더) |

권고(결정은 리더): 이 계약은 이미 문서화돼 있으므로 **H0 + 시험 이름 정정이 최소**다. 진단이 필요하다는 결정이 나오면 H1(작고 ABI 불변), ABA 까지 문제로 본다면 H2. 이 권고의 전제 "호출자가 파괴된 핸들을 쓰는 경로가 없다"는 `xpe_ghost_destroy` 호출자를 훑지 않았다 — 미검증이다.

## 6. 미검증 (Gaps) · 잔여 위험

- `clients`·`gui` 호출자 조사는 `session` 문자열 grep 한 번이다. P/Invoke 선언에서 `xpe_calib_load_*` 호출 자체는 열어 보지 않았으므로 "세션을 넘기는 호출자 0" 은 문자열 기준이다. 대조군(같은 명령으로 실재하는 단어를 찾는가)은 AI 세션 문구가 걸린 것으로 갈음했다.
- 실사용 교정 파일의 session_id 분포(생성기 외의 출처)는 모른다. "옛 맵이 깨지는가"는 코드 경로(생성기 `"generated"`, 시험 빈 값)까지만 확인했고 실제 사용자 파일은 보지 않았다.
- 비용 표의 "호출당 전역 잠금" 은 측정하지 않았다. 구현 규모(줄 수)도 추정하지 않았다.
- `RTM-CALIB-001` 이 FUNC-011 에 ✓ 를 단 문제는 QA-A-228 이 다뤘고 여기서 다시 보지 않았다.
- `xpe_calib_load_gain`·`xpe_calib_load_defect_map` 헤더에 세션 관련 문장이 있는지는 offset 쪽만 읽었다.
