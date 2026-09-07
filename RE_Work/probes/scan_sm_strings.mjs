// String sweep of lghub_software_manager.exe (and optional 2nd binary) to map
// pipeline URL construction, channel handling, and any crypto usage.
// Usage: node scan_sm_strings.mjs [path] [extra-regex]
import { readFileSync } from "node:fs";

const path = process.argv[2] ?? "C:/Program Files/LGHUB/lghub_software_manager.exe";
const extra = process.argv[3] ?? "";
const buf = readFileSync(path);
console.log(`scanning ${path} (${buf.length} bytes)`);

// Extract ASCII runs >= 5 chars with offsets
const ascii = [];
let run = 0, start = 0;
for (let i = 0; i < buf.length; i++) {
  const c = buf[i];
  if (c >= 0x20 && c < 0x7f) {
    if (run === 0) start = i;
    run++;
  } else {
    if (run >= 5) ascii.push({ off: start, s: buf.toString("latin1", start, start + run) });
    run = 0;
  }
}
if (run >= 5) ascii.push({ off: start, s: buf.toString("latin1", start, start + run) });

// UTF-16LE runs >= 5 chars
const utf16 = [];
run = 0; start = 0;
for (let i = 0; i + 1 < buf.length; i += 2) {
  const c = buf[i];
  if (c >= 0x20 && c < 0x7f && buf[i + 1] === 0) {
    if (run === 0) start = i;
    run++;
  } else {
    if (run >= 5) utf16.push({ off: start, s: buf.toString("utf16le", start, start + run * 2) });
    run = 0;
  }
}
if (run >= 5) utf16.push({ off: start, s: buf.toString("utf16le", start, start + run * 2) });

const patterns = [
  /pipeline/i, /details\.json/i, /update\.json/i, /channel/i,
  /ghub1[02]/i, /canary/i, /access/i, /key/i,
  /aes|gcm|cipher|encrypt|decrypt|pkcs|kdf|pbkdf/i,
  /mac|signature|rsa|sha/i,
  /install-id|app-version|User-Agent|downloader/i,
  /depot/i, /manifest/i, /settings/i, /version/i, /branch/i, /buildid/i,
  extra ? new RegExp(extra, "i") : null,
].filter(Boolean);

const seen = new Set();
for (const enc of [["ascii", ascii], ["utf16", utf16]]) {
  for (const p of patterns) {
    const hits = enc[1].filter((x) => p.test(x.s));
    for (const h of hits) {
      const key = h.s;
      if (seen.has(key)) continue;
      seen.add(key);
      console.log(`[${enc[0]} @0x${h.off.toString(16)}] ${key.length > 220 ? key.slice(0, 220) + "..." : key}`);
    }
  }
}
