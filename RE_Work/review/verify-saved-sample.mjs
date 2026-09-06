// Offline evidence check. Reads Qwen's saved fixtures; writes nothing.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createHash, createDecipheriv, pbkdf2Sync } from 'node:crypto';

const read = name => readFileSync(new URL(`../probes/${name}`, import.meta.url));
const json = name => JSON.parse(read(name));
const sha256 = bytes => createHash('sha256').update(bytes).digest('hex');
const manifest = json('resp_pipeline_v2_update_ghub10_win_public_details.json.json');

for (const name of ['driver_audio_osx', 'g560_dfu', 'applet_slobs', 'lua_scripting', 'release_notes']) {
  const bytes = read(`depot_${name}.bin`);
  const entry = manifest.depots.find(d => d.name === name);
  assert.equal(bytes.length, entry.size, `${name}: size`);
  assert.equal(sha256(bytes), entry.mac, `${name}: raw SHA-256`);
  assert.equal(bytes.readUInt32LE(0), 0x20170110);
}
console.log('PASS: five plaintext depot sizes and raw SHA-256 values');

const name = '85875e86-f3e1-4e79-91ee-232575e2807f';
const bytes = read(`depot_full_${name}.bin`);
const entry = manifest.depots.find(d => d.name === name);
assert.equal(bytes.length, entry.size);
assert.equal(sha256(bytes), entry.mac);
assert.equal(bytes.readUInt32LE(0), 0x20210506);
let offset = 8 + bytes.readUInt32LE(4);
assert.ok(offset <= bytes.length);
const header = JSON.parse(bytes.subarray(8, offset));
const chunks = [];
while (offset < bytes.length) {
  assert.ok(offset + 4 <= bytes.length, 'chunk length is present');
  const length = bytes.readUInt32LE(offset);
  offset += 4;
  assert.ok(length > 0 && offset + length <= bytes.length, 'chunk fits');
  chunks.push(bytes.subarray(offset, offset + length));
  offset += length;
}
const key = json('probe_access_ghub10_group_content.out').keys.find(k => k.name === header['key-id']);
assert.ok(key, 'header key-id exists in saved keymaster response');
const rawKey = Buffer.from(key.key, 'base64');
assert.equal(rawKey.length, 16);

function decodeAndCheck(ciphertext, expectedSha) {
  const iv = pbkdf2Sync(expectedSha, key.name, 1000, 32, 'sha512');
  const decoder = createDecipheriv('aes-128-gcm', rawKey, iv);
  // Reproduce the existing C++ stream transform for this saved sample only.
  // No GCM tag is supplied: plaintext is checked against the saved SHA-256.
  // This is NOT authenticated GCM decryption or a production extraction API.
  const plaintext = decoder.update(ciphertext);
  assert.equal(sha256(plaintext), expectedSha, 'decoded plaintext SHA-256');
  assert.throws(() => decoder.final(), 'GCM authentication is not established');
  return plaintext;
}

const inner = JSON.parse(decodeAndCheck(chunks[0], header['header-sha']));
assert.equal(inner.files.length, 8);
assert.equal(chunks.length, inner.files.length + 1);
for (const [index, file] of inner.files.entries()) {
  decodeAndCheck(chunks[index + 1], file.sha);
}
console.log('PASS: encrypted depot size/hash, header, and all eight file SHA-256 values');
console.log('LIMIT: saved-sample consistency only; GCM tags and manifest signatures not verified');
