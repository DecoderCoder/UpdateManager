// CloudFront HTTPS re-check for the H3 403: same request as fetch_depot.mjs
// (2026-09-06, HTTPS + LGHUB UA). Saves body on any status. 1 request.
import fs from "node:fs";
const dir = "C:/Users/Decode/source/repos/UpdateManager/RE_Work/probes/reprobe_2026_09_07_h3";
fs.mkdirSync(dir, { recursive: true });
const UA = "LGHUB/2026.6.957899 (Windows NT 10.0; x64)";
const url = "https://updates.ghub.logitechg.com/depots/780f7572-689c-45d5-894e-706f02c8f13e/g560_dfu.depot";
const r = await fetch(url, { headers: { "User-Agent": UA } });
const buf = new Uint8Array(await r.arrayBuffer());
fs.writeFileSync(`${dir}/h3_cloudfront_https.depot`, buf);
console.log("status:", r.status, "bytes:", buf.length);
console.log("content-type:", r.headers.get("content-type"));
console.log("server:", r.headers.get("server"));
console.log("body-head:", Buffer.from(buf.slice(0, 400)).toString("utf8").replace(/\n/g, " "));
