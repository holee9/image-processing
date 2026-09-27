# QA-A-53 — ABI 가드를 우회 불가로, 스테이징 산출물에 출처를

A-52 가 스스로 적은 미검증 두 건을 닫는 카드다. 운영 코드·기본값·판정 기준은 그대로다.

수정 파일
- `modules/preprocess/tools/xpe_real_frames.cpp` (`XPE_DICOM_BIND` 매크로, provenance 판독기)
- `modules/preprocess/CMakeLists.txt` (configure 시점 head SHA 주입)
- `.moai/reports/lane-pre/QA-A-52/Stage-DicomRuntime.ps1` (`provenance.json` 기록)

---

## §1. Claim — 주장

| # | 주장 |
|---|---|
| C1 | DICOM 진입점 **10개 전수 조사**. 이 도구가 재선언하는 것은 3개뿐이고, 나머지 7개는 **재선언하지 않는다** — 따라서 드리프트 표면이 아예 없다 |
| C2 | **안 쓰는 진입점을 재선언해 두는 것은 틀렸다**는 판단. 7개를 가드에 넣으려면 먼저 재선언해야 하고, 그건 없던 위험을 만드는 일이다 |
| C3 | `XPE_DICOM_BIND` 매크로가 **typedef 와 static_assert 를 함께** 낸다. A-52 가 남긴 "규칙이 코드로 강제되지 않는다"는 구멍이 닫혔다 |
| C4 | 반증 (a): 매크로로 묶인 `xpe_dicom_close` 에 타입 드리프트를 넣으면 **`C2338` 만** 난다. 호출부는 또 통과한다 — A-52 의 대비가 다른 진입점에서 재현됐다 |
| C5 | `Stage-DicomRuntime.ps1` 이 **GUI-C-41 과 동일한 필드**(`source`/`runId`/`headSha`/`files[{name,md5,length}]`)로 `provenance.json` 을 쓴다 |
| C6 | 하네스가 그 파일을 **첫 화면에 인쇄**하고, 없으면 **DICOM 입력을 거절**(exit 3)하며, `headSha` 가 다르면 **경고 후 진행**한다 |
| C7 | 반증 (b): 매니페스트를 지우면 DICOM 은 거절(숫자 0줄), raw 는 계속 동작 |
| C8 | 재측정 605/605, 69/69, 경고 0 |

---

## §2. Evidence — 증거

### 2-1. 진입점 전수표 (카드 1항)

`grep -n "^XPE_API" modules/dicom/include/xpe/dicom/dicom_api.h` — 10개.

| # | 진입점 | 이 도구가 쓰나 | 재선언 | 가드 | 판단 |
|---|---|---|---|---|---|
| 1 | `xpe_dicom_open` | **예** | 예 | **`XPE_DICOM_BIND`** | 필요 |
| 2 | `xpe_dicom_read_image` | **예** | 예 | **`XPE_DICOM_BIND`** | 필요 |
| 3 | `xpe_dicom_close` | **예** | 예 | **`XPE_DICOM_BIND`** | 필요 |
| 4 | `xpe_dicom_get_metadata` | 아니오 | **아니오** | 해당 없음 | 아래 참조 |
| 5 | `xpe_dicom_write` | 아니오 | **아니오** | 해당 없음 | 재선언 금지 |
| 6 | `xpe_dicom_write_j2k` | 아니오 | **아니오** | 해당 없음 | 재선언 금지 |
| 7 | `xpe_dicom_validate` | 아니오 | **아니오** | 해당 없음 | 재선언 금지 |
| 8 | `xpe_dicom_cstore` | 아니오 | **아니오** | 해당 없음 | 재선언 금지 |
| 9 | `xpe_dicom_cfind_mwl` | 아니오 | **아니오** | 해당 없음 | 재선언 금지 |
| 10 | `xpe_dicom_cancel` | 아니오 | **아니오** | 해당 없음 | 재선언 금지 |

**"안 쓰는 것을 재선언해 두는 것이 맞는가" 에 대한 답: 아니다.**
드리프트는 **재선언이 있어야** 생긴다. 7개는 이 파일 어디에도 적혀 있지 않으므로 드리프트할
대상 자체가 없다. 가드에 넣으려면 **먼저 재선언해야 하고**, 그건 없던 위험을 만들어 놓고
그 위험을 막는 장치를 다는 일이다. 리더가 "지우는 것도 답" 이라고 한 방향 그대로,
**이미 지워져 있는 상태가 최선**이며 이 카드는 그 상태를 확인하고 표로 고정했다.

`get_metadata` 만 경계 사례다. 프레임의 촬영 조건을 찍어 주면 쓸모가 있어 보이지만
**두 가지 이유로 넣지 않았다**: (1) Rows/Columns 는 이미 `XpeImageBuffer` 에서 나온다,
(2) `XpeImageMetadata` 에는 **환자 식별 정보**가 들어 있고, 검증 하네스가 그걸 찍으면
증거 로그에 PHI 가 남는다. 이 판단은 소스 주석에도 적었다.

### 2-2. 우회 불가 가드 (카드 1항)

```cpp
#define XPE_DICOM_BIND(name, ret, ...)                                          \
    typedef ret (*Pfn_##name)(__VA_ARGS__);                                     \
    static_assert(std::is_same<decltype(&name), Pfn_##name>::value,             \
                  #name " signature drifted from the re-declaration in this "   \
                  "file (see xpe/dicom/dicom_api.h)")

XPE_DICOM_BIND(xpe_dicom_open,       XpeErrorCode, const char*, XpeDicomHandle**);
XPE_DICOM_BIND(xpe_dicom_read_image, XpeErrorCode, XpeDicomHandle*, XpeImageBuffer*);
XPE_DICOM_BIND(xpe_dicom_close,      void,         XpeDicomHandle*);
```

A-52 의 가드는 typedef 3개와 assert 3개가 **따로** 있었다. 네 번째를 복사해 붙이면서
assert 를 빠뜨리는 것을 막는 것은 주석뿐이었고, 그 구멍을 A-52 §4-1 에 내가 적었다.
매크로는 **typedef 를 assert 없이 만들 수 없게** 한다 — 둘이 한 토큰에서 나온다.

### 2-3. 반증 (a) — 매크로로 묶인 진입점의 타입 드리프트 (카드 3-a)

A-52 는 `read_image` 로 반증했다. 이번엔 **아직 반증하지 않은** `xpe_dicom_close` 를 골랐다
(반환형이 `void` 라 모양도 다르다).

**조작**: `XPE_DICOM_BIND(xpe_dicom_close, void, XpeDicomHandle*)` → `..., void)`.

**빌드 결과** (`a53-falsify-guard-build.log`):
```
[1/2] Building CXX object ...xpe_real_frames.cpp.obj
FAILED: modules/preprocess/CMakeFiles/xpe_real_frames.dir/tools/xpe_real_frames.cpp.obj
...xpe_real_frames.cpp(412): error C2338: static_assert failed: 'xpe_dicom_close signature
drifted from the re-declaration in this file (see xpe/dicom/dicom_api.h)'
...xpe_real_frames.cpp(425): error C2338: static_assert failed: 'xpe_dicom_close signature
drifted from the re-declaration in this file (see xpe/dicom/dicom_api.h)'
ninja: build stopped: subcommand failed.
BUILD_EXIT=1
```

**또 `C2338` 뿐이다.** 호출부 `api.close(h)` 는 `XpeDicomHandle*` 를 `void*` 로 암묵 변환해
아무 불평 없이 컴파일된다. A-52 가 `read_image` 에서 본 대비가 **반환형이 다른 진입점에서
그대로 재현**됐다 — 타입 드리프트는 가드 말고는 아무도 안 잡는다.

**복원 확인** (`a53-restore-build.log`): 컴파일+링크 재실행, exit 0.

### 2-4. 매니페스트 — GUI-C-41 형식 대조 (카드 2항)

기준 원본: `clients/ImageProcTest.E2ETests/Staging/Stage-NativeArtifacts.ps1` 와
CI `ci.yml` 의 `Record staged-DLL provenance` 단계.

| 필드 | GUI-C-41 | QA-A-53 | 일치 |
|---|---|---|---|
| `source` | `'ci'` / `'lane'` | `'lane'` | ✔ |
| `runId` | CI run id / 인자 | `lane-local-<타임스탬프>` | ✔ (형식 동일, 값은 로컬) |
| `headSha` | `gh run view --json headSha` | `git -C <repo> rev-parse HEAD` | ✔ |
| `files[]` | `{name, md5, length}` | `{name, md5, length}` | ✔ |
| 기록 시점 | **모든 복사 성공 후 마지막** | 동일 | ✔ |

실제 출력 (`.moai/reports/lane-pre/QA-A-52/stage/provenance.json`):
```
fields: ['files', 'headSha', 'runId', 'source']
file[0]: ['length', 'md5', 'name'] dcmdata.dll 44554397 2433024
```

C-41 의 주의사항 하나를 그대로 가져왔다 — **복사가 다 끝난 뒤에 쓴다.** 중간에 실패하면
매니페스트가 디스크에 없는 해시를 가리키게 되고, **틀린 출처 기록은 없느니만 못하다.**

### 2-5. 하네스의 판독·거절·경고 (카드 2항)

**정상** (`a53-dicom-with-provenance.log`, 첫 화면):
```
STAGED-ARTIFACT PROVENANCE (GUI-C-41 format)
  source  lane
  runId   lane-local-20260912113652
  headSha 287744f4e14371955ca7614a1b766d2a0ae24185
  files   17
  head SHA matches this binary's configure-time commit (287744f4e143)
```
비교 대상인 "이 바이너리의 커밋"은 CMake configure 시점에 `git rev-parse HEAD` 로 읽어
`XPE_A53_BUILD_HEAD_SHA` 로 박아 넣는다. A-52 가 남긴 질문 — *스테이징한 DLL 이 현재 소스와
같은 커밋 빌드인가* — 에 실행할 때마다 답이 나온다.

**headSha 불일치** (`a53-sha-mismatch.log`, 매니페스트의 SHA 를 0으로 바꿔 확인):
```
  WARNING: staged artifacts are from 000000000000 but this binary was configured
           at 287744f4e143. Proceeding -- the numbers below may come from a
           different implementation than the source in this tree.
```
카드 지시대로 **경고 후 진행**이다. 불일치가 곧 오류는 아니다(의도적으로 옛 DLL 을 물릴 수
있다) — 다만 숫자 위에 그 사실이 먼저 찍힌다.

### 2-6. 반증 (b) — 매니페스트 제거 (카드 3-b)

`provenance.json` 을 치우고 두 가지 입력으로 실행 (`a53-falsify-provenance.log`):

```
=== DICOM input, no manifest ===
STAGED-ARTIFACT PROVENANCE: none (no provenance.json beside this binary)
  REFUSED: a DICOM input needs xpe_dicom.dll, which this lane's preset
  does not build -- so it was staged from somewhere, and nothing here
  records from where. ...
DICOM_EXIT=3

=== raw input, no manifest (must still run) ===
STAGED-ARTIFACT PROVENANCE: none (no provenance.json beside this binary)
  raw-only run out of a build tree: nothing was staged, nothing to attribute
== uniform.raw ==
  ... global sigma: A 10.3782  Bm 10.4836  Bm4 10.4836
```

**DICOM 은 측정값을 한 줄도 내지 않고 exit 3 으로 끝난다.** C-41 기준 그대로 —
출처 불명으로 숫자를 내지 않는다.

**범위에 대한 판단(들어내 적는다, 뒤집어도 좋다)**: 거절은 **DICOM 경로에만** 건다.
raw 입력은 이 레인 자신의 빌드 트리에서 돌며 **외부 산출물을 스테이징하지 않는다** —
귀속할 제3자 파일이 없는데 매니페스트를 요구하면 하네스의 본래 용도를 막게 된다.
매니페스트가 **있으면** raw 실행에서도 인쇄·대조한다. 카드 문구는 무조건 거절이므로,
이 좁힘이 과하다면 되돌리면 된다 — 한 줄 조건이다.

### 2-7. 재측정

```
100% tests passed, 0 tests failed out of 605     (build/ci-preprocess)
100% tests passed, 0 tests failed out of  69     (build/ci-common)
```
`a53-ctest-pre.log`, `a53-ctest-common.log`. 빌드 exit 0, `grep -ci warning` = **0**.
`Test-TrackedTextFiles.ps1` → `Tracked text file validation passed.`

---

## §3. Baseline-attribution — 무엇에 대고 쟀나

- **진입점 목록**: `modules/dicom/include/xpe/dicom/dicom_api.h` 를 이번 턴(main 병합 `287744f`
  이후)에 직접 grep. 기억이나 이전 보고서가 아니다
- **가드 기준**: 컴파일러가 같은 헤더의 선언과 직접 대조. 사람이 옮긴 사본 없음
- **매니페스트 형식**: `image-processing` 체크아웃의 `Stage-NativeArtifacts.ps1`(11~16, 54~124행)과
  `.github/workflows/ci.yml`(424~441행)을 읽고 필드 단위로 대조
- **DICOM 결과**: A-52 와 같은 `edge.dcm`·같은 판정 기준으로 이번 턴에 재실행. 수치는 A-52 와
  동일(값 분포·국소 σ·A/Bm/Bm4·flagged 1627/598)
- **테스트**: 이번 트리에서 실행. 운영 소스는 한 줄도 바뀌지 않았다

---

## §4. Gaps — 관측하지 않은 것

1. **매크로도 손으로 쓴 typedef 는 못 막는다.** `XPE_DICOM_BIND` 를 쓰지 않고 직접
   `typedef ... (*Pfn)(...)` 를 적으면 assert 없이 통과한다. 매크로는 **가장 흔한 누락 경로
   (복사-붙여넣기)** 를 막을 뿐, 문법적으로 봉인하지는 못한다
2. **`md5` 를 하네스가 검증하지 않는다.** 매니페스트를 읽어 인쇄하고 `headSha` 만 비교한다.
   파일이 매니페스트 작성 후 바뀌었는지는 **확인하지 않았다** — C-41 과 같은 수준이지만,
   여기서 한 걸음 더 갈 수 있는 지점이다
3. **`headSha` 는 configure 시점 값이다.** CMake 에 이식성 있는 per-build 훅이 없어서
   configure 후 커밋을 바꾸고 빌드만 다시 하면 낡은 SHA 가 박힌다. 재configure 하면 갱신된다
4. **거절을 DICOM 경로로 좁힌 것은 판단이다**(§2-6). 카드 문구는 무조건 거절이다
5. **`xpe_dicom.dll` 이 provenance 의 `headSha` 커밋에서 빌드됐는지는 여전히 모른다.**
   매니페스트는 *스테이징한 사람이 그때 어느 커밋에 있었는지* 를 기록할 뿐, **그 DLL 이 언제
   빌드됐는지** 를 기록하지 않는다. Lane B 의 `build/ci-dicom` 이 며칠 전 빌드라면 SHA 는
   맞고 구현은 낡았을 수 있다. A-52 의 위험이 **줄었지만 사라지지는 않았다**
6. **DICOM 은 여전히 인공 픽스처 1장뿐이다.** 압축 전송구문·부호 픽셀·RescaleSlope·다중 프레임
   미검증(A-52 §4-3 그대로)
7. `xpe_real_frames` 가 CI 에서 빌드되는지 미확인 — 가드는 이 워크트리 빌드에서만 돌았다

---

## §5. Residual-risk — 남는 위험

1. **Gap 5 가 이 카드에서 가장 큰 잔여 위험이다.** 매니페스트는 "누가 언제 어디서 모았나"를
   답하지, "이 DLL 이 그 소스에서 나왔나"를 답하지 않는다. 진짜 답은 **스테이징 시점에 DICOM
   빌드를 직접 수행**하는 것이고, 그러려면 vcpkg DCMTK 가 이 기계에 필요하다(현재 `VCPKG_ROOT`
   미설정, `vcpkg` PATH 없음)
2. **경고는 읽히지 않으면 없는 것과 같다.** `headSha` 불일치는 진행을 막지 않으므로,
   로그를 옮겨 적는 과정에서 첫 화면이 잘리면 사라진다. 증거 로그는 **첫 줄부터** 남겨야 한다
3. **매크로가 주는 안전감이 과할 수 있다.** Gap 1 그대로 — 매크로를 쓰지 않기로 하면 그만이다.
   근본 해법은 재선언을 없애는 것(헤더의 타입을 직접 쓰는 것)이지만, 그러면 "무엇을 기대하는가"
   라는 진술도 함께 사라진다. 지금은 **기대를 적고 그 기대를 검사하는** 쪽을 택했다
4. **거절이 raw 를 막지 않는다는 것이 회피 경로가 될 수 있다.** DICOM 이 거절당한 사람이
   파일을 raw 로 변환해 매니페스트 없이 돌릴 수 있다. 그 경로에는 스테이징한 DLL 이 없으므로
   위험 자체가 다르지만, "거절을 우회했다"는 형태로 보일 수는 있다
5. PHI 를 이유로 `get_metadata` 를 뺐지만, **실제 프레임의 픽셀 자체도 환자 데이터**다.
   증거 로그에는 통계량만 남고 픽셀은 남지 않는다는 점은 확인했으나, 그건 현재 출력 형식에
   의존한다

---

Refs #151 #148
