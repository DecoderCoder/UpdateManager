// Verify the plaintext-depot framing against the BINARY model:
//   [u32 magic 0x20170110][u32 len][files-list JSON]
//   then, per REGULAR file in list order: [u32 len][raw content]
//   (symlink entries - non-empty "link" - consume no data chunk)
// Offline only; reads RE_Work/probes/*.bin samples.
import { readFileSync, readdirSync } from "node:fs";
import { createHash } from "node:crypto";

const PLAIN_MAGIC = 0x20170110;

function hex(b) {
  return b.toString("hex");
}

for (const f of readdirSync("RE_Work/probes").filter((n) =>
  /^(depot_applet_slobs|depot_g560_dfu|depot_lua_scripting|depot_release_notes|depot_driver_audio_osx)\.bin$/.test(n),
)) {
  const bin = readFileSync(`RE_Work/probes/${f}`);
  const magic = bin.readUInt32LE(0);
  if (magic !== PLAIN_MAGIC) {
    console.log(`${f}: magic 0x${magic.toString(16)} != 0x20170110 — skipping`);
    continue;
  }
  let off = 4;
  const listLen = bin.readUInt32LE(off);
  off += 4;
  let listJson;
  try {
    listJson = JSON.parse(bin.subarray(off, off + listLen).toString("utf8"));
  } catch (e) {
    console.log(`${f}: files-list chunk not parseable (${e.message}); sample is likely truncated. first bytes: ${hex(bin.subarray(off, Math.min(off + 64, bin.length)))}`);
    continue;
  }
  off += listLen;
  const files = listJson.files ?? [];
  const regular = files.filter((x) => !x.link);
  console.log(`\n=== ${f} (${bin.length} B) ===`);
  console.log(
    `files-list: ${files.length} entries (${regular.length} regular, ${files.length - regular.length} symlink), fields=${Object.keys(files[0] ?? {})}`,
  );
  let ok = true;
  for (const [i, file] of files.entries()) {
    if (file.link) {
      console.log(`  [${i}] SYMLINK ${file.name} -> ${file.link}`);
      continue;
    }
    const len = bin.readUInt32LE(off);
    const start = off + 4;
    const end = start + len;
    if (end > bin.length) {
      console.log(`  [${i}] FILE ${file.name}: OOB (want ${len} B at ${start}, only ${bin.length - start} left)`);
      ok = false;
      break;
    }
    const content = bin.subarray(start, end);
    const sha = createHash("sha256").update(content).digest("hex");
    const expect = file.sha ?? "(no sha field)";
    const match = expect === "(no sha field)" ? "n/a" : sha === expect ? "MATCH" : `MISMATCH (${sha.slice(0, 12)}...)`;
    console.log(`  [${i}] FILE  ${file.name}: len=${len} mode=${(file.mode & 0o777).toString(8)} sha=${match}`);
    off = end;
  }
  console.log(ok ? `  RESULT: fully framed, exact EOF (off=${off}/${bin.length})` : `  RESULT: framing broke at off=${off}/${bin.length}`);
}
