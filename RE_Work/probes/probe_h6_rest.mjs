// H6 follow-up: update.json and /settings with/without logi-install-id (canary-bucket value 0123… vs none).
const APP = process.env.APP || 'ghub10';
const CH = process.env.CHANNEL || 'public';
const BASE = `https://updates.ghub.logitechg.com/pipeline/v2/update/${APP}/win/${CH}`;
const OUT = process.env.OUTDIR || `RE_Work/probes/reprobe_h6_rest_${new Date().toISOString().slice(0,10)}`;
import { mkdirSync, writeFileSync } from 'node:fs';
import path from 'node:path';
mkdirSync(OUT, { recursive: true });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const ID = '0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef';

async function get(name, url, withId) {
  const h = { 'user-agent': 'probe/1.0' };
  if (withId) { h['logi-install-id'] = ID; h['logi-app-version'] = '39.1.2'; }
  const res = await fetch(url, { headers: h });
  const body = Buffer.from(await res.arrayBuffer());
  writeFileSync(path.join(OUT, `${name}.body`), body);
  const text = body.toString('utf8');
  console.log(`== ${name} [${res.status}] ${body.length}B ${res.headers.get('content-type')}`);
  console.log(text.length > 1200 ? text.slice(0, 1200) + ' …[truncated]' : text);
  return body;
}

const u1 = await get('update_json_noid', `${BASE}/update.json`, false);
await sleep(700);
const u2 = await get('update_json_id', `${BASE}/update.json`, true);
await sleep(700);
const s1 = await get('settings_noid', `${BASE}/settings`, false);
await sleep(700);
const s2 = await get('settings_id', `${BASE}/settings`, true);

import { createHash } from 'node:crypto';
const sha = (b) => createHash('sha256').update(b).digest('hex');
writeFileSync(path.join(OUT, 'summary.json'), JSON.stringify({
  app: APP, channel: CH,
  update_json: { noid: sha(u1), id: sha(u2), identical: sha(u1) === sha(u2) },
  settings: { noid: sha(s1), id: sha(s2), identical: sha(s1) === sha(s2) },
}, null, 2), 'utf8');
