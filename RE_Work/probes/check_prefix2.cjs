const { readFileSync } = require("node:fs");
const pubUpd = readFileSync("RE_Work/probes/reprobe_channel_names_2026-09-07/public_update.body");
const pubDet = readFileSync("RE_Work/probes/reprobe_channel_names_2026-09-07/public_details.body");
const timUpd = readFileSync("RE_Work/probes/reprobe_channel_names_2026-09-07/tim_update.body");
const timDet = readFileSync("RE_Work/probes/reprobe_channel_names_2026-09-07/tim_details.body");
const stgUpd = readFileSync("RE_Work/probes/reprobe_channel_brute_2026-09-07/staging_update.body");
const stgDet = readFileSync("RE_Work/probes/reprobe_channel_brute_2026-09-07/staging_details.body");

function cpl(a, b) {
  const n = Math.min(a.length, b.length);
  let i = 0;
  while (i < n && a[i] === b[i]) i++;
  return i;
}

const D_pub = cpl(pubUpd, pubDet);
console.log("D_pub (plaintext common prefix update|details):", D_pub);
console.log("update tail after D_pub:", JSON.stringify(pubUpd.subarray(D_pub, D_pub + 8).toString("utf8")));
console.log("details at D_pub..D_pub+16:", JSON.stringify(pubDet.subarray(D_pub, D_pub + 16).toString("utf8")));

const C_tim = cpl(timUpd, timDet);
console.log("C_tim (ciphertext common prefix):", C_tim, " update len:", timUpd.length, " details len:", timDet.length);
console.log("=> IV length L = C_tim - D_pub =", C_tim - D_pub, "(if tim values same length as public)");

// staging pair: any common prefix between staging update and staging details?
console.log("C_stg update|details:", cpl(stgUpd, stgDet));
// cross-channel prefixes
console.log("prefix tim_upd|stg_upd:", cpl(timUpd, stgUpd));
console.log("prefix tim_det|stg_det:", cpl(timDet, stgDet));

// check staging details vs hypo plaintext prefix: where do they first 'look' different is not possible (encrypted).
// But: if staging == public+1byte with SAME build values, then stg_det plaintext prefix == pub_det prefix (up to channel field),
// and stg_upd plaintext == pub_upd with channel swapped.
