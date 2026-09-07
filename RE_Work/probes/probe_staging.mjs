// Fetch staging channel update.json + details.json bodies (2 GETs, polite).
import { mkdirSync, writeFileSync } from "node:fs";
import { setTimeout as sleep } from "node:timers/promises";

const APP = process.env.APP || "ghub10";
const HOST = "https://updates.ghub.logitechg.com";
const outdir = process.env.OUTDIR || "RE_Work/probes/reprobe_channel_brute_2026-09-07";
mkdirSync(outdir, { recursive: true });

for (const f of ["update.json", "details.json"]) {
  const url = `${HOST}/pipeline/v2/update/${APP}/win/staging/${f}`;
  const r = await fetch(url);
  const buf = Buffer.from(await r.arrayBuffer());
  writeFileSync(`${outdir}/staging_${f.replace(".json", "")}.body`, buf);
  writeFileSync(`${outdir}/staging_${f.replace(".json", "")}.resp.txt`,
    `GET ${url}\n${[...r.headers].map(([k, v]) => `${k}: ${v}`).join("\n")}\n\nstatus: ${r.status}\n`);
  console.log(`${f}: ${r.status} ${buf.length} B etag=${r.headers.get("etag")}`);
  await sleep(800);
}

// Compare staging plaintext (if JSON) with tim ciphertext prefix
const sUpd = readFileSync(`${outdir}/staging_update.body`);
const sDet = readFileSync(`${outdir}/staging_details.body`);
const tUpd = readFileSync(`RE_Work/probes/reprobe_channel_names_2026-09-07/tim_update.body`);
const tDet = readFileSync(`RE_Work/probes/reprobe_channel_names_2026-09-07/tim_details.body`);
console.log("staging_update head:", sUpd.subarray(0, 160).toString("latin1").replace(/\n/g, "\\n"));
console.log("staging_details head:", sDet.subarray(0, 160).toString("latin1").replace(/\n/g, "\\n"));
// common prefix staging vs tim (raw)
let n = 0; while (n < Math.min(sUpd.length, tUpd.length) && sUpd[n] === tUpd[n]) n++;
console.log("raw common prefix staging_update vs tim_update:", n);
n = 0; while (n < Math.min(sDet.length, tDet.length) && sDet[n] === tDet[n]) n++;
console.log("raw common prefix staging_details vs tim_details:", n);
