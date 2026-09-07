// Batch F: logi-app-version header behavior (3 requests).
import { createHash } from 'node:crypto';
import { mkdirSync, writeFileSync, appendFileSync } from 'node:fs';
import path from 'node:path';

const OUT = path.resolve('C:/Users/Decode/source/repos/UpdateManager/RE_Work/probes/api_matrix_run3_2026-09-07');
mkdirSync(OUT, { recursive: true });
const UPD = 'https://updates.ghub.logitechg.com/pipeline/v2/update/ghub10/win/public/update.json';
const CANARY_ID = '0000000000000000000000000000000000000000000000000000000000000000';

const probes = [
  { id: 'F1_appversion_cur', url: UPD, method: 'GET', headers: { 'logi-app-version': '2025.9.814156' }, note: 'app-version = current served version' },
  { id: 'F2_appversion_fut', url: UPD, method: 'GET', headers: { 'logi-app-version': '9999.1.1' }, note: 'app-version = far-future (server-side gating?)' },
  { id: 'F3_both_headers', url: UPD, method: 'GET', headers: { 'logi-install-id': CANARY_ID, 'logi-app-version': '9999.1.1' }, note: 'canary id + future app-version (interaction?)' },
];

const summary = [];
for (const p of probes) {
  const t0 = new Date().toISOString();
  const controller = new AbortController();
  const to = setTimeout(() => controller.abort(), 15000);
  let res, respHeaders = {}, buf = Buffer.alloc(0), err = null;
  try {
    res = await fetch(p.url, { method: p.method, headers: p.headers, redirect: 'manual', signal: controller.signal });
    res.headers.forEach((v, k) => { respHeaders[k] = v; });
    if (p.method !== 'HEAD') buf = Buffer.from(await res.arrayBuffer());
  } catch (e) { err = String(e && e.message || e); }
  finally { clearTimeout(to); }
  const rec = {
    id: p.id, note: p.note, url: p.url, method: p.method, reqHeaders: p.headers, tsUTC: t0,
    status: res ? res.status : null, respHeaders: res ? respHeaders : null,
    bodySha256: createHash('sha256').update(buf).digest('hex'), byteLen: buf.length, error: err,
  };
  if (buf.length > 0) writeFileSync(path.join(OUT, p.id + '.body'), buf);
  if (res) appendFileSync(path.join(OUT, 'all_resp.txt'),
    '\n=== ' + p.id + ' ===\n' + res.status + ' ' + res.statusText + '\n' +
    Object.entries(respHeaders).map(([k, v]) => k + ': ' + v).join('\n') + '\n');
  summary.push(rec);
  console.log(`[${p.id}] ${res ? res.status : 'ERR:' + err} len=${buf.length}`);
  await new Promise(r => setTimeout(r, 750));
}
writeFileSync(path.join(OUT, 'summary.json'), JSON.stringify({ outdir: OUT, count: summary.length, probes: summary }, null, 2));
console.log('done');
