# QA-B-206 M1c — Codex #110 보류 2건 (J2K 압축 길이, 안쪽 작성기 술어) (#251)

회신 전문: 메인 저장소 `.moai/state/codex-archive/110.md`. 앞선 커밋: M1 `df67fe3a`, M1b `cc19ff35`, M2 `a7b50631`.

## 0. 결과

| 항목 | 결과 |
|---|---|
| (1) 높음: J2K 압축 결과의 32비트 축소 | `narrow_fragment_length` 한 곳이 `Uint32` 로 줄이는 유일한 통로다. 압축 결과가 0xFFFFFFFE 바이트를 넘으면 쪼개지 않고 거부한다. `writeJ2K` 가 압축 직후(데이터셋을 만들기 전, 파일이 생기기 전) 확인해 `XPE_ERR_PROCESSING_FAILED`. `setJ2KPixelData` 도 같은 함수로 자기 검사를 한다 |
| (2) 낮음: 안쪽 작성기 술어 | `DicomWriter::write`·`writeJ2K` 가 공개 입구와 같은 형식·비트 술어(`image_format_is_writable`, `image_bits_are_writable`)를 쓴다. 공개 입구의 형식 검사도 같은 술어를 쓰도록 모았다 |
| 시험 | `DicomImageLimits` 2개(큰 `size_t` 로 압축기 없이), 새 실행 파일 `test_dicom_writer_inner` 4개(안쪽 작성기를 직접 호출) |
| 반증 | 10팔 중 8팔 터짐, 2팔은 관측(두 검사가 서로를 대신함), 소스 전부 바이트 단위 복원 |
| 검증(관측) | ci-dicom 전체 ctest(성능 시험 제외) 343 통과·0 실패, doxygen 경고 0, 헤더 점검 20개 0건 |

## 1. 높음 — 압축 결과의 길이

**소스.** `DicomImageLimits.h` 의 `narrow_fragment_length(uint64_t bytes, uint32_t* out)`: `bytes ≤ 0xFFFFFFFE`(OB/OW·item 길이 필드의 최대 짝수, `0xFFFFFFFF` 는 "정의되지 않은 길이") 일 때만 `*out` 을 쓰고 참을 돌려준다. 거부하면 `*out` 은 그대로다. 홀수 길이(예 0xFFFFFFFD)는 파일이 짝수로 채우므로 한계 안이다.
- 방향: 카드의 리더 결정대로 **나누지 않고 거부**. 압축 결과가 원본보다 작다는 보장이 없어서 원본 크기 술어(`image_size_is_representable`)로는 덮이지 않는다는 Codex 의 지적을 따라 압축 후 실제 길이로 확인한다.
- 오류 코드: `XPE_ERR_PROCESSING_FAILED`. 압축기가 만든 결과의 문제이고 호출자 입력의 문제가 아니라서 압축 실패와 같은 코드다. `xpe_dicom_write_j2k` 의 헤더 `@return` 를 "압축이 실패하거나 압축 결과가 한 fragment 로 표현되지 않을 때" 로 고쳤다(카드의 "헤더 오류 코드 문구와 일치하는지" 확인 결과: 기존 문구는 "if J2K compression fails" 뿐이라 문구를 넓혔다).
- 위치: `writeJ2K` — 압축이 비어 있지 않은 것을 확인한 직후. 이후 `populateDataset`·`saveFile` 이 있으므로 파일은 만들어지지 않는다.

**시험** (`DicomImageLimits.AJ2kFragmentLengthIsNarrowedOnlyWhenItFitsAndNeverWraps`, `test_dicom_writer.cpp`): 0, 1, 0xFFFFFFFD(홀수, 통과), 0xFFFFFFFE(통과), 0xFFFFFFFF(거부), 2³²(거부 — 32비트 캐스트는 0), 2³²+1(거부 — 캐스트는 1), 0x1FFFFFFFF, 2⁴⁰, UINT64_MAX(거부), 거부 때 출력이 그대로, 출력이 NULL 일 때. 압축기도 큰 버퍼도 쓰지 않는다.
- **끝에서 끝까지의 시험은 없다.** 4 GiB 가 넘는 압축 결과를 만드는 입력을 이 기계에서 실제로 만들지 않았다(Codex 도 같은 한계를 적었다). `writeJ2K` 가 거부를 파일 생성 전에 한다는 것은 코드의 순서(압축 → 이 검사 → 데이터셋 → 저장)와 술어 시험으로 보증하고, 실행으로는 관측하지 않았다(Gap).

## 2. 낮음 — 안쪽 작성기

**소스.** `DicomWriter::write`·`writeJ2K` 시작에서 `image_format_is_writable(img->format) && image_bits_are_writable(...)` 를 확인하고 아니면 `XPE_ERR_INVALID_INPUT`. 크기 술어는 M1b 에서 이미 안쪽에 있다.

**시험** (`test_dicom_writer_inner.cpp`, 새 실행 파일): `DicomWriter` 는 DLL 에서 내보내지 않으므로 그 소스를 시험 실행 파일에 직접 컴파일한다(`modules/dicom/CMakeLists.txt` 에 `test_dicom_writer_inner` 추가). 통제(정상 영상은 두 안쪽 작성기가 쓴다), 비트 서술자 5경우(8/8, 0/16, 32/16, 16/0, 16/17)와 UINT8·FLOAT32 형식을 두 함수에 직접 → `INVALID_INPUT`·파일 없음, 65535×32769 + 128 바이트 버퍼 → 거부와 버퍼 미독(안쪽 크기 검사).

## 3. 반증 (`arms_m1c.py.txt`, `arms_m1c_out.txt`)

| 끈 방어 | 결과 |
|---|---|
| 한계 검사 제거(맨 캐스트로 축소) | 길이 시험 빨강 |
| 한계가 하나 낮음(`>=`) | 길이 시험 빨강 |
| 정의되지 않은 길이 표지(0xFFFFFFFF)까지 허용 | 길이 시험 빨강 |
| 거부하면서도 출력을 씀 | 길이 시험 빨강 |
| 안쪽 `write` 의 형식·비트 검사 제거 | 안쪽 두 시험 빨강 |
| 안쪽 `writeJ2K` 의 형식·비트 검사 제거 | 안쪽 두 시험 빨강 |
| 안쪽 `write` 가 형식만 검사 | 서술자 시험 빨강 |
| 안쪽 `write` 가 비트만 검사 | 형식 시험 빨강 |
| `writeJ2K` 의 압축 직후 검사 제거(`setJ2KPixelData` 의 검사는 유지) | 빨간 시험 없음 — 관측: 두 곳이 같은 함수를 부르므로 한쪽이 다른 쪽을 대신한다(깊이 방어). 압축기가 큰 결과를 내는 입력이 없어 이 중복을 시험으로 가를 수 없다 |
| `setJ2KPixelData` 의 검사 제거(압축 직후 검사는 유지) | 빨간 시험 없음 — 같은 이유 |

## 4. 영향·비고
- 공개 시그니처 불변. 동작 변화: ① 압축 결과가 0xFFFFFFFE 바이트를 넘으면 `PROCESSING_FAILED`(이전엔 길이가 조용히 줄어든 파일), ② 안쪽 작성기를 직접 부르는 코드가 있으면 형식·비트 위반에 `INVALID_INPUT`(지금 호출자는 공개 두 함수뿐 — Codex 확인).
- 이번 커밋은 C3 작업(QA-B-207 M1, 미커밋)과 분리했다: `DicomReader.cpp` 의 C3 변경은 치워 두고 검증했고, `dicom_api.h` 는 M1c 부분만 부분 스테이징했다.

## 5. Gap / 잔여 위험
Gap
- 압축 결과가 실제로 4 GiB 를 넘는 입력의 끝에서 끝까지 실행은 관측하지 않았다(§1).
- `writeJ2K` 의 두 검사 중복을 시험으로 가르지 못했다(§3).
- OpenJPEG 가 한 번에 만들 수 있는 최대 출력 크기 자체의 한계는 조사하지 않았다.

잔여 위험
- 압축 결과가 한계를 넘는 영상은 이제 쓰기에 실패한다(전엔 길이가 틀린 파일). 이 크기의 영상을 쓰려는 호출자는 비압축 쓰기도 같은 크기에서 거부되므로(M1b) 어느 쪽으로도 쓸 수 없다.
