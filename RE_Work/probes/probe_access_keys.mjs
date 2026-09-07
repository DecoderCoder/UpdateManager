// probe_access_keys.mjs - fetch keymaster access-group objects from production pipeline.
// Approved: production host updates.ghub.logitechg.com only. Sequential, ~800ms delay, small count.
// URLs from lghub_updater.exe: build_content_json_url / build_iat_json_url
//   {server}/pipeline/v2/access/{accessGroup}/content.json
//   {server}/pipeline/v2/access/{accessGroup}/iat.json
// accessGroup 323e77f5-68c2-43d8-8457-818bbd663938 = keys.accessGroup from live public details.json (build 634218).

import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { fileURLToPath } from 'node:url';

const SERVER = process.env.SERVER || 'https://updates.ghub.logitechg.com';
const GROUP = process.env.GROUP || '323e77f5-68c2-43d8-8457-818bbd663938';
const OUTDIR = process.env.OUTDIR || `reprobe_access_keys_${new Date().toISOString().slice(0, 10)}`;
const here = path.dirname(fileURLToPath(import.meta.url));
const out = path.join(here, OUTDIR);
fs.mkdirSync(out, { recursive: true });

const targets = [
  ['content.json', `${SERVER}/pipeline/v2/access/${GROUP}/content.json`],
  ['iat.json', `${SERVER}/pipeline/v2/access/${GROUP}/iat.json`],
];

const summary = [];
for (const [name, url] of targets) {
  const t0 = Date.now();
  let status = 0, body = null, err = null, headers = {};
  try {
    const res = await fetch(url, { headers: { 'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)' } });
    status = res.status;
    for (const h of res.headers.entries()) headers[h[0]] = h[1];
    const buf = Buffer.from(await res.arrayBuffer());
    body = buf;
    const file = path.join(out, `${name}.body`);
    fs.writeFileSync(file, buf);
  } catch (e) { err = String(e); }
  const rec = { name, url, status, bytes: body ? body.length : 0, ms: Date.now() - t0, err, headers, out: path.relative(process.cwd(), out) };
  summary.push(rec);
  console.log(JSON.stringify(rec));
  if (name !== targets[targets.length - 1][0]) await new Promise(r => setTimeout(r, 800));
}
fs.writeFileSync(path.join(out, 'summary.json'), JSON.stringify({ server: SERVER, group: GROUP, targets: summary }, null, 2));
console.log('OUTDIR:', out);
