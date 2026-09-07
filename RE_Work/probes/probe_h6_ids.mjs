// H6 follow-up: is the canary switch presence-based or per-install-id (hash bucketing)?
// Tries several distinct install-id values against details.json and reports the served channel.
const APP = process.env.APP || 'ghub10';
const CH = process.env.CHANNEL || 'public';
const URL = `https://updates.ghub.logitechg.com/pipeline/v2/update/${APP}/win/${CH}/details.json`;
const OUT = process.env.OUTDIR || `RE_Work/probes/reprobe_h6_ids_${new Date().toISOString().slice(0,10)}`;
import { mkdirSync, writeFileSync } from 'node:fs';
import path from 'node:path';
mkdirSync(OUT, { recursive: true });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const ids = [
  ['id1', '0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef'], // prior probe value (was => canary)
  ['id2', 'ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff'],
  ['id3', 'deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef'],
  ['id4', '0000000000000000000000000000000000000000000000000000000000000000'],
  ['id5', 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'],
  ['id6', '1111111111111111111111111111111111111111111111111111111111111111'],
];
const rows = [];
for (const [name, id] of ids) {
  const res = await fetch(URL, { headers: { 'user-agent': 'probe/1.0', 'logi-install-id': id, 'logi-app-version': '39.1.2' } });
  const body = Buffer.from(await res.arrayBuffer());
  writeFileSync(path.join(OUT, `${name}.body`), body);
  const j = JSON.parse(body.toString());
  rows.push({ name, id: id.slice(0, 8) + '...', status: res.status, channel: j.channel, buildId: j.buildId, version: j.version, branch: j.branch, etag: res.headers.get('etag') });
  console.log(`${name} (${id.slice(0, 8)}…): status=${res.status} channel=${j.channel} buildId=${j.buildId} version=${j.version} etag=${res.headers.get('etag')}`);
  await sleep(700);
}
// also: empty header value
{
  const res = await fetch(URL, { headers: { 'user-agent': 'probe/1.0', 'logi-install-id': '', 'logi-app-version': '39.1.2' } });
  const body = Buffer.from(await res.arrayBuffer());
  const j = JSON.parse(body.toString());
  rows.push({ name: 'empty', id: '(empty)', status: res.status, channel: j.channel, buildId: j.buildId, version: j.version, etag: res.headers.get('etag') });
  console.log(`empty (): status=${res.status} channel=${j.channel} buildId=${j.buildId}`);
}
writeFileSync(path.join(OUT, 'summary.json'), JSON.stringify({ app: APP, requested_channel: CH, rows }, null, 2), 'utf8');
const chans = [...new Set(rows.map((r) => r.channel))];
console.log('distinct channels served:', chans.join(', '));
