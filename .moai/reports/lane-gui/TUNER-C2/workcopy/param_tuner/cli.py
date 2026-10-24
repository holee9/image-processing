"""헤드리스 실행 (UI 없이 최적화).

예) python param_tuner/cli.py --src a.raw --target t.raw --max-evals 100
    (인자를 생략하면 run_pipeline.py 의 RAW_PATH 와 '<RAW_PATH 접두>.post.target.raw' 사용)
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from param_tuner import bootstrap  # noqa: E402
bootstrap.init()

import run_pipeline as rp  # noqa: E402
from param_tuner.param_space import build_param_defs, default_params, params_from_dict  # noqa: E402
from param_tuner.tuner_core import JobConfig, TuneJob  # noqa: E402
from param_tuner.optimizer import OptConfig  # noqa: E402
import json  # noqa: E402


def default_target_path() -> str:
    base = rp.RAW_PATH
    cand = base.replace(".dcm.raw", ".dcm.post.target.raw")
    return cand if os.path.exists(cand) else ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default=rp.RAW_PATH)
    ap.add_argument("--target", default=default_target_path())
    ap.add_argument("--bit-depth", type=int, default=None)  # bootstrap 이 처리
    ap.add_argument("--width", type=int, default=rp.WIDTH)
    ap.add_argument("--height", type=int, default=rp.HEIGHT)
    ap.add_argument("--scale", type=float, default=1.0, help="loss compare scale (1.0 = full resolution)")
    ap.add_argument("--max-evals", type=int, default=100)
    ap.add_argument("--max-minutes", type=float, default=30)
    ap.add_argument("--iterations", "--rounds", dest="rounds", type=int, default=1,
                    help="전체 그룹 반복(Iteration) 횟수")
    ap.add_argument("--init", default=None, help="시작 파라미터 best_params.json")
    ap.add_argument("--groups", default=None, help="탐색 그룹 제한 (예: tone,noise,lut)")
    a = ap.parse_args()

    cfg = JobConfig(a.src, a.target, a.width, a.height, rp.OBJECT_TYPE,
                    [int(v) for v in rp.AUTO_SHUTTER_POINTS.flatten()],
                    rp.COLLIMATOR_EDGE_RADIUS, a.scale)
    cfg.opt = OptConfig(max_evals=a.max_evals, max_seconds=a.max_minutes * 60, rounds=a.rounds)
    init = default_params()
    if a.init:
        with open(a.init, encoding="utf-8") as f:
            init = params_from_dict(json.load(f))
    defs = build_param_defs(default_params(), object_type=cfg.object_type)
    if a.groups:
        gs = set(a.groups.split(","))
        for d in defs:
            d.enabled = d.enabled and d.group in gs

    def on_eval(info):
        mark = "*" if info["is_best"] else " "
        d = info["detail"]
        print(f"{mark}[{info['n']:4d}] loss={info['loss']:.5f} best={info['best_loss']:.5f} "
              f"PSNR={d['psnr']:.2f} SSIM={d['ssim']:.4f}", flush=True)

    job = TuneJob(cfg, print, on_eval, lambda t: print(f"== {t}", flush=True))
    res = job.run(init, defs)
    print("최종 지표:", res["detail"])
    print("저장 폴더:", res["out_dir"])
    changed = [(i, d, b) for i, (d, b) in enumerate(zip(init.post, res["best"].post)) if d != b]
    print("변경된 PostParam(idx, 초기→최종):", [(i, d, b) for i, d, b in changed])
    print("LUT:", res["best"].lut)


if __name__ == "__main__":
    main()
