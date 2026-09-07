// test_ks_deep.mjs
// Round 2: with full staging KS known, test:
//  (A) ECB hypothesis: repeated 16B ct blocks (non-adjacent) => ECB.
//  (B) Depot-style PBKDF2 IV: IV = PBKDF2-HMAC-SHA512(pwd, salt=keyname, 1000, 32B) with
//      pwd in {sha256hex(pt), sha256hex(pt) no-hex, sha256raw(pt), "logitech", channel, ...}
//      against each raw key, modes CTR16/GCM12.
//  (C) Derived-key candidates (hashes/pbkdf2 of the 71 keys, SSO ids, config tokens)
//      tested with the exact block-structure tests (CTR16-BE, CTR16-inc4, GCM12, OFB, CFB128, CBC).
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

function E(key, blk) { const c = crypto.createCipheriv('aes-128-ecb', key, null); c.setAutoPadding(false); return Buffer.concat([c.update(blk), c.final()]); }
function D(key, blk) { const c = crypto.createDecipheriv('aes-128-ecb', key, null); c.setAutoPadding(false); return Buffer.concat([c.update(blk), c.final()]); }
const incBE16 = (b) => { const x = Buffer.from(b); for (let i = 15; i >= 0; i--) { if (++x[i] < 256) break; } return x; };
const inc4 = (b) => { const x = Buffer.from(b); for (let i = 15; i >= 12; i--) { if (++x[i] < 256) break; } return x; };
const B = (buf, i) => buf.subarray(i * 16, i * 16 + 16);

// ---------- (A) ECB repeat check ----------
{
  const seen = new Map();
  let repeats = 0;
  for (let i = 0; i + 16 <= ct.length; i += 16) {
    const blk = ct.subarray(i, i + 16).toString('hex');
    if (seen.has(blk)) { repeats++; if (repeats <= 5) console.log(`repeat ct block at ${i} and ${seen.get(blk)}: ${blk}`); }
    else seen.set(blk, i);
  }
  console.log(`(A) ECB check: ${repeats} repeated 16B ct blocks out of ${Math.floor(ct.length / 16)} (nonzero => ECB)`);
}

// ---------- (B) depot-style PBKDF2 IV with known pt ----------
{
  const pth = crypto.createHash('sha256').update(pt).digest('hex');
  const ptraw = crypto.createHash('sha256').update(pt).digest();
  const pwds = [
    ['pthex', pth], ['ptraw', ptraw.toString('binary')],
    ['logitech', 'logitech'], ['staging', 'staging'], ['ghub10', 'ghub10'],
    ['details.json', 'details.json'], ['update.json', 'update.json'],
  ];
  const hits = [];
  for (const k of keys) {
    for (const [pname, pwd] of pwds) {
      for (const hname of ['sha512']) {
        const iv = crypto.pbkdf2Sync(pwd, k.name, 1000, 32, hname);
        // CTR16 with iv[0:16]
        const iv16 = iv.subarray(0, 16);
        if (E(k.raw, incBE16(iv16)).equals(B(ks, 1))) hits.push({ key: k.name, ivsrc: `pbkdf2(${pname},name)`, mode: 'CTR16', iv: iv16.toString('hex') });
        // GCM12 with iv[0:12]||1
        const iv12 = iv.subarray(0, 12);
        const ctr32 = (n) => { const b = Buffer.alloc(4); b.writeUInt32BE(n); return b; };
        const g0 = Buffer.concat([iv12, ctr32(1)]);
        if (E(k.raw, g0).equals(B(ks, 0)) && E(k.raw, Buffer.concat([iv12, ctr32(2)])).equals(B(ks, 1)))
          hits.push({ key: k.name, ivsrc: `pbkdf2(${pname},name)`, mode: 'GCM12', iv: iv12.toString('hex') });
        // also salt = raw key bytes
        const iv2 = crypto.pbkdf2Sync(pwd, k.raw, 1000, 32, 'sha512');
        if (E(k.raw, incBE16(iv2.subarray(0, 16))).equals(B(ks, 1))) hits.push({ key: k.name, ivsrc: `pbkdf2(${pname},raw)`, mode: 'CTR16', iv: iv2.subarray(0, 16).toString('hex') });
      }
    }
  }
  console.log(`(B) PBKDF2-IV hits: ${hits.length}`);
  for (const h of hits.slice(0, 10)) console.log('  ', JSON.stringify(h));
}

// ---------- (C) derived key candidates ----------
{
  const cands = [];
  const add = (src, b) => { if (b && b.length >= 16) cands.push({ src, key: b.subarray(0, 16) }); };
  for (const k of keys) {
    const n = Buffer.from(k.name, 'utf8');
    add(`raw:${k.name.slice(0, 8)}`, k.raw);
    add(`sha256(name):${k.name.slice(0, 8)}`, crypto.createHash('sha256').update(n).digest());
    add(`md5(name):${k.name.slice(0, 8)}`, crypto.createHash('md5').update(n).digest());
    add(`sha1(name):${k.name.slice(0, 8)}`, crypto.createHash('sha1').update(n).digest());
    add(`sha256(raw):${k.name.slice(0, 8)}`, crypto.createHash('sha256').update(k.raw).digest());
    add(`md5(raw):${k.name.slice(0, 8)}`, crypto.createHash('md5').update(k.raw).digest());
    add(`sha256(n+raw):${k.name.slice(0, 8)}`, crypto.createHash('sha256').update(Buffer.concat([n, k.raw])).digest());
    add(`sha256(raw+n):${k.name.slice(0, 8)}`, crypto.createHash('sha256').update(Buffer.concat([k.raw, n])).digest());
    add(`pbkdf2s256(n,n):${k.name.slice(0, 8)}`, crypto.pbkdf2Sync(k.name, k.name, 1000, 16, 'sha256'));
    add(`pbkdf2s512(n,n):${k.name.slice(0, 8)}`, crypto.pbkdf2Sync(k.name, k.name, 1000, 16, 'sha512'));
    for (const salt of ['logitech', 'ghub', 'ghub10', 'staging', 'win']) {
      add(`pbkdf2s256(n,${salt}):${k.name.slice(0, 8)}`, crypto.pbkdf2Sync(k.name, salt, 1000, 16, 'sha256'));
      add(`pbkdf2s512(raw,${salt}):${k.name.slice(0, 8)}`, crypto.pbkdf2Sync(k.raw.toString('hex'), salt, 1000, 16, 'sha512'));
    }
  }
  // SSO client ids (from config table, read live from IDB below) as utf8 keys
  const ssoIds = JSON.parse(fs.readFileSync(path.join(here, 'reprobe_access_keys_2026-09-07/sso_client_ids.json'), 'utf8'));
  for (const id of ssoIds) {
    cands.push({ src: `sso:${id.slice(0, 8)}`, key: Buffer.from(id, 'utf8').subarray(0, 16) });
  }
  // dedupe by key bytes
  const seen = new Set();
  const uniq = cands.filter(c => { const h = c.key.toString('hex'); if (seen.has(h)) return false; seen.add(h); return true; });
  console.log(`(C) testing ${uniq.length} derived key candidates`);

  const ks0 = B(ks, 0), ks1 = B(ks, 1), ks2 = B(ks, 2);
  const hits = [];
  for (const c of uniq) {
    const k = c.key;
    let ivBE = D(k, ks0);
    if (E(k, incBE16(ivBE)).equals(ks1) && E(k, incBE16(incBE16(ivBE))).equals(ks2)) hits.push({ src: c.src, mode: 'CTR16-BE', iv: ivBE.toString('hex') });
    let iv4 = D(k, ks0);
    if (E(k, inc4(iv4)).equals(ks1) && E(k, inc4(inc4(iv4))).equals(ks2)) hits.push({ src: c.src, mode: 'CTR16-inc4', iv: iv4.toString('hex') });
    const g0 = D(k, ks0);
    if (g0.readUInt32BE(12) === 1) {
      const g1 = Buffer.from(g0); g1.writeUInt32BE(2, 12);
      if (E(k, g1).equals(ks1)) hits.push({ src: c.src, mode: 'GCM12', iv: g0.subarray(0, 12).toString('hex') });
    }
    if (E(k, ks0).equals(ks1) && E(k, ks1).equals(ks2)) hits.push({ src: c.src, mode: 'OFB', iv: D(k, ks0).toString('hex') });
    if (E(k, B(ct, 0)).equals(ks1) && E(k, B(ct, 1)).equals(ks2)) hits.push({ src: c.src, mode: 'CFB128', iv: D(k, ks0).toString('hex') });
    if (D(k, Buffer.from(B(ct, 1).map((b, i) => b ^ ct[i]))).equals(B(pt, 1))) hits.push({ src: c.src, mode: 'CBC' });
  }
  console.log(`(C) derived-key hits: ${hits.length}`);
  for (const h of hits.slice(0, 10)) console.log('  ', JSON.stringify(h));
}
