// Per-chunk AES-128-GCM attempts + header-sha hypothesis check.
import fs from "node:fs";
import { createHash } from "node:crypto";

const dir = "C:/Users/Decode/source/repos/UpdateManager/RE_Work/probes";
const depotName = "85875e86-f3e1-4e79-91ee-232575e2807f";
const buf = fs.readFileSync(`${dir}/depot_full_${depotName}.bin`);
const dv = new DataView(buf.buffer);
const jsonLen = dv.getUint32(4, true);
const hdrEnd = 8 + jsonLen;
const headerJson = new TextDecoder().decode(buf.subarray(8, hdrEnd));
const hdr = JSON.parse(headerJson);
console.log("header:", headerJson);

// ---- header-sha hypothesis checks ----
const want = hdr["header-sha"];
const cands = {
  "payload[hdrEnd+4..EOF)": buf.subarray(hdrEnd + 4),
  "payload[hdrEnd..EOF)": buf.subarray(hdrEnd),
  "headerJSON[8..140)": buf.subarray(8, hdrEnd),
  "file[144..EOF) w/o lens? no": buf.subarray(144),
  "headerJSON no-space": Buffer.from(headerJson.replace(/\s/g, "")),
  "magic+headerJSON": buf.subarray(0, hdrEnd),
};
for (const [name, data] of Object.entries(cands)) {
  if (!data) continue;
  const h = createHash("sha256").update(data).digest("hex");
  console.log(`sha256(${name}) = ${h} ${h === want ? "  <<< MATCH" : ""}`);
}
// also: sha256 of concatenated chunk lengths (LE u32) and of (len+data) without per-chunk len words
let lens = Buffer.alloc(0); let bodyNoLens = Buffer.alloc(0);
{
  let off = hdrEnd;
  while (off + 4 <= buf.length) {
    const len = dv.getUint32(off, true);
    lens = Buffer.concat([lens, Buffer.from([len & 255, (len >> 8) & 255, (len >> 16) & 255, (len >> 24) & 255])]);
    bodyNoLens = Buffer.concat([bodyNoLens, buf.subarray(off + 4, off + 4 + len)]);
    off += 4 + len;
  }
}
for (const [name, data] of [["chunkLens", lens], ["bodyNoLens", bodyNoLens], ["chunkLens+body", Buffer.concat([lens, bodyNoLens])]]) {
  const h = createHash("sha256").update(data).digest("hex");
  console.log(`sha256(${name}) = ${h} ${h === want ? "  <<< MATCH" : ""}`);
}

// ---- chunks ----
const chunks = [];
{
  let off = hdrEnd;
  while (off + 4 <= buf.length) {
    const len = dv.getUint32(off, true);
    chunks.push({ off, data: buf.subarray(off + 4, off + 4 + len) });
    off += 4 + len;
  }
}
console.log("chunks:", chunks.map(c => c.data.length).join(","));

const content = JSON.parse(fs.readFileSync(`${dir}/probe_access_ghub10_group_content.out`, "utf8"));
const k = content.keys.find(x => x.name === hdr["key-id"]);
const keyBytes = Uint8Array.from(atob(k.key), c => c.charCodeAt(0));

const depotDirUuid = "f71ec3de-fb90-4b7d-8b2c-afa4f903b6e2"; // from manifest url
const manifest = JSON.parse(fs.readFileSync(`${dir}/resp_pipeline_v2_update_ghub10_win_public_details.json.json`, "utf8"));
const d = manifest.depots.find(x => x.name === depotName);
const depotDirUuid2 = d.url.match(/\/depots\/([0-9a-f-]+)\//)[1];

const PWS = [
  ["base64keystr", new TextEncoder().encode(k.key)],
  ["keyname", new TextEncoder().encode(hdr["key-id"])],
  ["keybytes", keyBytes],
  ["depotName", new TextEncoder().encode(depotName)],
  ["depotDirUuid", new TextEncoder().encode(depotDirUuid2)],
  ["empty", new TextEncoder().encode("")],
];
const SALTS = [
  ["keyname", new TextEncoder().encode(hdr["key-id"])],
  ["keyval", new TextEncoder().encode(k.key)],
  ["depotName", new TextEncoder().encode(depotName)],
];

async function tryGcm(pw, salt, data, label) {
  try {
    const ikm = await crypto.subtle.importKey("raw", pw, "PBKDF2", false, ["deriveKey"]);
    const key = await crypto.subtle.deriveKey(
      { name: "PBKDF2", hash: "SHA-512", salt, iterations: 1000 },
      ikm, { name: "AES-GCM", length: 128 }, false, ["decrypt"]);
    const ikm2 = await crypto.subtle.importKey("raw", pw, "PBKDF2", false, ["deriveBits"]);
    const iv = new Uint8Array(await crypto.subtle.deriveBits(
      { name: "PBKDF2", hash: "SHA-512", salt, iterations: 1000 }, ikm2, 256));
    const plain = await crypto.subtle.decrypt({ name: "AES-GCM", iv, additionalData: new Uint8Array(0) }, key, data);
    console.log(`SUCCESS ${label} (${data.byteLength} -> ${plain.byteLength}):`);
    console.log(Buffer.from(plain).toString("utf8").slice(0, 400).replace(/\n/g, "\\n"));
    fs.writeFileSync(`${dir}/decrypted_${depotName}_${label.replace(/[^a-z0-9]/gi, "_")}.bin`, Buffer.from(plain));
    return true;
  } catch { return false; }
}

let ok = false;
outer:
for (const [sl, salt] of SALTS) {
  for (const [pl, pw] of PWS) {
    // also per-chunk index passwords
    const pwVariants = [[pl, pw]];
    for (let i = 0; i < 9; i++) pwVariants.push([`idx${i}`, new TextEncoder().encode(String(i))]);
    for (const [vl, pv] of pwVariants) {
      const ci = 0;
      if (await tryGcm(pv, salt, chunks[ci].data, `${vl}+salt_${sl}_chunk0`)) { ok = true; break outer; }
    }
  }
}
if (!ok) console.log("no success on chunk 0 with candidates");
// if chunk0 fails, try each chunk with base pw/salt to see if ANY chunk decrypts
if (!ok) {
  const [pl, pw] = PWS[0]; const [sl, salt] = SALTS[0];
  for (let i = 1; i < chunks.length; i++) {
    if (await tryGcm(pw, salt, chunks[i].data, `base_chunk${i}`)) { ok = true; break; }
  }
}
console.log(ok ? "DECRYPTION ACHIEVED" : "still no success");
