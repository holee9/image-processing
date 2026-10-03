"""XPE AI model signing: the reference implementation of the signature format (QA-B-195, REQ-AI-007 / REQ-AI-091).

This is the INDEPENDENT side of the format. The verifier in modules/ai/src/ai_model_signer.cpp is written against
Windows CNG; this module builds the same message with Python's `cryptography` (OpenSSL), so a signature made here
and accepted there cannot be an accident of both sides sharing one mistake (DER versus r||s, byte order, how the
message is put together).

The format (design memo .moai/reports/lane-post/QA-B-195/design.md, sections 3 and 3.1):

    message  = "XPE-MODEL-SIG-1\\n"
               || u16le(len(role)) || role                       role = "bone_suppress" | "bodypart"
               || u64le(len(model)) || model
               || u8(has_sidecar) [ || u64le(len(sidecar)) || sidecar ]
    signature = ECDSA-P256( SHA-256(message) ), r||s, 64 bytes (IEEE P1363, the form CNG takes)
    .sig file = "XSIG" || u8(version = 1) || u8(algorithm = 1: ECDSA-P256-SHA256) || key_id (8) || signature (64)
    key_id    = SHA-256(X || Y of the public key, 32 + 32 bytes big-endian)[:8]

The role is part of the message so a model signed for one job cannot be placed where another is expected; the sidecar
is part of it because the labels in it change what a result means.
"""

import hashlib
import struct

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils

PREFIX = b"XPE-MODEL-SIG-1\n"
SIG_MAGIC = b"XSIG"
SIG_VERSION = 1
SIG_ALGORITHM = 1
SIG_FILE_BYTES = 4 + 1 + 1 + 8 + 64
ROLES = ("bone_suppress", "bodypart")


def build_message(role, model, sidecar=None):
    """The exact bytes that are hashed. `sidecar` None = no sidecar; b"" = an empty one (a different message)."""
    r = role.encode("utf-8")
    msg = PREFIX + struct.pack("<H", len(r)) + r + struct.pack("<Q", len(model)) + model
    if sidecar is None:
        msg += b"\x00"
    else:
        msg += b"\x01" + struct.pack("<Q", len(sidecar)) + sidecar
    return msg


def public_xy(public_key):
    n = public_key.public_numbers()
    return n.x.to_bytes(32, "big") + n.y.to_bytes(32, "big")


def key_id(public_key):
    return hashlib.sha256(public_xy(public_key)).digest()[:8]


def load_private_key(pem_path):
    with open(pem_path, "rb") as f:
        return serialization.load_pem_private_key(f.read(), password=None)


def sign(private_key, role, model, sidecar=None):
    """Return the .sig file bytes. Deterministic (RFC 6979), so regenerating an asset gives the same bytes."""
    if role not in ROLES:
        raise ValueError("role must be one of %r" % (ROLES,))
    der = private_key.sign(build_message(role, model, sidecar), ec.ECDSA(hashes.SHA256(), deterministic_signing=True))
    r, s = utils.decode_dss_signature(der)
    return (SIG_MAGIC + bytes([SIG_VERSION, SIG_ALGORITHM]) + key_id(private_key.public_key())
            + r.to_bytes(32, "big") + s.to_bytes(32, "big"))


def verify(public_key, sig_file, role, model, sidecar=None):
    """True when `sig_file` is a valid signature by `public_key` for exactly this role, model and sidecar."""
    if len(sig_file) != SIG_FILE_BYTES or sig_file[:4] != SIG_MAGIC:
        return False
    if sig_file[4] != SIG_VERSION or sig_file[5] != SIG_ALGORITHM or sig_file[6:14] != key_id(public_key):
        return False
    r = int.from_bytes(sig_file[14:46], "big")
    s = int.from_bytes(sig_file[46:78], "big")
    try:
        public_key.verify(utils.encode_dss_signature(r, s), build_message(role, model, sidecar), ec.ECDSA(hashes.SHA256()))
    except Exception:
        return False
    return True


# ---------------------------------------------------------------------------------------------------------------
# Command line (QA-B-195 M2)
#
#   python tools/ai/xpe_model_signing.py sign --key K.pem --role bodypart --model M.onnx [--sidecar S.json] --out M.sig
#   python tools/ai/xpe_model_signing.py sign-test-assets      (signs every model under modules/ai/tests/data)
#   python tools/ai/xpe_model_signing.py check-test-assets     (exit 1 on any missing, stale or orphan signature)
#
# The test assets are signed with the committed TEST key (tests/data/signing/test_key_1.pem). Nothing here ever
# touches a production key: how that key is generated, kept and used is decision D5 of the design memo and is not
# defined in this repository.
# ---------------------------------------------------------------------------------------------------------------

import argparse
import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
_ROOT = os.path.abspath(os.path.join(_HERE, "..", ".."))
TEST_DATA_DIR = os.path.join(_ROOT, "modules", "ai", "tests", "data")
TEST_KEY_PEM = os.path.join(TEST_DATA_DIR, "signing", "test_key_1.pem")
DEFAULT_ROLE = "bone_suppress"   # a fixture that is named for no role (min_scale2.onnx ...) is signed as this one


def _read(path):
    with open(path, "rb") as f:
        return f.read()


def asset_jobs(data_dir=TEST_DATA_DIR):
    """Every model under `data_dir`: (model path, role, sidecar path or None, sig path).

    The rules (the C++ staleness test, test_ai_model_assets_signed.cpp, repeats them on purpose):
      role     = the file stem when it is a known role ("bone_suppress", "bodypart"), otherwise DEFAULT_ROLE
      sidecar  = <stem>.json beside the model, when it exists (its ABSENCE is signed too)
      sig file = <stem>.sig beside the model
    """
    jobs = []
    for dirpath, dirnames, filenames in os.walk(data_dir):
        dirnames[:] = sorted(d for d in dirnames if d not in ("signing", "__pycache__"))
        for name in sorted(filenames):
            stem, ext = os.path.splitext(name)
            if ext != ".onnx":
                continue
            side = os.path.join(dirpath, stem + ".json")
            jobs.append((os.path.join(dirpath, name), stem if stem in ROLES else DEFAULT_ROLE,
                         side if os.path.exists(side) else None, os.path.join(dirpath, stem + ".sig")))
    return jobs


def sign_test_assets(data_dir=TEST_DATA_DIR):
    key = load_private_key(TEST_KEY_PEM)
    jobs = asset_jobs(data_dir)
    for model, role, side, sigpath in jobs:
        with open(sigpath, "wb") as f:
            f.write(sign(key, role, _read(model), _read(side) if side else None))
    return len(jobs)


def check_test_assets(data_dir=TEST_DATA_DIR):
    """Returns the list of problems (empty = every model has a valid, current signature and no signature is orphaned)."""
    pub = load_private_key(TEST_KEY_PEM).public_key()
    problems = []
    models = set()
    for model, role, side, sigpath in asset_jobs(data_dir):
        models.add(os.path.normcase(sigpath))
        if not os.path.exists(sigpath):
            problems.append("no signature: " + os.path.relpath(model, data_dir))
        elif not verify(pub, _read(sigpath), role, _read(model), _read(side) if side else None):
            problems.append("stale or invalid signature: " + os.path.relpath(sigpath, data_dir))
    for dirpath, dirnames, filenames in os.walk(data_dir):
        dirnames[:] = [d for d in dirnames if d not in ("signing", "__pycache__")]
        for name in filenames:
            if name.endswith(".sig") and os.path.normcase(os.path.join(dirpath, name)) not in models:
                problems.append("orphan signature (no model beside it): " + os.path.relpath(os.path.join(dirpath, name), data_dir))
    return problems


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("sign", help="sign one model")
    s.add_argument("--key", required=True)
    s.add_argument("--role", required=True, choices=ROLES)
    s.add_argument("--model", required=True)
    s.add_argument("--sidecar")
    s.add_argument("--out", required=True)
    sub.add_parser("sign-test-assets", help="sign every model under modules/ai/tests/data with the TEST key")
    sub.add_parser("check-test-assets", help="verify those signatures; exit 1 on any problem")
    a = ap.parse_args(argv)
    if a.cmd == "sign":
        with open(a.out, "wb") as f:
            f.write(sign(load_private_key(a.key), a.role, _read(a.model), _read(a.sidecar) if a.sidecar else None))
        print("wrote", a.out)
        return 0
    if a.cmd == "sign-test-assets":
        print("signed %d test models with the TEST key" % sign_test_assets())
        return 0
    problems = check_test_assets()
    for p in problems:
        print("PROBLEM:", p)
    print("%d test models checked, %d problem(s)" % (len(asset_jobs()), len(problems)))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
