// Test: named-channel body = [32B IV][tagless AES-GCM ciphertext],
// IV = PBKDF2-HMAC-SHA512(pwd, salt, 1000, 32), key = access-group key.
// Also tries CTR with first 16 IV bytes as control.
import { readFileSync } from "node:fs";
import crypto from "node:crypto";

const dir = "RE_Work/probes/reprobe_channel_names_2026-09-07";
const keysDoc = JSON.parse(readFileSync("RE_Work/probes/probe_access_ghub10_group_content.out", "utf8"));
const keys = keysDoc.keys.map((k) => ({ name: k.name, key: Buffer.from(k.key, "base64") }));
const bodies = {
  tim_update: readFileSync(`${dir}/tim_update.body`),
  tim_details: readFileSync(`${dir}/tim_details.body`),
};

const PWDS = ["tim", "ghub10", "win", "ghub10/win/tim", "pipeline", "logitech", "ghub10-win-tim", "tim", "public", "ghub", "details.json", "update.json", "win/tim", "ghub10/tim"];
const dedupPwd = [...new Set(PWDS)];
const SALT = "tim";

function gcmDecryptNoTag(key, iv, ct) {
  // node's EVP path supports non-12B IVs for GCM (J0 via GHASH). Try direct first.
  try {
    const d = crypto.createDecipheriv("aes-128-gcm", key, iv);
    return Buffer.concat([d.update(ct), d.final()]);
  } catch {
    return null;
  }
}

function checkJson(pt, label) {
  if (!pt) return false;
  const head = pt.subarray(0, 40).toString("latin1");
  if (head.startsWith('{\n') || head.startsWith('{"')) {
    console.log(`*** JSON HIT ${label}\n    ${pt.subarray(0, 160).toString("latin1").replace(/\n/g, "\\n")}`);
    return true;
  }
  return false;
}

const IVLEN = 32;
let hits = 0;
for (const [bname, body] of Object.entries(bodies)) {
  const ct = body.subarray(IVLEN);
  for (const k of keys) {
    for (const pwd of dedupPwd) {
      for (const salt of [k.name, SALT]) {
        const iv = crypto.pbkdf2Sync(pwd, salt, 1000, IVLEN, "sha512");
        const pt = gcmDecryptNoTag(k.key, iv, ct);
        const label = `${bname} key=${k.name} pwd=${pwd} salt=${salt === k.name ? "name" : SALT} mode=GCM-32IV`;
        if (checkJson(pt, label)) hits++;
        // control: CTR with iv[0..16]
        try {
          const d = crypto.createDecipheriv("aes-128-ctr", k.key, iv.subarray(0, 16));
          const pt2 = Buffer.concat([d.update(ct), d2fix(d)]);
          if (checkJson(pt2, label.replace("GCM-32IV", "CTR"))) hits++;
        } catch {}
      }
    }
  }
  console.log(`done ${bname} (no hits so far: ${hits === 0})`);
}
function d2fix(d) { return d.final(); }
console.log("total hits:", hits);
