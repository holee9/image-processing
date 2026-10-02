"""QA-B-184 probe: what does the module do with a conformant MONOCHROME1 DX file, end to end?

1. write A.dcm (MONOCHROME2 / IDENTITY, the module's writer) from a 12-bit ramp
2. patch the two attributes in place so the file is a conformant DX MONOCHROME1 file:
   PhotometricInterpretation MONOCHROME2 -> MONOCHROME1, Presentation LUT Shape IDENTITY -> INVERSE
   (PS3.3 C.8.11.3: INVERSE "shall be used if Photometric Interpretation is MONOCHROME1"). Both patches keep the length.
3. read it with xpe_dicom_read_image: are the words as stored? is there any alert? what does the metadata carry?
4. write what was read with xpe_dicom_write and look at the two attributes of the result
"""
import ctypes
import os
import sys

ROOT = 'D:/workspace-github/xpe-post'
BIN = ROOT + '/build/ci-dicom/bin'
VCPKG = 'D:/workspace-github/image-processing/build/release/vcpkg_installed/x64-windows/bin'
OUT = ROOT + '/build/g184-probe'
os.makedirs(OUT, exist_ok=True)
for d in (BIN, VCPKG):
    os.add_dll_directory(d.replace('/', '\\'))


class Buf(ctypes.Structure):
    _fields_ = [('width', ctypes.c_uint32), ('height', ctypes.c_uint32), ('bitsAllocated', ctypes.c_uint32),
                ('bitsStored', ctypes.c_uint32), ('format', ctypes.c_int32), ('data', ctypes.c_void_p),
                ('dataSize', ctypes.c_size_t)]


assert ctypes.sizeof(Buf) == 40
dll = ctypes.CDLL(BIN.replace('/', '\\') + '\\xpe_dicom.dll')
common = ctypes.CDLL(BIN.replace('/', '\\') + '\\xpe_common.dll')
dll.xpe_dicom_open.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
dll.xpe_dicom_read_image.argtypes = [ctypes.c_void_p, ctypes.POINTER(Buf)]
dll.xpe_dicom_get_metadata.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
dll.xpe_dicom_close.argtypes = [ctypes.c_void_p]
dll.xpe_dicom_write.argtypes = [ctypes.c_char_p, ctypes.POINTER(Buf), ctypes.c_void_p]
common.xpe_get_pending_alert_count.restype = ctypes.c_int32
common.xpe_clear_alerts.restype = None

W = H = 16
words = [(y * W + x) * 4095 // (W * H - 1) for y in range(H) for x in range(W)]
arr = (ctypes.c_uint16 * (W * H))(*words)
img = Buf(W, H, 16, 12, 0, ctypes.cast(arr, ctypes.c_void_p), W * H * 2)
meta = ctypes.create_string_buffer(96)

A = OUT + '/A_mono2.dcm'
rc = dll.xpe_dicom_write(A.encode(), ctypes.byref(img), meta)
print('write A rc =', rc)
raw = open(A, 'rb').read()
assert raw.count(b'MONOCHROME2') == 1 and raw.count(b'IDENTITY') == 1, (raw.count(b'MONOCHROME2'), raw.count(b'IDENTITY'))
B = OUT + '/B_mono1_inverse.dcm'
open(B, 'wb').write(raw.replace(b'MONOCHROME2', b'MONOCHROME1').replace(b'IDENTITY', b'INVERSE '))


def attrs(path):
    r = open(path, 'rb').read()
    pi = [x for x in (b'MONOCHROME1', b'MONOCHROME2') if x in r]
    shape = [x for x in (b'IDENTITY', b'INVERSE') if x in r]
    return pi, shape


def read(path):
    common.xpe_clear_alerts()
    h = ctypes.c_void_p()
    rc = dll.xpe_dicom_open(path.encode(), ctypes.byref(h))
    out = Buf()
    rrc = dll.xpe_dicom_read_image(h, ctypes.byref(out))
    m = ctypes.create_string_buffer(96)
    mrc = dll.xpe_dicom_get_metadata(h, m)
    alerts = common.xpe_get_pending_alert_count()
    got = list((ctypes.c_uint16 * (out.width * out.height)).from_address(out.data)) if rrc == 0 else None
    dll.xpe_dicom_close(h)
    return rc, rrc, mrc, alerts, got, bytes(m.raw)


print('A attrs (PI, Presentation LUT Shape) =', attrs(A))
print('B attrs (PI, Presentation LUT Shape) =', attrs(B))
_, ra, _, aa, ga, ma = read(A)
_, rb, mrb, ab, gb, mb = read(B)
print('read A rc=%d alerts=%d  read B rc=%d meta rc=%d alerts=%d' % (ra, aa, rb, mrb, ab))
print('words of B == words of A (no inversion):', ga == gb, '| first/last of B:', gb[0], gb[-1])
print('metadata struct of B == metadata struct of A (nothing tells the caller about the polarity):', ma == mb)

# write the words that were read from B
arr2 = (ctypes.c_uint16 * (W * H))(*gb)
img2 = Buf(W, H, 16, 12, 0, ctypes.cast(arr2, ctypes.c_void_p), W * H * 2)
C = OUT + '/C_rewritten_from_B.dcm'
print('write C rc =', dll.xpe_dicom_write(C.encode(), ctypes.byref(img2), meta))
print('C attrs (PI, Presentation LUT Shape) =', attrs(C))
_, rc_, _, _, gc, _ = read(C)
print('words of C == words of B:', gc == gb)
print('=> C carries the SAME words as B under the OPPOSITE polarity label: a viewer shows C inverted relative to B.')
