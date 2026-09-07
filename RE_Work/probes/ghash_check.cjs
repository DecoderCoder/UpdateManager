// ghash_check.cjs — verify GF(2^128) multiply + tag for GCM vector 4 (empty PT/AAD)
const crypto = require('crypto');

function ghash(H, X) {
  const R = 0xe1;
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
      if (carry) V[0] ^= R;
    }
  }
  return Z;
}

const tk = Buffer.from('feffe9928665731c6d6a8f9467308308', 'hex');
const e = crypto.createCipheriv('aes-128-ecb', tk, null); e.setAutoPadding(false);
const H = Buffer.concat([e.update(Buffer.alloc(16)), e.final()]);
console.log('H =', H.toString('hex'));
const J0 = Buffer.from('cafebabefacedbaddecaf88800000000', 'hex');
const tag = ghash(H, J0);
console.log('J0*H =', tag.toString('hex'));
console.log('expected (vector 4): 6494cfb05f6fa542be2e1e1402c9b747');
console.log('match:', tag.toString('hex') === '6494cfb05f6fa542be2e1e1402c9b747');
