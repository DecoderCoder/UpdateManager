// Batch E: representative GETs for new hits + channel x app cross-check + alt host.
import { createHash } from 'node:crypto';
import { mkdirSync, writeFileSync, appendFileSync } from 'node:fs';
import path from 'node:path';

const OUT = path.resolve('C:/Users/Decode/source/repos/UpdateManager/RE_Work/probes/api_matrix_run2_2026-09-07');
mkdirSync(OUT, { recursive: true });
const H = 'https://updates.ghub.logitechg.com';

const probes = [
  { id: 'E1_ghub13_update_get', url: H + '/pipeline/v2/update/ghub13/win/public/update.json', method: 'GET', note: 'ghub13 update.json body' },
  { id: 'E2_osx_update_get', url: H + '/pipeline/v2/update/ghub10/osx/public/update.json', method: 'GET', note: 'osx platform update.json body' },
  { id: 'E3_ghub12_tim_head', url: H + '/pipeline/v2/update/ghub12/win/tim/update.json', method: 'HEAD', note: 'tim channel on ghub12' },
  { id: 'E4_ghub12_staging_head', url: H + '/pipeline/v2/update/ghub12/win/staging/update.json', method: 'HEAD', note: 'staging channel on ghub12' },
  { id: 'E5_alt_host_head', url: 'https://pipeline.logitech.io/pipeline/v2/update/ghub10/win/public/update.json', method: 'HEAD', note: 'pipeline.logitech.io (4-host array alt) - HEAD only' },
];

const summary = [];
for (const p of probes) {
  const t0 = new Date().toISOString();
  const controller = new AbortController();
  const to = setTimeout(() => controller.abort(), 15000);
  let res, respHeaders = {}, buf = Buffer.alloc(0);
  let err = null;
  try {
    res = await fetch(p.url, { method: p.method, headers: {}, redirect: 'manual', signal: controller.signal });
    res.headers.forEach((v, k) => { respHeaders[k] = v; });
    if (p.method !== 'HEAD') buf = Buffer.from(await res.arrayBuffer());
  } catch (e) { err = String(e && e.message || e); }
  finally { clearTimeout(to); }
  const sha = createHash('sha256').update(buf).digest('hex');
  const rec = {
    id: p.id, note: p.note, url: p.url, method: p.method, tsUTC: t0,
    status: res ? res.status : null, statusText: res ? res.statusText : null,
    finalUrl: res ? res.url : null, respHeaders: res ? respHeaders : null,
    bodySha256: sha, byteLen: buf.length, error: err, fixture: null,
  };
  if (buf.length > 0) {
    rec.fixture = path.join(OUT, p.id + '.body');
    writeFileSync(rec.fixture, buf);
  }
  if (res) {
    appendFileSync(path.join(OUT, 'all_resp.txt'),
      '\n=== ' + p.id + ' ===\n' + res.status + ' ' + res.statusText + '\n' +
      Object.entries(respHeaders).map(([k, v]) => k + ': ' + v).join('\n') + '\n');
  }
  summary.push(rec);
  console.log(`[${p.id}] ${res ? res.status : 'ERR:' + err} len=${buf.length}`);
  await new Promise(r => setTimeout(r, 750));
}
writeFileSync(path.join(OUT, 'summary.json'), JSON.stringify({ outdir: OUT, count: summary.length, probes: summary }, null, 2));
console.log('Wrote ' + summary.length + ' records');
