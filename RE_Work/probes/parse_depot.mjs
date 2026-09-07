// Parse depot section structure + public/private sampling.
const fs = await import("node:fs");
const dir = "C:/Users/Decode/source/repos/UpdateManager/RE_Work/probes";
const UA = "LGHUB/2026.6.957899 (Windows NT 10.0; x64)";

function parseDepot(buf, name) {
  const dv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
  const out = [];
  out.push({ off: 0, what: "header", magic: "0x" + dv.getUint32(0, true).toString(16), jsonLen: dv.getUint32(4, true) });
  let p = 8;
  let jsonHead = "";
  try {
    jsonHead = new TextDecoder().decode(buf.subarray(p, p + dv.getUint32(4, true)));
  } catch (e) { jsonHead = "<decode err>"; }
  out.push({ off: 8, what: "header-json", len: dv.getUint32(4, true), text: jsonHead });
  p += dv.getUint32(4, true);
  let i = 0;
  while (p + 4 <= buf.length) {
    const len = dv.getUint32(p, true);
    if (len === 0 || p + 4 + len > buf.length) {
      out.push({ off: p, what: "trailing/unknown", bytes: buf.length - p, text: new TextDecoder().decode(buf.subarray(p, Math.min(p + 200, buf.length))).replace(/\n/g, "\\n") });
      break;
    }
    p += 4;
    const chunk = buf.subarray(p, p + len);
    let text = null;
    try {
      const t = new TextDecoder("utf-8", { fatal: true }).decode(chunk);
      text = t.length > 300 ? t.slice(0, 300) + "…(" + t.length + " chars)" : t;
    } catch (e) { text = null; }
    out.push({ off: p - 4, what: "section[" + i + "]", len, printable: text !== null, preview: text ? text.replace(/\n/g, "\\n") : "(binary " + len + " bytes)" });
    p += len;
    i++;
  }
  console.log("### " + name + " (" + buf.length + " bytes)");
  for (const o of out) console.log(JSON.stringify(o));
  console.log("");
}

for (const n of ["driver_audio_osx", "g560_dfu", "applet_slobs"]) {
  parseDepot(new Uint8Array(fs.readFileSync(`${dir}/depot_${n}.bin`)), n);
}

// --- public/private sampling ---
const manifest = JSON.parse(fs.readFileSync(`${dir}/resp_pipeline_v2_update_ghub10_win_public_details.json.json`, "utf8"));
const sample = [
  "core", "core_runtime_win", "lua_scripting", "release_notes",
  "g102_g203", "g560_dfu", "driver_usb", "driver_hid_virtual",
  "applet_discord", "g213_us", "gl", "streamcam",
];
for (const nm of sample) {
  const d = manifest.depots.find(x => x.name === nm);
  if (!d) { console.log(`${nm}\t(n/a)`); continue; }
  const url = "https://updates.ghub.logitechg.com" + d.url;
  try {
    const r = await fetch(url, { method: "HEAD", headers: { "User-Agent": UA } });
    console.log(`${nm}\t${d.size}\tHEAD ${r.status}\t${r.headers.get("content-length") || ""}`);
  } catch (e) {
    console.log(`${nm}\t${d.size}\tERR ${e.message}`);
  }
  await new Promise(res => setTimeout(res, 250));
}
