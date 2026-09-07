// Range-fetch first 64 bytes of assorted depots to inventory magics.
const fs = await import("node:fs");
const dir = "C:/Users/Decode/source/repos/UpdateManager/RE_Work/probes";
const UA = "LGHUB/2026.6.957899 (Windows NT 10.0; x64)";
const manifest = JSON.parse(fs.readFileSync(`${dir}/resp_pipeline_v2_update_ghub10_win_public_details.json.json`, "utf8"));

const names = ["core", "core_apps", "core_assets", "core_runtime_win", "core_qt_win", "core_systray_win",
  "gl", "gl_resources", "driver_audio_apo", "driver_audio_devices", "driver_logi_lamparray",
  "g213_us", "g305", "g403", "g502", "streamcam", "logi_install_tool_win", "applet_discord", "c520_dfu"];

const results = [];
for (const nm of names) {
  const d = manifest.depots.find(x => x.name === nm);
  if (!d) { console.log(`${nm}\t(n/a)`); continue; }
  const url = "https://updates.ghub.logitechg.com" + d.url;
  try {
    const r = await fetch(url, { headers: { "User-Agent": UA, "Range": "bytes=0-7" } });
    if (r.status !== 206 && r.status !== 200) { console.log(`${nm}\t${d.size}\tHTTP ${r.status}`); continue; }
    const buf = new Uint8Array(await r.arrayBuffer());
    const dv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
    const magic = dv.getUint32(0, true);
    const jsonLen = buf.length >= 8 ? dv.getUint32(4, true) : -1;
    const mm = magic.toString(16).padStart(8, "0");
    const date = `${mm.slice(0, 2)}-${mm.slice(2, 4)}-${mm.slice(4, 6)}-${mm.slice(6, 8)}`;
    console.log(`${nm}\t${d.size}\t0x${mm}\t(${date})\tjsonLen=${jsonLen}`);
    results.push({ name: nm, size: d.size, magic: "0x" + mm, jsonLen });
  } catch (e) { console.log(`${nm}\tERR ${e.message}`); }
  await new Promise(res => setTimeout(res, 200));
}
fs.writeFileSync(`${dir}/magic_inventory.json`, JSON.stringify(results, null, 2));
console.log("");
const byMagic = {};
for (const r of results) { (byMagic[r.magic] = byMagic[r.magic] || []).push(r.name); }
for (const [m, ns] of Object.entries(byMagic)) console.log(`${m}: ${ns.join(", ")}`);
