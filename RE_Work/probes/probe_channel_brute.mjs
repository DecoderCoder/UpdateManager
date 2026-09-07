// Named-channel brute-force (polite): HEAD /pipeline/v2/update/ghub10/win/{name}/update.json
// for a curated name set + controls; plus S3 ListBucket attempts.
// Usage: node probe_channel_brute.mjs   (env OUTDIR override)
import { mkdirSync, writeFileSync } from "node:fs";
import { setTimeout as sleep } from "node:timers/promises";

const APP = process.env.APP || "ghub10";
const HOST = "https://updates.ghub.logitechg.com";
const outdir = process.env.OUTDIR || `RE_Work/probes/reprobe_channel_brute_${new Date().toISOString().slice(0, 10)}`;
mkdirSync(outdir, { recursive: true });

const NAMES = ["tim", "bob", "tom", "sam", "max", "amy", "lee", "joe", "ann", "eve", "dev", "test", "qa", "rc1", "alpha", "staging"];

const rows = [];
for (const name of NAMES) {
  const url = `${HOST}/pipeline/v2/update/${APP}/win/${name}/update.json`;
  const t0 = Date.now();
  try {
    const r = await fetch(url, { method: "HEAD", redirect: "manual" });
    rows.push({ name, status: r.status, len: r.headers.get("content-length"), etag: r.headers.get("etag"), type: r.headers.get("content-type"), ms: Date.now() - t0 });
  } catch (e) {
    rows.push({ name, status: `ERR ${e.message}`, ms: Date.now() - t0 });
  }
  console.log(`${name.padEnd(8)} ${rows.at(-1).status} len=${rows.at(-1).len} ${rows.at(-1).etag || ""} ${rows.at(-1).ms}ms`);
  await sleep(750);
}

// S3 list attempts (cheap, single requests each)
const listAttempts = [
  `${HOST}/pipeline/v2/update/${APP}/win/?list-type=2&prefix=&max-keys=100`,
  `https://2pipeline.s3.amazonaws.com/pipeline/v2/update/${APP}/win/?list-type=2&prefix=&max-keys=100`,
];
const listResults = [];
for (const url of listAttempts) {
  try {
    const r = await fetch(url);
    const text = await r.text();
    listResults.push({ url, status: r.status, body: text.slice(0, 1200) });
    console.log(`LIST ${url.split("/").slice(2).join("/")}\u2192 ${r.status} (${text.length} B)`);
  } catch (e) {
    listResults.push({ url, status: `ERR ${e.message}` });
    console.log(`LIST ${url} -> ERR ${e.message}`);
  }
  await sleep(800);
}

writeFileSync(`${outdir}/brute_summary.json`, JSON.stringify({ app: APP, rows, listResults }, null, 2));
console.log(`saved -> ${outdir}/brute_summary.json`);
