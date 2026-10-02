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
