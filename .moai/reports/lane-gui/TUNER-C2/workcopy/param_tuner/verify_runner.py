"""M1 검증: PipelineRunner 결과가 run_pipeline.py 출력(output_post_14bit.raw 등)과 같은지 확인하고
평가 1회 소요 시간을 측정한다.

사용: python param_tuner/verify_runner.py [기존_출력.raw]
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from param_tuner import bootstrap  # noqa: E402
bit = bootstrap.init()

import numpy as np  # noqa: E402
import run_pipeline as rp  # noqa: E402
from pe_post.common import RAW_DTYPE, PIXEL_DTYPE  # noqa: E402
from param_tuner.param_space import default_params  # noqa: E402
from param_tuner.pipeline_runner import PipelineRunner, RunSettings  # noqa: E402


def main():
    ref_path = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("--") \
        else os.path.join(bootstrap.ROOT, f"output_post_{bit}bit.raw")
    src = rp.load_raw(rp.RAW_PATH, rp.WIDTH, rp.HEIGHT)
    st = RunSettings(rp.WIDTH, rp.HEIGHT, rp.OBJECT_TYPE,
                     rp.AUTO_SHUTTER_POINTS, rp.COLLIMATOR_EDGE_RADIUS)
    p = default_params()
    r = PipelineRunner(src, st, print)
    t = time.time(); r.prepare(p); print(f"prepare: {time.time() - t:.1f}s")
    t = time.time(); out = r.run(p); print(f"run (cold): {time.time() - t:.1f}s")
    p2 = p.copy(); p2.lut[1] += 1
    t = time.time(); r.run(p2); print(f"run (LUT only): {time.time() - t:.2f}s")
    p3 = p.copy(); p3.post[23] += 1
    t = time.time(); r.run(p3); print(f"run (NR change): {time.time() - t:.2f}s")
    p4 = p.copy(); p4.post[0] += 1
    t = time.time(); r.run(p4); print(f"run (MSE tone change): {time.time() - t:.2f}s")
    p5 = p.copy(); p5.post[14] += 1
    t = time.time(); r.run(p5); print(f"run (MSE freq change): {time.time() - t:.2f}s")
    p6 = p.copy(); p6.post[28] -= 1
    t = time.time(); r.run(p6); print(f"run (MSE suppress change): {time.time() - t:.2f}s")
    if os.path.exists(ref_path):
        ref = np.fromfile(ref_path, dtype=RAW_DTYPE).reshape(rp.HEIGHT, rp.WIDTH).astype(PIXEL_DTYPE)
        diff = np.abs(out.astype(np.int64) - ref.astype(np.int64))
        print(f"vs {os.path.basename(ref_path)}: max|diff|={diff.max()} mean={diff.mean():.4f}")
    else:
        print("비교용 기존 출력 없음:", ref_path)


if __name__ == "__main__":
    main()
