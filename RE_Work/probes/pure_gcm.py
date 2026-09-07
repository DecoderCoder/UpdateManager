# pure_gcm.py — strict RFC 5116 / NIST SP 800-38D GCM in pure Python (no OpenSSL)
# AES-128 from FIPS-197, GCM per SP 800-38D section 6.5.

def _xtime(a):
    a <<= 1
    if a & 0x100: a ^= 0x11b
    return a & 0xff

def _gmul(a, b):
    # GF(2^8) multiply (for MixColumns / not needed for GCM; kept for completeness)
    z = 0
    for _ in range(8):
        if b & 1: z ^= a
        b >>= 1
        a = _xtime(a)
    return z

SBOX = [
0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16,
]

def aes128_encrypt_block(key, blk):
    # key, blk: bytes(16) -> bytes(16)
    S = SBOX
    state = list(blk)  # column-major: state[r + 4*c]? FIPS-197: state[row][col], byte i = state[i mod 4][i div 4]
    # represent as 16 bytes in FIPS order: index = row + 4*col
    a = [key[i % 4 + 4 * (i // 4)] for i in range(16)]
    def add_round_key(rk):
        for i in range(16): a[i] ^= rk[i % 4 + 4 * (i // 4)]
    # expand key (AES-128: 4 round keys... 11 words)
    w = [list(key[4*i:4*i+4]) for i in range(4)]
    for i in range(4, 44):
        temp = list(w[i-1])
        if i % 4 == 0:
            temp = temp[1:] + temp[:1]
            temp = [S[x] for x in temp]
            temp[0] ^= [0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x11,0x13,0x15,0x17,0x19,0x1b,0x1d,0x1f,0x21,0x23,0x25,0x29,0x2b,0x31,0x37,0x39,0x3d][i//4 - 1]
        w.append([w[i-4][j] ^ temp[j] for j in range(4)])
    rks = [bytes(w[r*4+c][row] for c in range(4) for row in range(4)) for r in range(11)]
    add_round_key(rks[0])
    for rnd in range(1, 10):
        # SubBytes
        for i in range(16): a[i] = S[a[i]]
        # ShiftRows
        a = _shift_rows(a)
        # MixColumns
        a = _mix_columns(a)
        add_round_key(rks[rnd])
    # final round
    for i in range(16): a[i] = S[a[i]]
    a = _shift_rows(a)
    add_round_key(rks[10])
    return bytes(a[i % 4 + 4 * (i // 4)] for i in range(16))

def _shift_rows(a):
    # a in FIPS column-major index = row + 4*col
    def get(c, r): return a[r + 4*c]
    def setc(c, r, v): a[r + 4*c] = v
    out = [0]*16
    for c in range(4):
        for r in range(4):
            out[r + 4*c] = get((c + r) % 4, r)
    return out

def _mix_columns(a):
    out = [0]*16
    for c in range(4):
        c0, c1, c2, c3 = a[4*c], a[1+4*c], a[2+4*c], a[3+4*c]
        out[0+4*c] = _gmul(2,c0) ^ _gmul(3,c1) ^ c2 ^ c3
        out[1+4*c] = c0 ^ _gmul(2,c1) ^ _gmul(3,c2) ^ c3
        out[2+4*c] = c0 ^ c1 ^ _gmul(2,c2) ^ _gmul(3,c3)
        out[3+4*c] = _gmul(3,c0) ^ c1 ^ c2 ^ _gmul(2,c3)
    return out

def ecb_enc(key, blk):
    return aes128_encrypt_block(key, blk)

def ecb_dec(key, blk):
    # for verification only: invert via encrypt of... (need real decrypt; skip)
    raise NotImplementedError

def ghash_mult(X, H):
    # GCM bit order: MSB-first over the 128-bit string as byte-order with bit 0 = MSB of byte 0
    Z = [0]*16
    V = list(X)
    for i in range(16):
        for b in range(8):
            mb = (V[i] >> (7-b)) & 1
            if mb:
                Z = [z ^ h for z, h in zip(Z, H)]
            carry = 0
            for j in range(15, -1, -1):
                nb = V[j] & 1
                V[j] = (V[j] >> 1) | (carry << 7)
                carry = nb
            if carry:
                V[0] ^= 0xe1
    return bytes(Z)

def gcm(key, iv, pt, aad=b''):
    H = ecb_enc(key, bytes(16))
    if len(iv) == 12:
        J0 = iv + bytes(4)
    else:
        p = (16 - len(iv) % 16) % 16
        seq = iv + bytes(p) + bytes(8) + (len(iv)*8).to_bytes(8, 'big')
        X = bytes(16)
        J0 = bytes(16)
        for i in range(0, len(seq), 16):
            X = bytes(x ^ y for x, y in zip(J0, seq[i:i+16]))
            J0 = ghash_mult(X, H)
    # keystream
    ks = b''
    ctr = list(J0)
    while len(ks) < len(pt):
        for i in range(15, 11, -1):
            ctr[i] = (ctr[i] + 1) % 256
            if ctr[i] != 0: break
        ks += ecb_enc(key, bytes(ctr))
    ks = ks[:len(pt)]
    ct = bytes(a ^ b for a, b in zip(pt, ks))
    # tag
    def padded(b):
        p = (16 - len(b) % 16) % 16
        return b + bytes(p)
    X = bytes(16)
    seq = padded(aad) + padded(ct) + (len(aad)*8).to_bytes(8, 'big') + (len(ct)*8).to_bytes(8, 'big')
    Y = bytes(16)
    for i in range(0, len(seq), 16):
        Y = bytes(y ^ s for y, s in zip(Y, seq[i:i+16]))
        Y = ghash_mult(Y, H)
    tag = bytes(y ^ e for y, e in zip(Y, ecb_enc(key, J0)))
    return ct, tag

if __name__ == '__main__':
    # canonical AES-197
    k2 = bytes.fromhex('000102030405060708090a0b0c0d0e0f')
    p2 = bytes.fromhex('00112233445566778899aabbccddeeff')
    print('AES197:', ecb_enc(k2, p2).hex(), 'expect 69c4e0d86a7b0430d8cdb78070b4c55a')
    # GCM all-zeros
    key = bytes(16); iv = bytes(12)
    ct, tag = gcm(key, iv, bytes(16))
    print('GCM zeros: CT =', ct.hex())
    print('GCM zeros: TAG=', tag.hex())
    # GCM feffe9 case
    key2 = bytes.fromhex('feffe9928665731c6d6a8f9467308308')
    iv2 = bytes.fromhex('cafebabefacedbaddecaf888')
    pt2 = bytes.fromhex('d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39')
    ct2, tag2 = gcm(key2, iv2, pt2)
    print('GCM feffe9: CT  =', ct2.hex())
    print('GCM feffe9: TAG =', tag2.hex())
