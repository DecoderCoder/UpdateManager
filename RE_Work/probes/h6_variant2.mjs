import { readFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
const hex = readFileSync('RE_Work/probes/h6_blob_full.txt', 'utf8').trim();
const blob = Buffer.from(hex, 'hex');
const payload = blob.subarray(blob.length - 64);
const name = 'DESKTOP-IFDD7ML';
const vol = 0xfa6c8aa7;
const nameU16 = Buffer.from(name, 'utf16le');
const nameU8 = Buffer.from(name, 'utf8');
const volLE = Buffer.alloc(4); volLE.writeUInt32LE(vol);
const volBE = Buffer.alloc(4); volBE.writeUInt32BE(vol);
const sha512 = (b) => createHash('sha512').update(b).digest();
const sha256 = (b) => createHash('sha256').update(b).digest();
const tries = [
  ['sha512 nameU16+volLE', sha512(Buffer.concat([nameU16, volLE]))],
  ['sha512 nameU16+volBE', sha512(Buffer.concat([nameU16, volBE]))],
  ['sha512 nameU8+volLE', sha512(Buffer.concat([nameU8, volLE]))],
  ['sha512 nameU16 only', sha512(nameU16)],
  ['sha512 nameU8 only', sha512(nameU8)],
  ['sha512 volLE only', sha512(volLE)],
];
for (const [label, d] of tries) {
  console.log((d.equals(payload) ? '*** MATCH' : 'no'), label, '->', d.toString('hex').slice(0, 40) + '…');
}
// UTF-16LE hex-string scan: 64 ascii hex chars interleaved with 0x00
const isHex = (c) => (c >= 0x30 && c <= 0x39) || (c >= 0x61 && c <= 0x66) || (c >= 0x41 && c <= 0x46);
let runs = [];
let i = 0;
while (i + 128 <= blob.length) {
  let ok = true;
  for (let k = 0; k < 64; k++) {
    if (!isHex(blob[i + 2 * k]) || blob[i + 2 * k + 1] !== 0) { ok = false; break; }
  }
  if (ok) { runs.push(i); }
  i++;
}
console.log('utf16 hex64 runs at bytes:', runs.length ? runs.join(',') : 'none');
// also: is payload == sha256(ascii hex of something)? check payload vs sha256 of nameU16+volLE as ascii hex already done round1.
// Check: maybe value is 32B digest and next 32B = HMAC/second hash; print both halves
console.log('payload hi32:', payload.subarray(0, 32).toString('hex'));
console.log('payload lo32:', payload.subarray(32).toString('hex'));
// maybe the 64-byte region is NOT the id; search whole blob for the 32B sha256(nameU16+volLE)
const target = sha256(Buffer.concat([nameU16, volLE]));
console.log('blob contains sha256(nameU16+volLE)?', blob.includes(target));
const target512 = sha512(Buffer.concat([nameU16, volLE]));
console.log('blob contains sha512(nameU16+volLE)[0:32]?', blob.includes(target512.subarray(0, 32)));
console.log('done');
