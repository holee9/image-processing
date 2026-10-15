# QA-B-195 M1 — 모델 서명 검증기 (REQ-AI-007 / REQ-AI-091)

카드: QA-B-195 M1 · 관련: #130 · 설계 승인: `design.md` D1·D2·D3·D8 (리더 2026-10-03)
이 마일스톤은 **검증기만** 만든다. 로딩 코드에 연결하지 않았다(M3, 운영 키 D5 가 정해진 뒤). 동작은 바뀌지 않았다.

## 주장 (Claim)

1. `ai_model_signer.h/.cpp`(내부, 내보내지 않음)가 `VerifyModelSignature(keys, role, model, sidecar, sigFile)` 을 제공한다. 파일을 읽지 않고 상태가 없는 버퍼 위의 함수다. 형식(설계 §3·§3.1):
   - 메시지 = `"XPE-MODEL-SIG-1\n"` ‖ `u16le(len(role))` role ‖ `u64le(len(model))` model ‖ `u8(has_sidecar)` [‖ `u64le(len(sidecar))` sidecar]
   - 서명 = ECDSA P-256 over SHA-256(메시지), r‖s 64바이트
   - `.sig` = `"XSIG"` ‖ 버전 1 ‖ 알고리즘 1 ‖ 키 id 8 ‖ 서명 64 = 78바이트, 키 id = SHA-256(X‖Y)[:8]
2. 구현은 Windows CNG(SHA-256 스트리밍, ECDSA P-256 `BCryptVerifySignature`)다. 추가 의존성 없음. 이 파일은 `operator new` 할당을 하지 않는다.
3. 판정 순서는 싼 것 먼저: `.sig` 없음 → 크기·매직 → 버전·알고리즘 → 크기 상한(D8, **구현 안전 상한이지 요구가 아님**, 1 GiB) → 키 id(신뢰 목록에 없으면 해시하기 전에 거부) → 해시 → 서명. 이유마다 다른 상태값이다(`kNoSignatureFile`·`kMalformedSignature`·`kUnsupportedVersion`·`kTooLarge`·`kUnknownKey`·`kBadSignature`·`kVerifierError`).
4. **실패 닫힘**: 신뢰 목록의 키가 곡선 위의 점이 아니어서 가져올 수 없으면 `kVerifierError` 이고 다음 키로 넘어가지도, 통과시키지도 않는다. 신뢰 목록이 비어 있으면(운영 키가 아직 없는 운영 빌드, D5) 모든 서명이 `kUnknownKey` 다.
5. 한계는 헤더 머리글에 적었다(리더 D3 지시): 공개키가 DLL·워커 실행 파일 안에 있으므로 그것을 바꿀 수 있는 공격자는 키를 바꾼다 — 이 검증이 보장하는 것은 모델 파일(과 사이드카)이 바뀌지 않았다는 것이고 실행 파일의 보호(Authenticode, 설치 디렉터리 권한)는 이 모듈 밖이다. 유효하게 서명된 옛 모델은 받아들인다(D7, 롤백 방어 없음).

## 독립 구현 (이 마일스톤의 핵심 증거)

시험의 서명은 **검증기와 다른 구현**으로 만들었다: `tools/ai/xpe_model_signing.py` 는 Python `cryptography` 49.0.0(OpenSSL)로 같은 형식을 짠다(결정적 서명, RFC 6979). `tests/data/signing/make_signer_vectors.py` 가 8개 벡터를 `tests/signer_vectors.inc` 로 생성한다(세 번 돌려 같은 해시 `44c0c80…`, 결정적). 같은 구현이 서명하고 검증하면 규약(DER 대 r‖s, 바이트 순서, 메시지 조립)이 양쪽에서 똑같이 틀려도 통과한다. **두 구현이 일치한 것이 증거**이고, 실제로 첫 실행에서 Python 서명 8개가 CNG 에 모두 받아들여졌다.

시험 키 두 개(`tests/data/signing/test_key_{1,2}.pem`)는 **의도적으로 커밋**했다 — 비밀이 아니고 제품은 신뢰하지 않는다. `.gitignore` 의 `*.pem` 규칙에 이 두 파일만 예외를 추가했다(대조군: 같은 폴더의 다른 `.pem` 은 계속 무시됨, `git check-ignore` 로 확인).

## 증거 (Evidence)

- ci-ai: `[  PASSED  ] 446 tests.`(M5 시점 431 → +15), 스킵 5, 실패 0, 컴파일 경고 0. `xpe_ai_oom_tests` 8건 통과(검증기가 `XPE_AI_SOURCES` 에 들어가 그 실행 파일에도 컴파일된다). 스텁: `xpe_ai_tests` `[  PASSED  ] 368 tests.`(353 → +15, 스킵 83), `xpe_ai_oom_tests` 6건 + 건너뜀 2. 경고 0. 새 시험은 모델을 열지 않고 CNG 만 쓰므로 스텁에서도 그대로 돈다.
- 로컬 doxygen 1.12.0(CI 순서, `WARN_AS_ERROR`): `exit=0`, `: (warning|error)` 줄 0, `check_header_docs.py` 0 findings, 린트 0 error. (검증기 헤더는 `modules/ai/src` 의 내부 헤더라 Doxyfile 입력이 아니다 — 이번 커밋은 공개 헤더를 바꾸지 않았다.)
- 시험 15건(`test_ai_model_signer.cpp`):
  - 대조군: 모든 벡터(8개: 두 역할, 사이드카 있음·없음·빈 것, 키 2, 3 MiB 모델, 64 MiB 청크를 넘는 모델, 경계 벡터 둘) 검증됨 / 키 id 가 Python 이 서명에 쓴 것과 일치 / 서명한 키만 통과하고 그 키가 신뢰 목록에 있을 때만 통과, 빈 목록은 아무것도 신뢰하지 않음
  - 한 바이트 변조 **전수**: 모델의 768바이트 전부, 사이드카 34바이트 전부, `.sig` 78바이트 전부(각 위치마다 맞는 이유: 매직 → malformed, 버전·알고리즘 → unsupported, 키 id → unknown key, r‖s → bad signature). 모델이 한 바이트 길거나 짧거나 비어 있음
  - 서명이 묶는 것: 역할 바꿔치기(`bodypart`, 대소문자, 접두·접미, 빈 문자열), **사이드카 없음/빈 것/있음이 서로 다른 것**(양방향), 모델·사이드카 사이 경계 이동(`AB|C` 와 `A|BC` 가 서로의 서명으로 통과하지 않음)
  - 크기: 3 MiB 와 64 MiB+5 모델 검증, 청크 경계 앞뒤 바이트 변조 거부 / 상한을 넘는 크기는 1바이트짜리 버퍼로도 읽기 전에 `kTooLarge`
  - 실패 닫힘: 곡선 위가 아닌 키 → `kVerifierError` / 모든 상태가 서로 다른 비어 있지 않은 이유 문구
- 반증 17개(`m1_arms_out.txt`), 매번 빌드 성공, 원본 바이트 동일 복원, 대조군 15/15: A1 도메인 접두 / A2 역할 / A3 역할 길이 / A4 모델 길이 / A5 사이드카 없음=빈 것 / A6 사이드카 바이트 / A7 사이드카 길이를 해시에서 뺌 — 각각 여러 시험 빨강(A5 는 `NoSidecar…` 포함). A8 키 id 를 찾지 않음, A9 버전, B1 알고리즘, B3 매직 검사 제거, B2 긴 서명 파일 허용 — 각각 해당 시험만 빨강. B4 나쁜 서명을 ok 로 → 9건 빨강. B5 두 번째 청크가 한 바이트를 되풀이 → 큰 모델 시험만 빨강(3 MiB 시험은 한 청크라 못 잡고 64 MiB+5 벡터가 잡는다). B6 상한이 너무 큼 → 상한 시험만 빨강. B7 잘못된 키를 bad signature 로 → 해당 시험만 빨강.

## 기준 (Baseline)

같은 트리 `dev/postprocess`(f0f98b44 위), `cmake --build --preset ci-ai` 와 `ci-post`, 이 실행의 출력.

## 미검증 (Gaps)

- **반증 B8(해시 실패를 통과로 읽기)은 빨개지지 않았다**(`red(0)`): CNG 해시가 실패하는 상황을 만들 방법이 시험에 없다(주입 지점이 없다). 그 줄(`if (!h.Finish(digest)) return kVerifierError`)은 코드로만 확인했고 시험으로 증명하지 못했다. 같은 이유로 `Provider`/`Sha256` 의 CNG 실패 가지 전부(공급자 열기·해시 생성 실패)가 시험 밖이다.
- **반증 A8 은 처음에 컴파일되지 않았다**(상수 조건 경고가 오류로 처리됨, `m1_arms_out_first_run.txt` 의 A8 행 `build_ok=False`). 런타임 조건으로 다시 짜서 빨개지는 것을 확인했다.
- 상한 정확히 1 GiB 의 경계는 시험하지 않았다(해시에 수 초가 걸린다). 1 GiB+1 이 거부되는 것까지만 확인했다.
- 검증기는 아직 아무 데서도 호출되지 않는다: 세션 생성·사이드카 읽기·워커·알림·반환 코드는 M3·M4 의 일이다. 지금 이 모듈이 막는 공격은 없다.
- 운영 신뢰 목록(D5)은 만들지 않았다. 검증기는 목록을 인자로 받는다.
- ECDSA 서명의 "낮은 s" 정규화는 요구하지 않았다: 높은 s 서명도 받아들인다(가단성). 서명 파일의 바이트 동일성에 기대는 곳이 없으므로 위험으로 보지 않았으나 시험으로 고정하지 않았다.

## 잔여 위험

- 키가 DLL 에 있다는 한계(위 5).
- Python `cryptography` 가 이 PC 에 있다는 전제에 벡터 재생성이 기댄다. 생성 결과(`signer_vectors.inc`)는 커밋되므로 시험 실행에는 Python 이 필요 없다.
