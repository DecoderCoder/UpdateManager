// Compare the manifest depot entry for the sampled capsule against its header:
//   H2: depot.iv == header-sha  (server writes the same seed in both places;
//        client uses only the manifest field, header field is informational)
//   H3: depot.iv is the raw 32-byte IV itself
// Offline only.
import { readFileSync } from "node:fs";

const manifest = JSON.parse(
  readFileSync("RE_Work/probes/resp_pipeline_v2_update_ghub10_win_public_details.json.json", "utf8"),
);
const bin = readFileSync("RE_Work/probes/depot_full_85875e86-f3e1-4e79-91ee-232575e2807f.bin");
const jsonLen = bin.readUInt32LE(4);
const hdr = JSON.parse(bin.subarray(8, 8 + jsonLen).toString("utf8"));
const headerSha = hdr["header-sha"];
console.log("header-sha:", headerSha);
console.log("key-id    :", hdr["key-id"]);

const depots = manifest.depots;
const d = depots.find(
  (x) => x.url && x.url.includes("85875e86-f3e1-4e79-91ee-232575e2807f"),
);
if (!d) {
  console.log("depot not found by url; scanning all fields...");
  const hit = depots.find((x) => JSON.stringify(x).includes("85875e86-f3e1-4e79-91ee-232575e2807f"));
  if (hit) console.log(JSON.stringify(hit, null, 2));
  process.exit(1);
}
console.log("\ndepot entry:");
console.log(JSON.stringify(d, null, 2));

console.log("\nchecks:");
console.log("depot.iv === header-sha :", d.iv === headerSha);
if (typeof d.iv === "string" && d.iv.length === 64) {
  console.log("depot.iv is 64-hex (32 bytes) — indistinguishable by length from a sha256 digest");
}
console.log("depot.mac matches sha256(raw bytes) — recompute:");
import { createHash } from "node:crypto";
console.log(
  "  sha256(raw) =",
  createHash("sha256").update(bin).digest("hex"),
  "mac =", d.mac,
  "match:", createHash("sha256").update(bin).digest("hex") === d.mac,
);
