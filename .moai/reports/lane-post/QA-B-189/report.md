# QA-B-189 — 문서 초안 두 개: api-spec §11 을 실제 수출 함수로, RTM-DICOM 합계 재계산

카드: QA-B-189 (보고서만, 문서·코드 변경 없음) · 관련: #235 · 브랜치 `dev/postprocess`
문서는 리더 소유다. 아래는 리더가 옮길 초안이다.

## 결론 먼저

1. **api-spec §11.1~11.11 중 6개 절은 한 번도 수출된 적 없는 함수를 적고 있다**(`xpe_dicom_read`, `xpe_dicom_query_dimensions`, `xpe_dicom_read_tag_string`, `xpe_dicom_set_tag_string`, `xpe_gsps_create`, `xpe_gsps_apply`). 실제 수출 10개 중 5개(`open`, `read_image`, `get_metadata`, `close`, `cancel`)는 자기 절이 없다. 나머지 5개 중 `cfind_mwl`·`validate` 는 시그니처가 맞고, `write`·`write_j2k`·`cstore` 는 시그니처가 다르다.
2. **더 큰 발견 — GSPS(FR-DCM-301~306, TC-301~306)는 RTM 에서 ✓ 인데 구현도 시험도 저장소에 없다.** `xpe_gsps_*` 는 `modules` 이력에 한 번도 나타난 적이 없다. 이 사실은 문서 문구 수정으로 끝나지 않고 리더 결정이 필요하다(§3).
3. **RTM 합계가 틀린 곳은 6곳이다**(§2): `59/59` → `57/57`, §6.1 의 56 둘 → 57, `SRS → Test 67` → 65, §6.2 SWU-4.1 기능 20 → 21, 총계 56 → 57. 재계산할 수 없는 곳이 두 군데다(§6.2 안전 요구 열 `16`, §6.3 `72`).

## 1. api-spec §11

### 1.1 수출 목록은 DLL 에서 뽑았다

`build\ci-dicom\bin\xpe_dicom.dll` 을 이 트리에서 다시 빌드한 뒤 `dumpbin /exports` 로 뽑았다(`dll_exports.txt`, `build\g189-exports.bat` 의 출력 `===BUILD=0===`, `===DUMPBIN=0===`). 스크립트 `api_names_source.txt` → `api_names_out.txt`:

- 수출 10개 = 헤더 `XPE_API` 선언 10개, 양쪽 차집합 모두 없음. 대조군: dumpbin 요약의 `number of names` 10 과 파서가 찾은 수가 같다(스크립트가 단언).
- 문서 속 `xpe_dicom_*`·`xpe_gsps_*` 식별자 17종(대조군: 그중 수출된 것 10 = 수출 전체). 수출 10개 모두 문서 어딘가에 나온다.

### 1.2 절별 표 (처분)

| 문서 절 (줄) | 문서 이름 | 실제 이름 | 시그니처 차이 | 반환 코드 차이 | 처분 |
|---|---|---|---|---|---|
| 11.1 (L1364) | `xpe_dicom_read(filePath, imgOut, metaOut)` | 수출 없음. 대체: `xpe_dicom_open` → `xpe_dicom_read_image` → `xpe_dicom_get_metadata` → `xpe_dicom_close` | 한 호출이 4단계로 쪼개져 있다 | 문서: `IO_FAILED, INVALID_INPUT, UNSUPPORTED_FORMAT, OUT_OF_MEMORY`. 실제는 단계마다 다르다(§1.3) | **삭제하고 11.1~11.4 새로 씀** |
| 11.2 (L1379) | `xpe_dicom_query_dimensions` | 수출 없음 | — | — | **삭제**(이력상 한 번도 구현된 적 없음, §1.4) |
| 11.3 (L1395) | `xpe_dicom_read_tag_string` | 수출 없음 | — | — | **삭제** |
| 11.4 (L1410) | `xpe_dicom_write` | `xpe_dicom_write` | 문서 4번째 인자 `configJsonOrNull` 은 헤더에 없다(실제 3인자) | 문서는 `CONFIG_INVALID` 를 적었으나 실제 반환 목록에 없다. 실제: `INVALID_INPUT, IO_FAILED, PROCESSING_FAILED` | **고침** |
| 11.5 (L1426) | `xpe_dicom_write_j2k` | `xpe_dicom_write_j2k` | 문서 4번째 인자 `float compressionRatio` 는 헤더에 없다(실제 3인자, JPEG 2000 Lossless 만, 비트 정확) | 문서: `INVALID_INPUT, IO_FAILED, PROCESSING_FAILED` — 실제와 같다 | **고침** (손실 압축 서술 삭제) |
| 11.6 (L1442) | `xpe_dicom_set_tag_string` | 수출 없음 | — | — | **삭제** |
| 11.7 (L1457) | `xpe_gsps_create` | 수출 없음 | — | — | **삭제** (§3 결정 대기) |
| 11.8 (L1473) | `xpe_gsps_apply` | 수출 없음 | — | — | **삭제** (§3 결정 대기) |
| 11.9 (L1488) | `xpe_dicom_cstore(filePath, remoteAeTitle, remoteHost, remotePort, localAeTitle)` | `xpe_dicom_cstore(host, port, aet, filePath, timeoutMs)` | 순서·이름·개수는 같고 의미가 다르다: 문서의 `filePath` 가 첫 인자, 실제는 넷째. `localAeTitle`·`remoteAeTitle` 둘 대신 `aet`(호출 AE) 하나, `timeoutMs` 가 추가됨 | 문서: `NETWORK_FAILED, IO_FAILED, INVALID_INPUT`. 실제: `INVALID_INPUT, NETWORK_FAILED, IO_FAILED, PROCESSING_FAILED`(취소 래치) | **고침** |
| 11.10 (L1505) | `xpe_dicom_cfind_mwl` | 같음 | 7인자 전부 같음(스크립트: `same`) | 문서: `…, PROCESSING_FAILED`. 실제 목록과 같음 | **고침(문구만)** — 설명에 남은 `resultsJsonOut` 와 QA-B-39 정정 문장이 낡았다 |
| 11.11 (L1520) | `xpe_dicom_validate` | 같음 | 3인자 같음(스크립트: `same`) | 문서: `OK, INVALID_INPUT, IO_FAILED`. 실제: `OK, INVALID_INPUT, DICOM_INVALID, BUFFER_TOO_SMALL` | **고침(반환 코드)** |
| 없음 | — | `xpe_dicom_open`, `xpe_dicom_read_image`(§11.0 이 규칙만 적음), `xpe_dicom_get_metadata`, `xpe_dicom_close`, `xpe_dicom_cancel` | — | — | **새로 씀** |

§11 밖의 낡은 이름: **L1754–1759**(파일 경로 인자 표)가 `xpe_dicom_read`, `xpe_dicom_read_tag_string`, `xpe_dicom_set_tag_string` 을 계속 적고 있다. 같은 표의 `xpe_dicom_write`, `xpe_dicom_cstore`, `xpe_dicom_write_j2k` 는 수출된다. 표에서 세 줄을 `xpe_dicom_open`(입력 경로)으로 바꾸거나 지운다. §0 인벤토리(L36)는 이미 맞다.

### 1.3 바꿀 문장 초안

**§11 머리글** (L1343~1347 을 대체):

> ## 11. xpe_dicom.dll
>
> DICOM Part 10 file reading, writing, validation, and the network services C-STORE / C-FIND MWL. The DLL exports exactly 10 functions: `xpe_dicom_open`, `xpe_dicom_read_image`, `xpe_dicom_get_metadata`, `xpe_dicom_close`, `xpe_dicom_write`, `xpe_dicom_write_j2k`, `xpe_dicom_validate`, `xpe_dicom_cstore`, `xpe_dicom_cfind_mwl`, `xpe_dicom_cancel`. Reading is a session: open a handle, read the image and the metadata from it, close it. There is no one-shot read, no tag-string accessor and no GSPS function.
>
> Dependencies: xpe_common.dll.

**§11.0 의 `> 주의: …` 인용문**(L1351)은 삭제한다. 이 절의 개정으로 낡은 이름이 없어진다.

**§11.1 `xpe_dicom_open`** (신규):

```c
XPE_API XpeErrorCode xpe_dicom_open(const char* filePath, XpeDicomHandle** outHandle);
```

> **Description**: Opens and parses a DICOM Part 10 file and returns a handle in `*outHandle` (NULL on error). Judges readability only: a file with no Part 10 meta header still opens (a missing Transfer Syntax UID is read as Explicit VR Little Endian); use `xpe_dicom_validate` to judge Part 10 conformance.
> **Thread safety**: Different handles may be used from different threads; one handle must not be used by two threads at the same time (`xpe_dicom_read_image`, `xpe_dicom_get_metadata` and `xpe_dicom_close` on one handle are serialised by the caller).
> **Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT` (NULL argument), `XPE_ERR_IO_FAILED` (file missing or unreadable), `XPE_ERR_DICOM_INVALID` (not a DICOM Part 10 file), `XPE_ERR_UNSUPPORTED_FORMAT` (unsupported Transfer Syntax).

**§11.2 `xpe_dicom_read_image`** (신규, §11.0 의 규칙을 이 절 아래로 옮기고 시그니처 블록을 붙인다):

```c
XPE_API XpeErrorCode xpe_dicom_read_image(XpeDicomHandle* handle, XpeImageBuffer* outImg);
```

> **Description**: Extracts the pixel data of an open file into `outImg` (`XPE_PIXEL_UINT16`). The buffer is allocated by the module and owned by the caller (`xpe_free_image`). On any refusal `outImg` is left untouched; every refusal posts one `XPE_ALERT_ERROR` naming the cause. The refusal rules, the MONOCHROME1 inversion, the Rescale and Modality LUT Sequence handling and the bit masking are the rules listed under 11.2.1 below; the exact alert texts are in `dicom_api.h`.
> **Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_OUT_OF_MEMORY`, `XPE_ERR_DICOM_INVALID`, `XPE_ERR_UNSUPPORTED_FORMAT`.

(§11.0 의 현재 본문 L1353 이하가 그대로 "11.2.1 거부 규칙" 이 된다. 문구는 바꾸지 않는다 — Tc235_·Tc109_ 시험이 그 문구의 근거다.)

**§11.3 `xpe_dicom_get_metadata`** (신규):

```c
XPE_API XpeErrorCode xpe_dicom_get_metadata(XpeDicomHandle* handle, XpeImageMetadata* outMeta);
```

> **Description**: Extracts acquisition metadata from an open file. Missing tags are silently defaulted (empty string / 0.0 / 0); absence is never an error, and a caller cannot tell an absent tag from a present empty one.
> **Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT` (NULL argument), `XPE_ERR_DICOM_INVALID` (the file carries no dataset).

**§11.4 `xpe_dicom_close`** (신규):

```c
XPE_API void xpe_dicom_close(XpeDicomHandle* handle);
```

> **Description**: Closes a session and frees everything it holds. Passing NULL is a no-op. Returns nothing.

**§11.5 `xpe_dicom_write`** (L1410~1422 를 대체):

```c
XPE_API XpeErrorCode xpe_dicom_write(const char* filePath, const XpeImageBuffer* img, const XpeImageMetadata* meta);
```

> **Description**: Encodes `img` and `meta` into a DICOM Part 10 file (Explicit VR Little Endian) with a generated SOP Instance UID and a meta group regenerated from the dataset, so the output satisfies `xpe_dicom_validate`. The dataset is built from `img` and `meta` only: nothing is copied from the file the pixels were read from. Photometric Interpretation is always MONOCHROME2, Presentation LUT Shape IDENTITY, Rescale Slope / Intercept 1 / 0, and no Window Center / Width is written. A pixel value outside the declared BitsStored range is a write failure.
> **Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT` (NULL pointer, empty image, or `dataSize` inconsistent with the dimensions), `XPE_ERR_IO_FAILED`, `XPE_ERR_PROCESSING_FAILED`.

**§11.6 `xpe_dicom_write_j2k`** (L1426~1438 를 대체):

```c
XPE_API XpeErrorCode xpe_dicom_write_j2k(const char* filePath, const XpeImageBuffer* img, const XpeImageMetadata* meta);
```

> **Description**: Like `xpe_dicom_write`, with Transfer Syntax JPEG 2000 Lossless Only (1.2.840.10008.1.2.4.90); the round trip is bit-exact. The codestream is encoded at the declared BitsStored precision. There is no compression-ratio parameter and no lossy mode.
> **Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_IO_FAILED`, `XPE_ERR_PROCESSING_FAILED` (J2K compression failed, or a pixel value above the declared range).

**§11.7 `xpe_dicom_validate`**: 본문 설명은 그대로, **Error codes 줄만** `XPE_OK` (report produced; check `valid`), `XPE_ERR_INVALID_INPUT` (NULL argument or `reportBufLen` 0), `XPE_ERR_DICOM_INVALID` (file cannot be parsed; a report is still written), `XPE_ERR_BUFFER_TOO_SMALL` (required size written as `uint32_t` in the first 4 bytes; the buffer is not valid JSON). `IO_FAILED` 는 헤더의 반환 목록에 없다.

**§11.8 `xpe_dicom_cstore`** (L1488~1502 를 대체):

```c
XPE_API XpeErrorCode xpe_dicom_cstore(const char* host, uint16_t port, const char* aet, const char* filePath, uint32_t timeoutMs);
```

> **Description**: Sends a DICOM file to a remote Storage SCP via C-STORE. `host` may be `"CALLED_AE@hostname"` to name the called AE title (default `ANY-SCP`); `aet` is the calling AE title; `timeoutMs` 0 means no timeout. If the file's meta group names no SOP Class UID the dataset's is used, then DX For Presentation. Returns `XPE_OK` only for RSP status 0x0000.
> **Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_NETWORK_FAILED` (connection failure, timeout, rejection, non-success status), `XPE_ERR_IO_FAILED`, `XPE_ERR_PROCESSING_FAILED` (a cancel is latched; see `xpe_dicom_cancel`).

**§11.9 `xpe_dicom_cfind_mwl`**: 시그니처 블록은 그대로. 설명의 `Results are returned … in resultsJsonOut` 와 `**Corrected 2026-09-11 (QA-B-39):**…` 문장을 지우고 "Results are returned as a JSON array in `outJson` (`[]` when empty). Supported query keys: …" 로 바꾼다. 반환 코드는 `XPE_OK, INVALID_INPUT, NETWORK_FAILED, PROCESSING_FAILED(queryJson 파싱 불가 — 협상 뒤에 판정), BUFFER_TOO_SMALL(outJson 에 아무것도 쓰지 않음)` 으로 한다.

**§11.10 `xpe_dicom_cancel`** (신규):

```c
XPE_API void xpe_dicom_cancel(void);
```

> **Description**: Signals cancellation to an in-progress C-STORE or C-FIND. Thread-safe, callable from any thread, a no-op if nothing is running. The cancel flag is cleared on entry to `xpe_dicom_cstore` and `xpe_dicom_cfind_mwl`, so calling it before an operation does not pre-cancel it; it takes effect only when set during a running operation, and is observed before connecting and just after the association is established. A cancelled operation returns `XPE_ERR_PROCESSING_FAILED`.

**L1754–1759 표**: `xpe_dicom_read` · `xpe_dicom_read_tag_string` · `xpe_dicom_set_tag_string` 세 줄을 `xpe_dicom_open | filePath | input DICOM | "C:\\clinical\\patient_001.dcm"` 한 줄로 합치거나 삭제한다.

동작 서술의 근거: `dicom_api.h` 주석(이 트리의 `dicom_api.h` 를 읽어 옮겼다)과 시험 이름 — grep 으로 확인했다: `Tc235_Monochrome1_{Native,JpegLl,J2k}_InvertedWithInfoAlert`, `Tc235_RescaleSlopeOnly_{Native,JpegLl,J2k}_RefusedInvalid`, `Tc235_ModalityLutSequence_{Native,JpegLl,J2k}_StoredValuesWithWarning`, `Tc109_FrDcm109_Monochrome1IsInvertedAndMonochrome2IsKept_OnEveryPath`, `Tc109_CurrentBehaviour_WriteDoesNotCarryWindowRescaleOrPresentationShapeFromTheSourceFile`, `Issue235Decided_BitsAboveBitsStoredAreMaskedForMonochrome2Too`.

### 1.4 "한 번도 없었다"의 근거

`git log -S<이름> -- modules` 가 여섯 이름 모두 0 건이다. 대조군: 같은 명령이 수출되는 `xpe_dicom_read_image` 에는 20 건을 낸다. 파일 이름·`.def` 포함 `modules` 전체 이력이다. 이 문서가 다른 시점에 존재했던 계획을 적었을 가능성(문서 이력)은 보지 않았다.

## 2. RTM-DICOM 합계

스크립트 `rtm_recount_source.txt` → `rtm_recount_out.txt`. 표 17개, 데이터 행 171개를 모두 SRS ID 열로 FR / SR / 기타로 분류했다. 대조군: **미분류 0, 합계 일치**(스크립트가 단언). 요구사항 모수는 §2 의 PRD→SRS 표 네 개에서 뽑은 **고유 FR ID**다: SWU-4.1 21, SWU-4.2 17, SWU-4.3 6, SWU-4.4 13, **합계 57**(중복 ID 없음).

### 2.1 바꿀 숫자와 줄

| 줄 | 문서의 현재 값 | 재계산 | 바꿀 값 | 근거 |
|---|---|---|---|---|
| L257 | `59/59` (5.1 FR→Test) | 5.1 표 행 57 = 고유 FR ID 57, 모수 57 전부 시험 행 있음 | **`57/57`** | `59/59` 는 첫 커밋(aaab4fcd)에서 이미 행 56 개에 대해 틀려 있었다(`git show` 로 그 시점 56 행 확인). 2026-10-02 의 `52e8a29a` 가 FR-DCM-121 행을 더해 57 이 됐다 |
| L287 | PRD→SRS `56 / 56` | 57 / 57 | **`57 / 57`** | §2 표 네 개 21+17+6+13 |
| L288 | SRS→SAD `56 / 56` | §3.1 범위 네 개가 덮는 FR 57 | **`57 / 57`** | `FR-DCM-101~121` 은 21 개를 덮는데 문서의 SWU-4.1 이 20 이었다 |
| L289 | SRS→SHA `8 / 8` | §4.1 은 8 행이지만 **고유 SR ID 는 7**(SR-DCM-005 행 없음, SR-DCM-002 가 두 행) | 아래 **결정 1** | 행 수와 ID 수가 다르다 |
| L290 | SRS→Test `67 / 67` | 5.1 FR 57 + 5.2 SR 8 = 65 | **`65 / 65`** | 5.2 는 아래 2.2 의 분할 표 때문에 8 이다 |
| L297 | SWU-4.1 기능 `20` | 21 | **`21`**(총계 `25` → `26`) | §2.1 표 21 행(L53~L73). 2026-10-02 에 FR-DCM-121 행이 추가되며 요약이 갱신되지 않았다 |
| L301 | 총계 기능 `56` | 57 | **`57`** | 위 합계 |
| L301, L307 | 안전 `16`, 총계 `72`, Traced `72` | SR 은 8 개, 16 의 출처가 문서에 없음 | 아래 **결정 2** | 재계산 불가 |
| L75·L101·L116·L138·L153·L170 | `21/21 17/17 6/6 13/13 4/4 8/8` | 행 수와 모두 일치 | 변경 없음 | |
| L189 | `8/8` (4.1 coverage 줄) | 행 8 | 결정 1 에 따름 | |
| L277 | `8/8` (5.2) | 3 + 5 = 8 | 변경 없음 | 단 아래 2.2 |

### 2.2 구조 결함 하나 (합계가 아니라 표시)

§5.2 의 표가 `> **Record correction (2026-09-12, leader — QA-B-49)…` 인용문(L269)으로 **두 동강**이 났다. L263 의 표는 3 행(SR-001~003), L271 부터는 헤더·구분선 없이 `| SR-DCM-004 … |` 로 시작해 마크다운이 표로 그리지 않는다(본문 줄이 된다). 인용문을 표 아래로 옮기거나 L271~275 앞에 헤더와 구분선을 다시 쓴다. 이 때문에 스크립트도 두 표로 읽어 `L277 8/8` 이 "바로 위 표 5 행"과 어긋나 보인다(합쳐 8 이므로 숫자는 맞다).

### 2.3 결정이 필요한 둘

**결정 1 — SRS→SHA `8 / 8`.** §4.1 은 SR-DCM-005(MWL 환자 검증)의 행이 없다. HAZ-005("MWL 잘못된 선택")는 SR-DCM-002 의 둘째 행으로 올라 있다. "8" 은 행 수이고 SR ID 는 7 개다. 선택지: (a) `SR-DCM-005 | MWL 환자 검증 | HAZ-005` 행을 추가해 8/8 을 ID 로도 맞춘다, (b) 숫자를 `7 / 8` 로 쓰고 사유를 적는다. 어느 쪽이 설계 의도인지는 SHA 문서를 봐야 한다(이번 보고서는 SHA 를 읽지 않았다).

**결정 2 — §6.2 안전 요구 열 `5 / 5 / 3 / 3` (합 16)과 `72`.** 안전 요구(SR-DCM)는 고유 8 개인데 이 열의 합은 16 이다. 문서 어디에도 SR→SWU 대응표가 없어서 SR×SWU 연결 수인지, 다른 센 방식인지 이 문서만으로 재현할 수 없다(§3.2 의 SAD 절 열에서 SWU 가 명시된 것은 SR-006 → §2.3, SR-007 → §2.1 둘뿐이다). 선택지: (a) 열에 `—` 를 쓰고 총계 행을 기능 요구 57 + 안전 요구 8 = 65 로 고친다(§6.3 의 Traced 도 65), (b) SR→SWU 대응표를 문서에 먼저 추가한 뒤 다시 센다.

## 3. 리더 결정 요청 — GSPS

RTM §2.3 은 FR-DCM-301~306(GSPS IOD 생성·참조·주석·셔터·프리셋·적용) 6 개를 ✓ 로 추적하고, §5.1 은 TC-301~306 을 ✓ 로 적는다. 이 트리에서:

- `modules/dicom/src` 에 PresentationState 소스가 없다(`DicomNetworkSCU`, `DicomReader`, `DicomValidator`, `DicomWriter`, `dicom.cpp`).
- `gsps`·`PresentationState`·`TC-30[1-6]` 를 `modules/dicom` 과 `tests` 에서 찾으면 0 파일. 대조군: 같은 범위에서 `xpe_dicom_open` 은 7 파일.
- `xpe_gsps_*` 는 `modules` 이력에 한 번도 없다(§1.4).

즉 이 6 개 요구는 "추적됨 ✓" 이지만 구현·시험이 없다. `docs/dicom/README.md` L83·L85·L209 도 "GSPS 생성·적용"을 기능으로 적는다. 선택지(리더 결정): (a) 요구 6 개의 상태를 "구현 없음 / 미래 범위"로 정정한다 — 그러면 §6.3 의 `✗ Not Traced`·`⚠ Partial` 도 0 이 아니게 되어 §6.1~6.3 숫자가 또 바뀐다, (b) 구현 카드를 연다. 이번 카드의 §2 숫자는 "행 수" 기준이라 이 결정과 독립이다. 결정은 하지 않았다.

## 4. 증거 (Evidence) 와 기준 (Baseline)

- 빌드: `build\g189-exports.bat` → `===BUILD=0===`, `===DUMPBIN=0===`. 같은 트리(main 병합 후 `dev/postprocess`), ci-dicom 프리셋.
- `dll_exports.txt`(dumpbin 원본), `api_names_source.txt` / `api_names_out.txt`, `rtm_recount_source.txt` / `rtm_recount_out.txt`.
- 문서는 이 트리의 `docs/project/api-spec.md`(1786 줄)·`docs/dicom/RTM-DICOM-001_Requirements_Traceability_Matrix.md`(396 줄) 현재 내용이다. 줄 번호는 이 트리 기준이고, 리더가 문서를 고치는 중이면 밀린다(이름과 절 제목으로 찾을 수 있게 표에 둘 다 적었다).

## 5. 미검증 (Gaps)

- SHA 문서·SAD 문서를 읽지 않았다(결정 1·2 의 의도).
- §11.2 이하의 새 서술이 `dicom_api.h` 주석을 옮긴 것이지, 각 반환 코드를 시험으로 하나씩 다시 확인한 것은 아니다. 특히 `xpe_dicom_write` 의 `IO_FAILED`/`PROCESSING_FAILED` 와 `cstore` 의 `IO_FAILED` 는 헤더 문구를 그대로 인용했다.
- `xpe_dicom_validate` 의 문서 반환 코드 `IO_FAILED` 가 실제로 안 나오는지 코드로 확인하지 않았다. 헤더 주석에 없다는 사실만 근거다.
- §11 밖(예: §1~5, 부록)에 `xpe_dicom_*` 가 더 있는지는 위 식별자 17종 검색의 범위 안에서만 확인했다. 다른 표기(`xpe_dicom` 뒤 대시 등)는 보지 않았다.
- GUI 쪽 낡은 이름은 아래 §6 에 적었고 이 카드의 범위 밖이다.

## 6. 곁다리 발견 (범위 밖, 통보만)

`gui/ImageProcTest/Services/PipelineOrchestrator.cs:31` 의 `RequiredExports = new[] { "xpe_dicom_read", "xpe_dicom_write" }` 는 `GetProcAddress` 로 확인한다(L119). `xpe_dicom_read` 는 수출되지 않으므로 이 경로에서 `xpe_dicom.dll` 은 항상 "missing required exports" 로 실패한다. 같은 목록의 `xpe_display.dll` 이름(`xpe_apply_modality_lut` 등)도 api-spec §10 의 `xpe_modality_lut_apply` 와 다르게 보이나 DLL 에서 확인하지는 않았다. `clients/ImageProcTest/Diagnostics/XpeDicomReadinessProbe.cs` 는 열 개 이름이 모두 실제 수출과 맞다. gui 는 Lane C 소유라 고치지 않았다.

## 7. 잔여 위험 (Residual risk)

- 리더가 §11 을 새로 쓰는 동안 `dicom_api.h` 가 바뀌면 초안이 낡는다(이 초안은 `dicom_api.h` 현재 주석에 맞췄다).
- GSPS 결정이 미뤄지는 동안 RTM 의 ✓ 6 개는 증거 없는 ✓ 로 남는다.
