// Check: is the capsule header's "header-sha" field equal to sha256(header plaintext bytes)?
// If yes, the updater can recompute it (the binary contains NO "header-sha" literal),
// meaning the field is informational / used only as an IV seed by producers.
import { readFileSync } from "node:fs";
import { createHash } from "node:crypto";

const p = "RE_Work/probes/depot_full_85875e86-f3e1-4e79-91ee-232575e2807f.bin";
const b = readFileSync(p);
const magic = b.readUInt32LE(0);
const jsonLen = b.readUInt32LE(4);
const header = b.subarray(8, 8 + jsonLen);
const h = JSON.parse(header.toString("utf8"));
const actual = createHash("sha256").update(header).digest("hex");
console.log("magic        :", magic.toString(16));
console.log("header bytes :", jsonLen);
console.log("header-sha   :", h["header-sha"]);
console.log("sha256(hdr)  :", actual);
console.log("MATCH        :", h["header-sha"] === actual);
console.log("has 'compression' field:", "compression" in h ? h["compression"] : "(absent)");
