// Test embedded SM tokens as keys for named-channel manifests.
// Token = [32 hex chars = 16B][11 base64 chars = 8B] = 24B total (AES-192 candidate)
// Key variants: K16 (first 16B), K24 (full 24B), K8 (b64 8B, pad? no - skip invalid)
// IV variants: in-band 12/16/32, PBKDF2-SHA512(pwd=channel, salt=keyname, 1000, 12/32)
// Modes: tagless GCM, CTR.
import { readFileSync } from "node:fs";
import crypto from "node:crypto";

const bodies = {
  tim_update: readFileSync("RE_Work/probes/reprobe_channel_names_2026-09-07/tim_update.body"),
  tim_details: readFileSync("RE_Work/probes/reprobe_channel_names_2026-09-07/tim_details.body"),
  staging_update: readFileSync("RE_Work/probes/reprobe_channel_brute_2026-09-07/staging_update.body"),
  staging_details: readFileSync("RE_Work/probes/reprobe_channel_brute_2026-09-07/staging_details.body"),
};

const TOKENS = [
  ["e5fa04de24153837ce00d64e133a3be2", "q8d2Uz5cVYm"],
  ["1ad7bf2fb3fa4f2010f9ad9491977cfb", "WMewgzbA8KK"],
  ["b811e77fe061c9daeb13245391c4d9ae", "mApwm6FUvFG"],
  ["170d2d1f9153bf5117acbf2dd932c6d4", "EeJW4hM4lBE"],
  ["98120664d3c63d9e70fdac56826d95a5", "J2PD4QxHnvz"],
  ["c266f32f0fd8d0de16dafc65257bb2b4", "xjFvOew3fg5"],
];
const KEYNAME_UUID = "b7d2e981-0ca6-4806-91e6-0cbb3bdb94d6";

function check(pt) {
  if (!pt) return false;
  const head = pt.subarray(0, 32).toString("latin1");
  if (head.startsWith('{') || head.startsWith('\n{')) {
    return true;
  }
  return false;
}

function tryDecrypt(key, keyLen, iv, body, mode) {
  const k = key.subarray(0, keyLen);
  const bits = keyLen === 16 ? "128" : keyLen === 24 ? "192" : keyLen === 32 ? "256" : null;
  if (!bits) return null;
  try {
    if (mode === "gcm") {
      const d = crypto.createDecipheriv(`aes-${bits}-gcm`, k, iv);
      return Buffer.concat([d.update(body), d.final()]);
    } else {
      const d = crypto.createDecipheriv(`aes-${bits}-ctr`, k, iv.subarray(0, 16));
      return Buffer.concat([d.update(body), d.final()]);
    }
  } catch {
    return null;
  }
}

let hits = 0;
for (const [bname, body] of Object.entries(bodies)) {
  const channel = bname.split("_")[0]; // "tim" or "staging"
  for (const [hex16, b64x] of TOKENS) {
    const k16 = Buffer.from(hex16, "hex");
    const k8 = Buffer.from(b64x, "base64");
    const k24 = Buffer.concat([k16, k8]);
    const keyVariants = [
      ["K16", k16, 16],
      ["K24", k24, 24],
    ];
    for (const [klabel, kval, klen] of keyVariants) {
      const ivs = [
        ["ib12", body.subarray(0, 12), 12],
        ["ib16", body.subarray(0, 16), 16],
        ["ib32", body.subarray(0, 32), 32],
      ];
      for (const salt of [hex16, KEYNAME_UUID, channel]) {
        for (const ivlen of [12, 32]) {
          const iv = crypto.pbkdf2Sync(channel, salt, 1000, ivlen, "sha512");
          ivs.push([`pbkdf2-${ivlen}-s=${salt.slice(0, 8)}`, iv, ivlen]);
        }
      }
      for (const [ivlabel, iv, ivlen] of ivs) {
        const inBand = ivlabel.startsWith("ib");
        const ct = inBand ? body.subarray(ivlen) : body;
        for (const mode of ["gcm", "ctr"]) {
          if (mode === "ctr" && iv.length < 16) continue;
          const pt = tryDecrypt(kval, klen, iv, ct, mode);
          if (check(pt)) {
            hits++;
            console.log(`*** HIT ${bname} ${klabel} ${ivlabel} ${mode}: ${pt.subarray(0, 120).toString("latin1").replace(/\n/g, "\\n")}`);
          }
        }
      }
    }
  }
  console.log(`done ${bname}`);
}
console.log("total hits:", hits);
