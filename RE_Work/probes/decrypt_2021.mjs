// Live decrypt attempt for 0x20210506-format depots.
// Recipe (from lghub_updater DecryptedStream @ 0x140272FB0):
//   AES key = base64decode(keymaster Key.key)            (16 bytes)
//   IV(32)  = PBKDF2_HMAC_SHA512(password=?, salt=Key.name, c=1000, dkLen=32)
//   plain   = AES-128-GCM(key, iv, cipher||tag), AAD empty, tag check done by us
import fs from "node:fs";
import { createHash } from "node:crypto";

const dir = "C:/Users/Decode/source/repos/UpdateManager/RE_Work/probes";
const UA = "LGHUB/2026.6.957899 (Windows NT 10.0; x64)";
const depotName = "85875e86-f3e1-4e79-91ee-232575e2807f";

const manifest = JSON.parse(fs.readFileSync(`${dir}/resp_pipeline_v2_update_ghub10_win_public_details.json.json`, "utf8"));
const d = manifest.depots.find(x => x.name === depotName);
console.log("manifest:", d.name, d.url, "size", d.size, "mac", d.mac);

// --- fetch full depot ---
let buf;
for (let i = 0; i < 3; i++) {
  const r = await fetch("https://updates.ghub.logitechg.com" + d.url, { headers: { "User-Agent": UA } });
  console.log("fetch HTTP", r.status);
  if (r.status === 200) { buf = new Uint8Array(await r.arrayBuffer()); break; }
  await new Promise(res => setTimeout(res, 800));
}
if (!buf) { console.log("fetch failed"); process.exit(1); }
fs.writeFileSync(`${dir}/depot_full_${depotName}.bin`, buf);
console.log("saved", buf.length, "bytes; mac check:", d.mac, "match?", macOf(buf) === d.mac);

// --- parse header ---
const dv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
const magic = dv.getUint32(0, true);
console.log("magic 0x" + magic.toString(16));
const jsonLen = dv.getUint32(4, true);
const json = new TextDecoder().decode(buf.subarray(8, 8 + jsonLen));
console.log("header JSON:", json);
const hdr = JSON.parse(json);
const hdrEnd = 8 + jsonLen;
console.log("headerEnd =", hdrEnd);
// dump words after header
for (let off = hdrEnd; off < Math.min(hdrEnd + 64, buf.length); off += 4) {
  console.log("  u32 @" + off + " = 0x" + dv.getUint32(off, true).toString(16) + " (" + dv.getUint32(off, true) + ")");
}

// --- keymaster key ---
const content = JSON.parse(fs.readFileSync(`${dir}/probe_access_ghub10_group_content.out`, "utf8"));
const k = content.keys.find(x => x.name === hdr["key-id"]);
if (!k) { console.log("key-id NOT in keymaster!"); process.exit(1); }
console.log("keymaster key:", k.name, k.key);
const aesKeyBytes = Uint8Array.from(atob(k.key), c => c.charCodeAt(0));
console.log("aes key bytes:", aesKeyBytes.length);

// --- PBKDF2 via WebCrypto (SHA-512, 1000 iters, 32 bytes) ---
async function derive(passwordBytes, saltStr) {
  const ikm = await crypto.subtle.importKey("raw", passwordBytes, "PBKDF2", false, ["deriveKey"]);
  return crypto.subtle.deriveKey(
    { name: "PBKDF2", hash: "SHA-512", salt: new TextEncoder().encode(saltStr), iterations: 1000 },
    ikm, { name: "AES-GCM", length: 128 }, false, ["decrypt"]);
}

async function tryDecrypt(keyObj, iv, data, label) {
  try {
    const plain = await crypto.subtle.decrypt({ name: "AES-GCM", iv, additionalData: new Uint8Array(0) }, keyObj, data);
    console.log("SUCCESS", label, "->", plain.byteLength, "bytes");
    console.log("  head:", Buffer.from(plain).subarray(0, 200).toString("utf8").replace(/\n/g, "\\n"));
    fs.writeFileSync(`${dir}/decrypted_${depotName}${label.replace(/[^a-z0-9]/gi, "_")}.bin`, new Uint8Array(plain));
    return true;
  } catch (e) {
    console.log("fail  ", label, "-", e.message);
    return false;
  }
}

// payload region interpretations
const p0 = dv.getUint32(hdrEnd, true);           // first u32 after JSON
console.log("payloadLen word after JSON =", p0, "(0x" + p0.toString(16) + ")");

const candidates = [
  ["pw=base64keystr", new TextEncoder().encode(k.key)],
  ["pw=keyname", new TextEncoder().encode(hdr["key-id"])],
  ["pw=keybytes", aesKeyBytes],
];
// resource entries: [u32 entryLen][JSON entry header][entryLen-JSON bytes: cipher(+tag?)]
const entries = [];
let off = hdrEnd + 4;
while (off + 4 <= buf.length) {
  const entryLen = dv.getUint32(off, true);
  if (entryLen === 0 || off + 4 + entryLen > buf.length) { console.log("layout break @", off, "len", entryLen); break; }
  const jsonStart = off + 4;
  const json = new TextDecoder().decode(buf.subarray(jsonStart, Math.min(jsonStart + 512, off + 4 + entryLen)));
  const m = json.match(/^\{.*?\}(?=,|\s*$)/);
  entries.push({ off, entryLen, json: json.slice(0, 300) });
  off += 4 + entryLen;
}
console.log("entries:", entries.length);
for (const e of entries.slice(0, 20)) console.log("  @" + e.off + " len=" + e.entryLen, e.json.replace(/\n/g, " "));

async function deriveRaw(pw, saltStr) {
  const ikm = await crypto.subtle.importKey("raw", pw, "PBKDF2", false, ["deriveBits"]);
  return new Uint8Array(await crypto.subtle.deriveBits({ name: "PBKDF2", hash: "SHA-512", salt: new TextEncoder().encode(saltStr), iterations: 1000 }, ikm, 256));
}
async function makeGcmKey(pw, saltStr) {
  const ikm = await crypto.subtle.importKey("raw", pw, "PBKDF2", false, ["deriveKey"]);
  return crypto.subtle.deriveKey({ name: "PBKDF2", hash: "SHA-512", salt: new TextEncoder().encode(saltStr), iterations: 1000 }, ikm, { name: "AES-GCM", length: 128 }, false, ["decrypt"]);
}

const salts = [["salt=keyname", hdr["key-id"]]];
if (k) salts.push(["salt=keyval", k.key]);

let found = false;
for (const [saltLabel, salt] of salts) {
  for (const [pwLabel, pw] of candidates) {
    const label = pwLabel + "+" + saltLabel;
    let keyObj, ivBytes;
    try {
      ivBytes = await deriveRaw(pw, salt);
      keyObj = await makeGcmKey(pw, salt);
    } catch (e) { console.log("derive fail", label, e.message); continue; }
    // per entry: data after JSON header within entry
    for (let i = 0; i < Math.min(entries.length, 4); i++) {
      const e = entries[i];
      const dataStart = e.off + 4;
      // find end of JSON entry header
      let jsEnd = dataStart;
      let depth = 0, inStr = false, esc = false;
      for (let j = dataStart; j < e.off + 4 + e.entryLen; j++) {
        const c = buf[j];
        if (inStr) { if (esc) esc = false; else if (c === 0x5c) esc = true; else if (c === 0x22) inStr = false; continue; }
        if (c === 0x22) { inStr = true; continue; }
        if (c === 0x7b) depth++;
        if (c === 0x7d) { depth--; if (depth === 0) { jsEnd = j + 1; break; } }
      }
      const blob = buf.subarray(jsEnd, e.off + 4 + e.entryLen);
      const variants = [
        ["blob", blob],
        ["blob+4", buf.subarray(jsEnd, e.off + 4 + e.entryLen + 4)],
      ];
      for (const [vLabel, v] of variants) {
        if (await tryDecrypt(keyObj, ivBytes, v, `${label}_e${i}_${vLabel}`)) found = true;
      }
      if (found) break;
    }
    if (found) break;
  }
  if (found) break;
}
console.log("done", found ? "WITH SUCCESS" : "(no success)");

function macOf(u8) {
  return createHash("sha256").update(u8).digest("hex");
}
