"""진입점 공통 초기화.

- PE_analysis 폴더를 sys.path 에 추가 (pe_post, run_pipeline import 용)
- 비트 심도를 pe_post import 전에 환경변수로 확정
  우선순위: --bit-depth 인자 > param_tuner/last_settings.json > 14
- run_pipeline 이 import 시 PE_BIT_DEPTH 를 덮어쓰므로, pe_post.common 을
  먼저 import 해서 값을 고정시킨다.
"""
import json
import os
import sys

FROZEN = bool(getattr(sys, "frozen", False))      # PyInstaller 실행 파일 여부
HERE = os.path.dirname(os.path.abspath(__file__))
if FROZEN:
    RES_DIR = getattr(sys, "_MEIPASS", HERE)       # 번들된 리소스(Logo.ico 등) 위치
    ROOT = RES_DIR
    APP_DIR = os.path.dirname(sys.executable)      # 설정/결과를 쓰는 위치 (exe 옆)
else:
    RES_DIR = HERE
    ROOT = os.path.dirname(HERE)
    APP_DIR = HERE
SETTINGS_PATH = os.path.join(APP_DIR, "last_settings.json")
# 아이콘: 개발 실행은 packaging/Logo.ico, exe 는 spec 의 datas 로 번들된 루트의 Logo.ico
ICON_PATH = os.path.join(RES_DIR if FROZEN else os.path.join(HERE, "packaging"), "Logo.ico")


def _resolve_bit_depth() -> int:
    if "--bit-depth" in sys.argv:
        return int(sys.argv[sys.argv.index("--bit-depth") + 1])
    try:
        with open(SETTINGS_PATH, "r", encoding="utf-8") as f:
            return int(json.load(f).get("bit_depth", 14))
    except Exception:
        return 14


def init() -> int:
    import multiprocessing as mp
    mp.freeze_support()
    # 윈도우(console=False) 빌드에서는 stdout/stderr 가 None → print 가 있는 코드를 위해 대체
    for name in ("stdout", "stderr"):
        if getattr(sys, name) is None:
            setattr(sys, name, open(os.devnull, "w", encoding="utf-8"))
    if ROOT not in sys.path:
        sys.path.insert(0, ROOT)
    bit = _resolve_bit_depth()
    os.environ["PE_BIT_DEPTH"] = str(bit)
    import pe_post.common  # noqa: F401  (비트 심도 고정)
    return bit
