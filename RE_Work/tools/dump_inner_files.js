// One-off: print the decrypted files-list (chunk0) of the saved encrypted depot.
// Uses the exact recipe from RE_Work/review/verify-saved-sample.mjs (offline).
import { readFileSync } from 'node:fs';
import { createDecipheriv, pbkdf2Sync } from 'node:crypto';

const read = (name) => readFileSync(`RE_Work/probes/${name}`);
const bytes = read('depot_full_85875e86-f3e1-4e79-91ee-232575e2807f.bin');
const offset0 = 8 + bytes.readUInt32LE(4);
const header = JSON.parse(bytes.subarray(8, offset0));
let offset = offset0;
const chunks = [];
while (offset < bytes.length) {
  const length = bytes.readUInt32LE(offset);
  offset += 4;
  chunks.push(bytes.subarray(offset, offset + length));
  offset += length;
}
const content = JSON.parse(read('probe_access_ghub10_group_content.out'));
const key = content.keys.find((k) => k.name === header['key-id']);
const rawKey = Buffer.from(key.key, 'base64');

function decode(ciphertext, expectedSha) {
  const iv = pbkdf2Sync(expectedSha, key.name, 1000, 32, 'sha512');
  const d = createDecipheriv('aes-128-gcm', rawKey, iv);
  return d.update(ciphertext);
}

const inner = JSON.parse(decode(chunks[0], header['header-sha']));
console.log('chunk0 JSON keys:', Object.keys(inner).join(', '));
console.log('files count:', inner.files.length);
for (const f of inner.files) console.log(JSON.stringify(f));
// Show byte counts per chunk to document the size-prefix layout
console.log('chunk sizes:', chunks.map((c) => c.length).join(', '));
console.log('total:', chunks.reduce((a, c) => a + c.length, 0), 'bytes of chunk data (+', chunks.length * 4, 'bytes of length prefixes)');
