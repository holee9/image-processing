# QA-B-182d — J2K 경로의 HighBit 검사 (Codex #51, #235)

Refs #235

## 1. Claim

`BitsAllocated=16, BitsStored=12, HighBit=15` 와 12비트 코드스트림의 J2K 파일이 `XPE_OK` 로 읽혔다. 같은 reader 의 비압축·JPEG Lossless 경로는 이 기술을 거부한다. `checkSupportedImageModule` 의 J2K 분기에 `BitsStored` 범위와 `HighBit == BitsStored - 1` 검사를 더해 디코드 전에 거부한다.

## 2. Evidence

**빨강 → 초록** (`red_before_fix.txt`, `green_after_fix.txt`)

- 수정 전: `J2kScope_HighBitMustBeBitsStoredMinusOne` 에서 불일치 6건(16/12/15, 16/12/12, 16/16/14, 8/8/6, 8/8/15, 16/12/0xFFFF)이 모두 `rc=0` 으로 읽혔다. 정상 대조군 3건(8/8/7, 16/12/11, 16/16/15)은 통과했다.
- 수정 후: 같은 시험 1건 통과 (`[  PASSED  ] 1 test.`).
- 시험은 거부 건마다 출력 버퍼 미변경, 핸들이 메타데이터를 계속 제공함, 알림에 `HighBit` 가 이름으로 들어 있음을 단언한다.

**반증** (`arms_summary.txt`, `arm_check_removed.txt`)

```
check removed:                      build_ok=True passed=82  red=[DicomReaderTest.J2kScope_HighBitMustBeBitsStoredMinusOne]
check restored (full reader suite): build_ok=True passed=83  red=[]
```

새 검사를 `if (false)` 로 바꾸면 이 시험 한 건만 빨강이고 나머지 82건은 초록이다. 원복 후 파일이 바이트 단위로 같음을 단언했다.

**전체**: `ctest --test-dir build/ci-dicom` (`ctest_ci_dicom.txt`): `100% tests passed, 0 tests failed out of 218`, 건너뜀 1건(`DicomNetworkTest.CancelCStore_TerminatesOperation`, 이 카드와 무관한 기존 skip). 성능 시험 이름은 `-E` 로 제외했다.

**변경**: `modules/dicom/src/DicomReader.cpp` (J2K 분기 7줄), `modules/dicom/tests/test_dicom_reader.cpp` (시험 1건).

**알림 문구**: `BitsStored %u with HighBit %u for JPEG 2000 (HighBit must be BitsStored - 1, BitsStored 1..BitsAllocated %u)`. 비압축 경로의 문구와 같은 형식(`BitsStored %u with HighBit %u (...)`)이다.

## 3. 경로 × 태그 표 (카드 요구)

`checkSupportedImageModule` 와 그 뒤의 경로별 코드를 읽어 만들었다 (이 줄 수정 후 기준).

| 태그 | 비압축 | JPEG Lossless (.57/.70) | JPEG 2000 |
|------|--------|-----------------------|-----------|
| 다섯 태그 모두 존재(Type 1) | `checkSupportedImageModule` 공통 | 공통 | 공통 |
| SamplesPerPixel == 1 | 공통 (UNSUPPORTED) | 공통 | 공통 |
| PixelRepresentation == 0 | 공통 (UNSUPPORTED) | 공통 | 공통 |
| BitsAllocated | == 16 (UNSUPPORTED) | == 16 | 8 또는 16 (UNSUPPORTED) |
| BitsStored 범위 | 1..16 (UNSUPPORTED) | 1..16 | **이번에 추가**: 1..BitsAllocated (INVALID) |
| HighBit == BitsStored−1 | 있음 (UNSUPPORTED) | 있음 | **이번에 추가** (INVALID) |
| 비트스트림과 태그 대조 | 해당 없음(압축 아님) | **치수만** (`jpeg_frame_dimensions`) | 치수·성분 수·부호·정밀도 (`checkJ2kCodestreamShape`) |

남은 빈 칸 후보 (**수정하지 않았고 재지도 않았다**):

1. **JPEG Lossless 는 비트스트림 정밀도(SOF 의 P)를 BitsStored 와 대조하지 않는다.** 치수만 대조한다. J2K 는 `c.prec != bitsStored` 를 거부한다. 같은 모양의 한 칸 빠짐이다. 다만 JPEG LL 은 DCMTK 가 디코드하므로 불일치가 어떻게 나타나는지는 모른다. 실제 파일로 재 보지 않았다.
2. **오류 코드가 갈린다.** 비압축·JPEG LL 의 BitsStored/HighBit 불일치는 `XPE_ERR_UNSUPPORTED_FORMAT`, J2K 는 카드 지시대로 `XPE_ERR_DICOM_INVALID` 다. 같은 속성 불일치가 경로에 따라 다른 코드를 낸다. 기존 시험(`Scope_BitsStoredAndHighBitMustDescribeLowBitsOfA16BitWord`)이 UNSUPPORTED 를 고정하고 있어 이번에 맞추지 않았다. 어느 쪽이 옳은지는 리더 결정이다.

## 4. Baseline-attribution

모든 수치는 이 실행의 출력이다. 명령: `build/g182b-t.bat <필터> <절대경로>` (시험 exe `build/ci-dicom/bin/test_dicom_reader.exe`), `ctest --test-dir build/ci-dicom`. 트리: HEAD `3311ccc6` 위의 작업 트리.

## 5. Gaps

- JPEG LL 정밀도 대조(§3 후보 1)와 오류 코드 통일(후보 2)은 측정·수정하지 않았다.
- 시험 파일은 OpenJPEG 로 인코딩한 합성 코드스트림이다. 실기기 J2K 파일은 쓰지 않았다.
- 시험은 `BitsAllocated` 8 과 16 만 쓴다. J2K 분기가 받는 값이 그 둘뿐이라서다.
- `clients/gui` 가 이 알림 문구를 고정한 곳이 있는지는 리더의 grep(0건)에 의존한다. 이 세션에서는 grep 하지 않았다.

## 6. Residual-risk

- 알림 문구는 클라이언트와의 계약이다. 새 문구는 기존 BitsStored/HighBit 문구와 형식이 같아 정규식 앵커 시험이 `HighBit` 로만 매칭하면 깨지지 않지만, 확인하지 않았다.
- ctest 한 번의 결과다. 시험 순서 의존은 이 실행으로는 보이지 않는다.
