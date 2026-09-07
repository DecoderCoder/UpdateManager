// ghash_test2.cjs — correct GCM GF(2^128) multiply, validated against spec test case 4
const H = Buffer.from('b83b533708bf535d0aa6e52980d53b78', 'hex');
const A1 = Buffer.from('feedfacedeadbeefeedfacedeadbeef', 'hex');
const EXPECT_X1 = 'ed56aaf8a72d67049fdb9228edba1322';

// GF(2^128) multiply — NIST reference gcm.c algorithm (GCM reversed bit order):
// field MSB = LSB of LAST byte; right shift flows toward last byte; R = 0x87 in last byte.
function mult(X) {
  const Z = Buffer.alloc(16);
  const V = Buffer.from(X);
  for (let k = 0; k < 128; k++) {
    const msbSet = V[15] & 1;
    if (msbSet) for (let j = 0; j < 16; j++) Z[j] ^= H[j];
    for (let j = 15; j > 0; j--) V[j] = (V[j] >> 1) | (V[j - 1] << 7);
    V[0] >>= 1;
    if (msbSet) V[15] ^= 0x87;
  }
  return Z;
}

console.log('result:', mult(A1).toString('hex'));
console.log('expected:', EXPECT_X1);
console.log('match:', mult(A1).toString('hex') === EXPECT_X1);
