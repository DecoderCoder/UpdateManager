// Dump all ASCII + UTF-16LE strings (>=5 chars) of a binary to a text file.
// Usage: node dump_strings.mjs <in> <out>
import { readFileSync, writeFileSync } from "node:fs";

const [path, out] = process.argv.slice(2);
const buf = readFileSync(path);
const outLines = [];
let run = 0, start = 0;
for (let i = 0; i < buf.length; i++) {
  const c = buf[i];
  if (c >= 0x20 && c < 0x7f) {
    if (run === 0) start = i;
    run++;
  } else {
    if (run >= 5) outLines.push(`A 0x${start.toString(16)} ${buf.toString("latin1", start, start + run)}`);
    run = 0;
  }
}
if (run >= 5) outLines.push(`A 0x${start.toString(16)} ${buf.toString("latin1", start, start + run)}`);
run = 0; start = 0;
for (let i = 0; i + 1 < buf.length; i += 2) {
  const c = buf[i];
  if (c >= 0x20 && c < 0x7f && buf[i + 1] === 0) {
    if (run === 0) start = i;
    run++;
  } else {
    if (run >= 5) outLines.push(`W 0x${start.toString(16)} ${buf.toString("utf16le", start, start + run * 2)}`);
    run = 0;
  }
}
if (run >= 5) outLines.push(`W 0x${start.toString(16)} ${buf.toString("utf16le", start, start + run * 2)}`);
writeFileSync(out, outLines.join("\n") + "\n");
console.log(`wrote ${outLines.length} strings to ${out}`);
