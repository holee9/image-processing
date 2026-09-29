#!/usr/bin/env python3
"""Emit two minimal ONNX models by hand, with no onnx/protobuf package.

QA-B-160 (#130). The models exist to prove one thing: that the inference path
actually runs the model. Two models are generated rather than one, differing
ONLY in a scale constant, so a test can assert that swapping the model changes
the output. A single model cannot show that -- a stub that echoed its input
would pass just as well, and this repository has already shipped two models
that were computed, executed, and then cancelled out of the answer (#154, #155).

Graph:  Y = Mul(X, scale)   with X float32 [4] and scale a float32 initializer.

The encoder below is a minimal protobuf writer. Only the ONNX fields this graph
needs are implemented; it is not a general ONNX serializer.
"""
import struct
import sys
from pathlib import Path


def _varint(n: int) -> bytes:
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        out.append(b | (0x80 if n else 0))
        if not n:
            return bytes(out)


def _tag(field: int, wire: int) -> bytes:
    return _varint((field << 3) | wire)


def f_varint(field: int, value: int) -> bytes:
    return _tag(field, 0) + _varint(value)


def f_bytes(field: int, value: bytes) -> bytes:
    return _tag(field, 2) + _varint(len(value)) + value


def f_str(field: int, value: str) -> bytes:
    return f_bytes(field, value.encode("utf-8"))


def f_msg(field: int, value: bytes) -> bytes:
    return f_bytes(field, value)


def f_floats_packed(field: int, values) -> bytes:
    raw = b"".join(struct.pack("<f", v) for v in values)
    return f_bytes(field, raw)


# --- ONNX message builders -------------------------------------------------
# Field numbers are from onnx.proto (ONNX IR). Only what this graph needs.

FLOAT = 1  # TensorProto.DataType.FLOAT


def tensor_shape(dims) -> bytes:
    # TensorShapeProto { repeated Dimension dim = 1; }
    # Dimension { int64 dim_value = 1; }
    out = b""
    for d in dims:
        out += f_msg(1, f_varint(1, d))
    return out


def type_proto_tensor(elem_type: int, dims) -> bytes:
    # TypeProto { Tensor tensor_type = 1; }
    # Tensor { int32 elem_type = 1; TensorShapeProto shape = 2; }
    t = f_varint(1, elem_type) + f_msg(2, tensor_shape(dims))
    return f_msg(1, t)


def tensor_shape_dynamic(param: str) -> bytes:
    # One dimension carrying a NAME instead of a value: ONNX reads that as
    # "any length". xpe_bone_suppress hands the model width*height floats, and
    # that length is not known when the model is written (QA-B-161).
    # Dimension { int64 dim_value = 1; string dim_param = 2; }
    return f_msg(1, f_str(2, param))


def value_info(name: str, dims) -> bytes:
    # ValueInfoProto { string name = 1; TypeProto type = 2; }
    return f_str(1, name) + f_msg(2, type_proto_tensor(FLOAT, dims))


def value_info_dynamic(name: str, param: str) -> bytes:
    t = f_varint(1, FLOAT) + f_msg(2, tensor_shape_dynamic(param))
    return f_str(1, name) + f_msg(2, f_msg(1, t))


def tensor_initializer(name: str, dims, floats) -> bytes:
    # TensorProto { repeated int64 dims = 1; int32 data_type = 2;
    #               repeated float float_data = 4 [packed]; string name = 8; }
    out = b""
    for d in dims:
        out += f_varint(1, d)
    out += f_varint(2, FLOAT)
    out += f_floats_packed(4, floats)
    out += f_str(8, name)
    return out


def node(op_type: str, inputs, outputs, name: str) -> bytes:
    # NodeProto { repeated string input = 1; repeated string output = 2;
    #             string name = 3; string op_type = 4; }
    out = b""
    for i in inputs:
        out += f_str(1, i)
    for o in outputs:
        out += f_str(2, o)
    out += f_str(3, name)
    out += f_str(4, op_type)
    return out


def graph_dynamic(scale: float) -> bytes:
    """Same Mul graph, but the input length is decided by the caller."""
    out = b""
    out += f_msg(1, node("Mul", ["X", "scale"], ["Y"], "scale_node"))
    out += f_str(2, "xpe_min_dyn")
    out += f_msg(5, tensor_initializer("scale", [], [scale]))
    out += f_msg(11, value_info_dynamic("X", "N"))
    out += f_msg(12, value_info_dynamic("Y", "N"))
    return out


def model_dynamic(scale: float) -> bytes:
    out = b""
    out += f_varint(1, 8)
    out += f_str(2, "xpe-qa-b-161")
    out += f_msg(7, graph_dynamic(scale))
    out += f_msg(8, f_str(1, "") + f_varint(2, 13))
    return out


def graph(scale: float, length: int) -> bytes:
    # GraphProto { repeated NodeProto node = 1; string name = 2;
    #              repeated TensorProto initializer = 5;
    #              repeated ValueInfoProto input = 11; output = 12; }
    out = b""
    out += f_msg(1, node("Mul", ["X", "scale"], ["Y"], "scale_node"))
    out += f_str(2, "xpe_min")
    out += f_msg(5, tensor_initializer("scale", [], [scale]))
    out += f_msg(11, value_info("X", [length]))
    out += f_msg(12, value_info("Y", [length]))
    return out


def model(scale: float, length: int) -> bytes:
    # ModelProto { int64 ir_version = 1; string producer_name = 2;
    #              GraphProto graph = 7; repeated OperatorSetIdProto opset_import = 8; }
    # OperatorSetIdProto { string domain = 1; int64 version = 2; }
    out = b""
    out += f_varint(1, 8)                       # IR version 8
    out += f_str(2, "xpe-qa-b-160")
    out += f_msg(7, graph(scale, length))
    out += f_msg(8, f_str(1, "") + f_varint(2, 13))   # default domain, opset 13
    return out


def main() -> int:
    here = Path(__file__).resolve().parent
    length = 4
    for scale, name in ((2.0, "min_scale2.onnx"), (3.0, "min_scale3.onnx")):
        blob = model(scale, length)
        (here / name).write_bytes(blob)
        print(f"{name}: {len(blob)} bytes, Y = X * {scale}, shape [{length}]")
    # A file that is NOT a model, for the load-failure path.
    (here / "not_a_model.onnx").write_bytes(b"this is not an onnx model\n")
    print("not_a_model.onnx: deliberate garbage for the kModelLoadFailed path")

    # QA-B-161: the C ABI feeds width*height floats, a length not known here,
    # so the model directories carry a DYNAMIC-length twin. Two directories
    # rather than two file names, because xpe_ai_init takes a model DIRECTORY
    # and the C ABI resolves the file name itself -- swapping the directory is
    # how a test swaps the model without inventing a second lookup rule.
    for scale, dirname in ((2.0, "models_x2"), (3.0, "models_x3")):
        d = here / dirname
        d.mkdir(exist_ok=True)
        blob = model_dynamic(scale)
        (d / "bone_suppress.onnx").write_bytes(blob)
        print(f"{dirname}/bone_suppress.onnx: {len(blob)} bytes, Y = X * {scale}, shape [N]")

    # A directory whose model file is deliberately absent, so "no model there"
    # can be told apart from "a model that is there but broken". It needs a
    # tracked file of its own to survive git.
    nd = here / "models_missing"
    nd.mkdir(exist_ok=True)
    (nd / "README.txt").write_text(
        "QA-B-161: this directory has NO bone_suppress.onnx, on purpose.\n"
        "It pins the error code for a model that is not there, which must\n"
        "differ from the code for a model that is there but unreadable.\n")
    print("models_missing/: no model on purpose")

    # And a directory whose model file IS there but is not a model.
    bd = here / "models_broken"
    bd.mkdir(exist_ok=True)
    (bd / "bone_suppress.onnx").write_bytes(b"this is not an onnx model\n")
    print("models_broken/bone_suppress.onnx: garbage, for the load-failure code")
    return 0


if __name__ == "__main__":
    sys.exit(main())
