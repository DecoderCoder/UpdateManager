// ghash_test.cjs — test single GHASH multiplication against GCM spec test case 4 X1
const crypto = require('crypto');

function ghash(H, X) {
  let Z = Buffer.alloc(16);
  let V = Buffer.from(X);
  for (let i = 0; i < 16; i++) {
    for (let b = 0; b < 8; b++) {
      const mb = (V[i] >> (7 - b)) & 1;
      if (mb) {
        for (let j = 0; j < 16; j++) Z[j] ^= H[j];
      }
      let carry = 0;
      for (let j = 15; j >= 0; j--) {
        const nb = V[j] & 1;
        V[j] = (V[j] >> 1) | (carry << 7);
        carry = nb;
      }
      if (carry) V[0] ^= 0xe1;
    }
  }
  return Z;
}

const H = Buffer.from('b83b533708bf535d0aa6e52980d53b78', 'hex');
const A1 = Buffer.from('feedfacedeadbeefeedfacedeadbeef', 'hex');
const X1 = ghash(H, A1);
console.log('X1 =', X1.toString('hex'));
console.log('expected: ed56aaf8a72d67049fdb9228edba1322');
console.log('match:', X1.toString('hex') === 'ed56aaf8a72d67049fdb9228edba1322');

// also test zero: GHASH of all-zero block = 0
const Z0 = ghash(H, Buffer.alloc(16));
console.log('0*H =', Z0.toString('hex'), 'expect 000...0', Z0.every(b => b === 0));
