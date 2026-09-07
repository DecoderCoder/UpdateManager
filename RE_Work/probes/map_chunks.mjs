// Map the [u32 len][data] chain after the 2021 header.
import fs from "node:fs";
const dir = "C:/Users/Decode/source/repos/UpdateManager/RE_Work/probes";
const depotName = "85875e86-f3e1-4e79-91ee-232575e2807f";
const buf = fs.readFileSync(`${dir}/depot_full_${depotName}.bin`);
const dv = new DataView(buf.buffer);
const magic = dv.getUint32(0, true);
const jsonLen = dv.getUint32(4, true);
const hdrEnd = 8 + jsonLen;
console.log("magic", magic.toString(16), "jsonLen", jsonLen, "hdrEnd", hdrEnd, "filesize", buf.length);

let off = hdrEnd;
let i = 0;
while (off + 4 <= buf.length && i < 40) {
  const len = dv.getUint32(off, true);
  console.log(`chunk ${i}: @${off} len=${len} (0x${len.toString(16)}) end=${off + 4 + len}`);
  // peek first 16 bytes of data
  const data = buf.subarray(off + 4, Math.min(off + 4 + 16, buf.length));
  console.log("   ", [...data].map(b => b.toString(16).padStart(2, "0")).join(" "));
  off += 4 + len;
  i++;
}
console.log("chain ends at", off, "filesize", buf.length, off === buf.length ? "(exact EOF - chain confirmed)" : "(does NOT reach EOF)");
