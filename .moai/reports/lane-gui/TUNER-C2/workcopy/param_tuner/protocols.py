"""촬영 프로토콜 목록 관리 (이름 + 프로토콜 번호 + 선택적 엔진 ObjectType).

목록은 exe(또는 param_tuner 폴더) 옆의 protocols.json 에 저장되어 코드 수정/재빌드 없이
UI([관리] 버튼) 또는 파일 편집으로 추가할 수 있다.

    {"protocols": [{"name": "CHEST_PA", "code": 1107}, {"name": "ABDOMEN_ERECT", "code": 1002}, ...]}

- code        : 프로토콜 번호 (doc/protocol.xlsx 의 Protocol Number = Prefix + Item, 1001..4001).
- object_type : (선택) 파이프라인에 전달할 ObjectType 오버라이드. 생략하면 code 를 그대로 전달한다.
                code 와 ObjectType 은 C++ TObjectType 과 같은 4자리 코드(1001..4001)이며 별도 변환은 없다.
- 서로 다른 프로토콜이 같은 code 를 가질 수 있다 (이름만 유일하면 됨).
"""
import json
import os

from param_tuner.bootstrap import APP_DIR

PROTOCOLS_PATH = os.path.join(APP_DIR, "protocols.json")

# 최초 실행 시 파일이 없으면 이 목록으로 생성한다. (doc/protocol.xlsx 의 Protocol Number 체계: Prefix + Item, 총 136개)
DEFAULT_PROTOCOLS = [
    {"name": "ABDOMEN_DECU", "code": 1001},
    {"name": "ABDOMEN_ERECT", "code": 1002},
    {"name": "ABDOMEN_IVP", "code": 1003},
    {"name": "ABDOMEN_KUB", "code": 1004},
    {"name": "ABDOMEN_LAT", "code": 1005},
    {"name": "ABDOMEN_PRONE", "code": 1006},
    {"name": "ABDOMEN_SUPINE", "code": 1007},
    {"name": "CHEST_AP", "code": 1101},
    {"name": "CHEST_APICOGRAM", "code": 1102},
    {"name": "CHEST_DECU", "code": 1103},
    {"name": "CHEST_INFANTOGRAM", "code": 1104},
    {"name": "CHEST_LAT", "code": 1105},
    {"name": "CHEST_OBL", "code": 1106},
    {"name": "CHEST_PA", "code": 1107},
    {"name": "AC_JOINT_W_WEIGHTS", "code": 1201},
    {"name": "AC_JOINT_WO_WEIGHTS", "code": 1202},
    {"name": "CLAVICLE_AP", "code": 1203},
    {"name": "CLAVICLE_AXIAL", "code": 1204},
    {"name": "COCCYX_AP", "code": 1301},
    {"name": "COCCYX_AP_AXIAL", "code": 1302},
    {"name": "COCCYX_LAT", "code": 1303},
    {"name": "C_SPINE_AP", "code": 1401},
    {"name": "C_SPINE_EXT", "code": 1402},
    {"name": "C_SPINE_FLEX", "code": 1403},
    {"name": "C_SPINE_LAT", "code": 1404},
    {"name": "C_SPINE_OBL", "code": 1405},
    {"name": "C_SPINE_OPEN_MOUTH", "code": 1406},
    {"name": "C_SPINE_SWIMMER", "code": 1407},
    {"name": "CT_SPINE_AP", "code": 1501},
    {"name": "CT_SPINE_EXT", "code": 1502},
    {"name": "CT_SPINE_FLEX", "code": 1503},
    {"name": "CT_SPINE_LAT", "code": 1504},
    {"name": "CT_SPINE_OBL", "code": 1505},
    {"name": "CT_SPINE_SWIMMER", "code": 1506},
    {"name": "MAXILLARY_AP", "code": 1601},
    {"name": "NASAL_LAT", "code": 1602},
    {"name": "ORBITAL_AP", "code": 1603},
    {"name": "ORBITAL_AXIAL", "code": 1604},
    {"name": "ORBITAL_CALDWELL", "code": 1605},
    {"name": "ORBITAL_TOWNE", "code": 1606},
    {"name": "ORBITAL_WATERS", "code": 1607},
    {"name": "SELLA_TURCICA_LAT", "code": 1608},
    {"name": "TM_JOINT_CLOSE", "code": 1609},
    {"name": "TM_JOINT_OPEN", "code": 1610},
    {"name": "ZYGOMATIC_ARCH", "code": 1611},
    {"name": "FEMUR_AP", "code": 1701},
    {"name": "FEMUR_LAT", "code": 1702},
    {"name": "FEMUR_OBL", "code": 1703},
    {"name": "HIP_AP", "code": 1801},
    {"name": "HIP_AXIAL", "code": 1802},
    {"name": "HIP_FROG_LEG", "code": 1803},
    {"name": "HIP_LAT", "code": 1804},
    {"name": "HIP_OBL", "code": 1805},
    {"name": "SI_JOINT", "code": 1806},
    {"name": "SI_JOINT_AXIAL", "code": 1807},
    {"name": "SI_JOINT_OBL", "code": 1808},
    {"name": "HUMERUS_AP", "code": 1901},
    {"name": "HUMERUS_LAT", "code": 1902},
    {"name": "HUMERUS_OBL", "code": 1903},
    {"name": "HUMERUS_SWIMMER", "code": 1904},
    {"name": "L_SPINE_AP", "code": 2001},
    {"name": "L_SPINE_EXT", "code": 2002},
    {"name": "L_SPINE_FLEX", "code": 2003},
    {"name": "L_SPINE_LAT", "code": 2004},
    {"name": "L_SPINE_OBL", "code": 2005},
    {"name": "L_SPINE_SPOT", "code": 2006},
    {"name": "LS_SPINE_AP", "code": 2101},
    {"name": "LS_SPINE_EXT", "code": 2102},
    {"name": "LS_SPINE_FLEX", "code": 2103},
    {"name": "LS_SPINE_LAT", "code": 2104},
    {"name": "LS_SPINE_OBL", "code": 2105},
    {"name": "LS_SPINE_SPOT", "code": 2106},
    {"name": "MASTOID_LAWS", "code": 2201},
    {"name": "MASTOID_SCHULLER", "code": 2202},
    {"name": "MASTOID_STENVERS", "code": 2203},
    {"name": "MASTOID_TOWNE", "code": 2204},
    {"name": "PELVIS_AP", "code": 2301},
    {"name": "PELVIS_INLET", "code": 2302},
    {"name": "PELVIS_LAT", "code": 2303},
    {"name": "PELVIS_OBL", "code": 2304},
    {"name": "PELVIS_OUTLET", "code": 2305},
    {"name": "PNS_CALDWELL", "code": 2401},
    {"name": "PNS_LAT", "code": 2402},
    {"name": "PNS_TOWNE", "code": 2403},
    {"name": "PNS_WATERS", "code": 2404},
    {"name": "RIB_AP", "code": 2501},
    {"name": "RIB_AP_LOWER", "code": 2502},
    {"name": "RIB_AP_UPPER", "code": 2503},
    {"name": "RIB_OBL", "code": 2504},
    {"name": "RIB_OBL_LOWER", "code": 2505},
    {"name": "RIB_OBL_UPPER", "code": 2506},
    {"name": "RIB_PA", "code": 2507},
    {"name": "RIB_PA_LOWER", "code": 2508},
    {"name": "RIB_PA_UPPER", "code": 2509},
    {"name": "SACRUM_AP", "code": 2601},
    {"name": "SACRUM_AP_AXIAL", "code": 2602},
    {"name": "SACRUM_LAT", "code": 2603},
    {"name": "SC_JOINT_OBL", "code": 2701},
    {"name": "SC_JOINT_PA", "code": 2702},
    {"name": "SC_JOINT_AP", "code": 2703},
    {"name": "SC_JOINT_AXIAL", "code": 2704},
    {"name": "SCAPULA_AP", "code": 2801},
    {"name": "SCAPULA_LAT", "code": 2802},
    {"name": "SCAPULA_OBL", "code": 2803},
    {"name": "SHOULDER_AP", "code": 2901},
    {"name": "SHOULDER_AP_EXTERNAL", "code": 2902},
    {"name": "SHOULDER_AP_INTERNAL", "code": 2903},
    {"name": "SHOULDER_AXIAL", "code": 2904},
    {"name": "SHOULDER_GRASHEY", "code": 2905},
    {"name": "SHOULDER_OBL", "code": 2906},
    {"name": "SHOULDER_SWIMMER", "code": 2907},
    {"name": "SHOULDER_Y_VIEW", "code": 2908},
    {"name": "SKULL_AP", "code": 3001},
    {"name": "SKULL_CALDWELL", "code": 3002},
    {"name": "SKULL_LAT", "code": 3003},
    {"name": "SKULL_PA", "code": 3004},
    {"name": "SKULL_SUBMENTOVERTEX_VIEW", "code": 3005},
    {"name": "SKULL_TOWNE", "code": 3006},
    {"name": "SKULL_WATERS", "code": 3007},
    {"name": "STERNUM_LAT", "code": 3101},
    {"name": "STERNUM_OBL", "code": 3102},
    {"name": "TL_SPINE_AP", "code": 3201},
    {"name": "TL_SPINE_EXT", "code": 3202},
    {"name": "TL_SPINE_FLEX", "code": 3203},
    {"name": "TL_SPINE_LAT", "code": 3204},
    {"name": "TL_SPINE_OBL", "code": 3205},
    {"name": "T_SPINE_AP", "code": 3301},
    {"name": "T_SPINE_EXT", "code": 3302},
    {"name": "T_SPINE_FLEX", "code": 3303},
    {"name": "T_SPINE_LAT", "code": 3304},
    {"name": "T_SPINE_OBL", "code": 3305},
    {"name": "WHOLE_BODY_AP", "code": 3401},
    {"name": "WHOLE_BODY_LAT", "code": 3402},
    {"name": "LONG_BONE_AP", "code": 3501},
    {"name": "LONG_BONE_LAT", "code": 3502},
    {"name": "PHYSICAL_TEST", "code": 4001},
]


def validate(protocols):
    """목록 검증. 문제가 있으면 사용자에게 보여줄 메시지(str), 없으면 None."""
    seen = set()
    for i, p in enumerate(protocols, 1):
        name = str(p.get("name", "")).strip()
        if not name:
            return f"{i}번째 행의 프로토콜 이름이 비어 있습니다."
        if name.lower() in seen:
            return f"프로토콜 이름이 중복되었습니다: {name}"
        seen.add(name.lower())
        try:
            if int(p["code"]) < 0:
                raise ValueError
        except (KeyError, TypeError, ValueError):
            return f"'{name}' 의 코드는 0 이상의 정수여야 합니다."
        ot = p.get("object_type")
        if ot not in (None, ""):
            try:
                if int(ot) < 0:
                    raise ValueError
            except (TypeError, ValueError):
                return f"'{name}' 의 ObjectType 은 비우거나 0 이상의 정수여야 합니다."
    if not protocols:
        return "프로토콜이 하나도 없습니다."
    return None


def _clean(raw):
    out = []
    for p in raw:
        try:
            item = {"name": str(p["name"]).strip(), "code": int(p["code"])}
        except (KeyError, TypeError, ValueError):
            continue            # 형식이 맞지 않는 항목은 무시
        ot = p.get("object_type")
        if ot not in (None, ""):
            try:
                item["object_type"] = int(ot)
            except (TypeError, ValueError):
                pass
        out.append(item)
    return out


def engine_object_type(p):
    """파이프라인에 전달할 ObjectType (지정하지 않으면 프로토콜 번호 그대로)."""
    ot = p.get("object_type")
    return int(p["code"]) if ot in (None, "") else int(ot)


def save_protocols(protocols):
    data = {"protocols": _clean(protocols)}
    with open(PROTOCOLS_PATH, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)


def load_protocols():
    """저장된 목록을 읽는다. 파일이 없거나 비정상이면 기본 목록(없으면 파일도 생성)."""
    try:
        with open(PROTOCOLS_PATH, encoding="utf-8") as f:
            items = _clean(json.load(f).get("protocols", []))
        if items and validate(items) is None:
            return items
    except FileNotFoundError:
        try:
            save_protocols(DEFAULT_PROTOCOLS)       # 사용자가 편집할 수 있도록 최초 1회 생성
        except OSError:
            pass
    except (ValueError, OSError, AttributeError):
        pass                                          # 손상된 파일 → 기본 목록 사용 (파일은 덮어쓰지 않음)
    return [dict(p) for p in DEFAULT_PROTOCOLS]
