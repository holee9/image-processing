# QA-B-52 게이트 보고서 — 선언 치수 대 실제 크기, post 모듈 전수

**카드**: QA-B-52 (#142 #120)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-52/`
**커밋 1건**: `5b86d90` — **제품 코드 변경 0**
**선행**: `git merge origin/main` 완료 (B-51 병합 `0481609` 포함)

---

## 1. 전수 — 판독이 아니라 실행으로

**방법**: 버퍼 바로 뒤에 `PAGE_NOACCESS` 가드 페이지를 두고, `dataSize` 를 선언의
절반으로 준다. 약속된 이미지를 읽는 함수는 가드 페이지에서 접근 위반을 내고,
`__except` 가 그것을 잡아 **over-read** 로 기록한다 — 테스트 프로세스를 죽이지 않고
"넘겨 읽는가" 가 관측된다. 선언 `64×64 FLOAT32 = 16384 B`, 준 것 `8192 B`.

**대조군을 먼저 돌린다.** 같은 서술자에 **일관된** `dataSize` 로 한 번 호출해,
그 함수가 정상 입력을 받아들이는지부터 확인한다. 전부 거절하는 함수라면 짧은 쪽 거절이
`dataSize` 검사를 증명하지 않기 때문이다 — B-40 에서 8×8 픽스처가 크기로 거절돼 검증자를
credit 할 뻔했던 그 형태다.

### 1.1 결과표 (`_probe_post.log`, `_probe_ai.log` — 모두 BUILD=0)

| 모듈 | export | 대조군 rc | 짧은 rc | over-read | api-spec #123 대조 |
|---|---|---|---|---|---|
| enhance_basic | `xpe_log_transform` | 0 | **-1** | no | 일치 |
| enhance_basic | `xpe_log_inverse` | 0 | **-1** | no | 일치 |
| enhance_basic | `xpe_noise_reduce` | 0 | **-1** | no | 일치 |
| enhance_basic | `xpe_noise_estimate_sigma` | 0 | **-1** | no | 일치 |
| enhance_basic | `xpe_contrast_enhance` | 0 | **-1** | no | 일치 |
| enhance_basic | `xpe_edge_enhance` | 0 | **-1** | no | 일치 |
| enhance_basic | `xpe_calc_exposure_index` | -3¹ | **-1** | no | 일치 |
| enhance_advanced | `xpe_multiscale_process` | 0 | **-1** | no | 일치 |
| enhance_advanced | `xpe_fractional_process` | 0 | **-1** | no | 일치 |
| enhance_advanced | `xpe_detect_collimation` | 0 | **-1** | no | 일치 |
| enhance_advanced | `xpe_calc_exposure_index` | 0 | **-1** | no | 일치 |
| display | `xpe_apply_modality_lut` | 0 | **-1** | no | 일치 |
| display | `xpe_apply_voi_lut` | 0 | **-1** | no | 일치 |
| display | `xpe_apply_presentation_lut` | 0 | **-1** | **측정 안 함**² | rc 는 일치 |
| ai | `xpe_bodypart_recognize` | -3³ | **-1** | no | 일치 |
| ai | `xpe_dl_denoise` | -3³ | **-1** | no | 일치 |
| ai | `xpe_bone_suppress` | — | **-1** | no | 일치 |
| ai | `xpe_stitch_estimate_size` | 0 | **-1** | no | 일치 |
| ai | `xpe_stitch_images` | — | **-1** | no | 일치 |

¹ 평균 화소값이 0 이라 `PROCESSING_FAILED`. **입력 거절이 아니므로** 대조군 조건
(`≠ INVALID_INPUT`)은 성립한다. "정상 처리됐다" 는 증명하지 않는다.
² §2.2. ³ stub 빌드의 추론 실패. ¹과 같은 이유로 대조군 조건은 성립한다.

**판정: 19개 export 전부 `XPE_ERR_INVALID_INPUT`, over-read 0건 — api-spec #123 과 일치.
따라서 제품 코드를 고치지 않았다**(카드 2항, B-41 방식).

### 1.2 대조군이 실제로 잡은 오독 4건

첫 실행에서 네 함수가 `-1` 을 돌려줘 "가드가 있다" 로 읽힐 뻔했다. 대조군이 **같은 -1** 을
돌려주는 것을 보고서야 이유가 드러났다:

| 함수 | 진짜 거절 사유 | dataSize 와 무관 |
|---|---|---|
| `xpe_multiscale_process` | `meta == nullptr` | O |
| `xpe_calc_exposure_index` (advanced·basic) | 같음 | O |
| ai 전체 | `xpe_ai_init` 미호출 → `NOT_INITIALIZED(-6)` | O |
| `xpe_stitch_estimate_size` | `partCount < 2` | O |

**대조군이 없었다면 이 네 건이 "dataSize 검사 통과" 로 보고됐을 것이다.**

### 1.3 census by name 은 census 가 아니다

착수 시 `xpe_data_size_is_consistent` 로 grep 해 "ai 와 gsvg 에는 검사가 없다" 로
분류했는데, **ai 는 같은 검사를 이름 없는 인라인 블록으로 갖고 있었다**
(`ai.cpp:158-171`, `validateImageBuffer` 안). 심볼 이름으로 센 목록은 동작으로 센 목록이
아니다 — 실행 관측이 아니었으면 ai 를 결함으로 잘못 올릴 뻔했다.

---

## 2. gsvg — 성격이 다르고, 넘겨 읽는다

### 2.1 관측

gsvg 는 `XpeImageBuffer` 가 아니라 **생 포인터와 치수**를 받는다. `dataSize` 필드 자체가
없으므로 **#123 이 붙을 자리가 없다.** 헤더가 이미 결과를 적어 두었다:

> "Its length is NOT validated -- it is trusted to hold width * height entries,
> and a shorter map is read past its end." (`gsvg_api.h`, gainMap)

| 케이스 | 약속 | 실제 매핑 | rc | over-read |
|---|---|---|---|---|
| 대조군 (전 길이) | 4096 px | 4096 px | 0 | no |
| `src` 절반 | 4096 px | 2048 px | **0** | **YES** |
| `gainMap` 절반 (vignette 켬) | 4096 | 2048 | **0** | **YES** |

**둘 다 넘겨 읽고 성공을 돌려준다.** `KnownDivergence_` 로 현행만 고정했다 —
길이 인자 추가는 **공개 시그니처 변경**이라 카드 범위 밖이다. **리더 결정 요청.**

### 2.2 gainMap 에서 한 번 잘못 읽을 뻔했다

기본 핸들로는 over-read 가 **나오지 않았다.** 그대로 적었으면 "헤더가 틀렸다" 가 됐을
것이다. 틀리지 않았다 — vignette 단계는 **config 플래그가 켜져야** 돌고(`gsvg.cpp:218`),
그 플래그의 기본값이 **false** 다(`gsvg.cpp:190`). 헤더 문장은 옳고, **적히지 않은 전제**가
있었을 뿐이다. 플래그를 켜고 다시 재니 넘겨 읽는다. 경위를 테스트 주석에 남겼다.

### 2.3 presentation_lut 은 가드 페이지를 겨눌 수 없다

REQ-DISP-019 대로 float32 버퍼를 `std::free` 하고 uint16 을 `std::malloc` 해
갈아끼운다(`presentation_lut.cpp:32-61`). **VirtualAlloc 버퍼를 `std::free` 에 넘기는 것은
미정의 동작**이라, 거기서 나는 크래시는 제품이 아니라 **프로브의 잘못**이 된다.
반환 코드만 검사하고 over-read 는 **"측정 안 함"** 으로 로그와 주석에 명시했다.
덮지 않고 한계로 적는다.

---

## 3. 반증 — 가드가 막고 있는 것은 잘못된 숫자가 아니라 메모리 손상 (`_falsify.log`)

`enhance_advanced` 의 `data_size_is_consistent` 를 `* 4u >=` 로 **약화**(삭제 아님):

```
===BUILD=0===                    <- 빌드 성공
xpe_multiscale_process CONTROL dataSize=16384B rc=0 overread=no
xpe_multiscale_process declared=16384B dataSize=8192B rc=0 overread=YES
===EA_EXIT=-1073740940===        <- 0xC0000374 STATUS_HEAP_CORRUPTION
```

세 가지가 한 번에 나온다:

1. **프로브가 민감하다** — rc 가 `-1 → 0` 으로 뒤집히고 **over-read 가 YES 로 잡힌다.**
   가드가 없는 모듈이 있었다면 이 프로브가 잡았을 것이다.
2. **결과는 잘못된 숫자가 아니라 메모리 손상이다.** `__except` 가 접근 위반을 잡은 뒤에도
   프로세스가 **힙 손상으로 사망**했다 — 함수가 짧은 버퍼를 통해 이미 썼기 때문이다.
3. **`__except` 는 완전한 격리가 아니다.** over-read 를 *관측*할 수는 있어도 그 호출이
   남긴 손상까지 되돌리지는 못한다. 프로브의 한계로 적는다.

약화 형태를 고른 이유는 B-49~B-51 과 같다(조건 삭제 → 변수 미사용 → `/WX` 파손 →
낡은 바이너리가 통과를 찍음). 반증 뒤 원복했다.

---

## 4. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 전수 대상 | 19 export (7+4+3+5) | §1.1 |
| 짧은 dataSize 반환 | **전부 `-1`** | `_probe_post.log`, `_probe_ai.log` (BUILD=0) |
| over-read | **0건** (presentation_lut 1건은 미측정) | 같은 로그 |
| gsvg | src·gainMap **둘 다 over-read**, rc=0 | `_probe3.log` / `_probe_post.log` |
| 반증 | BUILD=0, rc 뒤집힘 + over-read=YES + 0xC0000374 | `_falsify.log` |
| 이전 ctest | 465 / 209 / 173 | QA-B-51 `_verify.log` |
| 현재 ctest | **472 / 211 / 173** (신규 7건) | `_verify.log` |
| 빌드 경고 | 0 (`grep -c "warning C"`) | `_verify.log` |
| 제품 코드 변경 | **0** | `git status` = 테스트 5개 + CMakeLists 5개 |

---

## 5. 미검증 (Gaps)

- **`xpe_apply_presentation_lut` 의 over-read 는 측정하지 않았다**(§2.3). 할당자 충돌이
  이유이고, 재려면 그 함수만을 위한 malloc 기반 가드(예: 페이지 경계에 맞춘 `_aligned_malloc`
  + 수동 보호)가 필요하다 — 이 카드에서 만들지 않았다.
- **`dataSize == 0`(미지정) 경로는 건드리지 않았다** — 기존 계약이고 카드가 제외했다.
- **선언보다 **큰** dataSize 는 재지 않았다.** #123 이 "크면 무방" 이라 명시하지만 실행으로
  확인하지 않았다.
- **UINT16 형식은 재지 않았다.** 전부 FLOAT32 로만 쟀다. 두 형식의 분기가 같은 헬퍼를
  지나므로 같을 것으로 **판독**되지만, 측정은 아니다.
- **`xpe_stitch_images` 의 출력 버퍼 쪽 짧은 dataSize 는 별개 계약**(B-42 가 다룬
  `BUFFER_TOO_SMALL` 축)이고 이 카드에서 재지 않았다.
- **ai 는 stub 빌드다.** ONNX 실경로에서 같은 입력이 어떻게 되는지는 여전히 미검증(#130).
- **가드 페이지 프로브는 Windows 전용**이다. 다른 플랫폼에서는 `#if defined(_WIN32)` 로
  전체가 비활성이라 아무것도 검증하지 않는다.

---

## 6. 잔여 위험 (Residual-risk)

- **gsvg 의 over-read 는 고쳐지지 않은 채 남아 있다.** 호출자가 길이를 틀리면 조용히 넘겨
  읽고 성공을 돌려준다. 헤더가 경고하고 있으나, §2.2 가 보여 준 대로 **헤더 문장에는
  적히지 않은 전제가 있을 수 있다** — 문서는 통제가 아니다.
- **"post 전체가 안전하다" 로 읽으면 안 된다.** 측정한 것은 **FLOAT32, Windows, stub
  빌드, 짧은 쪽 한 방향**이다. §5 의 축들은 열려 있다.
- **프로브 자체가 제품을 손상시킬 수 있다**(§3 의 3번). 가드가 없는 함수를 이 프로브로
  때리면 over-read 를 기록한 뒤 프로세스가 죽을 수 있다 — CI 에서 그 형태로 실패하면
  "프로브 오류" 가 아니라 **가드 부재의 신호**로 읽어야 한다.
- **헬퍼가 5개 파일에 복제돼 있다.** 모듈 테스트 디렉터리가 공유 유틸리티 타깃을 갖고
  있지 않고 `xpe_common` 은 다른 레인 소유라 택한 것인데, 한쪽만 고치면 갈라진다.
  주석에 사유를 적어 두었다.
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` | 환경 + 세 프리셋 재빌드·전체 ctest |
| `_b52.bat` / `_b52ai.bat` | ci-post 4개 타깃 / ci-ai 프로브 실행 |
| `_probe_ea.log` | enhance_advanced — 대조군이 오독을 잡은 첫 실행 포함 |
| `_probe2.log` / `_probe3.log` | display / gsvg 단계별 |
| `_probe_post.log` | ci-post 최종 (enhance_basic·advanced·display·gsvg) |
| `_probe_ai.log` | ai 최종 (5 export) |
| `_falsify.log` | 가드 약화 — rc 뒤집힘 + over-read=YES + **0xC0000374** |
| `_verify.log` | 최종 472 / 211 / 173, 경고 0 |
