// Known-plaintext attack on named-channel manifest encryption.
// hypo: staging_details plaintext == public_details plaintext with channel "public"->"staging"
// Recover keystream for in-band IV lengths L in {0,12,16,32}, then verify candidate keys
// by checking AES-CTR/GCM block 0 (and following blocks) against recovered keystream.
import { readFileSync } from "node:fs";
import crypto from "node:crypto";

const pub = readFileSync("RE_Work/probes/reprobe_channel_names_2026-09-07/public_details.body");
const stg = readFileSync("RE_Work/probes/reprobe_channel_brute_2026-09-07/staging_details.body");

// build hypothesized plaintext
let hypo = "";
{
  const s = pub.toString("utf8");
  const i = s.indexOf('"channel": "public"');
  if (i < 0) throw new Error("channel field not found");
  hypo = s.slice(0, i) + '"channel": "staging"' + s.slice(i + '"channel": "public"'.length);
}
const hypoBuf = Buffer.from(hypo, "utf8");
console.log("hypo pt len:", hypoBuf.length, "staging ct len:", stg.length, "match:", hypoBuf.length === stg.length);

const TOKENS = [
  ["e5fa04de24153837ce00d64e133a3be2", "q8d2Uz5cVYm"],
  ["1ad7bf2fb3fa4f2010f9ad9491977cfb", "WMewgzbA8KK"],
  ["b811e77fe061c9daeb13245391c4d9ae", "mApwm6FUvFG"],
  ["170d2d1f9153bf5117acbf2dd932c6d4", "EeJW4hM4lBE"],
  ["98120664d3c63d9e70fdac56826d95a5", "J2PD4QxHnvz"],
  ["c266f32f0fd8d0de16dafc65257bb2b4", "xjFvOew3fg5"],
];
const KEYNAME_UUID = "b7d2e981-0ca6-4806-91e6-0cbb3bdb94d6";

function ctrBlocks(key, iv16, nBlocks) {
  // produce nBlocks * 16 bytes of CTR keystream (counter in last 4 bytes BE, start 0)
  const out = [];
  for (let i = 0; i < nBlocks; i++) {
    const ctr = Buffer.from(iv16);
    const c = ctr.readUInt32BE(12) + i;
    ctr.writeUInt32BE(c >>> 0, 12);
    const e = crypto.createCipheriv("aes-128-ctr", key, iv16);
    // use fresh cipher per block with incremented counter
    out.push(Buffer.from(e.update(Buffer.alloc(16))));
  }
  return Buffer.concat(out);
}

function gcmJ0key(key, iv, kbits) {
  // J0 for GCM: if iv is 96-bit, J0 = iv || 0x00000001 ; else J0 = GHASH-based.
  // We only need keystream block 0 = E_K(J0 + 1).
  if (iv.length === 12) {
    const ctr = Buffer.concat([iv, Buffer.from([0, 0, 0, 1])]);
    const e = crypto.createCipheriv(`aes-${kbits}-ctr`, key, ctr.subarray(0, 16));
    return Buffer.from(e.update(Buffer.alloc(16)));
  }
  // non-96-bit IV: J0 = GHASH_H(IV padded || len block); J0+1 keystream = E_K(J0+1)
  // implement GHASH over GF(2^128)
  const H = (() => {
    const e = crypto.createCipheriv(`aes-${kbits}-ctr`, key, Buffer.alloc(16));
    return e.update(Buffer.alloc(16)); // E_K(0^16) = H
  })();
  function gfMul(X, Y) {
    // R = 0xe1000000... in GCM bit order (MSB-first representation)
    let R = Buffer.from([0xe1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
    let Z = Buffer.alloc(16);
    let V = Buffer.from(Y);
    for (let i = 0; i < 128; i++) {
      const xbit = (X[(127 - i) >> 3] >> ((127 - i) & 7)) & 1;
      const vmsb = V[15] & 1;
      if (xbit) {
        for (let j = 0; j < 16; j++) Z[j] ^= V[j];
      }
      if (vmsb) {
        V = Buffer.from([...V.slice(1), 0]);
        for (let j = 0; j < 16; j++) V[j] ^= R[j];
      } else {
        V = Buffer.from([...V.slice(1), 0]);
      }
    }
    return Z;
  }
  // pad IV to 128-bit multiple
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
  for (let i = 15; i >= 12; i--) {
    J01[i] = (J01[i] + 1) & 0xff;
    if (J01[i] !== 0) break;
  }
  const e = crypto.createCipheriv(`aes-${kbits}-ctr`, key, J01);
  return Buffer.from(e.update(Buffer.alloc(16)));
}

// For each L: recovered keystream (up to 128 bytes for block checks)
const Ls = [0, 12, 16, 32];
const recovered = {};
for (const L of Ls) {
  const ks = Buffer.alloc(Math.min(128, stg.length - L));
  for (let i = 0; i < ks.length; i++) ks[i] = stg[L + i] ^ hypoBuf[i];
  recovered[L] = ks;
}

const PWDS = ["staging", "ghub10", "win", "ghub10/win/staging", "pipeline", "public", "details.json"];
const SALTS = [KEYNAME_UUID, "staging", "ghub10"];

let found = false;
for (const [hex16, b64x] of TOKENS) {
  const k16 = Buffer.from(hex16, "hex");
  const k24 = Buffer.concat([k16, Buffer.from(b64x, "base64")]);
  const keys = [["K16", k16, 128], ["K24", k24, 192]];
  for (const [kl, kval, kbits] of keys) {
    // CTR candidate keystream block0..7
    const ctrIVs = [];
    for (const L of [16, 32]) {
      ctrIVs.push([`L${L}-inband16`, stg.subarray(0, 16), L]);
    }
    // check CTR with in-band 16 (body[0:16] as IV), keystream from recovered[L]
    for (const [lab, iv16, L] of ctrIVs) {
      const ks = recovered[L];
      // compute E_K(iv16)
      const e = crypto.createCipheriv(`aes-${kbits}-ctr`, kval, iv16);
      const b0 = e.update(Buffer.alloc(16));
      if (b0.equals(ks.subarray(0, 16))) {
        // verify more blocks
        const stream = [];
        for (let i = 0; i < 8; i++) {
          const ctr = Buffer.from(iv16);
          ctr.writeUInt32BE((ctr.readUInt32BE(12) + i) >>> 0, 12);
          stream.push(crypto.createCipheriv(`aes-${kbits}-ctr`, kval, ctr).update(Buffer.alloc(16)));
        }
        const ref = Buffer.concat(stream);
        const matchLen = ks.length;
        const ok = ref.subarray(0, matchLen).equals(ks);
        console.log(`!! CTR MATCH ${kl} ${lab} blocks-ok=${ok}`);
        found = true;
      }
    }
    // GCM candidate: J0+1 block
    for (const L of [12, 32, 0]) {
      let iv;
      if (L === 0) {
        // derived IVs
        for (const pwd of PWDS) for (const salt of SALTS) for (const ivlen of [12, 32]) {
          iv = crypto.pbkdf2Sync(pwd, salt, 1000, ivlen, "sha512");
          const ks = recovered[0];
          const b0 = gcmJ0key(kval, iv, kbits);
          if (b0.equals(ks.subarray(0, 16))) {
            console.log(`!! GCM MATCH ${kl} pbkdf2 pwd=${pwd} salt=${salt.slice(0, 8)} ivlen=${ivlen}`);
            found = true;
          }
        }
      } else {
        iv = stg.subarray(0, L);
        const ks = recovered[L];
        const b0 = gcmJ0key(kval, iv, kbits);
        if (b0.equals(ks.subarray(0, 16))) {
          console.log(`!! GCM MATCH ${kl} L${L}-inband`);
          found = true;
        }
      }
    }
  }
}
console.log(found ? "HIT FOUND" : "no hit among embedded tokens");
