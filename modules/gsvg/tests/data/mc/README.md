# MC 팬텀 — 가상 그리드 독립 검증용 시험 데이터 (#180, QA-B-95)

main `7b72124` 시점의 `tools/mcsim/phantoms/` 와 `tools/mcsim/tables/` 를 **그대로 복사**했다(pre 레인 산출물). 원본이 바뀌면 이 복사본도 같이 바꾼다.

| 파일 | 원본 (마지막 변경 커밋) |
|---|---|
| `step_80kVp*`, `wedge_80kVp*` | `tools/mcsim/phantoms/` (`2f61814`, QA-A-98) |
| `scatter_kernels_water_csi600.csv` | `tools/mcsim/tables/` (`25134b4`, QA-A-96) |
| `wet_water_csi600.csv` | `tools/mcsim/tables/` (`2f61814`, QA-A-98) |

영상은 80×80 float32 리틀엔디언이고, 행 = 검출기 z, 열 = 검출기 x 이다. 단위와 가정은 json 에 있다. 시뮬레이션 기반이며 보정 전이다.
