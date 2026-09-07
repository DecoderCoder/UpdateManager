// recover_staging.mjs
// staging_details = public_details with `"public"` -> `"staging"` at offset 57 (value bytes 57..64 pub / 57..65 stg).
// Derive full staging keystream, recover plaintext, analyze KS structure, dump keys section.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';
const here = path.dirname(fileURLToPath(import.meta.url));

const pub = fs.readFileSync(path.join(here, 'reprobe_channel_names_2026-09-07/public_details.body'));
const stg = fs.readFileSync(path.join(here, 'reprobe_channel_brute_2026-09-07/staging_details.body'));

const co = 46;                       // start of `"channel"`
const valStart = 57;                 // start of value (opening quote)
const pubValEnd = 65;                // byte after `"public"` closing quote (exclusive)
const stgValEnd = 66;                // byte after `"staging"` closing quote (exclusive)

console.log(`pub=${pub.length} stg=${stg.length} (expect stg=pub+1: ${stg.length === pub.length + 1})`);
// sanity: identical prefix up to valStart
let same = true;
for (let i = 0; i < valStart; i++) if (pub[i] !== stg[i]) { same = false; console.log('prefix mismatch at', i); break; }
console.log('prefix identical to', same ? valStart : '???');

const ks = Buffer.alloc(stg.length);
for (let i = 0; i < valStart; i++) ks[i] = stg[i] ^ pub[i];
// gap: stg[valStart..stgValEnd) = `"staging"`
const gap = Buffer.from('"staging"');
for (let j = 0; j < gap.length; j++) ks[valStart + j] = stg[valStart + j] ^ gap[j];
// tail: stg[i] ^ pub[i-1]
for (let i = stgValEnd; i < stg.length; i++) ks[i] = stg[i] ^ pub[i - 1];

fs.writeFileSync(path.join(here, 'reprobe_channel_brute_2026-09-07/staging_keystream.bin'), ks);
const dec = Buffer.alloc(stg.length);
for (let i = 0; i < stg.length; i++) dec[i] = stg[i] ^ ks[i];
fs.writeFileSync(path.join(here, 'reprobe_channel_brute_2026-09-07/staging_details_RECOVERED.json'), dec);

const txt = dec.toString('utf8');
// validate JSON
let ok = false, why = '';
try { JSON.parse(txt); ok = true; } catch (e) { why = e.message; }
console.log('recovered JSON parses:', ok, why);
console.log('---- head ----');
console.log(txt.slice(0, 500));
const ki = txt.lastIndexOf('"keys"');
console.log('---- keys section ----');
console.log(txt.slice(ki, ki + 300));

// ---- keystream structure analysis ----
console.log('---- KS structure ----');
// periodicity test up to 64KB
const N = 65536;
const periods = [];
for (const p of [1, 2, 4, 8, 16, 32, 64, 128, 256, 1024]) {
  let eq = 0;
  for (let i = 0; i + p < N; i++) if (ks[i] === ks[i + p]) eq++;
  const rate = eq / (N - p);
  periods.push({ p, rate });
  if (rate > 0.02) console.log(`period ${p}: match rate ${(rate * 100).toFixed(2)}%`);
}
if (!periods.some(p => p.rate > 0.02)) console.log('no period <= 1024 (random-looking)');

// AES-CTR structural test: if ks = E_k(IV+0)||E_k(IV+1)||..., adjacent 16B blocks are independent.
// We cannot verify without the key, but we CAN check the RC4 hypothesis later; first: chi-square uniformity
const hist = new Array(256).fill(0);
for (let i = 0; i < N; i++) hist[ks[i]]++;
const exp = N / 256;
let chi2 = 0;
for (const h of hist) chi2 += ((h - exp) ** 2) / exp;
console.log(`chi2 (256 bins, 64KB) = ${chi2.toFixed(1)} (expect ~255; >~350 = non-uniform)`);

// try: is ks consistent with RC4? Can't test without seed. Instead: check for 16B-block linearity? skip.
// Save KS stats
fs.writeFileSync(path.join(here, 'reprobe_channel_brute_2026-09-07/staging_ks_analysis.json'),
  JSON.stringify({ periods, chi2: chi2.toFixed(1) }, null, 2));
