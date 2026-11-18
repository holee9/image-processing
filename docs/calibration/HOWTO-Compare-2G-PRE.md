# 2G 결과와 PRE 결과를 앱에서 비교하기

같은 원시 영상을 2G(삼성 SDSDK 2.3.0 B03 X64W)와 새 전처리 엔진(PRE)에 각각 넣어 만든 두 결과를
운영자 앱 `gui/ImageProcTest` 한 창에서 번갈아 보고, 차이가 큰 곳을 찾는 방법이다.

이 문서는 절차만 다룬다. 비교 계약(manifest 형식, 사유 코드)은
[tools/calibration/README.md](../../tools/calibration/README.md)에 있다. 관련 이슈는
[#285](https://github.com/holee9/image-processing/issues/285)다.

## 먼저 알아 둘 점

- **앱은 엔진을 돌리지 않는다.** 이미 만들어 둔 두 결과 파일을 읽어서 보여 준다. 창의
  **Run real 2G** 버튼은 항상 꺼져 있다.
- 지금 열 수 있는 결과는 아래 한 묶음이다. 다른 원시 영상을 비교하려면 두 엔진 결과와 manifest를
  먼저 새로 만들어야 한다.

| 항목 | 값 |
|---|---|
| 2G 결과 | `D:/workspace-github/xpe-2g-offline/batch-20261022-1230/out` (2026-10-22 일괄 실행, 오프셋 끔·게인 켬·결함 켬) |
| PRE 결과 | `pre-w3072-20261117-1513` (2026-11-17, 커밋된 소스로 빌드) |
| 쌍 수 | 6세트 180쌍: `s0_gf`, `s1_um300`, `s4_csi_pre`, `s5_csi_post`, `s6_gad_pre`, `s7_gad_post` |
| manifest | `D:/workspace-github/xpe-2g-offline/compare-<세트>-<번호>.e2e.json` |
| 목록 | `D:/workspace-github/xpe-2g-offline/compare-index.tsv` — 세트, 번호, 파일 이름, 비교 가능 여부, 사유, 차이 후보 수 |

- 영상과 manifest는 저장소 밖에 있으며 **커밋하지 않는다.** 증거에는 경로·해시·숫자만 남긴다.

## 준비

1. 운영자 앱을 빌드한다.

   ```powershell
   dotnet build gui/ImageProcTest/ImageProcTest.csproj -c Debug
   ```

2. Python과 NumPy가 설치되어 있는지 확인한다. 창은 열 때마다 계약 검사 스크립트
   `tools/calibration/comparison_manifest.py`를 실행한다.

## 비교 창 열기

1. 앱 메뉴에서 **Tools → Compare Imported 2G and New PRE Outputs**를 연다.
2. 처음 한 번은 두 경로를 넣는다. 넣은 값은 다음에 다시 쓰인다.
   - **Contract script path**: 저장소의 `tools/calibration/comparison_manifest.py` 전체 경로. 기본값이 없다.
   - **Python executable**: 비워 두면 PATH의 `python`을 쓴다.
3. **Browse…**를 눌러 manifest를 고른다. 처음이라면 차이가 많은
   `compare-s5_csi_post-001.e2e.json` 또는 차이가 거의 없는 `compare-s0_gf-004.e2e.json`이 좋다.
4. **Open and validate**를 누른다. 2026-11-17 측정에서 열기부터 분석까지 1.35~1.52초가 걸렸다.

창 위쪽에 주황색 안내와 함께 `MODE_MISSING`, `MAP_IDENTITY_DIFFERS` 사유가 보인다. 180쌍 모두
같다. 촬영 모드 기록이 없고 두 엔진이 쓴 결함 화소 맵이 서로 다르기 때문이다. 이 표시는
"공식 비교 조건을 다 갖추지 못했다"는 뜻이며, 영상 보기와 차이 찾기는 그대로 된다.

## 두 결과 보기

| 하고 싶은 것 | 조작 |
|---|---|
| 나란히 보기 | **Side by side** — 확대·이동·밝기(W/L)가 두 영상에 함께 적용된다 |
| 한 자리에서 번갈아 보기 | **Show B** 또는 `Ctrl+Tab` |
| 차이 영상 보기 | **Difference A−B** 또는 `Ctrl+D` |
| 확대·맞춤 | **+**, **−**, **Fit**, **100%** |
| 밝기·대비 | Window·Level 값을 넣고 **Apply W/L** (화면 표시만 바뀐다) |

차이 영상의 색은 A(2G) − B(PRE) 값이다. 음수는 파랑, 0은 흰색, 양수는 빨강이고 숫자가 아닌
값은 자홍색이다. 기본 색 범위는 ±5·s다. s는 차이의 강건 척도(1.4826 × 중앙값 절대 편차)이며,
범위를 넘는 화소는 끝 색으로 칠하고 그 수를 범례에 적는다. 전체 값 범위로 보고 싶으면
**Difference: extrema range**를 켠다.

## 차이가 큰 곳 찾기

1. **Region k**(기본 5)와 **minimum N**(기본 16)을 확인한다. |차이|가 k·s보다 크고 서로 붙은
   화소가 N개 이상인 영역만 후보가 된다.
2. **Find difference regions**를 누른다.
3. 오른쪽 목록에서 영역을 누르면 두 영상이 그 자리로 확대·이동한다.

- 상자는 차이 합이 큰 순서로 최대 20개만 그린다. 후보가 20개를 넘으면 위쪽에
  "차이가 넓게 퍼짐 — 후보 M개 중 상위 20"이 뜬다. 이때는 한 곳에 몰린 결함이 아니라 넓은 영역의
  작은 차이일 가능성이 크다. 예를 들어 s5_csi_post/001은 후보 1377개였고, 큰 영역의 평균 차이는
  2~5 화소값이었다.
- **Defect-map disagreement (cyan)**를 켜면 한 엔진만 결함으로 보정한 화소를 하늘색으로 표시한다.
  결함 맵이 없는 쌍은 이 항목이 꺼지고 이유가 표시된다.
- 후보 계산은 미리보기 표본(가로·세로 한 칸 건너, 최대 4백만 표본)에서 한다. 원본 전체 해상도
  통계가 필요하면 **Compute and save A−B statistics**를 누른다.

## 결과 읽을 때 주의

- 차이 수치와 표식은 두 파일의 숫자 차이다. **어느 엔진이 더 좋다는 판정이 아니다.**
- 창은 입력이 실제 촬영 영상인지, 2G 결과가 제품 생산 기록인지 확인하지 않는다. 주황색 안내가
  이 점을 늘 표시한다.

## 문제가 생기면

| 증상 | 확인할 것 |
|---|---|
| "could not open" 또는 `MISSING_FILE` | manifest가 가리키는 2G·PRE 파일이 그 경로에 있는지 |
| "no response from the contract script" | **Contract script path**와 **Python executable** 값, NumPy 설치 여부 |
| 차이 영상이 거의 흰색 | **Difference: extrema range**가 켜져 있지 않은지 |
| 상자가 없고 목록만 있음 | 후보가 20개를 넘은 경우다. 위쪽 경고와 M 값을 본다 |
