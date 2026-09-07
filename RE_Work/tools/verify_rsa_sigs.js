// Offline RSA signature verification of a real G HUB depot capsule.
// TBS candidate: raw .depot file bytes. Key candidates: the 3 embedded PEMs.
// Digest candidates: sha1, sha256, sha512 (PKCS#1 v1.5).
const fs = require('fs');
const crypto = require('crypto');
const path = require('path');

const base = path.resolve(__dirname, '..', 'probes');
const depot = fs.readFileSync(path.join(base, 'depot_full_85875e86-f3e1-4e79-91ee-232575e2807f.bin'));
const manifest = JSON.parse(fs.readFileSync(path.join(base, 'resp_pipeline_v2_update_ghub10_win_public_details.json.json'), 'utf8'));
const d = manifest.depots.find(x => x.name === '85875e86-f3e1-4e79-91ee-232575e2807f');
const depotSig = Buffer.from(d.signatures.signatures[0].signature, 'hex');
const manifestSig = Buffer.from(manifest.signatures.signatures[0], 'hex');

const keys = [];
for (let i = 1; i <= 3; i++) {
  const f = path.join(base, `pipeline_pubkey_${i}.pem`);
  if (fs.existsSync(f)) keys.push([f, fs.readFileSync(f, 'utf8')]);
}

const digests = ['sha1', 'sha256', 'sha512'];

function tryVerify(tbs, tbsName, sig, label) {
  for (const [kf, pem] of keys) {
    for (const dgst of digests) {
      try {
        const ok = crypto.verify(`${dgst}WithRSAEncryption`, tbs, pem, sig);
        if (ok) console.log(`MATCH  tbs=${tbsName}  key=${path.basename(kf)}  digest=${dgst}  (${label})`);
      } catch (e) { /* keep going */ }
    }
  }
}

console.log('depot bytes:', depot.length, ' sha256:', crypto.createHash('sha256').update(depot).digest('hex'));
console.log('manifest mac field:', d.mac);
console.log('depot sig len:', depotSig.length, ' manifest sig len:', manifestSig.length);
console.log('--- verifying depot signature ---');
tryVerify(depot, 'raw depot bytes', depotSig, 'depot');
console.log('--- verifying manifest (top-level v2) signature ---');
tryVerify(Buffer.from(fs.readFileSync(path.join(base, 'resp_pipeline_v2_update_ghub10_win_public_details.json.json'))), 'raw details.json bytes', manifestSig, 'manifest');

// Also check: maybe manifest sig is over the JSON with some normalization.
// Try: minified JSON (no whitespace) of the same parsed object.
const minified = Buffer.from(JSON.stringify(manifest));
tryVerify(minified, 'minified details.json', manifestSig, 'manifest-minified');
