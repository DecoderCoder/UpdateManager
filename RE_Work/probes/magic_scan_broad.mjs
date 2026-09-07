// Broad random magic scan (60 depots).
const fs = await import("node:fs");
const dir = "C:/Users/Decode/source/repos/UpdateManager/RE_Work/probes";
const UA = "LGHUB/2026.6.957899 (Windows NT 10.0; x64)";
const manifest = JSON.parse(fs.readFileSync(`${dir}/resp_pipeline_v2_update_ghub10_win_public_details.json.json`, "utf8"));

// deterministic pseudo-random sample of 60
let seed = 42;
const rand = () => (seed = (seed * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff;
const idx = new Set();
while (idx.size < 60) idx.add(Math.floor(rand() * manifest.depots.length));

const mags = {};
for (const i of idx) {
  const d = manifest.depots[i];
  const url = "https://updates.ghub.logitechg.com" + d.url;
  try {
    const r = await fetch(url, { headers: { "User-Agent": UA, "Range": "bytes=0-7" } });
    if (r.status !== 206 && r.status !== 200) { console.log(`${d.name}\t${r.status}`); continue; }
    const buf = new Uint8Array(await r.arrayBuffer());
    const magic = new DataView(buf.buffer, buf.byteOffset, 8).getUint32(0, true).toString(16).padStart(8, "0");
    mags[magic] = mags[magic] || [];
    mags[magic].push(`${d.name}(${d.size})`);
  } catch (e) { console.log(`${d.name}\tERR`); }
  await new Promise(res => setTimeout(res, 120));
}
for (const [m, ns] of Object.entries(mags)) {
  console.log(`0x${m}: ${ns.length} depots`);
  for (const n of ns.slice(0, 10)) console.log("   " + n);
  if (ns.length > 10) console.log("   ... +" + (ns.length - 10) + " more");
}
fs.writeFileSync(`${dir}/magic_scan_broad.json`, JSON.stringify(mags, null, 2));
