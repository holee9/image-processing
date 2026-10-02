# QA-B-191 M1 — 부위 인식 장난감 모델 (제품 파일 0)

**이 모델들은 분류기가 아니다.** 배선(모델 출력 → 라벨·confidence·문턱·알림)과 "모델/영상을 바꾸면 답이 바뀐다"만 증명한다. 정확도·임상 성능·지연 시간에 대해 아무것도 말하지 않는다.

## 1. 주장 (Claim)

`modules/ai/tests/data/make_bodypart_models.py` 가 `Flatten → MatMul → Add` 그래프의 장난감 모델 12벌(`models_bodypart_*`)을 stdlib protobuf 로 만든다. ONNX Runtime 1.30.0 이 이 모델들을 읽고, 생성기가 의도한 수를 정확히 낸다.

## 2. 증거 (Evidence)

독립 프로브(모듈 코드를 건드리기 전, `probe_source.txt` → `build/g191_probe.exe`, 출력 `probe_out.txt`): `===COMPILE=0===`, `===EXIT=0===`, `===PROBE_BAD=0===`.

| 모델 | 입력 | 관측한 출력 |
|---|---|---|
| `models_bodypart_a` (상수) | 영 / 일 | `0.6 0.3 0.1` 둘 다(입력 무관) |
| `models_bodypart_b` (상수) | 영 | `0.1 0.3 0.6` (a 와 다른 답) |
| `models_bodypart_dep` (입력 의존) | 윗절반 0.75 / 아랫절반 0.75 / 전부 1 | `0.75 0 0` / `0 0.75 0` / `1 1 0` |
| `models_bodypart_nhwc` | 윗절반 0.75 (`[1,4,4,1]`) | `0.75 0 0` (NCHW 와 같은 수) |
| `models_bodypart_rank2` | `[1,16]` | `0.75 0 0` (ORT 는 돌린다. 모듈이 모양을 거절할 대상) |
| `models_bodypart_dynamic` | `[1,1,H,W]` 동적 | `0.75 0 0` (같은 이유) |
| `models_bodypart_range_high` / `_range_low` | 영 | `1.5 0 0` / `0.5 0.2 -0.1` (확률이 아님) |
| `models_bodypart_nonfinite` | 일 / 영 | 첫 값 `inf` / `NaN` |
| `models_bodypart_broken` (대조군) | — | 거절: `Protobuf parsing failed.` |

대조군의 뜻: 깨진 파일이 거절되므로 위의 OK 가 "ORT 가 아무 파일이나 받아들여서"가 아니라는 것을 보인다. 두 상수 모델이 서로 다른 수를 내고 입력 의존 모델이 영상에 따라 다른 수를 내므로, 메아리·항등·상수·스텁은 통과할 수 없다.

## 3. 프로브 첫 실행의 실패와 정정

프로브 첫 실행은 5개 칸이 FAIL 이었다(`0.8` 입력). 원인은 모델이 아니라 **프로브의 기대값**이다: 0.8f 는 이진 부동소수에서 정확히 표현되지 않아 8개 항의 합이 `0.8f` 와 한 단위 어긋날 수 있다. 입력을 정확히 표현되는 0.75 로 바꾸고 다시 돌려 전부 통과했다. **M2 시험도 이 값(0.75, 0.125 의 배수)을 쓴다** — 정확 비교를 하려면 입력이 이진으로 정확해야 한다.

## 4. 기준 (Baseline)

같은 트리(main 병합 후 `dev/postprocess`), ORT 1.30.0 사전 빌드(`D:\workspace-github\_deps\onnxruntime-win-x64-1.30.0`), 프로브 컴파일은 MSVC `cl /std:c++17`.

## 5. 미검증 (Gaps)

- 이 모델들을 **모듈의 `OnnxSession`** 이 읽는지는 M2 에서 본다(프로브는 ORT 를 직접 부른다).
- 사이드카 `bodypart.json` 의 `labels` 는 아직 아무 코드도 읽지 않는다(M2).
- 정확 비교는 이 입력들에서만 성립한다(0.75 의 배수). 임의 영상에서의 반올림은 주장하지 않는다.

## 6. 잔여 위험

장난감 모델의 통과가 실제 모델의 동작으로 읽힐 위험 — 생성기 머리 주석과 사이드카 `note` 필드가 "분류기가 아니다"를 적고 있다.
