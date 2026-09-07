// Extract embedded PEM public keys from lghub_updater.exe (PE image base 0x140000000).
// Usage: node RE_Work/tools/extract_pem_keys.js
const fs = require('fs');
const crypto = require('crypto');

const PE = 'C:\\Program Files\\LGHUB\\lghub_updater.exe';
const buf = fs.readFileSync(PE);

// --- minimal PE parsing ---
const e_lfanew = buf.readUInt32LE(0x3c);
const sig = buf.toString('ascii', e_lfanew, e_lfanew + 4);
if (sig !== 'PE\0\0') throw new Error('not a PE: ' + sig);
const coff = e_lfanew + 4;
const numSections = buf.readUInt16LE(coff + 2);
const optSize = buf.readUInt16LE(coff + 16);
const opt = coff + 20;
const magic = buf.readUInt16LE(opt);
const is64 = magic === 0x20b;
const numRvaSections = buf.readUInt16LE(opt + (is64 ? 6 : 2));
const secStart = opt + optSize;
const sections = [];
for (let i = 0; i < numRvaSections; i++) {
  const s = secStart + i * 40;
  const name = buf.toString('ascii', s, s + 8).replace(/\0.*$/, '');
  const vsize = buf.readUInt32LE(s + 8);
  const vaddr = buf.readUInt32LE(s + 12);
  const rsize = buf.readUInt32LE(s + 16);
  const rptr = buf.readUInt32LE(s + 20);
  sections.push({ name, vsize, vaddr, rsize, rptr });
}
function rvaToOff(rva) {
  for (const s of sections) {
    if (rva >= s.vaddr && rva < s.vaddr + Math.max(s.vsize, s.rsize))
      return s.rptr + (rva - s.vaddr);
  }
  return -1;
}
const IMAGEBASE = 0x140000000;

function readPemAt(va) {
  const off = rvaToOff(va - IMAGEBASE);
  if (off < 0) return { va, error: 'rva not in any section' };
  // nearest BEGIN within 256 bytes either direction
  let start = buf.lastIndexOf('-----BEGIN PUBLIC KEY-----', Math.min(off, buf.length - 1));
  const fwd = buf.indexOf('-----BEGIN PUBLIC KEY-----', Math.max(0, off - 32));
  if (fwd >= 0 && (start < 0 || fwd - off < off - start)) start = fwd;
  if (start < 0 || Math.abs(start - off) > 256)
    return { va, error: 'no PEM header near ' + va.toString(16) };
  const end = buf.indexOf('-----END PUBLIC KEY-----', start);
  if (end < 0) return { va, error: 'no END after start' };
  const stop = buf.indexOf(0x0a, end);
  const text = buf.toString('ascii', start, stop + 1);
  return { va, text, headerAt: start };
}

function describe(pemText) {
  // parse DER manually enough to get modulus bit length + fingerprint
  const b64 = pemText
    .replace(/-----BEGIN PUBLIC KEY-----/, '')
    .replace(/-----END PUBLIC KEY-----/, '')
    .replace(/\s+/g, '');
  const der = Buffer.from(b64, 'base64');
  if (der[0] !== 0x30) return { error: 'der[0]=0x' + der[0].toString(16), first8: [...der.slice(0, 8)].map(b => b.toString(16)).join(' ') };
  // walk: SEQ { SEQ { OID, NULL }, BIT STRING { RSAPubKey SEQ { INT n, INT e } } }
  let p = 0;
  const rdLen = (pp) => {
    const l = der[pp];
    pp++;
    if (l < 0x80) return [l, pp];
    const n = l & 0x7f;
    const v = der.readUIntBE(pp, n);
    return [v, pp + n];
  };
  if (der[p] !== 0x30) throw new Error('outer not SEQ');
  p++;
  let [l1, q] = rdLen(p); p = q;
  if (der[p] !== 0x30) throw new Error('algid not SEQ');
  p++;
  let [l2, q2] = rdLen(p); p = q2 + l2; // skip algid
  if (der[p] !== 0x03) throw new Error('not BIT STRING');
  p++;
  let [l3, q3] = rdLen(p); p = q3 + 1; // skip unused-bits
  if (der[p] !== 0x30) throw new Error('rsapubkey not SEQ');
  p++;
  let [l4, q4] = rdLen(p); p = q4;
  if (der[p] !== 0x02) throw new Error('modulus not INT');
  p++;
  let [mlen, q5] = rdLen(p); p = q5;
  const modulus = der.subarray(p, p + mlen);
  const bits = (modulus[0] ? (modulus[0] < 0x80 ? 0 : 8) : 0) + (modulus.length - (modulus[0] === 0 ? 1 : 0)) * 8;
  const fp = crypto.createHash('sha256').update(modulus).digest('hex').slice(0, 16);
  return { modulusBytes: mlen, bits, fp, derLen: der.length };
}

const targets = [
  0x140faa24d,
  0x141340df5, 0x141340fc5, 0x141341195,
  0x1413414b5, 0x1413417e5, 0x1413419b5,
];

const seen = new Map();
for (const va of targets) {
  const r = readPemAt(va);
  if (r.error) { console.log('0x' + va.toString(16) + ': ' + r.error); continue; }
  const d = describe(r.text);
  const key = d.fp;
  const firstAt = seen.get(key);
  console.log('VA 0x' + va.toString(16).padStart(12, '0') +
    (firstAt ? '  (same key as 0x' + firstAt + ')' : '  NEW') +
    '  ' + d.bits + '-bit  der=' + d.derLen + 'B  fp=' + d.fp);
  if (!firstAt) seen.set(key, va.toString(16));
}

// save distinct PEMs to fixtures for offline use
let n = 0;
for (const va of targets) {
  const r = readPemAt(va);
  if (r.error) continue;
  const d = describe(r.text);
  if (![...seen.values()].some(v => 'x')) break;
  if (!seen.has(d.fp)) continue;
  // only save if this occurrence is the first for its fingerprint
  let first = true;
  for (const other of targets) {
    if (other === va) break;
    const o = readPemAt(other);
    if (!o.error && describe(o.text).fp === d.fp) { first = false; break; }
  }
  if (first) {
    const out = `RE_Work/probes/pipeline_pubkey_${n++}.pem`;
    fs.writeFileSync(out, r.text);
    console.log('saved ' + out + '  (' + d.bits + ' bits)');
  }
}
