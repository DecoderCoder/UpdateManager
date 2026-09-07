// Print the plaintext depot headers of the saved depot samples.
// Depot header layout: [u32 magic LE][u32 jsonLen LE][header JSON]
// Usage: node RE_Work/tools/dump_depot_headers.js
import { readFileSync } from 'node:fs';

const P = (f) => `RE_Work/probes/${f}`;

function dumpHeader(file, maxJson = 1200) {
  const b = readFileSync(P(file));
  const magic = b.readUInt32LE(0);
  const jsonLen = b.readUInt32LE(4);
  const json = b.slice(8, 8 + jsonLen).toString('utf8');
  console.log(`\n===== ${file} =====`);
  console.log(`size=${b.length} magic=0x${magic.toString(16).padStart(8, '0')} headerJsonLen=${jsonLen} rest=${b.length - 8 - jsonLen}`);
  let obj;
  try { obj = JSON.parse(json); } catch { obj = null; }
  if (obj) {
    // Elide long fields for display
    const shallow = {};
    for (const [k, v] of Object.entries(obj)) {
      if (Array.isArray(v) && v.length > 3) shallow[k] = `<${v.length} items> first=${JSON.stringify(v[0])}`;
      else if (typeof v === 'string' && v.length > 200) shallow[k] = v.slice(0, 80) + `...<${v.length}>`;
      else shallow[k] = v;
    }
    console.log(JSON.stringify(shallow, null, 1));
  } else {
    console.log(json.slice(0, maxJson));
  }
  return { magic, jsonLen, json, obj };
}

dumpHeader('depot_full_85875e86-f3e1-4e79-91ee-232575e2807f.bin');
dumpHeader('depot_applet_slobs.bin');
dumpHeader('depot_lua_scripting.bin');
dumpHeader('depot_release_notes.bin');
dumpHeader('depot_g560_dfu.bin');
