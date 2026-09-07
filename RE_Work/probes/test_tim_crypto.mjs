// Test whether the named-channel ("tim") pipeline bodies are AES-128-GCM/CTR
// with a leading nonce/IV and keys taken from the ghub10 access-group.
// Structs tested (per body):
//   A: [12B nonce][ciphertext][16B tag]  AES-128-GCM
//   B: [16B iv][ciphertext]              AES-128-CTR
//   C: [16B nonce][ciphertext][16B tag]  AES-128-GCM (16B nonce)
//   D: [32B iv][ciphertext]              AES-128-CTR (16B iv = first half)
// Prints any plaintext prefix that looks like JSON ('{').
import { readFileSync } from "node:fs";
import crypto from "node:crypto";

const dir = "RE_Work/probes/reprobe_channel_names_2026-09-07";
const keysDoc = JSON.parse(readFileSync("RE_Work/probes/probe_access_ghub10_group_content.out", "utf8"));
const keys = (keysDoc.keys ?? []).map((k) => Buffer.from(k.key, "base64"));
console.log(`loaded ${keys.length} access-group keys (accessGroup=${keysDoc.accessGroup})`);

const bodies = {};
for (const name of ["tim_details", "tim_update"]) {
  bodies[name] = readFileSync(`${dir}/${name}.body`);
  console.log(`${name}: ${bodies[name].length} bytes`);
}

function looksLikeJson(buf, n = 64) {
  const head = buf.subarray(0, n).toString("latin1");
  return head.startsWith("{") || head.startsWith("\n{") || head.includes('"appId"');
}

function tryMode(name, buf, key, ivLen, tagLen, mode) {
  if (buf.length < ivLen + tagLen + 16) return null;
  const iv = buf.subarray(0, ivLen);
  const ct = buf.subarray(ivLen, buf.length - tagLen);
  const tag = tagLen ? buf.subarray(buf.length - tagLen) : undefined;
  try {
    if (mode === "gcm") {
      const d = crypto.createDecipheriv("aes-128-gcm", key, iv);
      if (tag) d.setAuthTag(tag);
      const pt = Buffer.concat([d.update(ct), d.final()]);
      if (tag && !d.getAuthTag()) return null;
      return pt;
    } else {
      const d = crypto.createDecipheriv("aes-128-ctr", key, iv.subarray(0, 16));
      return Buffer.concat([d.update(ct), d.final()]);
    }
  } catch {
    return null;
  }
}

const modes = [
  ["A: gcm nonce12 + tag16", 12, 16, "gcm"],
  ["B: ctr iv16 no tag", 16, 0, "ctr"],
  ["C: gcm nonce16 + tag16", 16, 16, "gcm"],
  ["D: ctr iv32(first16) no tag", 32, 0, "ctr"],
];

for (const [mname, ivLen, tagLen, mode] of modes) {
  for (const [bname, buf] of Object.entries(bodies)) {
    let hit = -1;
    for (let i = 0; i < keys.length; i++) {
      const pt = tryMode(mname, buf, keys[i], ivLen, tagLen, mode);
      if (pt && looksLikeJson(pt)) { hit = i; console.log(`*** HIT ${mname} ${bname} key#${i} -> ${pt.subarray(0, 120).toString("latin1").replace(/\n/g, "\\n")}`); break; }
    }
    if (hit < 0) console.log(`-- ${mname} ${bname}: no key produced JSON`);
  }
}

// Also: what if the plaintext prefix is not JSON but the depot-style envelope?
// XOR the first 16 bytes of tim_update against candidate plaintexts and print
// implied keystream, in case an analyst wants to match against a known AES block.
const u = bodies.tim_update;
console.log("\nimplied keystream vs '{\\n  \"appId\": \"ghub' (first 16B):");
const guess = Buffer.from('{\n  "appId": "ghu');
console.log(Array.from(u.subarray(0, 16).map((b, i) => b ^ guess[i])).map((b) => b.toString(16).padStart(2, "0")).join(" "));
