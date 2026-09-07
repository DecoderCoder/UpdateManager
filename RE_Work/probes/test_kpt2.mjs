// L=0 known-plaintext: staging bodies are pure ciphertext.
// hypo_stg_details_pt = public details with channel swapped; hypo_stg_upd_pt = public update with channel swapped.
// 1) Verify same-keystream within channel: ks_upd == ks_det[0:198]
// 2) Test candidate keys: for IV hypotheses, E_K(ctr0) vs ks[0:16]
import { readFileSync } from "node:fs";
import crypto from "node:crypto";

const pubUpd = readFileSync("RE_Work/probes/reprobe_channel_names_2026-09-07/public_update.body");
const pubDet = readFileSync("RE_Work/probes/reprobe_channel_names_2026-09-07/public_details.body");
const stgUpd = readFileSync("RE_Work/probes/reprobe_channel_brute_2026-09-07/staging_update.body");
const stgDet = readFileSync("RE_Work/probes/reprobe_channel_brute_2026-09-07/staging_details.body");

function swapChannel(buf, from, to) {
  const s = buf.toString("utf8");
  const f = `"channel": "${from}"`;
  const i = s.indexOf(f);
  if (i < 0) throw new Error("channel field not found in " + from);
  return Buffer.from(s.slice(0, i) + `"channel": "${to}"` + s.slice(i + f.length), "utf8");
}

const hypoUpd = swapChannel(pubUpd, "public", "staging");
const hypoDet = swapChannel(pubDet, "public", "staging");
console.log("hypo lengths:", hypoUpd.length, stgUpd.length, hypoDet.length, stgDet.length);
if (hypoUpd.length !== stgUpd.length || hypoDet.length !== stgDet.length) throw new Error("length mismatch");

function xor(a, b) {
  const n = Math.min(a.length, b.length);
  const out = Buffer.alloc(n);
  for (let i = 0; i < n; i++) out[i] = a[i] ^ b[i];
  return out;
}

const ksUpd = xor(stgUpd, hypoUpd); // 198 B
const ksDet = xor(stgDet, hypoDet); // 938800 B
const same = ksUpd.equals(ksDet.subarray(0, 198));
console.log("same keystream update vs details[0:198]:", same);
console.log("ks[0:32]:", ksDet.subarray(0, 32).toString("hex"));

// If not same, find first divergence
if (!same) {
  let i = 0;
  while (i < 198 && ksUpd[i] === ksDet[i]) i++;
  console.log("keystream divergence at", i);
}

// ---- key candidates ----
const TOKENS = [
  ["e5fa04de24153837ce00d64e133a3be2", "q8d2Uz5cVYm"],
  ["1ad7bf2fb3fa4f2010f9ad9491977cfb", "WMewgzbA8KK"],
  ["b811e77fe061c9daeb13245391c4d9ae", "mApwm6FUvFG"],
  ["170d2d1f9153bf5117acbf2dd932c6d4", "EeJW4hM4lBE"],
  ["98120664d3c63d9e70fdac56826d95a5", "J2PD4QxHnvz"],
  ["c266f32f0fd8d0de16dafc65257bb2b4", "xjFvOew3fg5"],
];
const KEYNAME_UUID = "b7d2e981-0ca6-4806-91e6-0cbb3bdb94d6";

// access-group keys (71) from round 4
const accessKeys = [];
try {
  const ak = readFileSync("RE_Work/probes/probe_access_ghub10_group_content.out", "utf8");
  // each line/entry contains a 32-hex key id + base64 key at +0x18; simplest: extract all 32-hex and 16-byte-b64 pairs
  // fallback: skip if format unknown
} catch {}

function ctrBlock0(key, iv16, bits) {
  return crypto.createCipheriv(`aes-${bits}-ctr`, key, iv16).update(Buffer.alloc(16));
}
function gcmBlock0(key, iv, bits) {
  if (iv.length === 12) {
    const ctr = Buffer.concat([iv, Buffer.from([0, 0, 0, 1])]);
    return crypto.createCipheriv(`aes-${bits}-ctr`, key, ctr).update(Buffer.alloc(16));
  }
  // GHASH J0 for 32-byte IV
  const H = crypto.createCipheriv(`aes-${bits}-ctr`, key, Buffer.alloc(16)).update(Buffer.alloc(16));
  const R = Buffer.from([0xe1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
  function gfMul(X, Y) {
    let Z = Buffer.alloc(16);
    let V = Buffer.from(Y);
    for (let i = 0; i < 128; i++) {
      const xbit = (X[(127 - i) >> 3] >> ((127 - i) & 7)) & 1;
      const vlsb = V[15] & 1;
      if (xbit) for (let j = 0; j < 16; j++) Z[j] ^= V[j];
      V = Buffer.from([...V.slice(1), 0]);
      if (vlsb) for (let j = 0; j < 16; j++) V[j] ^= R[j];
    }
    return Z;
  }
  let ivPad = Buffer.from(iv);
  const rem = iv.length % 16;
  if (rem) ivPad = Buffer.concat([ivPad, Buffer.alloc(16 - rem)]);
  const lenBlock = Buffer.alloc(16);
  lenBlock.writeBigUInt64BE(BigInt(iv.length * 8), 8);
  let Y = Buffer.alloc(16);
  for (let i = 0; i < ivPad.length; i += 16) {
    const block = ivPad.subarray(i, i + 16);
    for (let j = 0; j < 16; j++) Y[j] ^= block[j];
    Y = gfMul(Y, H);
  }
  for (let j = 0; j < 16; j++) Y[j] ^= lenBlock[j];
  const J0 = gfMul(Y, H);
  const J01 = Buffer.from(J0);
  for (let i = 15; i >= 12; i--) { J01[i] = (J01[i] + 1) & 0xff; if (J01[i] !== 0) break; }
  return crypto.createCipheriv(`aes-${bits}-ctr`, key, J01).update(Buffer.alloc(16));
}

const ks = ksDet; // details keystream (longer)
const ksU = ksUpd;
let hits = 0;

const keyList = [];
for (const [hex16, b64x] of TOKENS) {
  const k16 = Buffer.from(hex16, "hex");
  const k24 = Buffer.concat([k16, Buffer.from(b64x, "base64")]);
  keyList.push([hex16.slice(0, 8) + "-K16", k16, 128]);
  keyList.push([hex16.slice(0, 8) + "-K24", k24, 192]);
  // also: maybe key is the b64 part decoded to 8 bytes + something? skip (invalid size)
  // maybe key = hex16 as a *password* -> SHA-256 -> 32B key
  keyList.push([hex16.slice(0, 8) + "-sha256K32", crypto.createHash("sha256").update(k16).digest(), 256]);
}

const PWDS = ["staging", "public", "ghub10", "win", "ghub10/win/staging", "staging_details", "details.json", "update.json", "staging/update.json", "staging/details.json"];
const SALTS = [KEYNAME_UUID, "staging", "ghub10", "win"];
const iters = [1, 1000, 10000];

// IV hypotheses
const ivHypo = [];
// zero IV
ivHypo.push(["zero16", Buffer.alloc(16)]);
ivHypo.push(["zero32", Buffer.alloc(32)]);
// sha256 of simple strings
for (const s of ["staging", "ghub10", KEYNAME_UUID, "staging/ghub10", "ghub10/staging"]) {
  ivHypo.push(["sha256:" + s.slice(0, 10), crypto.createHash("sha256").update(s).digest()]);
}
// PBKDF2 matrix
for (const pwd of PWDS) for (const salt of SALTS) for (const it of iters) {
  for (const len of [12, 16, 32]) {
    ivHypo.push([`pbkdf2:${pwd}/${salt.slice(0, 8)}/${it}/${len}`, crypto.pbkdf2Sync(pwd, salt, it, len, "sha512")]);
  }
}
console.log("iv hypotheses:", ivHypo.length, " keys:", keyList.length);

for (const [kl, kval, kbits] of keyList) {
  for (const [il, iv] of ivHypo) {
    // CTR
    const c0 = ctrBlock0(kval, iv.length >= 16 ? iv.subarray(0, 16) : Buffer.concat([iv, Buffer.alloc(16 - iv.length)]), kbits);
    if (c0.equals(ks.subarray(0, 16))) {
      // verify block 1
      const iv1 = Buffer.from(iv.subarray(0, 16));
      iv1.writeUInt32BE((iv1.readUInt32BE(12) + 1) >>> 0, 12);
      const c1 = crypto.createCipheriv(`aes-${kbits}-ctr`, kval, iv1).update(Buffer.alloc(16));
      const ok1 = c1.equals(ks.subarray(16, 32));
      // cross-check with update keystream
      const okU = ok1 && c0.equals(ksU.subarray(0, 16));
      console.log(`*** CTR HIT ${kl} ${il} block1ok=${ok1} upd-ok=${okU}`);
      hits++;
    }
    // GCM
    const g0 = gcmBlock0(kval, iv, kbits);
    if (g0.equals(ks.subarray(0, 16))) {
      console.log(`*** GCM HIT ${kl} ${il}`);
      hits++;
    }
  }
}
console.log("total hits:", hits);
