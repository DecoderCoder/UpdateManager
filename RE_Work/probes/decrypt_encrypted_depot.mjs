// decrypt_encrypted_depot.mjs  [THREAD CLOSED 2026-09-07 — decryption not pursued; API reconstruction is the project goal]
//
// Authoritative GCM counter convention (verified from NIST SP 800-38D text +
// McGrew/Viega "GCM" spec Appendix B test vectors, both extracted locally):
//   - len(IV)=96: J0 = IV || 0^31 || 1   (SP 800-38D: "If len(IV)=96, then let J0 = IV || 0^31 || 1")
//   - len(IV)!=96: J0 = GHASH_H(IV || 0^s || 0^64 || [len(IV)]64)
//   - keystream blocks = E(J0+1), E(J0+2), ... (counter field increments from 1)
//   - so for a 12-byte IV the first keystream block is E(IV || 0x00000002)
//     (matches OpenSSL empirically: D(ks0) == IV||0^24||2 for 12-byte IVs)
//   - official all-zeros vector: C = 0388dace60b6a392f328c2b971b2fe78, T = ab6e47d42cec13bdf53a67b21257bddf
//   - GCM GF(2^128) multiply uses the NIST reference algorithm (gcm.c):
//     msb_set = V[15]&1 per step; bytes shift via V[j] = (V[j]>>1) | (V[j-1]<<7)
//     (low byte truncated = LSB of V[j-1] -> MSB of V[j]); reduce V[15] ^= 0x87.
//     GCM field MSB = LSB of LAST byte (bit-reversed representation, R = 0x87 last byte).
//
// FORMAT FINDINGS THAT STAND (independent of decryption success):
//   - encrypted depot (magic 0x20210506) header = {"header-sha":<64hex>,"key-id":<uuid>}
//   - key-id is a UUID that matches a `name` in the access-group content.json
//     (71 entries); the 16-byte AES-128 key is the base64 `key` field of that entry
//   - depots therefore reference protection keys BY NAME via the access-group key table
//
// decrypt_encrypted_depot.mjs
// Decrypt depot magic 0x20210506 with NEW header format {header-sha, key-id}:
//   key  = 16B base64 from access-group content.json entry with name == key-id
//   IV32 = PBKDF2-HMAC-SHA512(pwd, salt, 1000, 32)
//     chunk 0: pwd = header-sha (hex string); salt = key-id
//   AES-128-GCM, tagless. Test IV lengths 12/16/32 (from PBKDF2 output).
// After GCM keystream: expect xz.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';
const here = path.dirname(fileURLToPath(import.meta.url));

// --- self-test GCM keystream vs node's own GCM for IV lengths 12/16/32 ---
{
  const tk = Buffer.from('feffe9928665731c6d6a8f9467308308', 'hex');
  const tpt = Buffer.from('d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39', 'hex');
  const base = Buffer.from('cafebabefacedbaddecaf88800000000deadbeef01020304', 'hex'); // 24 bytes
  let allOk = true;
  for (const L of [12, 16, 24, 32]) {
    const tiv = base.subarray(0, L);
    const c = crypto.createCipheriv('aes-128-gcm', tk, tiv);
    const ct = Buffer.concat([c.update(tpt), c.final()]);
    const ks = gcmKeystream(tk, tiv, tpt.length);
    const ok = ks.equals(Buffer.from(tpt.map((b, i) => b ^ ct[i])));
    allOk = allOk && ok;
    console.log(`GCM self-test iv${L}:`, ok ? 'PASS' : 'FAIL');
  }
  if (!allOk) process.exit(2);
}

const depot = fs.readFileSync(process.argv[2] || path.join(here, 'reprobe_2026_09_07_h3/h3_cloudfront_https.depot'));
const content = JSON.parse(fs.readFileSync(path.join(here, 'reprobe_access_keys_2026-09-07/content.json.body'), 'utf8'));

const magic = depot.readUInt32LE(0);
const hdrLen = depot.readUInt32LE(4);
const hdr = JSON.parse(depot.subarray(8, 8 + hdrLen).toString('utf8'));
console.log('magic=0x' + magic.toString(16), 'header=', JSON.stringify(hdr));

const entry = content.keys.find(k => k.name === hdr['key-id']);
if (!entry) { console.error('key-id NOT in content.json'); process.exit(1); }
const key = Buffer.from(entry.key, 'base64');
console.log('key from content.json:', entry.name, key.toString('hex'));

const body = depot.subarray(8 + hdrLen);
// chunks: [u32 len][data]
let off = 0, ci = 0;
const results = [];
while (off + 4 <= body.length) {
  const len = body.readUInt32LE(off);
  if (len > body.length - off - 4) { console.log('chunk', ci, 'len', len, 'exceeds remaining', body.length - off - 4, '-> treating rest as one chunk'); break; }
  const data = body.subarray(off + 4, off + 4 + len);
  const pwd = ci === 0 ? hdr['header-sha'] : (hdr['files'] ? hdr.files[ci - 1].sha : null);
  const salt = hdr['key-id'];
  const results2 = tryDecrypt(data, key, pwd, salt, ci);
  off += 4 + len; ci++;
}

function tryDecrypt(data, key, pwd, salt, ci) {
  if (!pwd) { console.log(`chunk ${ci}: no pwd source (header has no files[]); skip`); return; }
  const iv32 = crypto.pbkdf2Sync(pwd, salt, 1000, 32, 'sha512');
  console.log(`\nchunk ${ci}: len=${data.length} pwd=${pwd} iv32=${iv32.toString('hex')}`);
  for (const ivlen of [12, 16, 32]) {
    const iv = iv32.subarray(0, ivlen);
    // direct GCM via node (accepts 12-byte; try 12/16/32)
    try {
      const d = crypto.createDecipheriv('aes-128-gcm', key, iv);
      d.setAuthTag(Buffer.alloc(16)); // tagless: zero tag -> will fail auth; use keystream approach instead
    } catch (e) { /* ignore */ }
    // keystream approach: GCM ks = AES-CTR over J0+1..
    const ks = gcmKeystream(key, iv, data.length);
    const dec = Buffer.from(data);
    for (let i = 0; i < dec.length; i++) dec[i] ^= ks[i];
    const head = dec.subarray(0, 6);
    const isXz = head[0] === 0xfd && head[1] === 0x37 && head[2] === 0x7a && head[3] === 0x58 && head[4] === 0x5a && head[5] === 0x00;
    console.log(`  iv${ivlen}: head=${dec.subarray(0, 16).toString('hex')} xz=${isXz}`);
    if (isXz) {
      fs.writeFileSync(path.join(here, `decrypt_test_iv${ivlen}_chunk${ci}.xz`), dec);
      console.log('  ** XZ confirmed, saved **');
      return ivlen;
    }
  }
  return null;
}

// GCM keystream for arbitrary IV length (RFC 5116 J0 derivation)
function gcmKeystream(key, iv, n) {
  const H = ecbEnc(key, Buffer.alloc(16));
  let J0;
  if (iv.length === 12) {
    // NIST SP 800-38D: len(IV)=96 => J0 = IV || 0^31 || 1 (counter field pre-set to 1)
    J0 = Buffer.concat([iv, Buffer.from([0, 0, 0, 1])]);
  } else {
    // J0 = GHASH_H( iv || zero-pad-to-16 || 0^64 || len64BE )
    const p = (16 - (iv.length % 16)) % 16;
    const padded = Buffer.concat([iv, Buffer.alloc(p)]);
    const lenBlock = Buffer.alloc(16);
    lenBlock.writeBigUInt64BE(BigInt(iv.length * 8), 8);
    const seq = Buffer.concat([padded, lenBlock]);
    J0 = Buffer.alloc(16);
    for (let i = 0; i < seq.length; i += 16) {
      let X = Buffer.alloc(16);
      seq.copy(X, 0, i, i + 16);
      J0 = ghash(H, Buffer.from(J0.map((b, j) => b ^ X[j])));
    }
  }
  // ks = E(J0+1), E(J0+2), ...
  const out = Buffer.alloc(n);
  let ctr = Buffer.from(J0);
  const inc = () => { for (let i = 15; i >= 12; i--) { if (++ctr[i] < 256) break; } };
  let pos = 0;
  for (let i = 1; pos < n; i++) {
    inc();
    const blk = ecbEnc(key, ctr);
    blk.copy(out, pos);
    pos += 16;
  }
  return out.subarray(0, n);
}
function ghash(H, X) {
  // GF(2^128) multiply (GCM bit ordering: MSB-first reflected per RFC 5116)
  const R = [0xe1, 0, 0, 0];
  let Z = Buffer.alloc(16);
  let V = Buffer.from(X);
  for (let i = 0; i < 16; i++) {
    for (let b = 0; b < 8; b++) {
      const mb = (V[i] >> (7 - b)) & 1;
      if (mb) {
        for (let j = 0; j < 16; j++) Z[j] ^= H[j];
      }
      // V >>= 1
      let carry = 0;
      for (let j = 15; j >= 0; j--) {
        const nb = V[j] & 1;
        V[j] = (V[j] >> 1) | (carry << 7);
        carry = nb;
      }
      if (carry) V[0] ^= R[0];
    }
  }
  return Z;
}
function ecbEnc(key, blk) {
  const c = crypto.createCipheriv('aes-128-ecb', key, null);
  c.setAutoPadding(false);
  return Buffer.concat([c.update(blk), c.final()]);
}
