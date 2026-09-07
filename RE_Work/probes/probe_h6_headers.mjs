// H6 live check: do pipeline JSON requests accept logi-install-id / logi-app-version headers?
// Compares details.json WITH vs WITHOUT the headers (expect identical body; documents server acceptance).
const APP = process.env.APP || 'ghub10';
const CH = process.env.CHANNEL || 'public';
const BASE = `https://updates.ghub.logitechg.com/pipeline/v2/update/${APP}/win/${CH}/details.json`;
const OUT = process.env.OUTDIR || `RE_Work/probes/reprobe_h6_headers_${new Date().toISOString().slice(0,10)}`;
import { mkdirSync, writeFileSync } from 'node:fs';
import path from 'node:path';
mkdirSync(OUT, { recursive: true });

async function get(name, url, headers) {
  const t0 = Date.now();
  const res = await fetch(url, { headers, redirect: 'follow' });
  const body = Buffer.from(await res.arrayBuffer());
  const ct = res.headers.get('content-type') || '';
  const etag = res.headers.get('etag') || '';
  const lm = res.headers.get('last-modified') || '';
  const hdrs = [...res.headers.entries()].map(([k, v]) => `${k}: ${v}`).join('\n');
  writeFileSync(path.join(OUT, `${name}.resp.txt`), `status: ${res.status}\n${hdrs}\n\n`, 'utf8');
  writeFileSync(path.join(OUT, `${name}.body`), body);
  console.log(`${name}: ${res.status} ${body.length}B ${ct} etag=${etag} lm=${lm} ${Date.now() - t0}ms`);
  return body;
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// 1) no custom headers (baseline; same shape as our earlier probes)
const b1 = await get('nohdr', BASE, { 'user-agent': 'probe/1.0' });
await sleep(800);
// 2) WITH logi-install-id + logi-app-version as the client sends them (machine id = 64-char hex; app version plausible)
const b2 = await get('installid', BASE, {
  'user-agent': 'probe/1.0',
  'logi-install-id': '0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef',
  'logi-app-version': '39.1.2',
});
await sleep(800);
// 3) same id repeated -> determinism check (server-side caching/AB? expect identical)
const b3 = await get('installid2', BASE, {
  'user-agent': 'probe/1.0',
  'logi-install-id': '0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef',
  'logi-app-version': '39.1.2',
});

import { createHash } from 'node:crypto';
const sha = (b) => createHash('sha256').update(b).digest('hex');
const h1 = sha(b1), h2 = sha(b2), h3 = sha(b3);
console.log('sha256 nohdr      :', h1);
console.log('sha256 install-id :', h2);
console.log('sha256 install-id2:', h3);
console.log('nohdr==installid  :', h1 === h2);
console.log('installid==repeat :', h2 === h3);
writeFileSync(path.join(OUT, 'summary.json'), JSON.stringify({
  app: APP, channel: CH, base: BASE,
  nohdr: h1, installid: h2, installid2: h3,
  nohdr_equals_installid: h1 === h2, installid_deterministic: h2 === h3,
}, null, 2), 'utf8');
