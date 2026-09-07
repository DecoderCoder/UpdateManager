// Decrypt the depot signature with the embedded RSA public key (PKCS#1 v1.5
// publicDecrypt = same padding removal as verification) to expose the
// DigestInfo: tells us padding type, hash OID, and the exact digest bytes.
const fs = require('fs');
const crypto = require('crypto');
const path = require('path');

const base = path.resolve(__dirname, '..', 'probes');
const depot = fs.readFileSync(path.join(base, 'depot_full_85875e86-f3e1-4e79-91ee-232575e2807f.bin'));
const manifest = JSON.parse(fs.readFileSync(path.join(base, 'resp_pipeline_v2_update_ghub10_win_public_details.json.json'), 'utf8'));
const d = manifest.depots.find(x => x.name === '85875e86-f3e1-4e79-91ee-232575e2807f');
const depotSig = Buffer.from(d.signatures.signatures[0].signature, 'hex');
const manifestSig = Buffer.from(manifest.signatures.signatures[0], 'hex');

const pem4096 = fs.readFileSync(path.join(base, 'pipeline_pubkey_2.pem'), 'utf8'); // ghub key
const pem2048a = fs.readFileSync(path.join(base, 'pipeline_pubkey_1.pem'), 'utf8'); // updaterservice
const pem2048b = fs.readFileSync(path.join(base, 'pipeline_pubkey_3.pem'), 'utf8'); // optionsplus

const OIDs = {
  '2a864886f70d01010a300c06082a8648ce3d0403020500': 'SHA-1',
  '2a864886f70d01010b300d06096086480165030402010500': 'SHA-256',
  '2a864886f70d01010c300d06096086480165030402030500': 'SHA-512',
  '2a864886f70d010109300c06082a8648ce3d0403020500': 'MD5',
  '2a864886f70d010108300c06082a8648ce3d0205050500': 'SHA-1(RC)',
};

function forensic(sig, label) {
  console.log(`=== ${label} ===`);
  for (const [name, pem] of [['ghub-4096', pem4096], ['updaterservice-2048', pem2048a], ['optionsplus-2048', pem2048b]]) {
    try {
      const pt = crypto.publicDecrypt({ key: pem, padding: crypto.constants.RSA_PKCS1_PADDING }, sig);
      console.log(`key ${name}: padding OK, plaintext ${pt.length}B:`);
      const hex = pt.toString('hex');
      // DigestInfo DER:
      //  sha256: 3031 300d 0609 608648016503040201 0500 0420 <32B>
      //  sha1:   3021 3009 0605 2b0e03021a        0500 0414 <20B>
      //  sha512: 3041 300d 0609 608648016503040203 0500 0430 <64B>
      const m = hex.match(/3031300d060960864801650304020105000420([0-9a-f]{64})/);
      const m1 = hex.match(/3021300906052b0e03021a05000414([0-9a-f]{40})/);
      const m3 = hex.match(/3041300d060960864801650304020305000430([0-9a-f]{128})/);
      if (m) console.log(`  PKCS1v15 DigestInfo sha256 -> digest=${m[1]}`);
      if (m1) console.log(`  PKCS1v15 DigestInfo sha1   -> digest=${m1[1]}`);
      if (m3) console.log(`  PKCS1v15 DigestInfo sha512 -> digest=${m3[1]}`);
      if (!m && !m1 && !m3) console.log(`  no DigestInfo pattern; tail=${hex.slice(-96)}`);
    } catch (e) {
      console.log(`key ${name}: publicDecrypt failed: ${String(e.message).slice(0, 80)}`);
    }
  }
}
forensic(depotSig, 'depot signature (v1)');
forensic(manifestSig, 'manifest signature (v2)');

// Candidate TBS digests (sha256/sha1/sha512) for the depot
const cands = {
  'raw depot': depot,
  'mac hex string': Buffer.from(d.mac, 'ascii'),
  'mac raw': Buffer.from(d.mac, 'hex'),
  'depot minus last 16': depot.subarray(0, depot.length - 16),
  'depot minus first 8': depot.subarray(8),
  'header json (plain)': Buffer.from(JSON.stringify({})), // placeholder
  'depot name': Buffer.from(d.name, 'ascii'),
  'depot url': Buffer.from(d.url, 'ascii'),
  'name+size': Buffer.from(d.name + ':' + d.size),
};
console.log('--- candidate digests ---');
for (const [n, b] of Object.entries(cands)) {
  const s256 = crypto.createHash('sha256').update(b).digest('hex');
  const s1 = crypto.createHash('sha1').update(b).digest('hex');
  console.log(`${n}: sha256=${s256.slice(0, 32)}… sha1=${s1.slice(0, 20)}…`);
}
