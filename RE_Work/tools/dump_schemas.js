// Dump compact schema samples from the saved pipeline JSON responses.
// Usage: node RE_Work/tools/dump_schemas.js
import { readFileSync } from 'node:fs';

const P = (f) => `RE_Work/probes/${f}`;

const v2 = JSON.parse(readFileSync(P('resp_pipeline_v2_update_ghub10_win_public_details.json.json'), 'utf8'));
console.log('== v2 top-level keys:', Object.keys(v2).join(', '));
console.log('== v2 top-level (depots elided):', JSON.stringify({ ...v2, depots: `<${v2.depots.length} entries>` }));
console.log('== v2 depot[0]:', JSON.stringify(v2.depots[0]));
const depDeps = v2.depots.find((d) => d.dependsOn);
if (depDeps) console.log('== v2 depot with dependsOn:', JSON.stringify(depDeps));
const depReq = v2.depots.find((d) => d.required);
if (depReq) console.log('== v2 depot with required:', JSON.stringify({ ...depReq, signatures: '<...>' }));
console.log('== v2 keys:', JSON.stringify(v2.keys));

const v2sigs = v2.depots.filter((d) => d.signatures).length;
console.log(`== v2 depots with signatures: ${v2sigs}/${v2.depots.length}`);
const sigLens = new Set(v2.depots.flatMap((d) => (d.signatures ? d.signatures.signatures.map((s) => (s.signature || '').length) : [])));
console.log('== v2 depot signature hex lengths:', [...sigLens].join(', '));
console.log('== v2 top signatures:', JSON.stringify(v2.signatures));

const v1 = JSON.parse(readFileSync(P('resp_pipeline_v1_update_ghub10_win_public_details.json.json'), 'utf8'));
console.log('== v1 top-level keys:', Object.keys(v1).join(', '));
console.log('== v1 top-level (depots elided):', JSON.stringify({ ...v1, depots: `<${v1.depots.length} entries>` }));
console.log('== v1 depot[0]:', JSON.stringify(v1.depots[0]));
const v1withDep = v1.depots.find((d) => d.dependsOn);
if (v1withDep) console.log('== v1 depot with dependsOn:', JSON.stringify(v1withDep));

const c = JSON.parse(readFileSync(P('probe_access_ghub10_group_content.out'), 'utf8'));
console.log('== access content top keys:', Object.keys(c).join(', '), '| key count:', c.keys.length);
console.log('== access content[0..1]:', JSON.stringify(c.keys.slice(0, 2)));

const iat = JSON.parse(readFileSync(P('probe_access_ghub10_group_iat.out'), 'utf8'));
console.log('== access iat:', JSON.stringify(iat));

// Depot URL + dir-UUID uniqueness stats (v2)
const urls = v2.depots.map((d) => d.url);
const dirs = new Set(urls.map((u) => u.split('/')[2]));
console.log(`== v2 depot urls: ${urls.length}, distinct dir uuids: ${dirs.size}`);
console.log('== v2 sample url:', urls[0]);
