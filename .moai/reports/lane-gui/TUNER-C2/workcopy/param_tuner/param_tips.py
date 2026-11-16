"""파라미터 툴팁(설명) 생성.

설명은 run_pipeline.py 의 주석과 pe_post/params.py 의 변환식(UI 값 → 내부값)에 근거한다.
"""
import re

from param_tuner.param_space import GROUP_TITLES, ParamDef
from param_tuner.theme import C, GROUP_COLORS

# idx -> (기능, 값을 바꾸면, UI→내부 변환)
_TIPS = {
    0:  ("전역 대비. 톤 곡선(시그모이드)의 기울기를 결정합니다.",
         "높을수록 S-curve가 가팔라져 대비가 강해집니다.", "UI 0~30 → 내부 1~61"),
    1:  ("밝기 편향. ROI(Region Of Interest) 중심값을 이동시켜 전체 밝기를 조절합니다.",
         "높을수록 영상이 밝아집니다.", "UI 0~30 → 내부 -60~60"),
    32: ("흉부 기울기 보상. 척추 방향의 좌우/상하 밝기 기울기를 보정합니다. (Chest 전용)",
         "높을수록 기울기 보정이 강해집니다.", "UI 0~50 → 내부 0~50"),
    2:  ("관용도(Latitude) 기본 강도. 밝은 영역의 동적범위를 압축합니다. 0이면 기능이 꺼집니다.",
         "높을수록 밝은 영역이 더 압축됩니다.", "UI 0~10 → 내부 0 / 0.50~0.68"),
    3:  ("밝은 영역 추가 압축. Latitude가 0보다 클 때만 적용됩니다.",
         "높을수록 밝은 영역 압축이 추가로 강해집니다.", "UI 0~25 → 내부 0~0.48"),
    13: ("어두운 영역 추가 압축. Latitude가 0보다 클 때만 적용됩니다.",
         "높을수록 어두운 영역 압축이 추가로 강해집니다.", "UI 0~25 → 내부 0~0.48"),
    4:  ("관용도 보존 구간. ROI(Region Of Interest) 영역의 톤을 얼마나 보존할지 정합니다.",
         "높을수록 중심 톤 보존 구간이 넓어집니다.", "UI 0~10 → 내부 0.50~1.00"),
    7:  ("ROI(Region Of Interest) 측정 영역 형태 (0=전체, 1=가로50%, 2=세로50%, 3=원형, 4=중심50%). "
         "Chest 는 파이프라인 12단계에서 2로 고정되므로 비-Chest 프로토콜에서만 자동 활성화됩니다.",
         "측정 영역 여백 비율이 달라져 ROI 대표 밝기 산출 범위가 바뀝니다.", "UI 0~4"),
    8:  ("ROI(Region Of Interest) 대표 백분위. ROI 안에서 어느 밝기를 대표값으로 삼을지 정합니다.",
         "대표 밝기 기준점이 이동해 전체 밝기/대비 균형이 바뀝니다.", "UI 0~50 → 내부 0~100 (%)"),
    9:  ("저콘트라스트 영역(폐 등)의 밝기 기준 백분위. (Chest 전용 — 비-Chest 에서는 자동 제외)",
         "폐 영역 기준점이 이동해 폐야 밝기가 바뀝니다.", "UI 0~30 → 내부 0~30"),
    10: ("시그모이드 중심 편향(key_feature_bias). ROI 대표값 대비 톤 곡선 중심을 이동시킵니다.",
         "중심 기준이 이동해 전체 밝기 밸런스가 바뀝니다.", "UI -20~20 → 내부 -80~80"),
    11: ("저콘트라스트 영역(폐) 목표 밝기. (v31 파이프라인 미참조 — 기본 탐색 제외)",
         "v31 경로에서는 출력에 영향을 주지 않습니다.", "UI 0~50 → 내부 0~50"),
    12: ("고콘트라스트 영역(종격/심장) 목표 밝기. (v31 파이프라인 미참조 — 기본 탐색 제외)",
         "v31 경로에서는 출력에 영향을 주지 않습니다.", "UI 0~50 → 내부 50~100"),
    14: ("고주파1 선예도. 가장 세밀한 디테일을 강조합니다.",
         "높을수록 미세 디테일(잔 구조/노이즈 포함)이 강조됩니다.", "UI 0~30 → 내부 0.1~6.1"),
    15: ("고주파2 선예도. 중간 주파수 대역의 선예도.",
         "높을수록 중간 크기 구조의 선명도가 올라갑니다.", "UI 0~30 → 내부 0.1~6.1"),
    16: ("저주파1 선예도. 구조/질감을 강조하는 대역1.",
         "높을수록 큰 구조의 대비가 강조됩니다.", "UI 0~30 → 내부 0.1~2.5"),
    17: ("저주파2 선예도. 구조/질감을 강조하는 대역2.",
         "높을수록 더 넓은 구조의 대비가 강조됩니다.", "UI 0~30 → 내부 0.1~2.5"),
    19: ("어두운 영역 감마. 검은색 영역의 압축 정도를 정합니다.",
         "값이 클수록 내부 계수가 작아집니다.", "UI 0~20 → 내부 0.95~0.05"),
    20: ("밝은 영역 감마. 흰색 영역의 압축 정도를 정합니다.",
         "값이 클수록 내부 계수가 커집니다.", "UI 0~20 → 내부 0.05~0.95"),
    5:  ("어두운 헤일로 억제 (저대역 DCS2(Detail Contrast Suppression 2)). 어두운 영역 디테일의 과대비를 억제합니다.",
         "내부값 1.0(UI 30)이면 억제 끔. 낮출수록 억제가 강해집니다.", "UI 0~30 → 내부 0.1~1.0"),
    21: ("디테일 대비 상한 (고주파 DCS2(Detail Contrast Suppression 2)). 고주파 디테일의 과대비를 억제합니다.",
         "내부값 1.0(UI 30)이면 억제 끔. 낮출수록 억제가 강해집니다.", "UI 0~30 → 내부 0.1~1.0"),
    33: ("대비 억제가 적용되는 밝기 범위 (contrast_reduction_range).",
         "높을수록 더 넓은 밝기 범위에 대비 억제가 적용됩니다.", "UI 0~30 → 내부 0~0.3"),
    28: ("밝은 영역 저대역 대비 억제 (NECR_W_01, DCS(Detail Contrast Suppression)[0,1]).",
         "내부값 1.0(UI 30)=억제 끔. 낮출수록 억제가 강해집니다.", "UI 0~30 → 내부 0.1~1.0"),
    29: ("밝은 영역 중대역 대비 억제 (NECR_W_23, DCS(Detail Contrast Suppression)[2,3]).",
         "내부값 1.0(UI 30)=억제 끔. 낮출수록 억제가 강해집니다.", "UI 0~30 → 내부 0.1~1.0"),
    30: ("어두운 영역 저대역 대비 억제 (NECR_B_01, DCS(Detail Contrast Suppression)[4,5]).",
         "내부값 1.0(UI 30)=억제 끔. 낮출수록 억제가 강해집니다.", "UI 0~30 → 내부 0.1~1.0"),
    31: ("어두운 영역 중대역 대비 억제 (NECR_B_23, DCS(Detail Contrast Suppression)[6,7]).",
         "내부값 1.0(UI 30)=억제 끔. 낮출수록 억제가 강해집니다.", "UI 0~30 → 내부 0.1~1.0"),
    6:  ("저대역 대비 상한 (IBDC_W_01). 저주파 레이어 대비의 상한을 정합니다.",
         "값이 클수록 저주파 대비 허용치가 커집니다.", "UI 0~30 → 내부 0.1~1.0"),
    36: ("베이스 기반 대비 억제 - 밝은 영역 중대역 (IBDC_W_23, contrast_reduction_base[2,3]).",
         "내부값 1.0(UI 30)=억제 끔. 낮출수록 억제가 강해집니다.", "UI 0~30 → 내부 0.1~1.0"),
    37: ("베이스 기반 대비 억제 - 어두운 영역 저대역 (IBDC_B_01, contrast_reduction_base[4,5]).",
         "내부값 1.0(UI 30)=억제 끔. 낮출수록 억제가 강해집니다.", "UI 0~30 → 내부 0.1~1.0"),
    38: ("베이스 기반 대비 억제 - 어두운 영역 고대역 (IBDC_B_23, contrast_reduction_base[6,7]).",
         "내부값 1.0(UI 30)=억제 끔. 낮출수록 억제가 강해집니다.", "UI 0~30 → 내부 0.1~1.0"),
    34: ("밝은 영역 미세 헤일로 감쇠 (detail/structure decay).",
         "내부값 1.0(UI 30)이면 끔. 낮출수록 밝은 헤일로가 더 감쇠됩니다.", "UI 0~30 → 내부 0.1~1.0"),
    35: ("어두운 영역 미세 헤일로 감쇠 (detail/structure decay under).",
         "내부값 1.0(UI 30)이면 끔. 낮출수록 어두운 헤일로가 더 감쇠됩니다.", "UI 0~30 → 내부 0.1~1.0"),
    24: ("노이즈 감소 방식 선택. 0=블록 NLM(Non-Local Means) 방식(포팅됨), 1=미포팅(bypass).",
         "1이면 아래 노이즈 파라미터(23/26)가 출력에 영향을 주지 않습니다. 먼저 탐색됩니다.", "0 또는 1"),
    23: ("노이즈 감소율. 블록 NLM(Non-Local Means) 방식의 감소 강도.",
         "높을수록 노이즈가 더 많이 줄고 질감이 부드러워집니다. (NR(Noise Reduction) 방식=0 일 때만 유효)", "UI 0~5 → 내부 0~50 (%)"),
    25: ("디블러링 강도. (v31 파이프라인 미참조 — 기본 탐색 제외)",
         "v31 경로에서는 출력에 영향을 주지 않습니다.", "UI 0~8 → 내부 0~80 (%)"),
    26: ("노이즈 백색화. 잔류 노이즈를 무상관 입자 형태로 바꿉니다.",
         "높을수록 노이즈가 더 균일한 입자로 바꿉니다.", "UI 0~5 → 내부 0~100 (%)"),
}

_COST = {
    "tone": "빠름 (피라미드/ECR1 캐시 재사용, 평가 1회 약 0.7초 / Chest Balance 변경 시 약 4초)",
    "freq": "빠름 (피라미드/Base/ECR 캐시 재사용, 평가 1회 약 0.5초)",
    "suppress": "빠름~중간 (NECR/IBDC 약 0.5초, Halo/Decay 변경 시 약 1.8초)",
    "noise": "최초 1회 약 40초 (블록 NLM), 이후 강도/백색화 변경 시 약 0.2초",
    "lut": "매우 빠름 (Manual LUT(Look-Up Table) 만 재실행, 약 0.2초)",
}


def _lut_tip(d: ParamDef):
    n, axis = d.name.split()[1], d.name.split()[2]
    if axis == "X":
        return (f"Manual LUT(Look-Up Table) 제어점(CP, Control Point) {n} 의 입력(x) 위치. 톤 곡선이 휘는 위치를 정합니다.",
                "x 는 CP1<CP2<CP3<CP4 순서를 유지해야 합니다(위반 후보는 건너뜀).", "UI 0~100 → 내부 0~1")
    return (f"Manual LUT(Look-Up Table) 제어점(CP, Control Point) {n} 의 출력(y) 값. 해당 입력 밝기의 출력 밝기를 정합니다.",
            "대각선(x=y)이면 항등, 곡선이 위로 볼록하면 밝아지고 아래로 오목하면 어두워집니다.",
            "UI 0~100 → 내부 0~1")


# key -> (제목, 색, 기능, 의미, 계산 방법 [줄 목록], 해석/주의)
_METRIC_TIPS = {
    "loss": ("BEST LOSS", C["accent"],
             "Output 과 Target 이 얼마나 다른지를 하나의 숫자로 나타낸 최적화 목표값입니다. 튜너는 이 값이 가장 작아지는 파라미터를 찾습니다.",
             "0 이면 두 영상이 (비교 영역에서) 완전히 같다는 뜻이고, 클수록 다릅니다. 카드에는 지금까지 찾은 best 값과 초기값 대비 증감(%)이 표시됩니다.",
             ["loss = w<sub>pix</sub>·L1·10 + w<sub>ssim</sub>·(1 - SSIM) + w<sub>edge</sub>·E·10 + w<sub>hist</sub>·H·10",
              "L1(L1 Norm) : 평균 절대 오차 (픽셀 오차)",
              "SSIM(Structural Similarity Index Measure) : 구조 유사도 (아래 SSIM 설명 참고)",
              "E : Sobel(3×3) 경사 크기 차이의 평균 (선예도/엣지 차이)",
              "H : 분위수(1·2·5·10·20…90·95·98·99%) 값 차이의 평균 (밝기·대비 분포 차이)",
              "비교 전 영상을 코드값 ÷ 최대값으로 [0,1] 정규화하고, 조사야(collimator) 안쪽 화소만 사용합니다. "
              "w 는 설정의 'Loss 가중치'(기본 1.0), ×10 은 항목 간 크기를 맞추는 상수입니다."],
             "가중치를 바꾸면 loss 의 절대값이 달라지므로, 서로 다른 가중치로 얻은 loss 끼리는 비교하지 마세요. "
             "post 처리 결과와 target 을 전체 해상도에서 비교해 계산합니다."),
    "psnr": ("PSNR(Peak Signal-to-Noise Ratio)", C["good"],
             "Output 과 Target 의 화소 오차를 신호 대 잡음비(dB)로 나타낸 값입니다.",
             "높을수록 두 영상이 비슷합니다. 대략 30 dB 이상이면 차이가 작고, 40 dB 이상이면 거의 구분하기 어렵습니다. 같은 영상이면 99 dB 로 표시됩니다.",
             ["MSE(Mean Squared Error) = mean( (Output - Target)² ),  값은 [0,1] 정규화 기준",
              "PSNR = 10 · log<sub>10</sub>( 1 / MSE )   [dB]",
              "최대 신호값은 비트 심도의 최대 코드값(14bit: 16383)에 해당하는 1.0 입니다."],
             "제곱 오차 기반이라 큰 오차(밝기 어긋남)에 민감하고, 구조/질감 차이는 SSIM(Structural Similarity Index Measure)이 더 잘 반영합니다. 조사야 안쪽 화소만 계산합니다."),
    "ssim": ("SSIM(Structural Similarity Index Measure)", C["accent"],
             "밝기, 대비, 구조(패턴)의 유사도를 지역 창 단위로 비교하는 지표입니다. 사람이 느끼는 유사도에 더 가깝습니다.",
             "1 에 가까울수록 같은 영상입니다. 0.95 이상이면 구조가 매우 비슷합니다. loss 에는 (1 - SSIM) 으로 들어갑니다.",
             ["각 화소에서 11×11 가우시안 창(σ = 1.5)으로 평균 μ, 분산 σ², 공분산 σ<sub>ab</sub> 를 구합니다.",
              "SSIM = (2μ<sub>a</sub>μ<sub>b</sub> + C1)(2σ<sub>ab</sub> + C2) / ((μ<sub>a</sub>² + μ<sub>b</sub>² + C1)(σ<sub>a</sub>² + σ<sub>b</sub>² + C2))",
              "C1 = 0.01², C2 = 0.03² (값 범위 1.0 기준), 조사야 안쪽 평균을 사용합니다."],
             "전체 밝기가 약간 어긋나도 SSIM 은 높게 나올 수 있으므로, 밝기 일치는 PSNR(Peak Signal-to-Noise Ratio)/MAE(Mean Absolute Error)와 함께 보세요."),
    "mae": ("MAE(Mean Absolute Error)", C["warn"],
            "Output 과 Target 의 평균 절대 오차입니다. 코드값(픽셀값) 단위라서 '평균적으로 몇 계조 차이가 나는지'를 직관적으로 보여 줍니다.",
            "낮을수록 비슷합니다. 14bit 영상(0~16383)에서 MAE 100 은 평균 약 0.6% 차이입니다.",
            ["MAE = mean( | Output - Target | ) × 최대 코드값",
             "[0,1] 정규화 L1(L1 Norm) 오차에 비트 심도의 최대 코드값(14bit: 16383)을 곱해 코드값 단위로 환산합니다. 조사야 안쪽 화소만 사용합니다."],
            "전체 밝기 편차(Brightness/LUT(Look-Up Table) 계열 파라미터)에 가장 민감하게 반응하는 지표입니다."),
    "evals": ("평가 / 시간", C["gray"],
              "파이프라인을 실제로 실행해 loss 를 계산한 횟수와, 튜닝을 시작한 뒤 지난 시간입니다.",
              "횟수가 '최대 평가수'에, 시간이 '최대 시간'에 먼저 도달하면 탐색이 종료됩니다. 아래 줄의 '개선 N회'는 best 가 갱신된 횟수입니다.",
              ["평가 1회 = 파라미터 한 벌로 14~17단계(slope, Multi-Scale, 노이즈 감소, LUT(Look-Up Table)) 실행 + loss 계산",
               "이미 평가한 같은 파라미터 조합은 결과를 재사용하므로 횟수에 포함하지 않습니다.",
               "중간 결과를 캐시하므로 LUT(Look-Up Table) 만 바뀌면 약 0.2초, 노이즈 파라미터는 약 40초(NLM(Non-Local Means) 활성 시), "
               "그 외는 약 5초가 걸립니다."],
              "개선 횟수가 오랫동안 늘지 않으면 수렴 중이라는 뜻입니다. 필요하면 [중지]로 현재 best 를 저장할 수 있습니다."),
}


def metric_tooltip_html(key: str) -> str:
    title, col, func, meaning, calc, note = _METRIC_TIPS[key]
    calc_html = "".join(f"<div style='color:{C['muted']}'>· {line}</div>" for line in calc)
    return (f"<table width='440'><tr><td>"      # Qt 리치텍스트는 max-width 미지원 → 폭 고정 테이블로 줄바꿈
            f"<div style='font-size:10.5pt'><b>{title}</b> <span style='color:{col}'>●</span></div><hr>"
            f"<div><b>기능</b><br>{func}</div><br>"
            f"<div style='color:{C['good']}'><b>의미</b><br>{meaning}</div><br>"
            f"<div><b>계산 방법</b></div>{calc_html}<br>"
            f"<div style='color:{C['warn']}'>{note}</div></td></tr></table>")


# 파라미터 이름에 포함된 약어의 풀네임 (확인된 것만. NECR/IBDC 는 소스에 정식 명칭이 없어 제외)
_ABBR = {"NR": "Noise Reduction", "ROI": "Region Of Interest", "LUT": "Look-Up Table", "CP": "Control Point"}


def tooltip_html(d: ParamDef) -> str:
    func, effect, rng = _lut_tip(d) if d.kind == "lut" else _TIPS.get(d.index, ("", "", ""))
    gcol = GROUP_COLORS[d.group]
    idx = f"[{d.index}]" if d.kind == "post" else ""
    rows = [f"<div style='font-size:10.5pt'><b>{d.name}</b> <span style='color:{C['muted']}'>{idx}</span> "
            f"&nbsp;<span style='color:{gcol}'>● {GROUP_TITLES[d.group]}</span></div><hr>",
            f"<div style='white-space:normal'>{func}</div>"]
    if effect and effect != "-":
        rows.append(f"<div style='color:{C['good']}'>→ {effect}</div>")
    if rng:
        rows.append(f"<div style='color:{C['muted']}'>변환: {rng}</div>")
    abbr = [f"{t}({_ABBR[t]})" for t in re.findall(r"[A-Z]{2,}", d.name) if t in _ABBR]
    if abbr:
        rows.insert(1, f"<div style='color:{C['muted']}'>약어: {', '.join(abbr)}</div>")
    rows.append(f"<div style='color:{C['muted']}'>탐색 범위: {d.lo} ~ {d.hi} · run_pipeline 기본값 {d.default}</div>")
    rows.append(f"<div style='color:{C['warn']}'>재평가 비용: {_COST[d.group]}</div>")
    return f"<table width='400'><tr><td>{''.join(rows)}</td></tr></table>"
