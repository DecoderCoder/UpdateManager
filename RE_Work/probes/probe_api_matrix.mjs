// API variant-matrix live probe (Batches A-D combined).
// Polite: sequential, 750 ms delay, 15 s timeout, one retry w/ backoff on 429/503.
// Captures per-request: url, method, reqHeaders, tsUTC, status, finalUrl,
// respHeaders, bodySha256, byteLen, fixture path. Redirects manual (recorded).
import { createHash } from 'node:crypto';
import { mkdirSync, writeFileSync } from 'node:fs';
import path from 'node:path';

const OUT = path.resolve('C:/Users/Decode/source/repos/UpdateManager/RE_Work/probes/api_matrix_run1_2026-09-07');
mkdirSync(OUT, { recursive: true });

const H = 'https://updates.ghub.logitechg.com';
const S3 = 'https://2pipeline.s3.amazonaws.com';
const DEPOT = H + '/depots/780f7572-689c-45d5-894e-706f02c8f13e/g560_dfu.depot';
const UPD = H + '/pipeline/v2/update/ghub10/win/public/update.json';
const DET = H + '/pipeline/v2/update/ghub10/win/public/details.json';

const CANARY_ID = '0000000000000000000000000000000000000000000000000000000000000000'; // 64 x 0 => canary (H6)

// Each probe: { id, url, method, headers, range? , note }
const probes = [
  // ---- Batch A: S3 ListObjects (corrected: bucket root + list-type=2 + prefix) ----
  { id: 'A1_s3_list_pipeline', url: S3 + '/?list-type=2&prefix=pipeline/&max-keys=5', method: 'GET', note: 'ListObjectsV2 bucket root prefix=pipeline/' },
  { id: 'A2_s3_list_update_key', url: S3 + '/?list-type=2&prefix=pipeline/v2/update/ghub10/win/public/&max-keys=5', method: 'GET', note: 'ListObjectsV2 exact update key prefix' },
  { id: 'A3_s3_list_depots', url: S3 + '/?list-type=2&prefix=depots/&max-keys=3', method: 'GET', note: 'ListObjectsV2 depots/' },

  // ---- Batch B: app / platform / channel cross-check (HEAD first) ----
  { id: 'B1_ghub13_update', url: H + '/pipeline/v2/update/ghub13/win/public/update.json', method: 'HEAD', note: 'new app id ghub13 (binary+SM)' },
  { id: 'B2_ghub13_details', url: H + '/pipeline/v2/update/ghub13/win/public/details.json', method: 'HEAD', note: 'ghub13 details' },
  { id: 'B3_mac_update', url: H + '/pipeline/v2/update/ghub10/mac/public/update.json', method: 'HEAD', note: 'platform=mac' },
  { id: 'B4_osx_update', url: H + '/pipeline/v2/update/ghub10/osx/public/update.json', method: 'HEAD', note: 'platform=osx' },
  { id: 'B5_linux_update', url: H + '/pipeline/v2/update/ghub10/linux/public/update.json', method: 'HEAD', note: 'platform=linux' },
  { id: 'B6_ghub99_update', url: H + '/pipeline/v2/update/ghub99/win/public/update.json', method: 'HEAD', note: 'nonexistent app (control) expect 403/404' },
  { id: 'B7_ghub10_staging_head', url: H + '/pipeline/v2/update/ghub10/win/staging/update.json', method: 'HEAD', note: 'staging HEAD (known 200 opaque)' },
  { id: 'B8_ghub12_public_update', url: H + '/pipeline/v2/update/ghub12/win/public/update.json', method: 'HEAD', note: 'ghub12 control (known exists)' },

  // ---- Batch C: header / query / HTTP behavior on update.json ----
  { id: 'C1_update_baseline', url: UPD, method: 'GET', note: 'baseline no custom headers' },
  { id: 'C2_update_canaryid', url: UPD, method: 'GET', headers: { 'logi-install-id': CANARY_ID }, note: 'well-formed 64-hex install-id => expect canary bucket' },
  { id: 'C3_update_badid', url: UPD, method: 'GET', headers: { 'logi-install-id': 'not-a-valid-id' }, note: 'malformed install-id => expect public bucket' },
  { id: 'C4_update_queryparam', url: UPD + '?foo=bar&channel=canary', method: 'GET', note: 'query params (ignored? bucket? error?)' },
  { id: 'C5_update_range', url: UPD, method: 'GET', headers: { Range: 'bytes=0-3' }, note: 'range 0-3 (accept-ranges: bytes)' },
  { id: 'C6_update_imds', url: UPD, method: 'GET', headers: { 'If-Modified-Since': 'Fri, 01 Jan 2035 00:00:00 GMT' }, note: 'conditional If-Modified-Since future => 304?' },
  { id: 'C7_update_trailslash', url: UPD + '/', method: 'GET', note: 'trailing slash' },
  { id: 'C8_update_uppercase', url: H + '/pipeline/v2/update/ghub10/win/public/UPDATE.JSON', method: 'GET', note: 'case sensitivity (UPDATE.JSON)' },

  // ---- Batch D: depot HEAD / Range / conditional ----
  { id: 'D1_depot_head', url: DEPOT, method: 'HEAD', note: 'depot HEAD (per-object CDN pathing? headers)' },
  { id: 'D2_depot_range16', url: DEPOT, method: 'GET', headers: { Range: 'bytes=0-15' }, note: 'depot range 16 B (header magic)' },
  { id: 'D3_depot_ifnone', url: DEPOT, method: 'GET', headers: { 'If-None-Match': '* ' }, note: 'If-None-Match garbage => 200? ' },
];

async function doFetch(p) {
  const t0 = new Date().toISOString();
  const controller = new AbortController();
  const to = setTimeout(() => controller.abort(), 15000);
  let res;
  try {
    res = await fetch(p.url, { method: p.method, headers: p.headers || {}, redirect: 'manual', signal: controller.signal });
  } finally { clearTimeout(to); }
  const respHeaders = {};
  res.headers.forEach((v, k) => { respHeaders[k] = v; });
  let buf = Buffer.alloc(0);
  if (p.method !== 'HEAD') { buf = Buffer.from(await res.arrayBuffer()); }
  const sha = createHash('sha256').update(buf).digest('hex');
  return { res, respHeaders, buf, sha, t0 };
}

const summary = [];
for (const p of probes) {
  let attempt = 0, lastErr = null, got = null;
  while (attempt < 2) {
    attempt++;
    try { got = await doFetch(p); break; }
    catch (e) { lastErr = String(e && e.message || e); }
    if (attempt === 1) { await new Promise(r => setTimeout(r, 3000)); } // backoff
  }
  const rec = {
    id: p.id, note: p.note, url: p.url, method: p.method,
    reqHeaders: p.headers || null, tsUTC: got ? got.t0 : null,
    attempts: attempt,
    status: got ? got.res.status : null,
    statusText: got ? got.res.statusText : null,
    finalUrl: got ? got.res.url : null,
    redirect: got ? (got.res.status >= 300 && got.res.status < 400 ? (got.respHeaders['location'] || null) : null) : null,
    respHeaders: got ? got.respHeaders : null,
    bodySha256: got ? got.sha : null,
    byteLen: got ? got.buf.length : null,
    error: got ? null : lastErr,
    fixture: null,
  };
  if (got && got.buf.length > 0) {
    const fx = path.join(OUT, p.id + '.body');
    writeFileSync(fx, got.buf);
    rec.fixture = fx;
  }
  // also record resp headers as txt for quick inspection
  if (got) writeFileSync(path.join(OUT, p.id + '.resp.txt'),
    'HTTP status: ' + got.res.status + ' ' + got.res.statusText + '\n' +
    Object.entries(got.respHeaders).map(([k, v]) => k + ': ' + v).join('\n') + '\n');
  summary.push(rec);
  const tag = got ? (got.res.status) : ('ERR:' + lastErr);
  console.log(`[${p.id}] ${tag} len=${got ? got.buf.length : '-'} url=${p.url}`);
  await new Promise(r => setTimeout(r, 750));
}
writeFileSync(path.join(OUT, 'summary.json'), JSON.stringify({ outdir: OUT, count: summary.length, probes: summary }, null, 2));
console.log('\nWrote ' + summary.length + ' records to ' + path.join(OUT, 'summary.json'));
