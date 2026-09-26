# QA-B-12 — enhance_advanced 오류코드 우선순위 교정 + Lane B 모듈 프로브 (#119)

**레인**: Lane B (`dev/postprocess`)
**계약**: `INVALID_INPUT`(널·범위) → `NOT_INITIALIZED` → 내용 검증 → 처리 오류

**결과**: RED → GREEN. 5곳 교정, `ci-post` **387/387 무실패**.
**범위를 좁힌 판단 1건**(§3.3)과 **프로브 위반 2건**(§5)이 판정 대상이다.

---

## 1. 주장 (Claim)

1. `DISABLED_` 를 떼고 **RED 를 관측**했다(§2).
2. 5개 파일의 **널 검사만** 초기화 검사 앞으로 옮겼다. 검사 내용은 불변(§3).
3. **치수(dimension) 검사는 옮기지 않았다** — 옮겼더니 기존 7건이 깨졌고, 준거 구현
   대조 결과 "범위" 가 이미지 치수를 포함한다는 근거가 없다(§3.3). 판정 요청 사항.
4. 교정 여파로 드러난 모듈 테스트 1건은 **기대치가 아니라 인자를 고쳐** 원래 의도를
   실제로 검증하게 했다(§4).
5. GREEN: 스위트 **171/171**, `ci-post` **387/387**.
6. Lane B 나머지 5개 모듈 프로브 — **위반 2건**(`gsvg`, `ai`). 고치지 않고 보고(§5).

---

## 2. 1단계 — RED 관측

`DISABLED_NullArgumentOutranksNotInitialized` 에서 접두사를 제거하고 실행:

```
test_integration_ext.cpp(327): xpe_multiscale_process(nullptr,nullptr,nullptr)
    Which is: -6        expected -1
test_integration_ext.cpp(328): xpe_fractional_process(nullptr,1.0f,nullptr)
    Which is: -6        expected -1
[  FAILED  ] ErrorPrecedenceTest.NullArgumentOutranksNotInitialized
[       OK ] ErrorPrecedenceTest.ValidArgumentsReachNotInitialized
===RUN_EXIT=1===
```
로그: `_red.log`

`DISABLED_` 시절의 사유 주석은 이제 사실이 아니므로 현재 상태를 적는 문구로 교체했다.

---

## 3. 2단계 — 5곳 교정

### 3.1 변경 형태 (5파일 공통)

```diff
-    // REQ-ADV-020: Not-initialized guard      <- 초기화 검사가 먼저였다
-    { lock; if (!g_initialized) return XPE_ERR_NOT_INITIALIZED; }
-
-    // REQ-ADV-022: NULL pointer guard
-    if (img == nullptr || ...) return XPE_ERR_INVALID_INPUT;
+    // REQ-ADV-022: NULL pointer guard          <- 널 검사를 앞으로
+    if (img == nullptr || ...) return XPE_ERR_INVALID_INPUT;
+
+    // REQ-ADV-020: Not-initialized guard
+    { lock; if (!g_initialized) return XPE_ERR_NOT_INITIALIZED; }
```

조건식·반환값·`REQ-ADV-*` 주석은 그대로다. **순서만** 바뀌었다.

| 파일 | 진입점 |
|---|---|
| `src/multiscale_process.cpp` | `xpe_multiscale_process` |
| `src/fractional_process.cpp` | `xpe_fractional_process` |
| `src/collimation_detect.cpp` | `xpe_detect_collimation` |
| `src/xpe_enhance_advanced.cpp` | `xpe_calc_exposure_index` |
| `src/xpe_collimation_detect.cpp` | (별도 구현 경로) |

### 3.2 자동 치환이 한 파일을 망가뜨렸다가 되돌렸다 (기록)

5곳을 한 번에 재배열하는 스크립트를 썼는데 `xpe_enhance_advanced.cpp` 를 깨뜨렸다.
스크립트가 `REQ-ADV-020` 을 **첫 등장 위치**로 찾았고, 그 파일의 첫 등장은 24행
`// @MX:ANCHOR: [AUTO] Initialization flag -- REQ-ADV-001, REQ-ADV-020` — 가드가 아니라
**주석**이었다. 그 결과 코드 블록이 함수 밖(파일 스코프, `} // extern "C"` 뒤)으로 옮겨졌다.

`git checkout -- <file>` 로 되돌리고, 마커 개수를 세어 원인을 확정한 뒤
(`020=3 022=3 070=2 071=2` — 나머지 4파일은 전부 1) 그 파일만 실제 가드 1곳을
지정해 손으로 고쳤다. 나머지 4파일은 각 마커가 1개뿐이라 스크립트 결과가 유효하며,
diff 를 눈으로 확인했다.

**교훈**: 마커 기반 일괄 치환은 마커가 주석에도 쓰이는 순간 조용히 엉뚱한 곳을 잡는다.
개수 검증을 치환 **전에** 했어야 했다.

### 3.3 치수 검사는 옮기지 않았다 — 근거

카드는 "널·**범위** 인자 검사를 초기화 검사 앞으로" 라고 했다. 문자 그대로 치수 검사
(`img->width == 0 || img->height == 0`, `INVALID_INPUT`)까지 앞으로 옮겨 보았다.
**기존 테스트 7건이 깨졌다**:

```
EnhanceAdvancedLifecycleTest.ShutdownAfterInit
EnhanceAdvancedLifecycleTest.ProcessingBeforeInit
IntegrationTest.T606_CoverageMeasurement
NotInitializedGuardTest.{Multiscale,Fractional,DetectCollimation,CalcExposureIndex}…
```

원인은 하나다 — 이들은 `XpeImageBuffer img{}`(값 초기화, `width==0`)로 미초기화 경로를
찌른다. 치수 검사를 앞으로 올리면 `-6` 대신 `-1` 이 난다. 특히 `NotInitializedGuardTest`
4건은 **leader 가 "초기화 가드는 정상" 의 근거로 인용한 바로 그 케이스**다.

그래서 "범위" 가 이미지 치수를 포함하는지 준거 구현으로 판별했다.
`preprocess`(계약을 이미 지킨다고 판정된 모듈), `defect_correct.cpp:115-127`:

```
115  null 인자        -> INVALID_INPUT
116  null data        -> INVALID_INPUT
117  format           -> UNSUPPORTED_FORMAT   <- 내용 검증이 치수보다 먼저
118  width/height==0  -> INVALID_INPUT        <- 치수는 format 뒤
127  not initialized  -> NOT_INITIALIZED
```

그리고 `xpe_preprocess.cpp:180-185` 는 `null -> NOT_INITIALIZED` 순이다.

두 구현에서 **공통으로 성립하는 불변식은 "널 검사가 초기화 검사보다 먼저" 하나뿐**이다.
치수를 format 앞으로 올리는 관행은 준거 구현에 없다(오히려 반대). 따라서 근거가 있는
최소 변경 — **널 검사만 이동** — 을 택했고, 치수 검사는 원위치(내용 검증 옆)에 남겼다.
소스 주석에도 이 유보를 적었다.

> **판정 요청**: "범위" 가 이미지 치수를 포함한다면 5곳을 더 옮기고 위 7건의 기대치를
> 함께 고쳐야 한다(별도 카드). 포함하지 않는다면 현 상태가 맞다.
> 참고로 `preprocess` 자신도 `format`·`치수`가 초기화보다 앞이라 계약의
> "NOT_INITIALIZED → 내용 검증" 을 만족하지 않는다 — 계약과 준거 구현이 완전히
> 일치하지는 않는다.

---

## 4. 교정 여파 1건 — 기대치가 아니라 인자를 고쳤다

널 검사를 앞으로 옮기자 모듈 쪽 기존 테스트 하나가 깨졌다:

```
test_integration.cpp(463): xpe_multiscale_process(&img, nullptr, nullptr)
    Which is: -1        expected -6
```

주석이 `// Path 5: Not initialized` 다. 즉 **의도는 미초기화 경로 검증**인데
메타데이터를 널로 주고 있었다 — 계약상 널이 먼저 걸리므로 이 호출로는 그 경로에
도달할 수 없다. 기대치를 `-1` 로 낮추면 통과는 하지만 **테스트가 이름과 다른 것을
검증하게 된다.**

그래서 기대치 대신 인자를 고쳤다 (`nullptr` → `&meta`, 같은 함수에 이미 선언된 유효
메타데이터). 사유를 주석으로 남겼다. **단언의 의미는 보존됐고, 이제 실제로 그 경로를 찌른다.**

---

## 5. 4단계 — Lane B 나머지 5개 모듈 프로브

저장소를 건드리지 않기 위해 **스크래치 디렉터리에 프로브 프로그램**을 작성해 각 모듈의
import lib 에 링크했다. 어떤 모듈에서도 init 을 호출하지 않았다 — 그 상태가 곧 미초기화다.

| 모듈 | 호출 (미초기화 + 널 인자) | 반환 | 계약 기대 | 판정 |
|---|---|---|---|---|
| enhance_basic | `xpe_log_transform(nullptr, 1000.0f)` | **-1** | -1 | 준수 |
| display | `xpe_apply_modality_lut(nullptr, nullptr)` | **-1** | -1 | 준수 |
| dicom | `xpe_dicom_validate(nullptr, nullptr, 0)` | **-1** | -1 | 준수 |
| gsvg | `xpe_gsvg_process(nullptr, nullptr, nullptr, 0, 0, nullptr)` | **-6** | -1 | **위반 후보** |
| ai | `xpe_dl_denoise(nullptr, nullptr, nullptr)` | **-6** | -1 | **위반** |

로그: `_probe.log` (링크 3건 모두 exit 0)

### 5.1 `ai` — 귀속을 분리했다

`-6` 이 "널 인자 때문" 인지 "초기화 검사가 먼저라서" 인지 구분하려고, **같은 미초기화
상태에서 유효 인자**로 한 번 더 불렀다:

```
ai xpe_dl_denoise(nullptr,nullptr,nullptr) = -6
ai xpe_dl_denoise(&img,&meta,nullptr)      = -6     <- 유효 인자도 -6
```

유효 인자에서도 `-6` 이므로 초기화 가드가 정상 동작하며, 널 인자에서도 `-6` 이라는 것은
**초기화 검사가 널 검사를 앞선다**는 뜻이다 — enhance_advanced 가 방금 고친 것과 **같은 형태**다.
`xpe_ai_init` 이 존재하므로 미초기화 상태가 실재한다.

### 5.2 `gsvg` — 판정이 갈릴 수 있다

`gsvg` 는 핸들 기반(`xpe_gsvg_init(void** handleOut, …)`)이고, 프로브는 `handle` 자리에
널을 준다. 이것을 **널 인자**로 보면 `-1` 이어야 하고, **미초기화 상태의 표현**으로 보면
`-6` 이 자연스럽다. `ai` 처럼 유효 인자 대조를 하려면 유효 핸들이 필요한데 그러려면
init 을 불러야 해서 미초기화 상태가 사라진다 — **이 프로브만으로는 귀속을 분리할 수 없다.**
위반으로 단정하지 않고 "후보" 로 보고한다.

### 5.3 준수 3건의 성격 (과대 해석 방지)

`enhance_basic` / `display` / `dicom` 은 **init 함수 자체가 없다**
(`grep -c xpe_.*_init` → 0). 즉 `NOT_INITIALIZED` 계층이 적용될 상태가 존재하지 않는다.
`-1` 은 "우선순위를 지킨다" 라기보다 **"경쟁할 검사가 없다"** 에 가깝다. 표의 "준수" 를
"우선순위 로직이 검증됐다" 로 읽지 말 것.

---

## 6. 3단계 — GREEN 및 재실측

스위트 단독 (`_green.log`):
```
===BUILD_EXIT=0===
[==========] 171 tests from 16 test suites ran.
[  PASSED  ] 171 tests.
===RUN_EXIT=0===
```

`ci-post` 전체 (`_verify.log`):

| 항목 | 결과 |
|---|---|
| `cmake --preset ci-post` | `CFG_EXIT=0` |
| `ctest -N -L "SPEC-XPE-P2-ADV"` | **171** |
| `ctest` 전체 | **100% passed, 0 failed out of 387**, `ALL_EXIT=0` |

## 7. baseline 귀속

| 항목 | baseline (출처) | 현재 | 판정 |
|---|---|---|---|
| enhance_advanced 실행 | 170 실행 / 1 Disabled (`QA-B-11/gate.md` §6) | **171 실행, 171 통과** | Disabled 해제 반영 |
| `ci-post` 총계 | 386 (`QA-B-11/gate.md` §7) | **387** | +1 = Disabled 해제분 |
| `ci-post` 실패 | 0 | **0** | 동일 |
| `-L SPEC-XPE-P2-ADV` | 171 (QA-B-11) | **171** | 동일 |
| did-not-run | `DegradedMode` 5 (Skipped) + 1 (Disabled) | `DegradedMode` 5 (Skipped) | Disabled 0 |

## 8. 미검증 (Gaps)

- **`gsvg` 위반 귀속 미분리**(§5.2). 유효 핸들을 만들면 미초기화 상태가 사라져
  이 프로브 설계로는 분리 불가. 다른 설계가 필요하다.
- **모듈당 진입점 1개만 프로브했다.** 카드 지시대로다. 같은 모듈의 다른 진입점이
  다른 순서를 쓸 수 있다(`enhance_advanced` 도 5곳이 제각기 있었다).
- **`enhance_basic`/`display`/`dicom` 은 미초기화 상태 자체가 없어** 우선순위가
  검증된 것이 아니다(§5.3).
- **치수 검사 이동 여부 미결**(§3.3). 계약 해석 판정 대기.
- **`preprocess` 는 계약의 "NOT_INITIALIZED → 내용 검증" 을 만족하지 않는다**(§3.3).
  Lane A 소유라 확인만 하고 손대지 않았다 — 계약이 준거 구현과 어긋나는 지점이다.
- **교정 5곳의 스레드 안전성 영향 미검토.** `collimation_detect`/`multiscale`/
  `fractional` 은 초기화 검사를 짧은 lock 스코프로 감싸는데, 널 검사가 그 앞으로
  가면서 lock 밖에서 인자를 역참조한다. 인자는 호출자 소유라 경합 대상이 아니라고
  보았으나 별도 검증은 하지 않았다.
- **비-MSVC / Linux 미검증.**

## 9. 잔여 위험 (Residual risk)

- §3.3 이 "치수도 포함" 으로 판정되면 이번 5곳을 다시 손대고 기존 7건 기대치도
  고쳐야 한다 — **이 커밋 위에 얹는 변경이지 되돌리기는 아니다.**
- §5 의 위반 2건이 별도 카드로 이어지지 않으면, `ai`/`gsvg` 는 계약과 다른 순서로
  남는다. 호출자가 오류코드로 분기할 때 모듈마다 다르게 동작한다.
- `xpe_calc_exposure_index` 는 함수 전체가 하나의 `lock_guard` 아래 있어 널 검사도
  lock 안에서 돈다. 다른 4곳과 lock 범위가 다르다 — 이번 변경이 만든 차이는 아니지만
  일관성 관점의 관찰 사항이다.

---

## 10. 합격 조건 대조

| 조건 | 결과 | 근거 |
|---|---|---|
| RED→GREEN 로그 | PASS | §2 `_red.log` / §6 `_green.log` |
| 5곳 diff | PASS | §3.1 (5파일), §3.2 사고 기록 |
| 재실측 카운트 | PASS | §6 스위트 171/171, ci-post 387/387 · §7 |
| 5개 모듈 프로브 표 | PASS | §5 (위반 2건 미수정 보고, 귀속 분리 포함) |
| footer `Refs #119` | PASS | 커밋 메시지 |
