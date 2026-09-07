// Tag-layout investigation for the 0x20210506 encrypted-capsule depot.
//
// Fixes over the previous version of this script:
//   1. The salt loop destructured each salt *string* into characters
//      (`for (const [sn, salt] of salts)`), collapsing the PBKDF2 salt to
//      one byte. Salts are now iterated as plain strings.
//   2. WebCrypto `subtle.decrypt(algorithm, key, data)` is three-argument;
//      a fourth "tag" argument is silently ignored, so the previous
//      tag experiments were void. Per the WebCrypto convention the tag is
//      the LAST 16 bytes of `data`; tag-first layouts are handled by
//      reassembling the buffer instead.
//   3. Each file's IV is now derived from that file's own SHA
//      (chunk i+1 <- files[i].sha), as the C++ implementation does
//      (Manager/UpdateManager/UpdateManager.cpp:817), instead of reusing
//      the chunk-0 IV for everything.
//
// Baseline: `node RE_Work/review/verify-saved-sample.mjs` (PASS without
// any GCM tag). This script answers the open question: does either
// tag layout (last-16 / first-16) authenticate? Offline only, no network.
//
// Usage: node RE_Work/probes/decrypt_headersha.mjs
import { readFileSync } from "node:fs";
import { createHash, createDecipheriv, pbkdf2Sync } from "node:crypto";

const read = (name) => readFileSync(new URL(`./${name}`, import.meta.url));
const sha256hex = (buf) => createHash("sha256").update(buf).digest("hex");

// --- load depot + header + keymaster key ---
const bin = read("depot_full_85875e86-f3e1-4e79-91ee-232575e2807f.bin");
const magic = bin.readUInt32LE(0);
const jsonLen = bin.readUInt32LE(4);
const hdr = JSON.parse(bin.subarray(8, 8 + jsonLen).toString("utf8"));
console.log("magic=0x" + magic.toString(16), "header:", JSON.stringify(hdr));

const content = JSON.parse(read("probe_access_ghub10_group_content.out"));
const keyEntry = content.keys.find((k) => k.name === hdr["key-id"]);
if (!keyEntry) throw new Error("key-id not in saved keymaster fixture");
const keyBytes = Buffer.from(keyEntry.key, "base64");
console.log("key:", keyEntry.name, "b64 len", keyEntry.key.length, "raw bytes", keyBytes.length);

// --- walk length-prefixed chunks ---
const chunks = [];
let off = 8 + jsonLen;
while (off + 4 <= bin.length) {
  const len = bin.readUInt32LE(off);
  if (len === 0 || off + 4 + len > bin.length) { console.log("chain end/err at off", off, "len", len); break; }
  chunks.push(bin.subarray(off + 4, off + 4 + len));
  off += 4 + len;
}
console.log("chunks:", chunks.map((c) => c.length).join(","), "| consumed to", off, "of", bin.length);

// --- crypto primitives ---
const subtleAesKey = (await crypto.subtle.importKey("raw", keyBytes, "AES-GCM", false, ["decrypt"]));
const deriveIv = (shaHex, salt) => pbkdf2Sync(shaHex, salt, 1000, 32, "sha512"); // 32-byte IV, per C++
const salt = keyEntry.name;
const aad = new Uint8Array(0);

// WebCrypto: data must be ciphertext || tag(16). Build per layout.
const layouts = [
  ["tag-last16", (d) => new Uint8Array(d)],                 // as-is
  ["tag-first16", (d) => { const out = new Uint8Array(d.length); out.set(d.subarray(16), 0); out.set(d.subarray(0, 16), d.length - 16); return out; }],
];

async function tryAuth(chunk, iv) {
  for (const [layout, build] of layouts) {
    try {
      const pt = await crypto.subtle.decrypt(
        { name: "AES-GCM", iv: new Uint8Array(iv), additionalData: aad, tagLength: 128 },
        subtleAesKey, build(chunk));
      return { layout, pt: Buffer.from(pt) };
    } catch { /* layout failed auth */ }
  }
  return null;
}

// --- step 1: authenticate chunk0 (IV from header-sha) under each layout ---
const iv0 = deriveIv(hdr["header-sha"], salt);
console.log("\n== chunk0 (IV from header-sha) ==");
let files = null, usedLayout = null;
for (const { layout, build } of layouts.map(([l, b]) => ({ layout: l, build: b }))) {
  try {
    const pt = Buffer.from(await crypto.subtle.decrypt(
      { name: "AES-GCM", iv: new Uint8Array(iv0), additionalData: aad, tagLength: 128 },
      subtleAesKey, build(chunks[0])));
    const j = JSON.parse(pt.toString("utf8"));
    console.log(`${layout}: AUTHENTICATED, files=${j.files.length}`);
    files = j.files; usedLayout = layout;
  } catch (e) {
    console.log(`${layout}: auth failed (${e.name})`);
  }
}

// --- step 2: per-file authenticated decrypt (IV from files[i].sha) ---
if (files) {
  console.log(`\n== per-file chunks with layout '${usedLayout}' (IV from files[i].sha) ==`);
  const build = layouts.find(([l]) => l === usedLayout)[1];
  for (let i = 0; i < files.length; i++) {
    const iv = deriveIv(files[i].sha, salt);
    try {
      const pt = Buffer.from(await crypto.subtle.decrypt(
        { name: "AES-GCM", iv: new Uint8Array(iv), additionalData: aad, tagLength: 128 },
        subtleAesKey, build(chunks[i + 1])));
      const ok = sha256hex(pt) === files[i].sha;
      console.log(`chunk${i + 1} ${files[i].name}: authenticated, sha-match=${ok} (${pt.length} B)`);
    } catch {
      console.log(`chunk${i + 1} ${files[i].name}: auth FAILED`);
    }
  }
}

// --- step 3: unauthenticated diagnostic (no tag supplied at all) ---
console.log("\n== unauthenticated diagnostic (baseline recipe, no GCM tag) ==");
let diagFiles = files;
if (!diagFiles) {
  // No authenticated pass: recover the files list without auth (baseline
  // recipe always recovers plaintext that matches the expected SHAs).
  const iv = deriveIv(hdr["header-sha"], salt);
  const d = createDecipheriv("aes-128-gcm", keyBytes, iv);
  diagFiles = JSON.parse(d.update(chunks[0]).toString("utf8")).files;
}
const expected = [hdr["header-sha"], ...diagFiles.map((f) => f.sha)];
let allMatch = true;
for (let i = 0; i < chunks.length; i++) {
  const iv = deriveIv(expected[i], salt);
  const d = createDecipheriv("aes-128-gcm", keyBytes, iv);
  const pt = d.update(chunks[i]);
  let finalOk = false;
  try { d.final(); finalOk = true; } catch { finalOk = false; }
  const match = sha256hex(pt) === expected[i];
  allMatch = allMatch && match;
  console.log(`chunk${i}: plaintext sha-match=${match}, final() ok=${finalOk} (${pt.length} B)`);
}
console.log(allMatch ? "DIAG: all plaintexts match expected SHAs without any tag" : "DIAG: mismatch in unauthenticated pass");

// --- verdict ---
console.log("\n== verdict ==");
if (usedLayout) console.log(`GCM tag FOUND at ${usedLayout}: decryption is authenticated.`);
else console.log("GCM tag NOT found in last-16 or first-16 layout: the saved stream does not contain a verifiable 16-byte tag in either position; the C++ reference likewise never supplies one. GCM is effectively used unauthenticated for this sample (or the tag lives elsewhere — still open).");
