// Fetch prefix of new-format (0x20210506) depots.
const fs = await import("node:fs");
const dir = "C:/Users/Decode/source/repos/UpdateManager/RE_Work/probes";
const UA = "LGHUB/2026.6.957899 (Windows NT 10.0; x64)";
const manifest = JSON.parse(fs.readFileSync(`${dir}/resp_pipeline_v2_update_ghub10_win_public_details.json.json`, "utf8"));

const targets = manifest.depots.filter(d =>
  d.name === "85875e86-f3e1-4e79-91ee-232575e2807f" || d.name === "81a7e5b3-9c1d-4800-9d6f-2a1975f897c6");

for (const d of targets) {
  const url = "https://updates.ghub.logitechg.com" + d.url;
  const r = await fetch(url, { headers: { "User-Agent": UA, "Range": "bytes=0-511" } });
  console.log(`${d.name} HTTP ${r.status} range=${r.headers.get("content-range")}`);
  const buf = new Uint8Array(await r.arrayBuffer());
  fs.writeFileSync(`${dir}/depot_prefix_${d.name}.bin`, buf);
  for (let off = 0; off < buf.length; off += 32) {
    const hex = [...buf.subarray(off, off + 32)].map(b => b.toString(16).padStart(2, "0")).join(" ");
    const asc = [...buf.subarray(off, off + 32)].map(b => (b >= 32 && b < 127) ? String.fromCharCode(b) : ".").join("");
    console.log(off.toString(16).padStart(8) + "  " + hex.padEnd(96) + "  " + asc);
  }
  console.log("");
}
