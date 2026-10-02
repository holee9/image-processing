#!/usr/bin/env python3
"""Emit the toy body-part models, by hand, with no onnx/protobuf package.

QA-B-191 M1 (#130, T-006). READ THIS BEFORE TRUSTING ANY TEST THAT USES THESE.

These are NOT classifiers. They exist to prove that xpe_bodypart_recognize is
WIRED: that a number the model produces reaches the label, the confidence, the
threshold decision and the alert -- and that swapping the model, or changing the
image, changes the answer. They say nothing about whether a real model would
recognise a body part, how accurate it is, or how fast it runs. No test name,
report or comment may claim otherwise.

Graph (every model):   X -> Flatten -> MatMul(W) -> Add(B) -> Y
  X is a 4x4 single-channel float image (NCHW [1,1,4,4], or NHWC [1,4,4,1]);
  Y is [1,3]: three class scores. The module expects the model to emit
  PROBABILITIES (it applies no softmax), so the weights below are chosen to give
  values in [0,1] on purpose, and the "bad" models give values that are not.

The protobuf encoder is the one in make_min_models.py (a minimal writer, not a
general ONNX serialiser); only Flatten, MatMul and Add are needed.
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from make_min_models import (  # noqa: E402
    FLOAT, _varint, f_bytes, f_msg, f_str, f_varint, node, tensor_initializer,
)

INT64 = 7  # TensorProto.DataType.INT64

LABELS = ["CHEST", "ABDOMEN", "SPINE"]
INF = float("inf")


def dim_value(v: int) -> bytes:
    return f_msg(1, f_varint(1, v))


def dim_param(name: str) -> bytes:
    return f_msg(1, f_str(2, name))


def value_info_dims(name: str, dims) -> bytes:
    """dims: ints, or str for a dynamic (named) axis."""
    shape = b"".join(dim_param(d) if isinstance(d, str) else dim_value(d) for d in dims)
    t = f_varint(1, FLOAT) + f_msg(2, shape)
    return f_str(1, name) + f_msg(2, f_msg(1, t))


def int64_initializer(name: str, dims, ints) -> bytes:
    # TensorProto { repeated int64 dims = 1; int32 data_type = 2; repeated int64 int64_data = 7 [packed];
    #               string name = 8; }
    out = b""
    for d in dims:
        out += f_varint(1, d)
    out += f_varint(2, INT64)
    out += f_bytes(7, b"".join(_varint(v) for v in ints))
    out += f_str(8, name)
    return out


def model_bytes(in_dims, w, b, fail_at_run=False) -> bytes:
    """w: 16x3 nested list, b: 3 list. Output [1,3].

    fail_at_run: append Gather(A, IDX) with IDX = [5] on a [1,3] tensor. The graph is valid, loads, has the same
    input and output shapes and passes every check this module makes at load time -- and ONNX Runtime refuses to
    run it (index 5 is out of bounds for an axis of length 1). It is the model that EXISTS and FAILS TO RUN.
    """
    flat_w = [v for row in w for v in row]
    g = b""
    g += f_msg(1, node("Flatten", ["X"], ["F"], "flatten"))
    g += f_msg(1, node("MatMul", ["F", "W"], ["M"], "matmul"))
    if fail_at_run:
        g += f_msg(1, node("Add", ["M", "B"], ["A"], "add"))
        g += f_msg(1, node("Gather", ["A", "IDX"], ["Y"], "gather"))
    else:
        g += f_msg(1, node("Add", ["M", "B"], ["Y"], "add"))
    g += f_str(2, "xpe_bodypart_toy")
    g += f_msg(5, tensor_initializer("W", [16, 3], flat_w))
    g += f_msg(5, tensor_initializer("B", [3], b))
    if fail_at_run:
        g += f_msg(5, int64_initializer("IDX", [1], [5]))
    g += f_msg(11, value_info_dims("X", in_dims))
    g += f_msg(12, value_info_dims("Y", [1, 3]))
    out = b""
    out += f_varint(1, 8)
    out += f_str(2, "xpe-qa-b-191")
    out += f_msg(7, g)
    out += f_msg(8, f_str(1, "") + f_varint(2, 13))
    return out


ZERO_W = [[0.0, 0.0, 0.0] for _ in range(16)]
# Class 0 = mean of the top two rows, class 1 = mean of the bottom two rows, class 2 = nothing.
DEP_W = [[0.125 if i < 8 else 0.0, 0.125 if i >= 8 else 0.0, 0.0] for i in range(16)]


def sidecar(labels) -> str:
    return json.dumps({
        "model_id": "bodypart_toy",
        "version": "0.0.1",
        "note": "QA-B-191 toy model for wiring tests: not a classifier, says nothing about accuracy",
        "labels": labels,
    }, indent=2) + "\n"


def write_dir(here: Path, name: str, blob: bytes, labels=LABELS, with_sidecar=True) -> None:
    d = here / name
    d.mkdir(exist_ok=True)
    (d / "bodypart.onnx").write_bytes(blob)
    sc = d / "bodypart.json"
    if with_sidecar:
        sc.write_text(sidecar(labels), encoding="utf-8", newline="\n")
    elif sc.exists():
        sc.unlink()
    print(f"{name}/bodypart.onnx: {len(blob)} bytes" + ("" if with_sidecar else " (no sidecar, on purpose)"))


def main() -> int:
    here = Path(__file__).resolve().parent
    nchw = [1, 1, 4, 4]

    # Constant models: the answer does not depend on the image, so the confidence is exactly a number
    # we chose. 0.6f is the default threshold, which makes the boundary testable.
    write_dir(here, "models_bodypart_a", model_bytes(nchw, ZERO_W, [0.6, 0.3, 0.1]))
    write_dir(here, "models_bodypart_b", model_bytes(nchw, ZERO_W, [0.1, 0.3, 0.6]))
    # Image-dependent: top-bright and bottom-bright images must give DIFFERENT labels.
    write_dir(here, "models_bodypart_dep", model_bytes(nchw, DEP_W, [0.0, 0.0, 0.0]))
    write_dir(here, "models_bodypart_nhwc", model_bytes([1, 4, 4, 1], DEP_W, [0.0, 0.0, 0.0]))
    # Input shapes the module must refuse.
    write_dir(here, "models_bodypart_rank2", model_bytes([1, 16], DEP_W, [0.0, 0.0, 0.0]))
    write_dir(here, "models_bodypart_dynamic", model_bytes([1, 1, "H", "W"], DEP_W, [0.0, 0.0, 0.0]))
    # Outputs that are not a valid probability vector.
    nonfinite_w = [row[:] for row in DEP_W]
    nonfinite_w[0][0] = INF       # 0 * inf = NaN and x * inf = inf: non-finite for any image
    write_dir(here, "models_bodypart_nonfinite", model_bytes(nchw, nonfinite_w, [0.0, 0.0, 0.0]))
    write_dir(here, "models_bodypart_range_high", model_bytes(nchw, ZERO_W, [1.5, 0.0, 0.0]))
    write_dir(here, "models_bodypart_range_low", model_bytes(nchw, ZERO_W, [0.5, 0.2, -0.1]))
    # Sidecar problems.
    write_dir(here, "models_bodypart_labels_mismatch", model_bytes(nchw, ZERO_W, [0.6, 0.3, 0.1]),
              labels=["CHEST", "ABDOMEN"])
    write_dir(here, "models_bodypart_no_labels", model_bytes(nchw, ZERO_W, [0.6, 0.3, 0.1]), with_sidecar=False)
    # A model that loads, has the right shapes and labels, and fails when it is RUN (QA-B-191 M4c).
    write_dir(here, "models_bodypart_runfail", model_bytes(nchw, ZERO_W, [0.6, 0.3, 0.1], fail_at_run=True))
    # A model file that is not a model.
    bd = here / "models_bodypart_broken"
    bd.mkdir(exist_ok=True)
    (bd / "bodypart.onnx").write_bytes(b"this is not an onnx model\n")
    (bd / "bodypart.json").write_text(sidecar(LABELS), encoding="utf-8", newline="\n")
    print("models_bodypart_broken/bodypart.onnx: garbage, for the load-failure path")
    return 0


if __name__ == "__main__":
    sys.exit(main())
