// User-reported: /pipeline/v2/update/ghub10/win/<name>/details.json with an
// arbitrary name in the channel slot (e.g. "tim") returns 200 with encrypted
// content. Verify: is it name-specific, what is the content type, is it a depot?
const APP = process.env.APP || 'ghub10';
const OUT = process.env.OUTDIR || `RE_Work/probes/reprobe_channel_names_${new Date().toISOString().slice(0,10)}`;
import { mkdirSync, writeFileSync } from 'node:fs';
import path from 'node:path';
mkdirSync(OUT, { recursive: true });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const cases = [
  ['tim_details', `https://updates.ghub.logitechg.com/pipeline/v2/update/${APP}/win/tim/details.json`],
  ['tim_update', `https://updates.ghub.logitechg.com/pipeline/v2/update/${APP}/win/tim/update.json`],
  ['bob_details', `https://updates.ghub.logitechg.com/pipeline/v2/update/${APP}/win/bob/details.json`],
  ['zzqq_details', `https://updates.ghub.logitechg.com/pipeline/v2/update/${APP}/win/zzqq/details.json`],
  ['public_details', `https://updates.ghub.logitechg.com/pipeline/v2/update/${APP}/win/public/details.json`],
];

const magicOf = (b) => {
  if (b.length >= 4 && b.readUInt32LE(0) === 0x20170110) return 'DEPOT_PLAIN 0x20170110';
  if (b.length >= 4 && b.readUInt32LE(0) === 0x20210506) return 'DEPOT_ENC 0x20210506';
  if (b.length >= 4 && b.readUInt32LE(0) === 0x20210521) return 'DEPOT_SINGLE 0x20210521';
  if (b[0] === 0xfd && b[1] === 0x37 && b[2] === 0x7a) return 'XZ';
  if (b[0] === 0x50 && b[1] === 0x4b) return 'ZIP';
  if (b[0] === 0x7b) return 'JSON-ish';
  if (b[0] === 0x3c) return 'XML-ish';
  return 'unknown';
};

const rows = [];
for (const [name, url] of cases) {
  const t0 = Date.now();
  const res = await fetch(url, { headers: { 'user-agent': 'probe/1.0' } });
  const body = Buffer.from(await res.arrayBuffer());
  const file = path.join(OUT, `${name}.body`);
  writeFileSync(file, body);
  const hdrs = {};
  res.headers.forEach((v, k) => { hdrs[k] = v; });
  writeFileSync(file.replace(/\.body$/, '.resp.txt'),
    `GET ${url}\n${res.status} ${res.statusText}\n${JSON.stringify(hdrs, null, 2)}\n\n(first 64 bytes)\n` +
    body.subarray(0, 64).toString('hex') + '\n');
  const head = body.subarray(0, 48).toString('hex').replace(/(.{2})/g, '$1 ').trim();
  rows.push({ name, status: res.status, bytes: body.length, type: res.headers.get('content-type'), magic: magicOf(body), etag: res.headers.get('etag'), ms: Date.now() - t0 });
  console.log(`${name}: ${res.status} ${body.length} B type=${res.headers.get('content-type')} magic=${magicOf(body)} etag=${res.headers.get('etag')}`);
  console.log(`   head: ${head}`);
  await sleep(800);
}
writeFileSync(path.join(OUT, 'summary.json'), JSON.stringify({ app: APP, rows }, null, 2), 'utf8');
console.log('summary ->', path.join(OUT, 'summary.json'));
