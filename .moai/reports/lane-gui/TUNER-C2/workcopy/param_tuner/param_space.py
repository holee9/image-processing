"""튜닝 대상 파라미터 정의 (PostParam 40개 + LUT 12개).

default 값은 run_pipeline.py 상단 상수를 그대로 사용한다 (단일 출처).
탐색 제외(고정): idx 18(대역 레이어셋), 22, 27, 39.
"""
from dataclasses import dataclass, field
from typing import List

N_POST = 40
N_LUT = 12

# 고정 파라미터: idx -> (run_pipeline 상수명 | 고정값)
FIXED_POST = {18: "STRUCT_CONTRAST0", 22: 31, 27: 0, 39: 1}

# 파이프라인(v31) 코드에서 참조되지 않는 무효 파라미터 (탐색 제외)
UNUSED_POST = {11, 12, 25}
# 부위별 조건부 유효 파라미터 (CHEST_ORIENTATION_TYPES = (1101, 1102, 1103, 1104, 1107) 기준)
CHEST_ONLY_POST = {9, 32}       # 9: Key Feature L (폐 백분위), 32: Chest Balance (기울기 보상)
NON_CHEST_ONLY_POST = {7}       # 7: ROI Type (Chest 는 12단계에서 2로 덮어써짐, 비-Chest 에서만 0~4 유효)

# idx -> (표시명, run_pipeline 상수명, 그룹, 최소, 최대, 기본 탐색 여부)
# 범위는 UI 슬라이더 범위. run_pipeline 기본값이 범위 밖이면 기본값을 포함하도록 확장.
_POST_DEFS = [
    (0,  "Contrast",           "CONTRAST",              "tone",     0, 30, True),
    (1,  "Brightness",         "BRIGHTNESS",            "tone",     0, 30, True),
    (32, "Chest Balance",      "CHEST_BALANCE",         "tone",     0, 50, True),   # Chest 전용
    (2,  "Latitude",           "LATITUDE",              "tone",     0, 10, True),
    (3,  "Latitude Over",      "LATITUDE_OVER",         "tone",     0, 25, True),
    (13, "Latitude Under",     "LATITUDE_UNDER",        "tone",    -5, 25, True),
    (4,  "Latitude ROI",       "LATITUDE_ROI",          "tone",     0, 20, True),
    (7,  "ROI Type",           "ROI_TYPE",              "tone",     0,  4, False),  # 비-Chest 전용 (0~4만 유효)
    (8,  "Feature Pt",         "FEATURE_PT",            "tone",     0, 50, True),
    (9,  "Key Feature L",      "KEY_FEATURE_PERCENT_L", "tone",     0, 30, True),   # Chest 전용
    (10, "Key Feature S",      "KEY_FEATURE_PERCENT_S", "tone",   -20, 20, True),
    (11, "Target Bright L",    "TARGET_BRIGHTNESS_L",   "tone",     0, 50, False),  # v31 미참조
    (12, "Target Bright S",    "TARGET_BRIGHTNESS_S",   "tone",     0, 50, False),  # v31 미참조
    (14, "Fine Sharpness",     "FINE_SHARPNESS",        "freq",     0, 30, True),
    (15, "Sharpness",          "SHARPNESS",             "freq",     0, 30, True),
    (16, "Struct Strength2",   "STRUCT_STRENGTH2",      "freq",     0, 30, True),
    (17, "Struct Strength3",   "STRUCT_STRENGTH3",      "freq",     0, 30, True),
    (19, "Struct Contrast1",   "STRUCT_CONTRAST1",      "freq",     0, 20, True),
    (20, "Struct Contrast2",   "STRUCT_CONTRAST2",      "freq",     0, 20, True),
    (5,  "Large Black Halo",   "LARGE_BLACK_HALO",      "suppress", 0, 30, True),
    (21, "Struct Density",     "STRUCT_DENSITY",        "suppress", 0, 30, True),
    (33, "Detail ROI",         "DETAIL_ROI",            "suppress", 0, 30, True),
    (28, "NECR_W_01",          "NECR_W_01",             "suppress", 0, 30, True),
    (29, "NECR_W_23",          "NECR_W_23",             "suppress", 0, 30, True),
    (30, "NECR_B_01",          "NECR_B_01",             "suppress", 0, 30, True),
    (31, "NECR_B_23",          "NECR_B_23",             "suppress", 0, 30, True),
    (6,  "IBDC_W_01",          "IBDC_W_01",             "suppress", 0, 30, True),
    (36, "IBDC_W_23",          "IBDC_W_23",             "suppress", 0, 30, True),
    (37, "IBDC_B_01",          "IBDC_B_01",             "suppress", 0, 30, True),
    (38, "IBDC_B_23",          "IBDC_B_23",             "suppress", 0, 30, True),
    (34, "Small White Halo",   "SMALL_WHITE_HALO",      "suppress", 0, 30, True),
    (35, "Small Black Halo",   "SMALL_BLACK_HALO",      "suppress", 0, 30, True),
    # NR_SELECTION: 0=블록NLM(포팅됨), 1=bypass(NR 파라미터 무효). 먼저 탐색해야 23/26 이 의미를 가짐
    (24, "NR Selection",       "NR_SELECTION",          "noise",    0, 1,  True),
    (23, "Noise Reduction",    "NOISE_REDUCTION",       "noise",    0, 5,  True),
    (25, "NR Deblurring",      "NR_DEBLURRING",         "noise",    0, 8,  False),  # v31 미참조
    (26, "NR Whitening",       "NR_WHITENING",          "noise",    0, 5,  True),
]

GROUP_ORDER = ["tone", "freq", "suppress", "noise", "lut"]
GROUP_TITLES = {"tone": "계조/톤", "freq": "주파수/선예도", "suppress": "과대비 억제",
                "noise": "노이즈", "lut": "Manual LUT"}


@dataclass
class ParamDef:
    key: str          # "post.N" | "lut.N"
    kind: str         # "post" | "lut"
    index: int        # 리스트 내 위치
    name: str
    attr: str         # run_pipeline 상수명
    group: str
    lo: int
    hi: int
    default: int
    enabled: bool = True


@dataclass
class Params:
    """튜닝 파라미터 한 벌 (PostParam[40] + LUT[12])."""
    post: List[int] = field(default_factory=lambda: [0] * N_POST)
    lut: List[int] = field(default_factory=lambda: [0] * N_LUT)

    def copy(self) -> "Params":
        return Params(list(self.post), list(self.lut))

    def key(self) -> tuple:
        return tuple(self.post) + tuple(self.lut)

    def get(self, d: ParamDef) -> int:
        return (self.post if d.kind == "post" else self.lut)[d.index]

    def set(self, d: ParamDef, v: int):
        (self.post if d.kind == "post" else self.lut)[d.index] = int(v)

    def lut_valid(self) -> bool:
        """CP x 좌표는 단조 증가여야 한다."""
        xs = [self.lut[i * 3] for i in range(4)]
        return all(xs[i] < xs[i + 1] for i in range(3)) and xs[0] >= 0 and xs[3] <= 100


def _rp():
    import run_pipeline
    return run_pipeline


def default_params() -> Params:
    """run_pipeline.py 상단 상수로 default Params 생성."""
    rp = _rp()
    p = Params()
    for idx, v in FIXED_POST.items():
        p.post[idx] = int(getattr(rp, v)) if isinstance(v, str) else int(v)
    for (idx, _n, attr, *_r) in _POST_DEFS:
        p.post[idx] = int(getattr(rp, attr))
    for n in range(4):
        for j, c in enumerate("XYW"):
            p.lut[n * 3 + j] = int(getattr(rp, f"LUT_CP{n + 1}_{c}"))
    return p


def is_chest_object_type(object_type: int) -> bool:
    """촬영부위(object_type 또는 프로토콜 코드)가 Chest 전용 알고리즘 대상(1101~1104, 1107)인지 판별."""
    from pe_post.common import CHEST_ORIENTATION_TYPES
    return int(object_type) in CHEST_ORIENTATION_TYPES


def apply_object_type_mask(defs: List[ParamDef], object_type: int) -> List[ParamDef]:
    """촬영부위(object_type)에 따라 Chest 전용 / 비-Chest 전용 / 무효 파라미터의 enabled 를 갱신한다."""
    is_chest = is_chest_object_type(object_type)
    for d in defs:
        if d.kind != "post":
            continue
        if d.index in UNUSED_POST:
            d.enabled = False
        elif d.index in CHEST_ONLY_POST:
            d.enabled = is_chest
        elif d.index in NON_CHEST_ONLY_POST:
            d.enabled = not is_chest
    return defs


def build_param_defs(defaults: Params = None, object_type: int = None) -> List[ParamDef]:
    """탐색 후보 ParamDef 목록 (그룹 순서대로 정렬). object_type 지정 시 부위별 활성 상태 자동 반영."""
    defaults = defaults or default_params()
    defs: List[ParamDef] = []
    for (idx, name, attr, grp, lo, hi, en) in _POST_DEFS:
        v = defaults.post[idx]
        defs.append(ParamDef(f"post.{idx}", "post", idx, name, attr, grp,
                             min(lo, v), max(hi, v), v, en))
    for n in range(4):
        for j, c in enumerate("XY"):  # weight(W)는 고정
            i = n * 3 + j
            defs.append(ParamDef(f"lut.{i}", "lut", i, f"LUT CP{n + 1} {c}",
                                 f"LUT_CP{n + 1}_{c}", "lut", 0, 100,
                                 defaults.lut[i], True))
    defs.sort(key=lambda d: GROUP_ORDER.index(d.group))  # 안정 정렬: 그룹 내 순서 유지
    if object_type is not None:
        apply_object_type_mask(defs, object_type)
    return defs


def params_to_dict(p: Params) -> dict:
    return {"post_param": list(p.post), "lut_param": list(p.lut)}


def params_from_dict(d: dict) -> Params:
    post, lut = list(d["post_param"]), list(d["lut_param"])
    if len(post) != N_POST or len(lut) != N_LUT:
        raise ValueError("파라미터 길이 불일치")
    return Params([int(v) for v in post], [int(v) for v in lut])


def make_snippet(p: Params, protocol: str = "") -> str:
    """run_pipeline.py 상단에 붙여넣을 수 있는 상수 스니펫. protocol 을 주면 머리말에 프로토콜 이름을 적는다."""
    lines = ["# param_tuner 가 생성한 최적 파라미터 (run_pipeline.py 상단 상수 대체용)"]
    if protocol:
        lines.append(f"# 프로토콜: {protocol}")
    items = [(idx, name, attr) for (idx, name, attr, *_r) in _POST_DEFS]
    for idx, attr in FIXED_POST.items():
        if isinstance(attr, str):
            items.append((idx, "Struct Contrast0 (LayerSet)", attr))
    for (idx, name, attr) in sorted(items, key=lambda t: t[0]):
        lines.append(f"{attr:<24}= {p.post[idx]:<4}  # {idx}: {name}")
    for n in range(4):
        x, y, w = p.lut[n * 3:n * 3 + 3]
        lines.append(f"LUT_CP{n + 1}_X, LUT_CP{n + 1}_Y, LUT_CP{n + 1}_W = {x}, {y}, {w}")
    return "\n".join(lines) + "\n"
