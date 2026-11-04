"""최적화 결과 저장: best_params.json / snippet / output raw+png / diff / history / settings."""
import csv
import json
import os
import time

import cv2
import numpy as np

from param_tuner.param_space import Params, make_snippet, params_to_dict

from param_tuner.bootstrap import APP_DIR  # noqa: E402  (exe 빌드 시 exe 옆 폴더)

RESULTS_DIR = os.path.join(APP_DIR, "results")


def to_u8(img: np.ndarray, lo: float, hi: float) -> np.ndarray:
    x = (img.astype(np.float32) - lo) / max(hi - lo, 1e-6)
    return (np.clip(x, 0, 1) * 255 + 0.5).astype(np.uint8)


_INVALID_CHARS = '\\/:*?"<>|'          # Windows 파일/폴더 이름에 쓸 수 없는 문자


def safe_name(name: str) -> str:
    """프로토콜 이름을 폴더 이름에 쓸 수 있게 정리 (예: 'AC JOINT W/ WEIGHTS' -> 'AC JOINT W_ WEIGHTS')."""
    s = "".join("_" if c in _INVALID_CHARS or ord(c) < 32 else c for c in str(name or ""))
    return s.strip().rstrip(".")


def result_dir_name(protocol: str, stamp: str = None) -> str:
    """결과 폴더 이름: '<프로토콜>_<날짜_시각>' (프로토콜이 없으면 날짜_시각만). 예: ABDOMEN SUPINE_20261013_132404"""
    stamp = stamp or time.strftime("%Y%m%d_%H%M%S")
    prefix = safe_name(protocol)
    return f"{prefix}_{stamp}" if prefix else stamp


def save_results(best: Params, out_img: np.ndarray, target: np.ndarray, pix_max: int,
                 bit_depth: int, detail: dict, history: list, settings: dict,
                 out_dir: str = None) -> str:
    protocol = settings.get("protocol", "")
    out_dir = out_dir or os.path.join(RESULTS_DIR, result_dir_name(protocol))
    os.makedirs(out_dir, exist_ok=True)

    # best_params.json: 어떤 프로토콜의 결과인지 함께 기록 (파라미터 읽기는 post_param/lut_param 만 사용)
    ident = {"protocol": protocol, "protocol_code": settings.get("protocol_code"),
             "object_type": settings.get("object_type")}
    with open(os.path.join(out_dir, "best_params.json"), "w", encoding="utf-8") as f:
        json.dump({**ident, **params_to_dict(best), "metrics": detail, "bit_depth": bit_depth},
                  f, indent=2, ensure_ascii=False)
    with open(os.path.join(out_dir, "best_params_snippet.py"), "w", encoding="utf-8") as f:
        f.write(make_snippet(best, protocol))
    with open(os.path.join(out_dir, "settings.json"), "w", encoding="utf-8") as f:
        json.dump(settings, f, indent=2, ensure_ascii=False)

    raw = out_img.astype(np.int16) if bit_depth <= 14 else np.clip(out_img, 0, pix_max).astype(np.uint16)
    raw.tofile(os.path.join(out_dir, "output_post.raw"))
    lo, hi = np.percentile(target, 0.5), np.percentile(target, 99.5)
    cv2.imwrite(os.path.join(out_dir, "output_post.png"), to_u8(out_img, lo, hi))
    cv2.imwrite(os.path.join(out_dir, "target_post.png"), to_u8(target, lo, hi))
    diff = np.abs(out_img.astype(np.float32) - target.astype(np.float32))
    cv2.imwrite(os.path.join(out_dir, "diff.png"), to_u8(diff, 0, (hi - lo) * 0.25))

    with open(os.path.join(out_dir, "history.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["eval", "loss"] + [f"post{i}" for i in range(40)] + [f"lut{i}" for i in range(12)])
        for n, loss, key in history:
            w.writerow([n, f"{loss:.6f}"] + list(key))
    return out_dir
