// Decisive test of the depot-signature TBS layout, offline.
//
// Given the saved capsule depot 85875e86 (raw bytes + its 1024-hex RSA sig
// from the ghub10 manifest + pipeline_pubkey_2.pem), compute:
//   A = sha256(raw 32-byte mac digest)          // TBS = raw digest bytes
//   B = sha256(64-char hex mac string, ASCII)   // TBS = JSON "mac" string
// then RSA-public-decrypt the signature, strip the PKCS#1 v1.5 DigestInfo
// prefix, and compare the embedded digest against A and B.
import { createHash, createPublicKey } from "node:crypto";
import { readFileSync } from "node:fs";

const root = "C:/Users/Decode/source/repos/UpdateManager/RE_Work";
const raw = readFileSync(`${root}/probes/depot_full_85875e86-f3e1-4e79-91ee-232575e2807f.bin`);
const manifest = JSON.parse(readFileSync(`${root}/probes/resp_pipeline_v2_update_ghub10_win_public_details.json.json`, "utf8"));
const pub = createPublicKey({ key: readFileSync(`${root}/probes/pipeline_pubkey_2.pem`), format: "pem" });

// find the depot entry
const depot = manifest.depots.find((d) => d.name.startsWith("85875e86"));
if (!depot) throw new Error("depot 85875e86 not found in manifest");
const sigHex = depot.signatures.signatures[0].signature;

const mac = createHash("sha256").update(raw).digest();
const macHex = mac.toString("hex");
console.log("sha256(raw depot)        :", macHex);
console.log("json mac                 :", depot.mac);
console.log("mac match                :", macHex === depot.mac);

const A = createHash("sha256").update(mac).digest();
const B = createHash("sha256").update(Buffer.from(macHex, "ascii")).digest();

// RSA PKCS#1 v1.5 public decrypt of the 512-byte signature — raw s^e mod n
// via BigInt (OpenSSL refuses NO_PADDING on public keys).
const jwk = pub.export({ format: "jwk" });
const n = BigInt("0x" + Buffer.from(jwk.n, "base64url").toString("hex"));
const e = BigInt(jwk.e ? "0x" + Buffer.from(jwk.e, "base64url").toString("hex") : "10001");
const sigBuf = Buffer.from(sigHex, "hex");
const modpow = (base, exp, mod) => {
  let result = 1n;
  base %= mod;
  while (exp > 0n) {
    if (exp & 1n) result = (result * base) % mod;
    exp >>= 1n;
    base = (base * base) % mod;
  }
  return result;
};
let m = BigInt("0x" + (sigBuf.toString("hex").padStart(2, "0")));
m = modpow(m, e, n);
const out = Buffer.alloc(sigBuf.length);
let tmp = m.toString(16).padStart(sigBuf.length * 2, "0");
for (let i = 0; i < sigBuf.length; i++) out[i] = parseInt(tmp.slice(i * 2, i * 2 + 2), 16);
console.log("sig bytes                :", sigBuf.length, "out bytes:", out.length);
// PKCS#1 v1.5: 00 01 FF..FF 00 DigestInfo(0x30 0x31 0x30 0x0d 0x06 0x09 60 86 48 01 65 03 04 02 01 05 00 04 0x20) digest(32)
const diLen = out.length;
let digestStart = -1;
for (let i = 0; i < diLen - 32; i++) {
  if (out[i] === 0x30 && out[i + 1] === 0x31 && out[i + 2] === 0x30 && out[i + 3] === 0x0d) {
    digestStart = i + 19; // 2 + 1 + 9 + 1 + 1 + 1 + 1 + 1 = 19? 0x30 0x31 (2) + seq(11: 0x30 0x0d 06 09 60 86 48 01 65 03 04 02 01 05 00 = 15) hmm compute below
    break;
  }
}
// robust: find "04 20" (Octet String, len 32) after the DigestInfo header
let idx = out.indexOf(Buffer.from([0x04, 0x20]));
const digest = out.subarray(idx + 2, idx + 34);
console.log("embedded digest          :", digest.toString("hex"));
console.log("A = sha256(raw mac bytes):", A.toString("hex"), digest.equals(A) ? "  <-- MATCH (TBS = raw 32-byte digest)" : "");
console.log("B = sha256(hex mac str)  :", B.toString("hex"), digest.equals(B) ? "  <-- MATCH (TBS = 64-char hex string)" : "");
