// Fetch + parse newer-format depots.
const fs = await import("node:fs");
const dir = "C:/Users/Decode/source/repos/UpdateManager/RE_Work/probes";
const UA = "LGHUB/2026.6.957899 (Windows NT 10.0; x64)";
const manifest = JSON.parse(fs.readFileSync(`${dir}/resp_pipeline_v2_update_ghub10_win_public_details.json.json`, "utf8"));

function parseDepot(buf, name) {
  const dv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
  const magic = dv.getUint32(0, true);
  console.log("### " + name + " (" + buf.length + " bytes) magic=0x" + magic.toString(16) + " date=" +
    (magic >> 16) + "-" + String((magic >> 8) & 0xff).padStart(2, "0") + "-" + String(magic & 0xff).padStart(2, "0"));
  let p = 8;
  if (magic === 0x20170110) {
    const jsonLen = dv.getUint32(4, true);
    console.log("header-json (" + jsonLen + "): " + new TextDecoder().decode(buf.subarray(p, p + jsonLen)).replace(/\n/g, "\\n"));
    p += jsonLen;
    let i = 0;
    while (p + 4 <= buf.length) {
      const len = dv.getUint32(p, true);
      if (len === 0 || p + 4 + len > buf.length) break;
      p += 4;
      const chunk = buf.subarray(p, p + len);
      let printable = true;
      try { new TextDecoder("utf-8", { fatal: true }).decode(chunk); } catch (e) { printable = false; }
      const preview = printable ? new TextDecoder().decode(chunk.subarray(0, 120)).replace(/\n/g, "\\n") :
        "binary: " + [...chunk.subarray(0, 24)].map(b => b.toString(16).padStart(2, "0")).join(" ");
      console.log(`  section[${i}] len=${len} printable=${printable} :: ${preview}`);
      p += len;
      i++;
    }
  } else {
    // unknown magic: dump first 128 bytes hex + try to find embedded JSON
    console.log("first 128: " + [...buf.subarray(0, 128)].map(b => b.toString(16).padStart(2, "0")).join(" "));
    console.log("bytes 8..64: " + [...buf.subarray(8, 64)].map(b => b.toString(16).padStart(2, "0")).join(" "));
  }
  console.log("");
}

for (const nm of ["lua_scripting", "release_notes"]) {
  const d = manifest.depots.find(x => x.name === nm);
  const url = "https://updates.ghub.logitechg.com" + d.url;
  for (let attempt = 1; attempt <= 3; attempt++) {
    try {
      const r = await fetch(url, { headers: { "User-Agent": UA } });
      if (r.status !== 200) { console.log(`${nm} attempt ${attempt}: HTTP ${r.status}`); await new Promise(res => setTimeout(res, 1500)); continue; }
      const buf = new Uint8Array(await r.arrayBuffer());
      fs.writeFileSync(`${dir}/depot_${nm}.bin`, buf);
      console.log(`${nm} fetched ${buf.length} bytes`);
      parseDepot(buf, nm);
      break;
    } catch (e) { console.log(`${nm} attempt ${attempt}: ERR ${e.message}`); }
  }
  await new Promise(res => setTimeout(res, 500));
}
