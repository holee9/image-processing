# QA-B-18 — `xpe_calc_exposure_index` 널 픽셀 버퍼 거절 + 비정사각형 검증 (#122 / #121 후속)

**레인**: Lane B (`dev/postprocess`)
**결과**: RED → GREEN. 네 진입점의 널 데이터 응답이 `-1` 로 일치. 스위트 **188/188**, `ci-post` **404/404**.
**카드 지시 하나를 좁혔다**(§3.2) — `dataSize == 0` 검사는 유효 입력을 깨뜨려 제외했고, 근거를 남긴다.

---

## 1. 주장 (Claim)

1. 프로브로 네 진입점의 널 데이터 응답을 **교정 전/후 모두 실측**했다(§2, §4).
2. 위반은 `xpe_calc_exposure_index` **하나뿐**이었다 — 나머지 셋은 이미 `-1`(§2).
3. RED(현재 `0`) → 가드 추가 → GREEN(§3).
4. **카드가 지시한 `dataSize == 0` 은 넣지 않았다.** 넣었더니 기존 테스트 3건이 깨졌고,
   원인이 "기존 호출자가 `dataSize` 를 채우지 않는다" 였다(§3.2).
5. `#121` 후속: 비정사각형·극단 종횡비 3종을 ASan 으로 확인 — 유도식이 `min(w,h)` 를
   쓰는 것이 관측과 맞는다(§5).

---

## 2. 3단계 — 널 데이터 프로브 (교정 전)

저장소 밖 프로브에서 네 진입점에 동일한 버퍼(`width=32, height=32, FLOAT32,
data=nullptr, dataSize=0`)를 넘겼다. 로그: `_probe_before.log`

| 진입점 | 교정 전 | 계약 기대 | 판정 |
|---|---|---|---|
| `xpe_multiscale_process` | **-1** | -1 | 준수 |
| `xpe_fractional_process` | **-1** | -1 | 준수 |
| `xpe_detect_collimation` | **-1** | -1 | 준수 |
| `xpe_calc_exposure_index` | **0** (`ei=0.0001 di=-63.9794`) | -1 | **위반** |

카드는 "위반이면 이 카드에서 함께 교정" 이라 했으나 **위반은 1건뿐**이라 추가 교정은 없다.

`0` 이 단순한 오답보다 나쁜 이유: 호출자에게 오류가 아니라 **그럴듯한 측정값**으로 도달한다.
`ei=0.0001`, `di=-63.98` 은 형식상 정상 범위의 수치다.

---

## 3. 1·2단계 — RED → GREEN

### 3.1 RED

`ExposureIndexRejectsNullPixelData` 를 추가(`INVALID_INPUT` 단언):

```
    Which is: 0
[  FAILED  ] EnhanceAdvancedConfigTest.ExposureIndexRejectsNullPixelData
===RUN_EXIT=1===
```
로그: `_red.log`

### 3.2 카드의 `dataSize == 0` 을 넣었다가 뺐다 — 근거

카드 2번은 "널 `data`(및 `dataSize == 0`) 검사" 였다. **그대로 넣었더니 기존 3건이 깨졌다**:

```
ExposureIndexTest.T502_EIComputationIEC62494
IntegrationTest.T604_ThreadSafety
IntegrationTest.T608_PerformanceBudgetVerification
```

T502 의 실패 지점을 읽으니 원인이 나왔다 — 512×512 실제 이미지를 넘기는데 `-1` 이 났다.
헬퍼를 확인했다(`test_exposure_index.cpp:47-58`):

```cpp
XpeImageBuffer img;                 // 값 초기화 없음
img.width  = ...;
img.height = ...;
img.format = XPE_PIXEL_FLOAT32;
img.data   = imageData_.back().data();
                                    // dataSize 를 설정하지 않는다
```

즉 **`dataSize` 는 기존 호출자가 신뢰성 있게 채우는 필드가 아니다.** 그것으로 거절하면
유효한 입력이 거부된다 — 계약 규칙 1(필수 포인터 널)이 요구하는 범위를 넘어선다.

같은 모듈의 선례도 포인터만 본다: `multiscale_process.cpp:66` 은
`if (img->data == nullptr)` 하나다(QA-B-16 에서 케이스로 덮은 그 줄).

**따라서 포인터 검사만 넣었다.** 사유를 소스 주석에 남겼다.

> 카드 지시를 좁힌 유일한 지점이며, 측정이 근거다. `dataSize` 를 계약에 포함하려면
> 기존 호출자·헬퍼가 먼저 그 필드를 채워야 한다 — 별도 사안이다.

### 3.3 GREEN

```
[       OK ] EnhanceAdvancedConfigTest.ExposureIndexRejectsNullPixelData
[       OK ] EnhanceAdvancedConfigTest.MultiscaleMaxLevelsOnNonSquareImages
===RUN_EXIT=0===
```
로그: `_green.log`

---

## 4. 교정 후 프로브 — 네 진입점 일치

로그: `_probe_after.log`

| 진입점 | 교정 후 |
|---|---|
| `xpe_multiscale_process` | -1 |
| `xpe_fractional_process` | -1 |
| `xpe_detect_collimation` | -1 |
| `xpe_calc_exposure_index` | **-1** (`ei=-1 di=-1` — 출력 미변경, 호출자 초기값 그대로) |

출력 파라미터를 건드리지 않고 반환한다는 점도 확인했다(프로브가 `-1` 로 초기화한 값이 유지됨).

---

## 5. 5단계(추가) — 비정사각형·극단 종횡비 (#121 후속)

QA-B-17 의 상한 유도는 `min(w,h)` 를 쓴다. 그 선택이 맞는지 정사각형 밖에서 확인했다.

`MultiscaleMaxLevelsOnNonSquareImages` — `{"levels": 99}` 로 세 종류:

| 크기 | `min(w,h)` | 유도 상한 | 결과 |
|---|---|---|---|
| 256×32 | 32 | 6 | `XPE_OK` |
| 32×256 | 32 | 6 | `XPE_OK` |
| 1024×8 | 8 | 4 | `XPE_OK` |

**ASan 확인 1회** (`_asan.log`, 같은 스크래치 빌드 `build/asan-adv`):
```
[==========] 17 tests from 1 test suite ran.
[  PASSED  ] 17 tests.
AddressSanitizer 보고: 0건
===RUN_EXIT=0===
```

`1024×8` 은 긴 변이 1024(상한 11)인데 짧은 변이 8(상한 4)이다. **긴 변을 기준으로 삼았다면
레벨 11까지 허용돼 짧은 변에서 오버플로가 났을 것**이므로, `min(w,h)` 가 맞는 선택임을
관측이 뒷받침한다. 대칭성(256×32 / 32×256)도 함께 확인했다.

---

## 6. 재실측

| 항목 | 결과 | 로그 |
|---|---|---|
| 스위트 (RelWithDebInfo) | **188 passed / 0 failed**, `SUITE_EXIT=0` | `_verify.log` |
| `ctest` 전체 (ci-post) | **404 passed / 0 failed**, `ALL_EXIT=0` | `_verify.log` |
| ASan (해당 스위트) | 17/17, 보고 0건 | `_asan.log` |

## 7. baseline 귀속

| 항목 | baseline (출처) | 현재 | 판정 |
|---|---|---|---|
| enhance_advanced 스위트 | 186 (`QA-B-17/gate.md` §5.3) | **188** | +2 = 신규 케이스 |
| `ci-post` 전체 | 402 (`QA-B-17/gate.md` §5.3) | **404** | +2 = 동일 |
| exposure_index 널 데이터 | `0` (`_probe_before.log`) | **-1** | 교정됨 |
| 나머지 3개 진입점 | -1 | **-1** | 변화 없음 |

## 8. 미검증 (Gaps)

- **`dataSize` 불일치 자체는 해소하지 않았다**(§3.2). 헬퍼가 `dataSize` 를 채우지 않고
  구조체를 값 초기화도 하지 않는다(`XpeImageBuffer img;`) — **읽으면 미초기화 값**이다.
  이 카드에서는 손대지 않았고, 다른 모듈 헬퍼도 조사하지 않았다.
- **`dataSize` 와 `width*height*bytesPerPixel` 의 정합성 검사는 없다.** 널이 아니지만
  **크기가 부족한** 버퍼는 여전히 통과한다 — 널 포인터보다 위험한 입력일 수 있다.
- **비정사각형 케이스는 `xpe_multiscale_process` 만 확인했다.** collimation/fractional 의
  비정사각형 동작은 보지 않았다.
- **극단 종횡비의 하한을 훑지 않았다.** 1024×8 은 확인했으나 예컨대 4096×2 는 안 봤다.
- **ASan 은 enhance_advanced 만, 이 스위트만 계측했다.**
- **비-MSVC / Linux 미검증.**

## 9. 잔여 위험 (Residual risk)

- §8 의 "크기가 부족한 버퍼" 가 남은 진짜 구멍이다. 이번 교정은 `data == nullptr` 만
  막으므로, 32×32 라고 선언하고 16픽셀만 할당한 버퍼는 여전히 통과해 읽기 오버런이 난다.
  `dataSize` 를 신뢰할 수 없다는 §3.2 의 사실이 이 검사를 막고 있다.
- 이번 변경은 **행위 변경**이다. 널 버퍼로 EI 를 호출해 `0.0001` 을 받아 쓰던 호출자가
  있었다면 이제 `-1` 을 받는다. 계약상 정당하지만 조용한 변화는 아니다.
- 비정사각형 확인은 3종뿐이다. 유도식이 모든 종횡비를 덮는다고 **증명**한 것은 아니고,
  `min(w,h)` 선택이 관측과 모순되지 않음을 보인 것이다.

---

## 10. 합격 조건 대조

| 조건 | 결과 | 근거 |
|---|---|---|
| RED→GREEN | PASS | §3.1 `_red.log` → §3.3 `_green.log` |
| 프로브 표 | PASS | §2 교정 전 / §4 교정 후 — 네 진입점 |
| 5번(비정사각형·극단 종횡비, ASan 1회) | PASS | §5 — 3종 + ASan 보고 0건 |
| 재실측 | PASS | §6 — 188/188, 404/404 |
| footer `Refs #122` | PASS | 커밋 메시지 |
