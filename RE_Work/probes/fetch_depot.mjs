// Depot + endpoint fetcher via bun native TLS (sandbox pwsh TLS is broken).
const fs = await import("node:fs");
const dir = "C:/Users/Decode/source/repos/UpdateManager/RE_Work/probes";
const UA = "LGHUB/2026.6.957899 (Windows NT 10.0; x64)";

async function sha256hex(buf) {
  const h = await crypto.subtle.digest("SHA-256", buf);
  return [...new Uint8Array(h)].map(b => b.toString(16).padStart(2, "0")).join("");
}

const targets = [
  { name: "driver_audio_osx", url: "https://updates.ghub.logitechg.com/depots/eb219bff-a6f0-47d5-9013-085e8c0d0a3a/driver_audio_osx.depot", mac: "081a763d19114d02341e0c144ddae566827162752959ff9a808c5b0ae8e417f4" },
  { name: "g560_dfu", url: "https://updates.ghub.logitechg.com/depots/780f7572-689c-45d5-894e-706f02c8f13e/g560_dfu.depot", mac: null },
  { name: "applet_slobs", url: "https://updates.ghub.logitechg.com/depots/40ff5d18-6538-4854-96a5-82b897048b53/applet_slobs.depot", mac: null },
  { name: "lua_scripting", url: "https://updates.ghub.logitechg.com/depots/6a2b0da5-7d5d-4d43-b8c8-7801aa53e8f57/lua_scripting.depot", mac: null },
];

// read manifest for macs
const manifest = JSON.parse(fs.readFileSync(`${dir}/resp_pipeline_v2_update_ghub10_win_public_details.json.json`, "utf8"));
for (const t of targets) {
  if (!t.mac) {
    const d = manifest.depots.find(x => x.name === t.name);
    t.mac = d ? d.mac : null;
  }
}

const settingsUrl = "https://updates.ghub.logitechg.com/pipeline/v2/update/ghub10/win/public/settings";

for (const t of targets) {
  try {
    const r = await fetch(t.url, { headers: { "User-Agent": UA } });
    const buf = new Uint8Array(await r.arrayBuffer());
    const hex = await sha256hex(buf);
    fs.writeFileSync(`${dir}/depot_${t.name}.bin`, buf);
    const match = t.mac ? hex === t.mac.toLowerCase() : "?";
    console.log(`${t.name}\t${r.status}\t${buf.length} bytes\tmac-match=${match}\t${hex}`);
  } catch (e) {
    console.log(`${t.name}\tERR\t${e.message}`);
  }
  await new Promise(res => setTimeout(res, 400));
}

// settings endpoint probe (status + body only)
try {
  const r = await fetch(settingsUrl, { headers: { "User-Agent": UA } });
  const body = await r.text();
  fs.writeFileSync(`${dir}/probe_settings_https.out`, body);
  console.log(`settings\t${r.status}\t${body.length} bytes\t${body.slice(0, 200).replace(/\n/g, " ")}`);
} catch (e) {
  console.log(`settings\tERR\t${e.message}`);
}
