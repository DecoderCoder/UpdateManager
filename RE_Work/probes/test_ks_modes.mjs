// test_ks_modes.mjs
// With the FULL staging keystream known (ks = ct ^ pt), identify the cipher:
// for each of the 71 access-group keys, test whether ks is AES under:
//   - CTR (16B IV, BE full-block increment)
//   - CTR (16B IV, increment last 4 bytes only)
//   - GCM-style (12B IV || 32-bit BE counter starting at 1)
//   - OFB (ks[i+1] = E_k(ks[i]))
//   - CFB-128 (ks[i+1] = E_k(ct[i]))
//   - CBC (pt[i] = D_k(ct[i] ^ ct[i-1]) for i>=1)
// Any consistent multi-block match = identification.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';
const here = path.dirname(fileURLToPath(import.meta.url));

const content = JSON.parse(fs.readFileSync(path.join(here, 'reprobe_access_keys_2026-09-07/content.json.body'), 'utf8'));
const keys = content.keys.map(k => ({ name: k.name, raw: Buffer.from(k.key, 'base64') }));
const ks = fs.readFileSync(path.join(here, 'reprobe_channel_brute_2026-09-07/staging_keystream.bin'));
const ct = fs.readFileSync(path.join(here, 'reprobe_channel_brute_2026-09-07/staging_details.body'));
const pt = fs.readFileSync(path.join(here, 'reprobe_channel_brute_2026-09-07/staging_details_RECOVERED.json'));

function E(key, blk) {
  const c = crypto.createCipheriv('aes-128-ecb', key, null);
  c.setAutoPadding(false);
  return Buffer.concat([c.update(blk), c.final()]);
}
function D(key, blk) {
  const c = crypto.createDecipheriv('aes-128-ecb', key, null);
  c.setAutoPadding(false);
  return Buffer.concat([c.update(blk), c.final()]);
}
const incBE16 = (b) => { const x = Buffer.from(b); for (let i = 15; i >= 0; i--) { if (++x[i] < 256) break; } return x; };
const inc4 = (b) => { const x = Buffer.from(b); for (let i = 15; i >= 12; i--) { if (++x[i] < 256) break; } return x; };
const B = (buf, i) => buf.subarray(i * 16, i * 16 + 16);

const hits = [];
for (const k of keys) {
  const ks0 = B(ks, 0), ks1 = B(ks, 1), ks2 = B(ks, 2), ks3 = B(ks, 3);
  // CTR-16 BE
  const ivBE = D(k.raw, ks0);
  if (E(k.raw, incBE16(ivBE)).equals(ks1) && E(k.raw, incBE16(incBE16(ivBE))).equals(ks2))
    hits.push({ key: k.name, mode: 'CTR16-BE', iv: ivBE.toString('hex') });
  // CTR-16 inc4
  const iv4 = D(k.raw, ks0);
  if (E(k.raw, inc4(iv4)).equals(ks1) && E(k.raw, inc4(inc4(iv4))).equals(ks2))
    hits.push({ key: k.name, mode: 'CTR16-inc4', iv: iv4.toString('hex') });
  // GCM-style 12B IV
  const g0 = D(k.raw, ks0);
  if (g0.readUInt32BE(12) === 1) {
    const g1 = Buffer.from(g0); g1.writeUInt32BE(2, 12);
    if (E(k.raw, g1).equals(ks1)) {
      const g2 = Buffer.from(g0); g2.writeUInt32BE(3, 12);
      if (E(k.raw, g2).equals(ks2)) hits.push({ key: k.name, mode: 'GCM-12IV', iv: g0.subarray(0, 12).toString('hex') });
    }
  }
  // OFB
  const ofb0 = D(k.raw, ks0);
  if (E(k.raw, ks0).equals(ks1) && E(k.raw, ks1).equals(ks2))
    hits.push({ key: k.name, mode: 'OFB', iv: ofb0.toString('hex') });
  // CFB128
  const cfb0 = D(k.raw, ks0);
  if (E(k.raw, B(ct, 0)).equals(ks1) && E(k.raw, B(ct, 1)).equals(ks2))
    hits.push({ key: k.name, mode: 'CFB128', iv: cfb0.toString('hex') });
  // CBC
  if (D(k.raw, Buffer.from(B(ct, 1).map((b, i) => b ^ ct[i]))).equals(B(pt, 1))
    && D(k.raw, Buffer.from(B(ct, 2).map((b, i) => b ^ ct[i + 16]))).equals(B(pt, 2)))
    hits.push({ key: k.name, mode: 'CBC', iv: Buffer.from(B(ct, 0).map((b, i) => b ^ E(k.raw, B(pt, 0))[i])).toString('hex') });
}
console.log(`Tested ${keys.length} keys x 6 modes. Hits: ${hits.length}`);
for (const h of hits) console.log('HIT:', JSON.stringify(h));
fs.writeFileSync(path.join(here, 'reprobe_channel_brute_2026-09-07/test_ks_modes_result.json'), JSON.stringify({ hits }, null, 2));
console.log('Wrote test_ks_modes_result.json');
