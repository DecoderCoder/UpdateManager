// test_access_keys.mjs
// Test all access-group keys from live content.json (group 323e77f5-...) against the
// encrypted staging/tim channel manifest bodies (update.json + details.json).
//
// Known plaintext (from public channel, identical layout):
//   pt[0..] = '{\n  "appId": "ghub10",\n  "platform": "win",\n  "channel": "<chan>", ...'
// Same-channel update.json and details.json share key+IV (identical ct prefixes observed).
//
// Modes tested per key/IV (node crypto, synchronous):
//   AES-128-CTR (16B IV), AES-128-GCM (12B IV, keystream only),
//   AES-128-CBC (first-block keystream = E_k(IV)), AES-128-ECB (first block decrypt).
// Success = derived keystream XOR ct[0:12] == fixed pt prefix (12 bytes).

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import crypto from 'node:crypto';

const here = path.dirname(fileURLToPath(import.meta.url));

const content = JSON.parse(fs.readFileSync(path.join(here, 'reprobe_access_keys_2026-09-07/content.json.body'), 'utf8'));
const keys = content.keys.map(k => ({ name: k.name, raw: Buffer.from(k.key, 'base64'), lm: k.lastModified, version: k.version }));
console.log(`Loaded ${keys.length} keys from content.json (group ${content.accessGroup})`);

const targets = {
  staging_update:  'reprobe_channel_brute_2026-09-07/staging_update.body',
  staging_details: 'reprobe_channel_brute_2026-09-07/staging_details.body',
  tim_update:      'reprobe_channel_names_2026-09-07/tim_update.body',
  tim_details:     'reprobe_channel_names_2026-09-07/tim_details.body',
};
const CT = {};
for (const [n, p] of Object.entries(targets)) CT[n] = fs.readFileSync(path.join(here, p));

const PT12 = Buffer.from([0x7b, 0x0a, 0x20, 0x20, 0x22, 0x61, 0x70, 0x70, 0x49, 0x64, 0x22, 0x3a]);
console.log('pt prefix:', PT12.toString('hex'));

const ZERO16 = Buffer.alloc(16);

function ksCtr(key, iv16) {
  const c = crypto.createCipheriv('aes-128-ctr', key, iv16);
  return Buffer.concat([c.update(ZERO16), c.final()]);
}
function ksGcm(key, iv12) {
  // GCM keystream block i = E_k(IV || 0^31 || (i+1))  ==  CTR over IV||00000001
  const c = crypto.createCipheriv('aes-128-ctr', key, Buffer.concat([iv12, Buffer.from([0, 0, 0, 1])]));
  return Buffer.concat([c.update(ZERO16), c.final()]);
}
function ksCbc(key, iv16) {
  const c = crypto.createCipheriv('aes-128-cbc', key, iv16);
  c.setAutoPadding(false);
  return Buffer.concat([c.update(ZERO16), c.final()]);
}
function ecbDec(key, block) {
  const c = crypto.createDecipheriv('aes-128-ecb', key, null);
  c.setAutoPadding(false);
  return Buffer.concat([c.update(block), c.final()]);
}

function ivCandidates(k, channel) {
  const n = Buffer.from(k.name, 'utf8');
  const out = [];
  const push = (name, d) => { out.push({ name, iv16: d.length >= 16 ? d.slice(0, 16) : null, iv12: d.length >= 12 ? d.slice(0, 12) : null }); };
  push('zero', Buffer.alloc(32));
  push('md5(name)', crypto.createHash('md5').update(n).digest());
  push('sha1(name)', crypto.createHash('sha1').update(n).digest());
  push('sha256(name)', crypto.createHash('sha256').update(n).digest());
  push('md5(raw)', crypto.createHash('md5').update(k.raw).digest());
  push('sha256(raw)', crypto.createHash('sha256').update(k.raw).digest());
  push('md5(name+raw)', crypto.createHash('md5').update(Buffer.concat([n, k.raw])).digest());
  push('sha256(name+raw)', crypto.createHash('sha256').update(Buffer.concat([n, k.raw])).digest());
  push('sha256(raw+name)', crypto.createHash('sha256').update(Buffer.concat([k.raw, n])).digest());
  push('pbkdf2(pwd=name,salt=name)', crypto.pbkdf2Sync(k.name, k.name, 1000, 32, 'sha512'));
  push('pbkdf2(pwd=rawhex,salt=name)', crypto.pbkdf2Sync(k.raw.toString('hex'), k.name, 1000, 32, 'sha512'));
  push('pbkdf2(pwd=name,salt=raw)', crypto.pbkdf2Sync(k.name, k.raw, 1000, 32, 'sha512'));
  push('pbkdf2(pwd=raw,salt=name)', crypto.pbkdf2Sync(k.raw.toString('binary'), k.name, 1000, 32, 'sha512'));
  for (const salt of ['logitech', 'logitechg', 'ghub', 'ghub10', channel, 'content', 'keys', 'update.json', 'details.json', 'pipeline', 'v2', 'win', 'content.json']) {
    push(`pbkdf2(name,${salt})`, crypto.pbkdf2Sync(k.name, salt, 1000, 32, 'sha512'));
    push(`pbkdf2(raw,${salt})`, crypto.pbkdf2Sync(k.raw.toString('hex'), salt, 1000, 32, 'sha512'));
  }
  for (let c = 0; c <= 3; c++) {
    const iv = Buffer.alloc(16);
    iv.writeUInt32BE(c, 12);
    out.push({ name: `ctrzero-c${c}`, iv16: iv, iv12: iv.slice(0, 12) });
  }
  out.push({ name: 'name[0:16]', iv16: n.slice(0, 16), iv12: n.slice(0, 12) });
  return out;
}

function testOne(keyObj, ct, label) {
  const ct12 = ct.subarray(0, 12);
  const xors = (ks) => Buffer.from(ct12.map((b, i) => b ^ ks[i]));
  for (const ivc of ivCandidates(keyObj, label)) {
    if (ivc.iv16) {
      const k1 = xors(ksCtr(keyObj.raw, ivc.iv16));
      if (Buffer.compare(k1, PT12) === 0) return { hit: true, mode: 'CTR', iv: ivc.name, keyName: keyObj.name, ivHex: ivc.iv16.toString('hex') };
      const k2 = xors(ksCbc(keyObj.raw, ivc.iv16));
      if (Buffer.compare(k2, PT12) === 0) return { hit: true, mode: 'CBC', iv: ivc.name, keyName: keyObj.name, ivHex: ivc.iv16.toString('hex') };
    }
    if (ivc.iv12) {
      const k3 = xors(ksGcm(keyObj.raw, ivc.iv12));
      if (Buffer.compare(k3, PT12) === 0) return { hit: true, mode: 'GCM', iv: ivc.name, keyName: keyObj.name, ivHex: ivc.iv12.toString('hex') };
    }
  }
  const dec = ecbDec(keyObj.raw, ct.subarray(0, 16));
  if (Buffer.compare(dec.subarray(0, 12), PT12) === 0)
    return { hit: true, mode: 'ECB', iv: 'n/a', keyName: keyObj.name, ivHex: '' };
  return null;
}

const results = [];
let total = 0;
for (const [tName, ct] of Object.entries(CT)) {
  for (const k of keys) {
    total++;
    const r = testOne(k, ct, tName.split('_')[0]);
    if (r) {
      results.push({ target: tName, ...r });
      console.log('HIT:', JSON.stringify({ target: tName, ...r }));
    }
  }
  console.log(`done ${tName} (${keys.length} keys)`);
}
console.log(`\nTested ${total} (key,target) pairs. Hits: ${results.length}`);
fs.writeFileSync(path.join(here, 'reprobe_access_keys_2026-09-07/test_access_keys_result.json'), JSON.stringify({ group: content.accessGroup, keys: keys.length, results }, null, 2));
console.log('Wrote reprobe_access_keys_2026-09-07/test_access_keys_result.json');
