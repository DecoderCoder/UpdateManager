# gcm_truth.py — independent ground truth via python-cryptography
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

key = bytes.fromhex('feffe9928665731c6d6a8f9467308308')
iv12 = bytes.fromhex('cafebabefacedbaddecaf888')
pt1 = bytes.fromhex('d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39')

def gcm_encrypt(key, iv, pt, aad=None):
    e = Cipher(algorithms.AES(key), modes.GCM(iv, aad)).encryptor()
    ct = e.update(pt) + e.finalize()
    return ct, e.tag

def ecb_enc(key, blk):
    e = Cipher(algorithms.AES(key), modes.ECB()).encryptor()
    return e.update(blk) + e.finalize()

# vector 4 tag (empty)
ct4, tag4 = gcm_encrypt(key, iv12, b'')
print('vector4 tag:', tag4.hex())

# vector 1 ks0
ct1, tag1 = gcm_encrypt(key, iv12, pt1)
print('vector1 ct:', ct1.hex()[:64], '...')
print('vector1 tag:', tag1.hex())
ks0 = bytes(a ^ b for a, b in zip(pt1[:16], ct1[:16]))
print('vector1 ks0 (pt^ct):', ks0.hex())

# H
H = ecb_enc(key, bytes(16))
print('H = E(0^16):', H.hex())

# E(J0+1)
J0 = iv12 + bytes(4)
J01 = J0[:12] + struct.pack('>I', 1) if False else (J0[:-4] + bytes(1) + b'\x01')
# do it cleanly:
J01 = J0[:12] + (4).to_bytes(4, 'big')
print('E(J0+1):', ecb_enc(key, J01).hex())
