// Try candidate derivations of the stored 64-byte payload (last 64 B of 396-B blob).
import { readFileSync } from 'node:fs';
import { createHash } from 'node:crypto';

const hex = readFileSync('RE_Work/probes/h6_blob_full.txt', 'utf8').trim();
const blob = Buffer.from(hex, 'hex');
console.log('blob length:', blob.length);
const payload = blob.subarray(blob.length - 64);
console.log('payload:', payload.toString('hex'));

const name = 'DESKTOP-IFDD7ML';
const vol = 0xfa6c8aa7;
const nameU16 = Buffer.from(name, 'utf16le');
const nameU8 = Buffer.from(name, 'utf8');
const volLE = Buffer.alloc(4); volLE.writeUInt32LE(vol);
const volHEX = vol.toString(16).toUpperCase().padStart(8, '0');

const sha = (buf) => createHash('sha256').update(buf).digest();
const tries = [
  ['nameU16 + volLE', Buffer.concat([nameU16, volLE])],
  ['nameU16 + volLE + 2NUL (padded name)', Buffer.concat([nameU16, Buffer.from([0, 0]), volLE])],
  ['nameU16 + volHEX_upper', Buffer.concat([nameU16, Buffer.from(volHEX, 'ascii')])],
  ['nameU16 + volHEX_lower', Buffer.concat([nameU16, Buffer.from(volHEX.toLowerCase(), 'ascii')])],
  ['nameU8 + volLE', Buffer.concat([nameU8, volLE])],
  ['nameU8 + volHEX_upper', Buffer.concat([nameU8, Buffer.from(volHEX, 'ascii')])],
  ['volLE + nameU16', Buffer.concat([volLE, nameU16])],
  ['nameU16 only', nameU16],
  ['nameU8 only', nameU8],
  ['volLE only', volLE],
  ['nameU16 + volLE + nameU16', Buffer.concat([nameU16, volLE, nameU16])],
  ['volLE + volLE', Buffer.concat([volLE, volLE])],
  ['nameU16 + volBE', Buffer.concat([nameU16, Buffer.from([vol >>> 24, (vol >>> 16) & 0xff, (vol >>> 8) & 0xff, vol & 0xff])])],
  // two-stage: hash the hex string of a first-stage digest
  ['2stage hex(sha(nameU16+volLE))', Buffer.from(sha(Buffer.concat([nameU16, volLE])).toString('hex'), 'ascii')],
  ['2stage raw(sha(nameU16+volLE))', sha(Buffer.concat([nameU16, volLE]))],
  // HDD serial variants (disk0 serial "9")
  ['hdd "9" utf8', Buffer.from('9', 'utf8')],
  ['hdd "9" utf16', Buffer.from('9', 'utf16le')],
];
for (const [label, buf] of tries) {
  const d = sha(buf);
  const hit = d.equals(payload);
  if (hit) console.log('*** MATCH:', label, '->', d.toString('hex'));
  else console.log('no:', label, '->', d.toString('hex').slice(0, 32) + '…');
}
// also print payload as potential ascii-hex string check
console.log('payload as ascii:', JSON.stringify(payload.toString('latin1')));
// check if payload could be 32-char hex (ascii) — i.e., 16 bytes only? no, 64 bytes.
// maybe stored id = hex string elsewhere in blob? scan blob for 64 ascii-hex runs
const latin = blob.toString('latin1');
const re = /[0-9a-f]{64}/g;
let m;
while ((m = re.exec(latin)) !== null) {
  console.log('ascii-hex64 run at byte', m.index, ':', m[0]);
  if (createHash('sha256').update(m[0], 'ascii').digest().equals(payload)) console.log('  ^^ sha matches payload');
}
console.log('done');
